#include "GraphicsContext.h"

namespace WIP3D
{

	RenderContext::SharedPtr RenderContext::create(CommandQueueHandle queue)
	{
		return SharedPtr(new RenderContext(queue));
	}

    CopyContext::~CopyContext() = default;

    CopyContext::CopyContext(LowLevelContextData::CommandQueueType type, CommandQueueHandle queue)
    {
        mpLowLevelData = LowLevelContextData::create(type, queue);
        assert(mpLowLevelData);
    }

    CopyContext::SharedPtr CopyContext::create(CommandQueueHandle queue)
    {
        assert(queue);
        return SharedPtr(new CopyContext(LowLevelContextData::CommandQueueType::Copy, queue));
    }

    void CopyContext::flush(bool wait)
    {
        if (mCommandsPending)
        {
            mpLowLevelData->flush();
            mCommandsPending = false;
        }
        else
        {
            // We need to signal even if there are no commands to execute. We need this because some resources may have been released since the last flush(), and unless we signal they will not be released
            // 触发信号以便释放资源
            // 参考
            mpLowLevelData->getFence()->gpuSignal(mpLowLevelData->getCommandQueue());
        }

        bindDescriptorHeaps();

        if (wait)
        {
            mpLowLevelData->getFence()->syncCpu();
        }
    }

    CopyContext::ReadTextureTask::SharedPtr CopyContext::asyncReadTextureSubresource(const Texture * pTexture, uint32_t subresourceIndex)
    {
        return CopyContext::ReadTextureTask::create(this, pTexture, subresourceIndex);
    }

    std::vector<uint8_t> CopyContext::readTextureSubresource(const Texture * pTexture, uint32_t subresourceIndex)
    {
        CopyContext::ReadTextureTask::SharedPtr pTask = asyncReadTextureSubresource(pTexture, subresourceIndex);
        return pTask->getData();
    }

    bool CopyContext::resourceBarrier(const Resource * pResource, Resource::State newState, const ResourceViewInfo * pViewInfo)
    {
        const Texture* pTexture = dynamic_cast<const Texture*>(pResource);
        if (pTexture)
        {
            bool globalBarrier = pTexture->isStateGlobal();
            if (pViewInfo)
            {
                globalBarrier = globalBarrier && pViewInfo->firstArraySlice == 0;
                globalBarrier = globalBarrier && pViewInfo->mostDetailedMip == 0;
                globalBarrier = globalBarrier && pViewInfo->mipCount == pTexture->getMipCount();
                globalBarrier = globalBarrier && pViewInfo->arraySize == pTexture->getArraySize();
            }

            if (globalBarrier)
            {
                return textureBarrier(pTexture, newState);
            }
            else
            {
                return subresourceBarriers(pTexture, newState, pViewInfo);
            }
        }
        else
        {
            const Buffer* pBuffer = dynamic_cast<const Buffer*>(pResource);
            return bufferBarrier(pBuffer, newState);
        }
    }

    bool CopyContext::subresourceBarriers(const Texture * pTexture, Resource::State newState, const ResourceViewInfo * pViewInfo)
    {
        ResourceViewInfo fullResource;
        bool setGlobal = false;
        if (pViewInfo == nullptr)
        {
            fullResource.arraySize = pTexture->getArraySize();
            fullResource.firstArraySlice = 0;
            fullResource.mipCount = pTexture->getMipCount();
            fullResource.mostDetailedMip = 0;
            setGlobal = true;
            pViewInfo = &fullResource;
        }

        bool entireViewTransitioned = true;

        for (uint32_t a = pViewInfo->firstArraySlice; a < pViewInfo->firstArraySlice + pViewInfo->arraySize; a++)
        {
            for (uint32_t m = pViewInfo->mostDetailedMip; m < pViewInfo->mipCount + pViewInfo->mostDetailedMip; m++)
            {
                Resource::State oldState = pTexture->getSubresourceState(a, m);
                if (oldState != newState)
                {
                    apiSubresourceBarrier(pTexture, newState, oldState, a, m);
                    if (setGlobal == false) pTexture->setSubresourceState(a, m, newState);
                    mCommandsPending = true;
                }
                else entireViewTransitioned = false;
            }
        }
        if (setGlobal) pTexture->setGlobalState(newState);
        return entireViewTransitioned;
    }

    void CopyContext::updateTextureData(const Texture * pTexture, const void* pData)
    {
        mCommandsPending = true;
        uint32_t subresourceCount = pTexture->getArraySize() * pTexture->getMipCount();
        if (pTexture->getType() == Texture::Type::TextureCube)
        {
            subresourceCount *= 6;
        }
        updateTextureSubresources(pTexture, 0, subresourceCount, pData);
    }

    void CopyContext::updateSubresourceData(const Texture * pDst, uint32_t subresource, const void* pData, const uint3 & offset, const uint3 & size)
    {
        mCommandsPending = true;
        updateTextureSubresources(pDst, subresource, 1, pData, offset, size);
    }

    void CopyContext::updateBuffer(const Buffer * pBuffer, const void* pData, size_t offset, size_t numBytes)
    {
        if (numBytes == 0)
        {
            numBytes = pBuffer->getSize() - offset;
        }

        if (pBuffer->adjustSizeOffsetParams(numBytes, offset) == false)
        {
            logWarning("CopyContext::updateBuffer() - size and offset are invalid. Nothing to update.");
            return;
        }

        mCommandsPending = true;
        // Allocate a buffer on the upload heap
        Buffer::SharedPtr pUploadBuffer = Buffer::create(numBytes, Buffer::BindFlags::None, Buffer::CpuAccess::Write, pData);

        copyBufferRegion(pBuffer, offset, pUploadBuffer.get(), 0, numBytes);
    }

