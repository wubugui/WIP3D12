#include "Device.h"
#include "Application.h"

namespace WIP3D
{
    void createNullViews();
    void releaseNullViews();

    Device::SharedPtr gpDevice;
	Device::SharedPtr Device::create(Window::SharedPtr& pWindow, const Desc& desc)
	{
        if (gpDevice)
        {
            LOG_WARN("Only supports a single device");
            return nullptr;
        }
        gpDevice = SharedPtr(new Device(pWindow, desc));
        if (gpDevice->init() == false) { gpDevice = nullptr; }
        return gpDevice;
	}

    

    bool Device::init()
    {
        //至少要创建Graphics Contex
        const uint32_t kDirectQueueIndex = (uint32_t)LowLevelContextData::CommandQueueType::Direct;
        assert(mDesc.cmdQueues[kDirectQueueIndex] > 0);

        if (apiInit() == false) return false;

        // Create the descriptor pools
        DescriptorPool::Desc poolDesc;
        // For DX12 there is no difference between the different SRV/UAV types. 
        // For Vulkan it matters, hence the #ifdef
        // DX12 guarantees at least 1,000,000 descriptors
        poolDesc.setDescCount(DescriptorPool::Type::TextureSrv, 1000000)
            .setDescCount(DescriptorPool::Type::Sampler, 2048)
            .setShaderVisible(true);
#ifndef WIP_D3D12
        poolDesc.setDescCount(DescriptorPool::Type::Cbv, 16 * 1024)
            .setDescCount(DescriptorPool::Type::TextureUav, 16 * 1024);
        poolDesc.setDescCount(DescriptorPool::Type::StructuredBufferSrv, 2 * 1024)
            .setDescCount(DescriptorPool::Type::StructuredBufferUav, 2 * 1024)
            .setDescCount(DescriptorPool::Type::TypedBufferSrv, 2 * 1024)
            .setDescCount(DescriptorPool::Type::TypedBufferUav, 2 * 1024)
            .setDescCount(DescriptorPool::Type::RawBufferSrv, 2 * 1024)
            .setDescCount(DescriptorPool::Type::RawBufferUav, 2 * 1024);
#endif
        mpFrameFence = GpuFence::create();
        mpGpuDescPool = DescriptorPool::create(poolDesc, mpFrameFence);
        poolDesc.setShaderVisible(false).setDescCount(DescriptorPool::Type::Rtv, 16 * 1024).setDescCount(DescriptorPool::Type::Dsv, 1024);
        mpCpuDescPool = DescriptorPool::create(poolDesc, mpFrameFence);

        mpUploadHeap = GpuMemoryHeap::create(GpuMemoryHeap::Type::Upload, 1024 * 1024 * 2, mpFrameFence);
        createNullViews();
        mpRenderContext = RenderContext::create(mCmdQueues[(uint32_t)LowLevelContextData::CommandQueueType::Direct][0]);

        assert(mpRenderContext);
        mpRenderContext->flush();  // This will bind the descriptor heaps.
        // TODO: Do we need to flush here or should RenderContext::create() bind the descriptor heaps automatically without flush? See #749.

        // Update the FBOs
        if (updateDefaultFBO(mpWindow->GetClientAreaSize().x, mpWindow->GetClientAreaSize().y, mDesc.colorFormat, mDesc.depthFormat) == false)
        {
            return false;
        }

        return true;
    }

    void Device::releaseFboData()
    {
        // First, delete all FBOs
        for (auto& pFbo : mpSwapChainFbos)
        {
            pFbo->attachColorTarget(nullptr, 0);
            pFbo->attachDepthStencilTarget(nullptr);
        }

        // Now execute all deferred releases
        decltype(mDeferredReleases)().swap(mDeferredReleases);
    }

    bool Device::updateDefaultFBO(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat)
    {
        ResourceHandle apiHandles[kSwapChainBuffersCount] = {};
        getApiFboData(width, height, colorFormat, depthFormat, apiHandles, mCurrentBackBufferIndex);

        for (uint32_t i = 0; i < kSwapChainBuffersCount; i++)
        {
            // Create a texture object
            auto pColorTex = Texture::SharedPtr(new Texture(width, height, 1, 1, 1, 1, colorFormat, Texture::Type::Texture2D, Texture::BindFlags::RenderTarget));
            pColorTex->mApiHandle = apiHandles[i];
            // Create the FBO if it's required
            if (mpSwapChainFbos[i] == nullptr) mpSwapChainFbos[i] = Fbo::create();
            mpSwapChainFbos[i]->attachColorTarget(pColorTex, 0);

            // Create a depth texture
            if (depthFormat != ResourceFormat::Unknown)
            {
                auto pDepth = Texture::create2D(width, height, depthFormat, 1, 1, nullptr, Texture::BindFlags::DepthStencil);
                mpSwapChainFbos[i]->attachDepthStencilTarget(pDepth);
            }
        }
        return true;
    }

    Fbo::SharedPtr Device::getSwapChainFbo() const
    {
        return mpSwapChainFbos[mCurrentBackBufferIndex];
    }

    std::weak_ptr<QueryHeap> Device::createQueryHeap(QueryHeap::Type type, uint32_t count)
    {
        QueryHeap::SharedPtr pHeap = QueryHeap::create(type, count);
        mTimestampQueryHeaps.push_back(pHeap);
        return pHeap;
    }

