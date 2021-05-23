#include "GraphicsCommon.h"
#include "Device.h"

namespace WIP3D
{
    WIP3D::D3D12DescriptorHeap::~D3D12DescriptorHeap()
    {
    }

    D3D12DescriptorHeap::SharedPtr WIP3D::D3D12DescriptorHeap::create(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descCount, bool shaderVisible)
    {
        return SharedPtr();
    }


    WIP3D::D3D12DescriptorHeap::Allocation::~Allocation()
    {
    }

    D3D12DescriptorHeap::Allocation::SharedPtr WIP3D::D3D12DescriptorHeap::Allocation::create(D3D12DescriptorHeap::SharedPtr pHeap, uint32_t baseIndex, uint32_t descCount, std::shared_ptr<Chunk> pChunk)
    {
        return D3D12DescriptorHeap::Allocation::SharedPtr();
    }
    D3D12DescriptorHeap::Allocation::SharedPtr WIP3D::D3D12DescriptorHeap::allocateDescriptors(uint32_t count)
    {
        return Allocation::SharedPtr();
    }

    D3D12DescriptorHeap::CpuHandle WIP3D::D3D12DescriptorHeap::getCpuHandle(uint32_t index) const
    {
        return D3D12DescriptorHeap::CpuHandle();
    }

    D3D12DescriptorHeap::GpuHandle WIP3D::D3D12DescriptorHeap::getGpuHandle(uint32_t index) const
    {
        return D3D12DescriptorHeap::GpuHandle();
    }
	
    DescriptorPool::SharedPtr DescriptorPool::create(const Desc& desc, const GpuFence::SharedPtr& pFence)
    {
        return SharedPtr(new DescriptorPool(desc, pFence));
    }

    DescriptorPool::DescriptorPool(const Desc& desc, const GpuFence::SharedPtr& pFence)
        : mDesc(desc)
        , mpFence(pFence)
    {
        apiInit();
    }

    DescriptorPool::~DescriptorPool() = default;

    void DescriptorPool::executeDeferredReleases()
    {
        uint64_t gpuVal = mpFence->getGpuValue();
        while (mpDeferredReleases.size() && mpDeferredReleases.top().fenceValue <= gpuVal)
        {
            mpDeferredReleases.pop();
        }
    }

    void DescriptorPool::releaseAllocation(std::shared_ptr<DescriptorSetApiData> pData)
    {
        DeferredRelease d;
        d.pData = pData;
        d.fenceValue = mpFence->getCpuValue();
        mpDeferredReleases.push(d);
    }
}
