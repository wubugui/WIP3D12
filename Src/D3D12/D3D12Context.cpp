#include "../Device.h"
#include "../GraphicsContext.h"
#include "../Util.h"

namespace WIP3D
{
    struct LowLevelContextApiData
    {
        FencedPool<CommandAllocatorHandle>::SharedPtr pAllocatorPool;
    };
    template<D3D12_COMMAND_LIST_TYPE type>
    static ID3D12CommandAllocatorPtr newCommandAllocator(void* pUserData)
    {
        ID3D12CommandAllocatorPtr pAllocator;
        if (FAILED(gpDevice->getApiHandle()->CreateCommandAllocator(type, IID_PPV_ARGS(&pAllocator))))
        {
            logError("Failed to create command allocator");
            return nullptr;
        }
        return pAllocator;
    }

    template<typename ApiType>
    ApiType createCommandList(ID3D12Device* pDevice, D3D12_COMMAND_LIST_TYPE type, CommandAllocatorHandle allocator)
    {
        ApiType pList;
        HRESULT hr = pDevice->CreateCommandList(0, type, allocator, nullptr, IID_PPV_ARGS(&pList));
        return (FAILED(hr)) ? nullptr : pList;
    }

    LowLevelContextData::SharedPtr LowLevelContextData::create(CommandQueueType type, CommandQueueHandle queue)
    {
        return SharedPtr(new LowLevelContextData(type, queue));
    }

    LowLevelContextData::LowLevelContextData(CommandQueueType type, CommandQueueHandle queue)
        : mType(type)
        , mpQueue(queue)
    {
        mpFence = GpuFence::create();
        mpApiData = new LowLevelContextApiData;
        assert(mpFence && mpApiData);

        // Create a command allocator
        D3D12_COMMAND_LIST_TYPE cmdListType = gpDevice->getApiCommandQueueType(type);
        switch (cmdListType)
        {
        case D3D12_COMMAND_LIST_TYPE_DIRECT:
            mpApiData->pAllocatorPool = FencedPool<CommandAllocatorHandle>::create(mpFence, newCommandAllocator<D3D12_COMMAND_LIST_TYPE_DIRECT>);
            break;
        case D3D12_COMMAND_LIST_TYPE_COMPUTE:
            mpApiData->pAllocatorPool = FencedPool<CommandAllocatorHandle>::create(mpFence, newCommandAllocator<D3D12_COMMAND_LIST_TYPE_COMPUTE>);
            break;
        case D3D12_COMMAND_LIST_TYPE_COPY:
            mpApiData->pAllocatorPool = FencedPool<CommandAllocatorHandle>::create(mpFence, newCommandAllocator<D3D12_COMMAND_LIST_TYPE_COPY>);
            break;
        default:
            should_not_get_here();
        }
        mpAllocator = mpApiData->pAllocatorPool->newObject();
        assert(mpAllocator);

        d3d_call(gpDevice->getApiHandle()->CreateCommandList(0, cmdListType, mpAllocator, nullptr, IID_PPV_ARGS(&mpList)));
        assert(mpList);
    }

    LowLevelContextData::~LowLevelContextData()
    {
        safe_delete(mpApiData);
    }

    void LowLevelContextData::flush()
    {
        // 关闭list record commands，会校验所有的records，有返回值
        // https://docs.microsoft.com/zh-cn/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-close
        d3d_call(mpList->Close());
        ID3D12CommandList* pList = mpList.GetInterfacePtr();
        assert(mpQueue);
        // https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-executecommandlists
        // 两个commandlist分别调一次此函数保证有序，但同时传进去，就不一定，可能两个是同时执行的，也可能第二个先执行
        // 优化：尽量多的仅使用一次ExecuteCommandLists提交所有的command lists
        mpQueue->ExecuteCommandLists(1, &pList);
        // 发出同步指令，但是并不在这里等待
        mpFence->gpuSignal(mpQueue);
        // 申请一个新的分配器
        // 此处申请的对象并不一定就是cache的，因为前一条指令并不一定就已经执行完毕了
        mpAllocator = mpApiData->pAllocatorPool->newObject();
        // 此分配器可能是cache的，而不是新建的，调用reset以便重用
        // https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandallocator-reset
        d3d_call(mpAllocator->Reset());
        // 第二个参数可以是空，参考文档
        // https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-reset
        d3d_call(mpList->Reset(mpAllocator, nullptr));
    }

    void CopyContext::bindDescriptorHeaps()
    {
        const DescriptorPool* pGpuPool = gpDevice->getGpuDescriptorPool().get();
        const DescriptorPool::ApiData* pData = pGpuPool->getApiData();
        ID3D12DescriptorHeap* pHeaps[ARRAY_COUNT(pData->pHeaps)];
        uint32_t heapCount = 0;
        for (uint32_t i = 0; i < ARRAY_COUNT(pData->pHeaps); i++)
        {
            if (pData->pHeaps[i])
            {
                pHeaps[heapCount] = pData->pHeaps[i]->getApiHandle();
                heapCount++;
            }
        }
        mpLowLevelData->getCommandList()->SetDescriptorHeaps(heapCount, pHeaps);
    }

    //
    void copySubresourceData(const D3D12_SUBRESOURCE_DATA& srcData, 
        const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& dstFootprint, uint8_t* pDstStart, 
        uint64_t rowSize, uint64_t rowsToCopy)
    {
        const uint8_t* pSrc = (uint8_t*)srcData.pData;
        uint8_t* pDst = pDstStart + dstFootprint.Offset;
        const D3D12_SUBRESOURCE_FOOTPRINT& dstData = dstFootprint.Footprint;

        for (uint32_t z = 0; z < dstData.Depth; z++)
        {
            uint8_t* pDstSlice = pDst + rowsToCopy * dstData.RowPitch * z;
            const uint8_t* pSrcSlice = pSrc + srcData.SlicePitch * z;

            for (uint32_t y = 0; y < rowsToCopy; y++)
            {
                const uint8_t* pSrcRow = pSrcSlice + srcData.RowPitch * y;
                uint8_t* pDstRow = pDstSlice + dstData.RowPitch * y;
                memcpy(pDstRow, pSrcRow, rowSize);
            }
        }
    }