    void Device::releaseResource(ApiObjectHandle pResource)
    {
        if (pResource)
        {
            // Some static objects get here when the application exits
            if (this)
            {
                mDeferredReleases.push({ mpFrameFence->getCpuValue(), pResource });
            }
        }
    }

    bool Device::isFeatureSupported(SupportedFeatures flags) const
    {
        return is_set(mSupportedFeatures, flags);
    }

    void Device::executeDeferredReleases()
    {
        mpUploadHeap->executeDeferredReleases();
        uint64_t gpuVal = mpFrameFence->getGpuValue();
        while (mDeferredReleases.size() && mDeferredReleases.front().frameID <= gpuVal)
        {
            mDeferredReleases.pop();
        }
        mpCpuDescPool->executeDeferredReleases();
        mpGpuDescPool->executeDeferredReleases();
    }

    void Device::toggleVSync(bool enable)
    {
        mDesc.enableVsync = enable;
    }

    void Device::cleanup()
    {
        toggleFullScreen(false);
        mpRenderContext->flush(true);
        // Release all the bound resources. Need to do that before deleting the RenderContext
        for (uint32_t i = 0; i < ARRAY_COUNT(mCmdQueues); i++) mCmdQueues[i].clear();
        for (uint32_t i = 0; i < kSwapChainBuffersCount; i++) mpSwapChainFbos[i].reset();
        mDeferredReleases = decltype(mDeferredReleases)();
        releaseNullViews();
        mpRenderContext.reset();
        mpUploadHeap.reset();
        mpCpuDescPool.reset();
        mpGpuDescPool.reset();
        mpFrameFence.reset();
        for (auto& heap : mTimestampQueryHeaps) heap.reset();

        destroyApiObjects();
        mpWindow.reset();
    }

    void Device::present()
    {
        mpRenderContext->resourceBarrier(mpSwapChainFbos[mCurrentBackBufferIndex]->getColorTexture(0).get(), Resource::State::Present);
        mpRenderContext->flush();
        apiPresent();
        mpFrameFence->gpuSignal(mpRenderContext->getLowLevelData()->getCommandQueue());
        if (mpFrameFence->getCpuValue() >= kSwapChainBuffersCount) mpFrameFence->syncCpu(mpFrameFence->getCpuValue() - kSwapChainBuffersCount);
        executeDeferredReleases();
        mFrameID++;
    }

    void Device::flushAndSync()
    {
        mpRenderContext->flush(true);
        mpFrameFence->gpuSignal(mpRenderContext->getLowLevelData()->getCommandQueue());
        executeDeferredReleases();
    }

    Fbo::SharedPtr Device::resizeSwapChain(uint32_t width, uint32_t height)
    {
        assert(width > 0 && height > 0);

        mpRenderContext->flush(true);

        // Store the FBO parameters
        ResourceFormat colorFormat = mpSwapChainFbos[0]->getColorTexture(0)->getFormat();
        const auto& pDepth = mpSwapChainFbos[0]->getDepthStencilTexture();
        ResourceFormat depthFormat = pDepth ? pDepth->getFormat() : ResourceFormat::Unknown;

        // updateDefaultFBO() attaches the resized swapchain to new Texture objects, with Undefined resource state.
        // This is fine in Vulkan because a new swapchain is created, but D3D12 can resize without changing
        // internal resource state, so we must cache the Falcor resource state to track it correctly in the new Texture object.
        // #TODO Is there a better place to cache state within D3D12 implementation instead of #ifdef-ing here?

#ifdef WIP_D3D12
        // Save FBO resource states
        std::array<Resource::State, kSwapChainBuffersCount> fboColorStates;
        std::array<Resource::State, kSwapChainBuffersCount> fboDepthStates;
        for (uint32_t i = 0; i < kSwapChainBuffersCount; i++)
        {
            assert(mpSwapChainFbos[i]->getColorTexture(0)->isStateGlobal());
            fboColorStates[i] = mpSwapChainFbos[i]->getColorTexture(0)->getGlobalState();

            const auto& pSwapChainDepth = mpSwapChainFbos[i]->getDepthStencilTexture();
            if (pSwapChainDepth != nullptr)
            {
                assert(pSwapChainDepth->isStateGlobal());
                fboDepthStates[i] = pSwapChainDepth->getGlobalState();
            }
        }
#endif

        assert(mpSwapChainFbos[0]->getSampleCount() == 1);

        // Delete all the FBOs
        releaseFboData();
        apiResizeSwapChain(width, height, colorFormat);
        updateDefaultFBO(width, height, colorFormat, depthFormat);

#ifdef WIP_D3D12
        // Restore FBO resource states
        for (uint32_t i = 0; i < kSwapChainBuffersCount; i++)
        {
            assert(mpSwapChainFbos[i]->getColorTexture(0)->isStateGlobal());
            mpSwapChainFbos[i]->getColorTexture(0)->setGlobalState(fboColorStates[i]);
            const auto& pSwapChainDepth = mpSwapChainFbos[i]->getDepthStencilTexture();
            if (pSwapChainDepth != nullptr)
            {
                assert(pSwapChainDepth->isStateGlobal());
                pSwapChainDepth->setGlobalState(fboDepthStates[i]);
            }
        }
#endif

#if !defined(WIP_D3D12) && !defined(WIP_VK)
#error Verify state handling on swapchain resize for this API
#endif

        return getSwapChainFbo();
    }
}