    ComputeContext::SharedPtr ComputeContext::create(CommandQueueHandle queue)
    {
        auto pCtx = SharedPtr(new ComputeContext(LowLevelContextData::CommandQueueType::Compute, queue));
        pCtx->bindDescriptorHeaps(); // TODO: Should this be done here?
        return pCtx;
    }

    bool ComputeContext::applyComputeVars(ComputeVars* pVars, RootSignature* pRootSignature)
    {
        bool varsChanged = (pVars != mpLastBoundComputeVars);

        // FIXME TODO Temporary workaround
        varsChanged = true;

        if (pVars->apply(this, varsChanged, pRootSignature) == false)
        {
            logWarning("ComputeContext::applyComputeVars() - applying ComputeVars failed, most likely because we ran out of descriptors. Flushing the GPU and retrying");
            flush(true);
            if (!pVars->apply(this, varsChanged, pRootSignature))
            {
                logError("ComputeVars::applyComputeVars() - applying ComputeVars failed, most likely because we ran out of descriptors");
                return false;
            }
        }
        return true;
    }

    void ComputeContext::flush(bool wait)
    {
        CopyContext::flush(wait);
        mpLastBoundComputeVars = nullptr;
    }

    RenderContext::SharedPtr RenderContext::create(CommandQueueHandle queue)
    {
        return SharedPtr(new RenderContext(queue));
    }

    void RenderContext::clearFbo(const Fbo* pFbo, const float4& color, float depth, uint8_t stencil, FboAttachmentType flags)
    {
        bool hasDepthStencilTexture = pFbo->getDepthStencilTexture() != nullptr;
        ResourceFormat depthStencilFormat = hasDepthStencilTexture ? pFbo->getDepthStencilTexture()->getFormat() : ResourceFormat::Unknown;

        bool clearColor = (flags & FboAttachmentType::Color) != FboAttachmentType::None;
        bool clearDepth = hasDepthStencilTexture && ((flags & FboAttachmentType::Depth) != FboAttachmentType::None);
        bool clearStencil = hasDepthStencilTexture && ((flags & FboAttachmentType::Stencil) != FboAttachmentType::None) && isStencilFormat(depthStencilFormat);

        if (clearColor)
        {
            for (uint32_t i = 0; i < Fbo::getMaxColorTargetCount(); i++)
            {
                if (pFbo->getColorTexture(i))
                {
                    clearRtv(pFbo->getRenderTargetView(i).get(), color);
                }
            }
        }

        if (clearDepth || clearStencil)
        {
            clearDsv(pFbo->getDepthStencilView().get(), depth, stencil, clearDepth, clearStencil);
        }
    }

    void RenderContext::clearTexture(Texture* pTexture, const float4& clearColor)
    {
        assert(pTexture);

        // Check that the format is either Unorm, Snorm or float
        auto format = pTexture->getFormat();
        auto fType = getFormatType(format);
        if (fType == FormatType::Sint || fType == FormatType::Uint || fType == FormatType::Unknown)
        {
            logWarning("RenderContext::clearTexture() - Unsupported texture format " + to_string(format) + ". The texture format must be a normalized or floating-point format");
            return;
        }

        auto bindFlags = pTexture->getBindFlags();
        // Select the right clear based on the texture's binding flags
        if (is_set(bindFlags, Resource::BindFlags::RenderTarget)) clearRtv(pTexture->getRTV().get(), clearColor);
        else if (is_set(bindFlags, Resource::BindFlags::UnorderedAccess)) clearUAV(pTexture->getUAV().get(), clearColor);
        else if (is_set(bindFlags, Resource::BindFlags::DepthStencil))
        {
            if (isStencilFormat(format) && (clearColor.y != 0))
            {
                logWarning("RenderContext::clearTexture() - when clearing a depth-stencil texture the stencil value(clearColor.y) must be 0. Received " + std::to_string(clearColor.y) + ". Forcing stencil to 0");
            }
            clearDsv(pTexture->getDSV().get(), clearColor.r, 0);
        }
        else
        {
            logWarning("Texture::clear() - The texture does not have a bind flag that allows us to clear!");
        }
    }

    bool RenderContext::applyGraphicsVars(GraphicsVars* pVars, RootSignature* pRootSignature)
    {
        bool bindRootSig = (pVars != mpLastBoundGraphicsVars);
        if (pVars->apply(this, bindRootSig, pRootSignature) == false)
        {
            logWarning("RenderContext::prepareForDraw() - applying GraphicsVars failed, most likely because we ran out of descriptors. Flushing the GPU and retrying");
            flush(true);
            if (!pVars->apply(this, bindRootSig, pRootSignature))
            {
                logError("RenderContext::applyGraphicsVars() - applying GraphicsVars failed, most likely because we ran out of descriptors");
                return false;
            }
        }
        return true;
    }

    void RenderContext::flush(bool wait)
    {
        ComputeContext::flush(wait);
        mpLastBoundGraphicsVars = nullptr;
    }
}