    void CopyContext::updateTextureSubresources(const Texture* pTexture, uint32_t firstSubresource, uint32_t subresourceCount, const void* pData, const uint3& offset, const uint3& size)
    {
        bool copyRegion = (offset != uint3(0)) || (size != uint3(-1));
        assert(subresourceCount == 1 || (copyRegion == false));

        mCommandsPending = true;

        uint32_t arraySize = (pTexture->getType() == Texture::Type::TextureCube) ? pTexture->getArraySize() * 6 : pTexture->getArraySize();
        assert(firstSubresource + subresourceCount <= arraySize * pTexture->getMipCount());

        // Get the footprint
        D3D12_RESOURCE_DESC texDesc = pTexture->getApiHandle()->GetDesc();
        std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprint(subresourceCount);
        std::vector<uint32_t> rowCount(subresourceCount);
        std::vector<uint64_t> rowSize(subresourceCount);
        uint64_t bufferSize;

        if (copyRegion)
        {
            footprint[0].Offset = 0;
            footprint[0].Footprint.Format = getDxgiFormat(pTexture->getFormat());
            uint32_t mipLevel = pTexture->getSubresourceMipLevel(firstSubresource);
            footprint[0].Footprint.Width = (size.x == -1) ? pTexture->getWidth(mipLevel) - offset.x : size.x;
            footprint[0].Footprint.Height = (size.y == -1) ? pTexture->getHeight(mipLevel) - offset.y : size.y;
            footprint[0].Footprint.Depth = (size.z == -1) ? pTexture->getDepth(mipLevel) - offset.z : size.z;
            footprint[0].Footprint.RowPitch = align_to(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT, footprint[0].Footprint.Width * getFormatBytesPerBlock(pTexture->getFormat()));
            rowCount[0] = footprint[0].Footprint.Height;
            rowSize[0] = footprint[0].Footprint.RowPitch;
            bufferSize = rowSize[0] * rowCount[0] * footprint[0].Footprint.Depth;
        }
        else
        {
            ID3D12Device* pDevice = gpDevice->getApiHandle();
            pDevice->GetCopyableFootprints(&texDesc, firstSubresource, subresourceCount, 0, footprint.data(), rowCount.data(), rowSize.data(), &bufferSize);
        }

        // Allocate a buffer on the upload heap
        Buffer::SharedPtr pBuffer = Buffer::create(bufferSize, Buffer::BindFlags::None, Buffer::CpuAccess::Write, nullptr);
        // Map the buffer
        uint8_t* pDst = (uint8_t*)pBuffer->map(Buffer::MapType::WriteDiscard);
        ID3D12ResourcePtr pResource = pBuffer->getApiHandle();

        // Get the offset from the beginning of the resource
        uint64_t vaOffset = pBuffer->getGpuAddressOffset();
        resourceBarrier(pTexture, Resource::State::CopyDest);

        const uint8_t* pSrc = (uint8_t*)pData;
        for (uint32_t s = 0; s < subresourceCount; s++)
        {
            uint32_t physicalWidth = footprint[s].Footprint.Width / getFormatWidthCompressionRatio(pTexture->getFormat());
            uint32_t physicalHeight = footprint[s].Footprint.Height / getFormatHeightCompressionRatio(pTexture->getFormat());

            D3D12_SUBRESOURCE_DATA src;
            src.pData = pSrc;
            src.RowPitch = physicalWidth * getFormatBytesPerBlock(pTexture->getFormat());
            src.SlicePitch = src.RowPitch * physicalHeight;
            copySubresourceData(src, footprint[s], pDst, rowSize[s], rowCount[s]);
            pSrc = (uint8_t*)pSrc + footprint[s].Footprint.Depth * src.SlicePitch;

            // Dispatch a command
            footprint[s].Offset += vaOffset;
            uint32_t subresource = s + firstSubresource;
            D3D12_TEXTURE_COPY_LOCATION dstLoc = { pTexture->getApiHandle(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, subresource };
            D3D12_TEXTURE_COPY_LOCATION srcLoc = { pResource, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, footprint[s] };

            mpLowLevelData->getCommandList()->CopyTextureRegion(&dstLoc, offset.x, offset.y, offset.z, &srcLoc, nullptr);
        }

        pBuffer->unmap();
    }

    CopyContext::ReadTextureTask::SharedPtr CopyContext::ReadTextureTask::create(CopyContext* pCtx, const Texture* pTexture, uint32_t subresourceIndex)
    {
        SharedPtr pThis = SharedPtr(new ReadTextureTask);
        pThis->mpContext = pCtx;
        //Get footprint
        D3D12_RESOURCE_DESC texDesc = pTexture->getApiHandle()->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint = pThis->mFootprint;
        uint64_t rowSize;
        uint64_t size;
        ID3D12Device* pDevice = gpDevice->getApiHandle();
        pDevice->GetCopyableFootprints(&texDesc, subresourceIndex, 1, 0, &footprint, &pThis->mRowCount, &rowSize, &size);

        //Create buffer
        pThis->mpBuffer = Buffer::create(size, Buffer::BindFlags::None, Buffer::CpuAccess::Read, nullptr);

        //Copy from texture to buffer
        D3D12_TEXTURE_COPY_LOCATION srcLoc = { pTexture->getApiHandle(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, subresourceIndex };
        D3D12_TEXTURE_COPY_LOCATION dstLoc = { pThis->mpBuffer->getApiHandle(), D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, footprint };
        pCtx->resourceBarrier(pTexture, Resource::State::CopySource);
        pCtx->getLowLevelData()->getCommandList()->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
        pCtx->setPendingCommands(true);

        // Create a fence and signal
        pThis->mpFence = GpuFence::create();
        pCtx->flush(false);
        pThis->mpFence->gpuSignal(pCtx->getLowLevelData()->getCommandQueue());
        pThis->mTextureFormat = pTexture->getFormat();

        return pThis;
    }

    std::vector<uint8_t> CopyContext::ReadTextureTask::getData()
    {
        mpFence->syncCpu();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint = mFootprint;

        // Calculate row size. GPU pitch can be different because it is aligned to D3D12_TEXTURE_DATA_PITCH_ALIGNMENT
        assert(footprint.Footprint.Width % getFormatWidthCompressionRatio(mTextureFormat) == 0); // Should divide evenly
        uint32_t actualRowSize = (footprint.Footprint.Width / getFormatWidthCompressionRatio(mTextureFormat)) * getFormatBytesPerBlock(mTextureFormat);

        // Get buffer data
        std::vector<uint8_t> result;
        result.resize(mRowCount * actualRowSize * footprint.Footprint.Depth);
        uint8_t* pData = reinterpret_cast<uint8_t*>(mpBuffer->map(Buffer::MapType::Read));

        for (uint32_t z = 0; z < footprint.Footprint.Depth; z++)
        {
            const uint8_t* pSrcZ = pData + z * footprint.Footprint.RowPitch * mRowCount;
            uint8_t* pDstZ = result.data() + z * actualRowSize * mRowCount;
            for (uint32_t y = 0; y < mRowCount; y++)
            {
                const uint8_t* pSrc = pSrcZ + y * footprint.Footprint.RowPitch;
                uint8_t* pDst = pDstZ + y * actualRowSize;
                memcpy(pDst, pSrc, actualRowSize);
            }
        }

        mpBuffer->unmap();
        return result;
    }

    static void d3d12ResourceBarrier(const Resource* pResource, Resource::State newState, Resource::State oldState, uint32_t subresourceIndex, ID3D12GraphicsCommandList* pCmdList)
    {
        D3D12_RESOURCE_BARRIER barrier;
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = pResource->getApiHandle();
        barrier.Transition.StateBefore = getD3D12ResourceState(oldState);
        barrier.Transition.StateAfter = getD3D12ResourceState(newState);
        barrier.Transition.Subresource = subresourceIndex;

        // Check that resource has required bind flags for before/after state to be supported
        D3D12_RESOURCE_STATES beforeOrAfterState = barrier.Transition.StateBefore | barrier.Transition.StateAfter;
        if (beforeOrAfterState & D3D12_RESOURCE_STATE_RENDER_TARGET)
        {
            assert(is_set(pResource->getBindFlags(), Resource::BindFlags::RenderTarget));
        }

        if (beforeOrAfterState & (D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE))
        {
            assert(is_set(pResource->getBindFlags(), Resource::BindFlags::ShaderResource));
        }

        if (beforeOrAfterState & D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
        {
            assert(is_set(pResource->getBindFlags(), Resource::BindFlags::UnorderedAccess));
        }

        pCmdList->ResourceBarrier(1, &barrier);
    }

    static bool d3d12GlobalResourceBarrier(const Resource* pResource, Resource::State newState, ID3D12GraphicsCommandList* pCmdList)
    {
        if (pResource->getGlobalState() != newState)
        {
            d3d12ResourceBarrier(pResource, newState, pResource->getGlobalState(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, pCmdList);
            return true;
        }
        return false;
    }

    bool CopyContext::textureBarrier(const Texture* pTexture, Resource::State newState)
    {
        bool recorded = d3d12GlobalResourceBarrier(pTexture, newState, mpLowLevelData->getCommandList());
        pTexture->setGlobalState(newState);
        mCommandsPending = mCommandsPending || recorded;
        return recorded;
    }

    bool CopyContext::bufferBarrier(const Buffer* pBuffer, Resource::State newState)
    {
        if (pBuffer && pBuffer->getCpuAccess() != Buffer::CpuAccess::None) return false;
        bool recorded = d3d12GlobalResourceBarrier(pBuffer, newState, mpLowLevelData->getCommandList());
        pBuffer->setGlobalState(newState);
        mCommandsPending = mCommandsPending || recorded;
        return recorded;
    }

    void CopyContext::apiSubresourceBarrier(const Texture* pTexture, Resource::State newState, Resource::State oldState, uint32_t arraySlice, uint32_t mipLevel)
    {
        uint32_t subresourceIndex = pTexture->getSubresourceIndex(arraySlice, mipLevel);
        d3d12ResourceBarrier(pTexture, newState, oldState, subresourceIndex, mpLowLevelData->getCommandList());
    }

    void CopyContext::uavBarrier(const Resource* pResource)
    {
        D3D12_RESOURCE_BARRIER barrier;
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.UAV.pResource = pResource->getApiHandle();

        // Check that resource has required bind flags for UAV barrier to be supported
        static const Resource::BindFlags reqFlags = Resource::BindFlags::UnorderedAccess | Resource::BindFlags::AccelerationStructure;
        assert(is_set(pResource->getBindFlags(), reqFlags));
        mpLowLevelData->getCommandList()->ResourceBarrier(1, &barrier);
        mCommandsPending = true;
    }

    void CopyContext::copyResource(const Resource* pDst, const Resource* pSrc)
    {
        resourceBarrier(pDst, Resource::State::CopyDest);
        resourceBarrier(pSrc, Resource::State::CopySource);
        mpLowLevelData->getCommandList()->CopyResource(pDst->getApiHandle(), pSrc->getApiHandle());
        mCommandsPending = true;
    }

    void CopyContext::copySubresource(const Texture* pDst, uint32_t dstSubresourceIdx, const Texture* pSrc, uint32_t srcSubresourceIdx)
    {
        resourceBarrier(pDst, Resource::State::CopyDest);
        resourceBarrier(pSrc, Resource::State::CopySource);

        D3D12_TEXTURE_COPY_LOCATION pSrcCopyLoc;
        D3D12_TEXTURE_COPY_LOCATION pDstCopyLoc;

        pDstCopyLoc.pResource = pDst->getApiHandle();
        pDstCopyLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        pDstCopyLoc.SubresourceIndex = dstSubresourceIdx;

        pSrcCopyLoc.pResource = pSrc->getApiHandle();
        pSrcCopyLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        pSrcCopyLoc.SubresourceIndex = srcSubresourceIdx;

        mpLowLevelData->getCommandList()->CopyTextureRegion(&pDstCopyLoc, 0, 0, 0, &pSrcCopyLoc, NULL);
        mCommandsPending = true;
    }

    void CopyContext::copyBufferRegion(const Buffer* pDst, uint64_t dstOffset, const Buffer* pSrc, uint64_t srcOffset, uint64_t numBytes)
    {
        resourceBarrier(pDst, Resource::State::CopyDest);
        resourceBarrier(pSrc, Resource::State::CopySource);
        mpLowLevelData->getCommandList()->CopyBufferRegion(pDst->getApiHandle(), dstOffset, pSrc->getApiHandle(), pSrc->getGpuAddressOffset() + srcOffset, numBytes);
        mCommandsPending = true;
    }

    void CopyContext::copySubresourceRegion(const Texture* pDst, uint32_t dstSubresource, const Texture* pSrc, uint32_t srcSubresource, const uint3& dstOffset, const uint3& srcOffset, const uint3& size)
    {
        resourceBarrier(pDst, Resource::State::CopyDest);
        resourceBarrier(pSrc, Resource::State::CopySource);

        D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = dstSubresource;
        dstLoc.pResource = pDst->getApiHandle();

        D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcLoc.SubresourceIndex = srcSubresource;
        srcLoc.pResource = pSrc->getApiHandle();

        D3D12_BOX box;
        box.left = srcOffset.x;
        box.top = srcOffset.y;
        box.front = srcOffset.z;
        uint32_t mipLevel = pSrc->getSubresourceMipLevel(dstSubresource);
        box.right = (size.x == -1) ? pSrc->getWidth(mipLevel) - box.left : size.x;
        box.bottom = (size.y == -1) ? pSrc->getHeight(mipLevel) - box.top : size.y;
        box.back = (size.z == -1) ? pSrc->getDepth(mipLevel) - box.front : size.z;

        mpLowLevelData->getCommandList()->CopyTextureRegion(&dstLoc, dstOffset.x, dstOffset.y, dstOffset.z, &srcLoc, &box);

        mCommandsPending = true;
    }

    namespace
    {
        struct ComputeContextApiData
        {
            size_t refCount = 0;
            CommandSignatureHandle pDispatchCommandSig = nullptr;
            static void init();
            static void release();
        };

        ComputeContextApiData sApiData;

        void ComputeContextApiData::init()
        {
            if (!sApiData.pDispatchCommandSig)
            {
                D3D12_COMMAND_SIGNATURE_DESC sigDesc;
                sigDesc.NumArgumentDescs = 1;
                sigDesc.NodeMask = 0;
                D3D12_INDIRECT_ARGUMENT_DESC argDesc;
                sigDesc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
                argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
                sigDesc.pArgumentDescs = &argDesc;
                gpDevice->getApiHandle()->CreateCommandSignature(&sigDesc, nullptr, IID_PPV_ARGS(&sApiData.pDispatchCommandSig));
            }
            sApiData.refCount++;
        }

        void ComputeContextApiData::release()
        {
            sApiData.refCount--;
            if (sApiData.refCount == 0) sApiData = {};
        }
    }

    ComputeContext::ComputeContext(LowLevelContextData::CommandQueueType type, CommandQueueHandle queue)
        : CopyContext(type, queue)
    {
        assert(queue);
        ComputeContextApiData::init();
    }

    ComputeContext::~ComputeContext()
    {
        ComputeContextApiData::release();
    }

    bool ComputeContext::prepareForDispatch(ComputeState* pState, ComputeVars* pVars)
    {
        assert(pState);

        auto pCSO = pState->getCSO(pVars);

        // Apply the vars. Must be first because applyComputeVars() might cause a flush
        if (pVars)
        {
            if (applyComputeVars(pVars, pCSO->getDesc().getProgramKernels()->getRootSignature().get()) == false) return false;
        }
        else mpLowLevelData->getCommandList()->SetComputeRootSignature(RootSignature::getEmpty()->getApiHandle());

        mpLastBoundComputeVars = pVars;
        mpLowLevelData->getCommandList()->SetPipelineState(pCSO->getApiHandle());
        mCommandsPending = true;
        return true;
    }

    void ComputeContext::dispatch(ComputeState* pState, ComputeVars* pVars, const uint3& dispatchSize)
    {
        // Check dispatch dimensions. TODO: Should be moved into Falcor.
        if (dispatchSize.x > D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION ||
            dispatchSize.y > D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION ||
            dispatchSize.z > D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION)
        {
            logError("ComputePass::execute() - Dispatch dimension exceeds maximum. Skipping.");
            return;
        }

        if (prepareForDispatch(pState, pVars) == false) return;
        mpLowLevelData->getCommandList()->Dispatch(dispatchSize.x, dispatchSize.y, dispatchSize.z);
    }


    template<typename ClearType>
    void clearUavCommon(ComputeContext* pContext, const UnorderedAccessView* pUav, const ClearType& clear, ID3D12GraphicsCommandList* pList)
    {
        pContext->resourceBarrier(pUav->getResource(), Resource::State::UnorderedAccess);
        UavHandle uav = pUav->getApiHandle();
        if (typeid(ClearType) == typeid(float4))
        {
            pList->ClearUnorderedAccessViewFloat(uav->getGpuHandle(0), uav->getCpuHandle(0), pUav->getResource()->getApiHandle(), (float*)value_ptr(clear), 0, nullptr);
        }
        else if (typeid(ClearType) == typeid(uint4))
        {
            pList->ClearUnorderedAccessViewUint(uav->getGpuHandle(0), uav->getCpuHandle(0), pUav->getResource()->getApiHandle(), (uint32_t*)value_ptr(clear), 0, nullptr);
        }
        else
        {
            should_not_get_here();
        }
    }

    void ComputeContext::clearUAV(const UnorderedAccessView* pUav, const float4& value)
    {
        clearUavCommon(this, pUav, value, mpLowLevelData->getCommandList().GetInterfacePtr());
        mCommandsPending = true;
    }

    void ComputeContext::clearUAV(const UnorderedAccessView* pUav, const uint4& value)
    {
        clearUavCommon(this, pUav, value, mpLowLevelData->getCommandList().GetInterfacePtr());
        mCommandsPending = true;
    }

    void ComputeContext::clearUAVCounter(const Buffer::SharedPtr& pBuffer, uint32_t value)
    {
        if (pBuffer->getUAVCounter())
        {
            clearUAV(pBuffer->getUAVCounter()->getUAV().get(), uint4(value));
        }
    }

    void ComputeContext::dispatchIndirect(ComputeState* pState, ComputeVars* pVars, const Buffer* pArgBuffer, uint64_t argBufferOffset)
    {
        if (prepareForDispatch(pState, pVars) == false) return;
        resourceBarrier(pArgBuffer, Resource::State::IndirectArg);
        mpLowLevelData->getCommandList()->ExecuteIndirect(sApiData.pDispatchCommandSig, 1, pArgBuffer->getApiHandle(), argBufferOffset, nullptr, 0);
    }

    namespace
    {
        struct RenderContextApiData
        {
            size_t refCount = 0;

            CommandSignatureHandle pDrawCommandSig;
            CommandSignatureHandle pDrawIndexCommandSig;

            struct
            {
                std::shared_ptr<FullScreenPass> pPass;
                Fbo::SharedPtr pFbo;

                Sampler::SharedPtr pLinearSampler;
                Sampler::SharedPtr pPointSampler;
                Sampler::SharedPtr pLinearMinSampler;
                Sampler::SharedPtr pPointMinSampler;
                Sampler::SharedPtr pLinearMaxSampler;
                Sampler::SharedPtr pPointMaxSampler;

                ParameterBlock::SharedPtr pBlitParamsBuffer;
                float2 prevSrcRectOffset = float2(0, 0);
                float2 prevSrcReftScale = float2(0, 0);

                // Variable offsets in constant buffer
                UniformShaderVarOffset offsetVarOffset;
                UniformShaderVarOffset scaleVarOffset;
                ProgramReflection::BindLocation texBindLoc;

                // Parameters for complex blit
                float4 prevComponentsTransform[4] = { float4(0), float4(0), float4(0), float4(0) };
                UniformShaderVarOffset compTransVarOffset[4];

            } blitData;

            static void init();
            static void release();
        };

        RenderContextApiData sApiData;

        void RenderContextApiData::init()
        {
            assert(gpDevice);
            auto& blitData = sApiData.blitData;
            if (blitData.pPass == nullptr)
            {
                // Init the blit data.
                Program::Desc d;
                d.addShaderLibrary("Core/API/BlitReduction.slang").vsEntry("vs").psEntry("ps");
                blitData.pPass = FullScreenPass::create(d);
                blitData.pFbo = Fbo::create();
                assert(blitData.pPass && blitData.pFbo);

                blitData.pBlitParamsBuffer = blitData.pPass->getVars()->getParameterBlock("BlitParamsCB");
                blitData.offsetVarOffset = blitData.pBlitParamsBuffer->getVariableOffset("gOffset");
                blitData.scaleVarOffset = blitData.pBlitParamsBuffer->getVariableOffset("gScale");
                blitData.prevSrcRectOffset = float2(-1.0f);
                blitData.prevSrcReftScale = float2(-1.0f);

                Sampler::Desc desc;
                desc.setAddressingMode(Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp, Sampler::AddressMode::Clamp);
                desc.setReductionMode(Sampler::ReductionMode::Standard);
                desc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Point);
                blitData.pLinearSampler = Sampler::create(desc);
                desc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point);
                blitData.pPointSampler = Sampler::create(desc);
                // Min reductions.
                desc.setReductionMode(Sampler::ReductionMode::Min);
                desc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Point);
                blitData.pLinearMinSampler = Sampler::create(desc);
                desc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point);
                blitData.pPointMinSampler = Sampler::create(desc);
                // Max reductions.
                desc.setReductionMode(Sampler::ReductionMode::Max);
                desc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Point);
                blitData.pLinearMaxSampler = Sampler::create(desc);
                desc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point);
                blitData.pPointMaxSampler = Sampler::create(desc);

                const auto& pDefaultBlockReflection = blitData.pPass->getProgram()->getReflector()->getDefaultParameterBlock();
                blitData.texBindLoc = pDefaultBlockReflection->getResourceBinding("gTex");

                // Init the draw signature
                D3D12_COMMAND_SIGNATURE_DESC sigDesc;
                sigDesc.NumArgumentDescs = 1;
                sigDesc.NodeMask = 0;
                D3D12_INDIRECT_ARGUMENT_DESC argDesc;

                // Draw
                sigDesc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
                argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
                sigDesc.pArgumentDescs = &argDesc;
                d3d_call(gpDevice->getApiHandle()->CreateCommandSignature(&sigDesc, nullptr, IID_PPV_ARGS(&sApiData.pDrawCommandSig)));

                // Draw index
                sigDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
                argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
                sigDesc.pArgumentDescs = &argDesc;
                d3d_call(gpDevice->getApiHandle()->CreateCommandSignature(&sigDesc, nullptr, IID_PPV_ARGS(&sApiData.pDrawIndexCommandSig)));

                // Complex blit parameters

                blitData.compTransVarOffset[0] = blitData.pBlitParamsBuffer->getVariableOffset("gCompTransformR");
                blitData.compTransVarOffset[1] = blitData.pBlitParamsBuffer->getVariableOffset("gCompTransformG");
                blitData.compTransVarOffset[2] = blitData.pBlitParamsBuffer->getVariableOffset("gCompTransformB");
                blitData.compTransVarOffset[3] = blitData.pBlitParamsBuffer->getVariableOffset("gCompTransformA");
                blitData.prevComponentsTransform[0] = float4(1.0f, 0.0f, 0.0f, 0.0f);
                blitData.prevComponentsTransform[1] = float4(0.0f, 1.0f, 0.0f, 0.0f);
                blitData.prevComponentsTransform[2] = float4(0.0f, 0.0f, 1.0f, 0.0f);
                blitData.prevComponentsTransform[3] = float4(0.0f, 0.0f, 0.0f, 1.0f);
                for (uint32_t i = 0; i < 4; i++) blitData.pBlitParamsBuffer->setVariable(blitData.compTransVarOffset[i], blitData.prevComponentsTransform[i]);
            }

