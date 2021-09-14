#include "../DescriptorSet.h"
#include "../Device.h"
#include "../GraphicsContext.h"
#include "WIPD3D12.h"

namespace WIP3D
{
    D3D12_DESCRIPTOR_HEAP_TYPE falcorToDxDescType(DescriptorPool::Type t);

    static D3D12DescriptorHeap* getHeap(const DescriptorPool* pPool, DescriptorSet::Type type)
    {
        auto dxType = falcorToDxDescType(type);
        D3D12DescriptorHeap* pHeap = pPool->getApiData()->pHeaps[dxType].get();
        assert(pHeap);
        assert(pHeap->getType() == dxType);
        return pHeap;
    }

    void DescriptorSet::apiInit()
    {
        mpApiData = std::make_shared<DescriptorSetApiData>();
        uint32_t count = 0;
        const auto falcorType = mLayout.getRange(0).type;
        const auto d3dType = falcorToDxDescType(falcorType);

        // For each range we need to allocate a table from a heap
        mpApiData->rangeBaseOffset.resize(mLayout.getRangeCount());

        for (size_t i = 0; i < mLayout.getRangeCount(); i++)
        {
            const auto& range = mLayout.getRange(i);
            mpApiData->rangeBaseOffset[i] = count;
            assert(d3dType == falcorToDxDescType(range.type)); // We can only allocate from a single heap
            count += range.descCount;
        }

        D3D12DescriptorHeap* pHeap = getHeap(mpPool.get(), falcorType);
        mpApiData->pAllocation = pHeap->allocateDescriptors(count);
        if (mpApiData->pAllocation == nullptr)
        {
            // Execute deferred releases and try again
            mpPool->executeDeferredReleases();
            mpApiData->pAllocation = pHeap->allocateDescriptors(count);
        }

        // Allocation failed again, there is nothing else we can do.
        if (mpApiData->pAllocation == nullptr) throw std::exception("Failed to create descriptor set");
    }

    DescriptorSet::CpuHandle DescriptorSet::getCpuHandle(uint32_t rangeIndex, uint32_t descInRange) const
    {
        uint32_t index = mpApiData->rangeBaseOffset[rangeIndex] + descInRange;
        return mpApiData->pAllocation->getCpuHandle(index);
    }

    DescriptorSet::GpuHandle DescriptorSet::getGpuHandle(uint32_t rangeIndex, uint32_t descInRange) const
    {
        uint32_t index = mpApiData->rangeBaseOffset[rangeIndex] + descInRange;
        return mpApiData->pAllocation->getGpuHandle(index);
    }

    void setCpuHandle(DescriptorSet* pSet, uint32_t rangeIndex, uint32_t descIndex, const DescriptorSet::CpuHandle& handle)
    {
        auto dstHandle = pSet->getCpuHandle(rangeIndex, descIndex);
        gpDevice->getApiHandle()->CopyDescriptorsSimple(1, dstHandle, handle, falcorToDxDescType(pSet->getRange(rangeIndex).type));
    }

    void DescriptorSet::setSrv(uint32_t rangeIndex, uint32_t descIndex, const ShaderResourceView* pSrv)
    {
        setCpuHandle(this, rangeIndex, descIndex, pSrv->getApiHandle()->getCpuHandle(0));
    }

    void DescriptorSet::setUav(uint32_t rangeIndex, uint32_t descIndex, const UnorderedAccessView* pUav)
    {
        setCpuHandle(this, rangeIndex, descIndex, pUav->getApiHandle()->getCpuHandle(0));
    }

    void DescriptorSet::setSampler(uint32_t rangeIndex, uint32_t descIndex, const Sampler* pSampler)
    {
        setCpuHandle(this, rangeIndex, descIndex, pSampler->getApiHandle()->getCpuHandle(0));
    }

    void DescriptorSet::bindForGraphics(CopyContext* pCtx, const RootSignature* pRootSig, uint32_t rootIndex)
    {
        pCtx->getLowLevelData()->getCommandList()->SetGraphicsRootDescriptorTable(rootIndex, getGpuHandle(0));
    }

    void DescriptorSet::bindForCompute(CopyContext* pCtx, const RootSignature* pRootSig, uint32_t rootIndex)
    {
        pCtx->getLowLevelData()->getCommandList()->SetComputeRootDescriptorTable(rootIndex, getGpuHandle(0));
    }

    void DescriptorSet::setCbv(uint32_t rangeIndex, uint32_t descIndex, ConstantBufferView* pView)
    {
        setCpuHandle(this, rangeIndex, descIndex, pView->getApiHandle()->getCpuHandle(0));
    }
}