            sApiData.refCount++;
        }

        void RenderContextApiData::release()
        {
            sApiData.refCount--;
            if (sApiData.refCount == 0) sApiData = {};
        }
    }

    RenderContext::RenderContext(CommandQueueHandle queue)
        : ComputeContext(LowLevelContextData::CommandQueueType::Direct, queue)
    {
        RenderContextApiData::init();
    }

    RenderContext::~RenderContext()
    {
        RenderContextApiData::release();
    }

    void RenderContext::clearRtv(const RenderTargetView* pRtv, const float4& color)
    {
        resourceBarrier(pRtv->getResource(), Resource::State::RenderTarget);
        mpLowLevelData->getCommandList()->ClearRenderTargetView(pRtv->getApiHandle()->getCpuHandle(0), glm::value_ptr(color), 0, nullptr);
        mCommandsPending = true;
    }

    void RenderContext::clearDsv(const DepthStencilView* pDsv, float depth, uint8_t stencil, bool clearDepth, bool clearStencil)
    {
        uint32_t flags = clearDepth ? D3D12_CLEAR_FLAG_DEPTH : 0;
        flags |= clearStencil ? D3D12_CLEAR_FLAG_STENCIL : 0;

        resourceBarrier(pDsv->getResource(), Resource::State::DepthStencil);
        mpLowLevelData->getCommandList()->ClearDepthStencilView(pDsv->getApiHandle()->getCpuHandle(0), D3D12_CLEAR_FLAGS(flags), depth, stencil, 0, nullptr);
        mCommandsPending = true;
    }

    static void D3D12SetVao(RenderContext* pCtx, ID3D12GraphicsCommandList* pList, const Vao* pVao)
    {
        D3D12_VERTEX_BUFFER_VIEW vb[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
        D3D12_INDEX_BUFFER_VIEW ib = {};

        if (pVao)
        {
            // Get the vertex buffers
            for (uint32_t i = 0; i < pVao->getVertexBuffersCount(); i++)
            {
                const Buffer* pVB = pVao->getVertexBuffer(i).get();
                if (pVB)
                {
                    vb[i].BufferLocation = pVB->getGpuAddress();
                    vb[i].SizeInBytes = (uint32_t)pVB->getSize();
                    vb[i].StrideInBytes = pVao->getVertexLayout()->getBufferLayout(i)->getStride();
                    pCtx->resourceBarrier(pVB, Resource::State::VertexBuffer);
                }
            }

            const Buffer* pIB = pVao->getIndexBuffer().get();
            if (pIB)
            {
                ib.BufferLocation = pIB->getGpuAddress();
                ib.SizeInBytes = (uint32_t)pIB->getSize();
                ib.Format = getDxgiFormat(pVao->getIndexBufferFormat());
                pCtx->resourceBarrier(pIB, Resource::State::IndexBuffer);
            }
        }

        pList->IASetVertexBuffers(0, ARRAYSIZE(vb), vb);
        pList->IASetIndexBuffer(&ib);
    }

    static void D3D12SetFbo(RenderContext* pCtx, const Fbo* pFbo)
    {
        // We are setting the entire RTV array to make sure everything that was previously bound is detached.
        // We're using 2D null views for any unused slots.
        uint32_t colorTargets = Fbo::getMaxColorTargetCount();
        auto pNullRtv = RenderTargetView::getNullView(RenderTargetView::Dimension::Texture2D);
        std::vector<HeapCpuHandle> pRTV(colorTargets, pNullRtv->getApiHandle()->getCpuHandle(0));
        HeapCpuHandle pDSV = DepthStencilView::getNullView(DepthStencilView::Dimension::Texture2D)->getApiHandle()->getCpuHandle(0);

        if (pFbo)
        {
            for (uint32_t i = 0; i < colorTargets; i++)
            {
                auto pTexture = pFbo->getColorTexture(i);
                if (pTexture)
                {
                    pRTV[i] = pFbo->getRenderTargetView(i)->getApiHandle()->getCpuHandle(0);
                    pCtx->resourceBarrier(pTexture.get(), Resource::State::RenderTarget);
                }
            }

            auto& pTexture = pFbo->getDepthStencilTexture();
            if (pTexture)
            {
                pDSV = pFbo->getDepthStencilView()->getApiHandle()->getCpuHandle(0);
                if (pTexture)
                {
                    pCtx->resourceBarrier(pTexture.get(), Resource::State::DepthStencil);
                }
            }
        }
        ID3D12GraphicsCommandList* pCmdList = pCtx->getLowLevelData()->getCommandList().GetInterfacePtr();
        pCmdList->OMSetRenderTargets(colorTargets, pRTV.data(), FALSE, &pDSV);
    }

    static void D3D12SetSamplePositions(ID3D12GraphicsCommandList* pList, const Fbo* pFbo)
    {
        if (!pFbo) return;
        ID3D12GraphicsCommandList1* pList1;
        pList->QueryInterface(IID_PPV_ARGS(&pList1));

        bool featureSupported = gpDevice->isFeatureSupported(Device::SupportedFeatures::ProgrammableSamplePositionsPartialOnly) ||
            gpDevice->isFeatureSupported(Device::SupportedFeatures::ProgrammableSamplePositionsFull);

        const auto& samplePos = pFbo->getSamplePositions();

#if _LOG_ENABLED
        if (featureSupported == false && samplePos.size() > 0)
        {
            logError("The FBO specifies programmable sample positions, but the hardware does not support it");
        }
        else if (gpDevice->isFeatureSupported(Device::SupportedFeatures::ProgrammableSamplePositionsPartialOnly) && samplePos.size() > 1)
        {
            logError("The FBO specifies multiple programmable sample positions, but the hardware only supports one");
        }
#endif
        if (featureSupported)
        {
            static_assert(offsetof(Fbo::SamplePosition, xOffset) == offsetof(D3D12_SAMPLE_POSITION, X), "SamplePosition.X");
            static_assert(offsetof(Fbo::SamplePosition, yOffset) == offsetof(D3D12_SAMPLE_POSITION, Y), "SamplePosition.Y");

            if (samplePos.size())
            {
                pList1->SetSamplePositions(pFbo->getSampleCount(), pFbo->getSamplePositionsPixelCount(), (D3D12_SAMPLE_POSITION*)samplePos.data());
            }
            else
            {
                pList1->SetSamplePositions(0, 0, nullptr);
            }
        }
    }

    static void D3D12SetViewports(ID3D12GraphicsCommandList* pList, const GraphicsState::Viewport* vp)
    {
        static_assert(offsetof(GraphicsState::Viewport, originX) == offsetof(D3D12_VIEWPORT, TopLeftX), "VP originX offset");
        static_assert(offsetof(GraphicsState::Viewport, originY) == offsetof(D3D12_VIEWPORT, TopLeftY), "VP originY offset");
        static_assert(offsetof(GraphicsState::Viewport, width) == offsetof(D3D12_VIEWPORT, Width), "VP Width offset");
        static_assert(offsetof(GraphicsState::Viewport, height) == offsetof(D3D12_VIEWPORT, Height), "VP Height offset");
        static_assert(offsetof(GraphicsState::Viewport, minDepth) == offsetof(D3D12_VIEWPORT, MinDepth), "VP MinDepth offset");
        static_assert(offsetof(GraphicsState::Viewport, maxDepth) == offsetof(D3D12_VIEWPORT, MaxDepth), "VP TopLeftX offset");

        pList->RSSetViewports(D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE, (D3D12_VIEWPORT*)vp);
    }

    static void D3D12SetScissors(ID3D12GraphicsCommandList* pList, const GraphicsState::Scissor* sc)
    {
        static_assert(offsetof(GraphicsState::Scissor, left) == offsetof(D3D12_RECT, left), "Scissor.left offset");
        static_assert(offsetof(GraphicsState::Scissor, top) == offsetof(D3D12_RECT, top), "Scissor.top offset");
        static_assert(offsetof(GraphicsState::Scissor, right) == offsetof(D3D12_RECT, right), "Scissor.right offset");
        static_assert(offsetof(GraphicsState::Scissor, bottom) == offsetof(D3D12_RECT, bottom), "Scissor.bottom offset");

        pList->RSSetScissorRects(D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE, (D3D12_RECT*)sc);
    }

    bool RenderContext::prepareForDraw(GraphicsState* pState, GraphicsVars* pVars)
    {
        assert(pState);
        // Vao must be valid so at least primitive topology is known
        assert(pState->getVao().get());

        auto pGSO = pState->getGSO(pVars);

        if (is_set(StateBindFlags::Vars, mBindFlags))
        {
            // Apply the vars. Must be first because applyGraphicsVars() might cause a flush
            if (pVars)
            {
                // TODO(tfoley): Need to find a way to pass the specialization information
                // from computing the GSO down into `applyGraphicsVars` so that parameters
                // can be bound using an appropriate layout.
                //
                if (applyGraphicsVars(pVars, pGSO->getDesc().getRootSignature().get()) == false) return false;
            }
            else mpLowLevelData->getCommandList()->SetGraphicsRootSignature(RootSignature::getEmpty()->getApiHandle());
            mpLastBoundGraphicsVars = pVars;
        }

        ID3D12GraphicsCommandList* pList = mpLowLevelData->getCommandList();


        if (is_set(StateBindFlags::Topology, mBindFlags))           pList->IASetPrimitiveTopology(getD3DPrimitiveTopology(pState->getVao()->getPrimitiveTopology()));
        if (is_set(StateBindFlags::Vao, mBindFlags))                D3D12SetVao(this, pList, pState->getVao().get());
        if (is_set(StateBindFlags::Fbo, mBindFlags))                D3D12SetFbo(this, pState->getFbo().get());
        if (is_set(StateBindFlags::SamplePositions, mBindFlags))    D3D12SetSamplePositions(pList, pState->getFbo().get());
        if (is_set(StateBindFlags::Viewports, mBindFlags))          D3D12SetViewports(pList, &pState->getViewport(0));
        if (is_set(StateBindFlags::Scissors, mBindFlags))           D3D12SetScissors(pList, &pState->getScissors(0));
        if (is_set(StateBindFlags::PipelineState, mBindFlags))      pList->SetPipelineState(pGSO->getApiHandle());

        BlendState::SharedPtr blendState = pState->getBlendState();
        if (blendState != nullptr)  pList->OMSetBlendFactor(glm::value_ptr(blendState->getBlendFactor()));

        const auto pDsState = pState->getDepthStencilState();
        pList->OMSetStencilRef(pDsState == nullptr ? 0 : pDsState->getStencilRef());

        mCommandsPending = true;
        return true;
    }

    void RenderContext::drawInstanced(GraphicsState* pState, GraphicsVars* pVars, uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation)
    {
        if (prepareForDraw(pState, pVars) == false) return;
        mpLowLevelData->getCommandList()->DrawInstanced(vertexCount, instanceCount, startVertexLocation, startInstanceLocation);
    }

    void RenderContext::draw(GraphicsState* pState, GraphicsVars* pVars, uint32_t vertexCount, uint32_t startVertexLocation)
    {
        drawInstanced(pState, pVars, vertexCount, 1, startVertexLocation, 0);
    }

    void RenderContext::drawIndexedInstanced(GraphicsState* pState, GraphicsVars* pVars, uint32_t indexCount, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation)
    {
        if (prepareForDraw(pState, pVars) == false) return;
        mpLowLevelData->getCommandList()->DrawIndexedInstanced(indexCount, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
    }

    void RenderContext::drawIndexed(GraphicsState* pState, GraphicsVars* pVars, uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation)
    {
        drawIndexedInstanced(pState, pVars, indexCount, 1, startIndexLocation, baseVertexLocation, 0);
    }

    void drawIndirectCommon(RenderContext* pContext, const CommandListHandle& pCommandList, ID3D12CommandSignature* pCommandSig, uint32_t maxCommandCount, const Buffer* pArgBuffer, uint64_t argBufferOffset, const Buffer* pCountBuffer, uint64_t countBufferOffset)
    {
        pContext->resourceBarrier(pArgBuffer, Resource::State::IndirectArg);
        if (pCountBuffer != nullptr && pCountBuffer != pArgBuffer) pContext->resourceBarrier(pCountBuffer, Resource::State::IndirectArg);
        pCommandList->ExecuteIndirect(pCommandSig, maxCommandCount, pArgBuffer->getApiHandle(), argBufferOffset, (pCountBuffer != nullptr ? pCountBuffer->getApiHandle() : nullptr), countBufferOffset);
    }

    void RenderContext::drawIndirect(GraphicsState* pState, GraphicsVars* pVars, uint32_t maxCommandCount, const Buffer* pArgBuffer, uint64_t argBufferOffset, const Buffer* pCountBuffer, uint64_t countBufferOffset)
    {
        if (prepareForDraw(pState, pVars) == false) return;
        drawIndirectCommon(this, mpLowLevelData->getCommandList(), sApiData.pDrawCommandSig, maxCommandCount, pArgBuffer, argBufferOffset, pCountBuffer, countBufferOffset);
    }

    void RenderContext::drawIndexedIndirect(GraphicsState* pState, GraphicsVars* pVars, uint32_t maxCommandCount, const Buffer* pArgBuffer, uint64_t argBufferOffset, const Buffer* pCountBuffer, uint64_t countBufferOffset)
    {
        if (prepareForDraw(pState, pVars) == false) return;
        drawIndirectCommon(this, mpLowLevelData->getCommandList(), sApiData.pDrawIndexCommandSig, maxCommandCount, pArgBuffer, argBufferOffset, pCountBuffer, countBufferOffset);
    }

    void RenderContext::raytrace(RtProgram* pProgram, RtProgramVars* pVars, uint32_t width, uint32_t height, uint32_t depth)
    {
        auto pRtso = pProgram->getRtso(pVars);

        pVars->apply(this, pRtso.get());

        const auto& pShaderTable = pVars->getShaderTable();
        resourceBarrier(pShaderTable->getBuffer().get(), Resource::State::NonPixelShader);

        D3D12_GPU_VIRTUAL_ADDRESS startAddress = pShaderTable->getBuffer()->getGpuAddress();

        D3D12_DISPATCH_RAYS_DESC raytraceDesc = {};
        raytraceDesc.Width = width;
        raytraceDesc.Height = height;
        raytraceDesc.Depth = depth;

        // RayGen data
        //
        // TODO: We could easily support specifying the ray-gen program to invoke by an index in
        // the call to `raytrace()`.
        //
        raytraceDesc.RayGenerationShaderRecord.StartAddress = startAddress + pShaderTable->getRayGenTableOffset();
        raytraceDesc.RayGenerationShaderRecord.SizeInBytes = pShaderTable->getRayGenRecordSize();;

        // Miss data
        if (pShaderTable->getMissRecordCount() > 0)
        {
            raytraceDesc.MissShaderTable.StartAddress = startAddress + pShaderTable->getMissTableOffset();
            raytraceDesc.MissShaderTable.StrideInBytes = pShaderTable->getMissRecordSize();
            raytraceDesc.MissShaderTable.SizeInBytes = pShaderTable->getMissRecordSize() * pShaderTable->getMissRecordCount();
        }

        // Hit data
        if (pShaderTable->getHitRecordCount() > 0)
        {
            raytraceDesc.HitGroupTable.StartAddress = startAddress + pShaderTable->getHitTableOffset();
            raytraceDesc.HitGroupTable.StrideInBytes = pShaderTable->getHitRecordSize();
            raytraceDesc.HitGroupTable.SizeInBytes = pShaderTable->getHitRecordSize() * pShaderTable->getHitRecordCount();
        }

        auto pCmdList = getLowLevelData()->getCommandList();
        pCmdList->SetComputeRootSignature(pRtso->getGlobalRootSignature()->getApiHandle().GetInterfacePtr());

        // Dispatch
        GET_COM_INTERFACE(pCmdList, ID3D12GraphicsCommandList4, pList4);
        pList4->SetPipelineState1(pRtso->getApiHandle().GetInterfacePtr());
        pList4->DispatchRays(&raytraceDesc);
    }

    void RenderContext::blit(ShaderResourceView::SharedPtr pSrc, RenderTargetView::SharedPtr pDst, const uint4& srcRect, const uint4& dstRect, Sampler::Filter filter)
    {
        const Sampler::ReductionMode componentsReduction[] = { Sampler::ReductionMode::Standard, Sampler::ReductionMode::Standard, Sampler::ReductionMode::Standard, Sampler::ReductionMode::Standard };
        const float4 componentsTransform[] = { float4(1.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 1.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 1.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 1.0f) };

        blit(pSrc, pDst, srcRect, dstRect, filter, componentsReduction, componentsTransform);
    }

    void RenderContext::blit(ShaderResourceView::SharedPtr pSrc, RenderTargetView::SharedPtr pDst, const uint4& srcRect, const uint4& dstRect, Sampler::Filter filter, const Sampler::ReductionMode componentsReduction[4], const float4 componentsTransform[4])
    {
        auto& blitData = sApiData.blitData;
        const Texture* pSrcTexture = dynamic_cast<const Texture*>(pSrc->getResource());
        const bool complexBlit = pSrcTexture->getSampleCount() <= 1 &&
            !((componentsReduction[0] == Sampler::ReductionMode::Standard) && (componentsReduction[1] == Sampler::ReductionMode::Standard) && (componentsReduction[2] == Sampler::ReductionMode::Standard) && (componentsReduction[3] == Sampler::ReductionMode::Standard) &&
                (componentsTransform[0] == float4(1.0f, 0.0f, 0.0f, 0.0f)) && (componentsTransform[1] == float4(0.0f, 1.0f, 0.0f, 0.0f)) && (componentsTransform[2] == float4(0.0f, 0.0f, 1.0f, 0.0f)) && (componentsTransform[3] == float4(0.0f, 0.0f, 0.0f, 1.0f)));

        if (complexBlit)
        {
            // Complex blit doesn't work with multi-sampled textures.
            assert(pSrcTexture->getSampleCount() <= 1);

            blitData.pPass->addDefine("COMPLEX_BLIT", "1");

            Sampler::SharedPtr usedSampler[4];
            for (uint32_t i = 0; i < 4; i++)
            {
                assert(componentsReduction[i] != Sampler::ReductionMode::Comparison);        // Comparison mode not supported.

                if (componentsReduction[i] == Sampler::ReductionMode::Min) usedSampler[i] = (filter == Sampler::Filter::Linear) ? blitData.pLinearMinSampler : blitData.pPointMinSampler;
                else if (componentsReduction[i] == Sampler::ReductionMode::Max) usedSampler[i] = (filter == Sampler::Filter::Linear) ? blitData.pLinearMaxSampler : blitData.pPointMaxSampler;
                else usedSampler[i] = (filter == Sampler::Filter::Linear) ? blitData.pLinearSampler : blitData.pPointSampler;
            }

            blitData.pPass->getVars()->setSampler("gSamplerR", usedSampler[0]);
            blitData.pPass->getVars()->setSampler("gSamplerG", usedSampler[1]);
            blitData.pPass->getVars()->setSampler("gSamplerB", usedSampler[2]);
            blitData.pPass->getVars()->setSampler("gSamplerA", usedSampler[3]);

            // Parameters for complex blit
            for (uint32_t i = 0; i < 4; i++)
            {
                if (blitData.prevComponentsTransform[i] != componentsTransform[i])
                {
                    blitData.pBlitParamsBuffer->setVariable(blitData.compTransVarOffset[i], componentsTransform[i]);
                    blitData.prevComponentsTransform[i] = componentsTransform[i];
                }
            }
        }
        else
        {
            blitData.pPass->removeDefine("COMPLEX_BLIT");

            blitData.pPass->getVars()->setSampler("gSampler", (filter == Sampler::Filter::Linear) ? blitData.pLinearSampler : blitData.pPointSampler);
        }

        assert(pSrc->getViewInfo().arraySize == 1 && pSrc->getViewInfo().mipCount == 1);
        assert(pDst->getViewInfo().arraySize == 1 && pDst->getViewInfo().mipCount == 1);

        Texture* pDstTexture = dynamic_cast<Texture*>(pDst->getResource());
        assert(pSrcTexture != nullptr && pDstTexture != nullptr);

        float2 srcRectOffset(0.0f);
        float2 srcRectScale(1.0f);
        uint32_t srcMipLevel = pSrc->getViewInfo().mostDetailedMip;
        uint32_t dstMipLevel = pDst->getViewInfo().mostDetailedMip;
        GraphicsState::Viewport dstViewport(0.0f, 0.0f, (float)pDstTexture->getWidth(dstMipLevel), (float)pDstTexture->getHeight(dstMipLevel), 0.0f, 1.0f);

        // If src rect specified
        if (srcRect.x != (uint32_t)-1)
        {
            const float2 srcSize(pSrcTexture->getWidth(srcMipLevel), pSrcTexture->getHeight(srcMipLevel));
            srcRectOffset = float2(srcRect.x, srcRect.y) / srcSize;
            srcRectScale = float2(srcRect.z - srcRect.x, srcRect.w - srcRect.y) / srcSize;
        }

        // If dest rect specified
        if (dstRect.x != (uint32_t)-1)
        {
            dstViewport = GraphicsState::Viewport((float)dstRect.x, (float)dstRect.y, (float)(dstRect.z - dstRect.x), (float)(dstRect.w - dstRect.y), 0.0f, 1.0f);
        }

        // Update buffer/state
        if (srcRectOffset != blitData.prevSrcRectOffset)
        {
            blitData.pBlitParamsBuffer->setVariable(blitData.offsetVarOffset, srcRectOffset);
            blitData.prevSrcRectOffset = srcRectOffset;
        }

        if (srcRectScale != blitData.prevSrcReftScale)
        {
            blitData.pBlitParamsBuffer->setVariable(blitData.scaleVarOffset, srcRectScale);
            blitData.prevSrcReftScale = srcRectScale;
        }

        if (pSrcTexture->getSampleCount() > 1)
        {
            blitData.pPass->addDefine("SAMPLE_COUNT", std::to_string(pSrcTexture->getSampleCount()));
        }
        else
        {
            blitData.pPass->removeDefine("SAMPLE_COUNT");
        }

        Texture::SharedPtr pSharedTex = std::static_pointer_cast<Texture>(pDstTexture->shared_from_this());
        blitData.pFbo->attachColorTarget(pSharedTex, 0, pDst->getViewInfo().mostDetailedMip, pDst->getViewInfo().firstArraySlice, pDst->getViewInfo().arraySize);
        blitData.pPass->getVars()->setSrv(blitData.texBindLoc, pSrc);
        blitData.pPass->getState()->setViewport(0, dstViewport);
        blitData.pPass->execute(this, blitData.pFbo, false);

        // Release the resources we bound
        blitData.pPass->getVars()->setSrv(blitData.texBindLoc, nullptr);
    }

    void RenderContext::resolveSubresource(const Texture::SharedPtr& pSrc, uint32_t srcSubresource, const Texture::SharedPtr& pDst, uint32_t dstSubresource)
    {
        DXGI_FORMAT format = getDxgiFormat(pDst->getFormat());
        mpLowLevelData->getCommandList()->ResolveSubresource(pDst->getApiHandle(), dstSubresource, pSrc->getApiHandle(), srcSubresource, format);
        mCommandsPending = true;
    }

    void RenderContext::resolveResource(const Texture::SharedPtr& pSrc, const Texture::SharedPtr& pDst)
    {
        bool match = true;
        match = match && (pSrc->getMipCount() == pDst->getMipCount());
        match = match && (pSrc->getArraySize() == pDst->getArraySize());
        if (!match)
        {
            logWarning("Can't resolve a resource. The src and dst textures have a different array-size or mip-count");
        }

        resourceBarrier(pSrc.get(), Resource::State::ResolveSource);
        resourceBarrier(pDst.get(), Resource::State::ResolveDest);

        uint32_t subresourceCount = pSrc->getMipCount() * pSrc->getArraySize();
        for (uint32_t s = 0; s < subresourceCount; s++)
        {
            resolveSubresource(pSrc, s, pDst, s);
        }
    }
};