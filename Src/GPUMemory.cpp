#include "Common.h"
#include "GPUMemory.h"

namespace WIP3D
{
	GpuMemoryHeap::SharedPtr GpuMemoryHeap::create(Type type, size_t pageSize, const GpuFence::SharedPtr& pFence)
	{
		return SharedPtr(new GpuMemoryHeap(type, pageSize, pFence));
	}
	GpuMemoryHeap::~GpuMemoryHeap()
	{
		mDeferredReleases = decltype(mDeferredReleases)();
	}
	GpuMemoryHeap::GpuMemoryHeap(Type type, size_t pageSize, const GpuFence::SharedPtr& pFence)
		: mType(type)
		, mPageSize(pageSize)
		, mpFence(pFence)
	{
		allocateNewPage();
	}
    void GpuMemoryHeap::allocateNewPage()
    {
        if (mpActivePage)
        {
            mUsedPages[mCurrentPageId] = std::move(mpActivePage);
        }

        if (mAvailablePages.size())
        {
            mpActivePage = std::move(mAvailablePages.front());
            mAvailablePages.pop();
            mpActivePage->allocationsCount = 0;
            mpActivePage->currentOffset = 0;
        }
        else
        {
            mpActivePage = std::make_unique<PageData>();
            initBasePageData((*mpActivePage), mPageSize);
        }

        mpActivePage->currentOffset = 0;
        mCurrentPageId++;
    }

    GpuMemoryHeap::Allocation GpuMemoryHeap::allocate(size_t size, size_t alignment)
    {
        Allocation data;
        if (size > mPageSize)
        {
            data.pageID = GpuMemoryHeap::Allocation::kMegaPageId;
            initBasePageData(data, size);
        }
        else
        {
            // Calculate the start
            size_t currentOffset = align_to(alignment, mpActivePage->currentOffset);
            if (currentOffset + size > mPageSize)
            {
                currentOffset = 0;
                allocateNewPage();
            }

            data.pageID = mCurrentPageId;
            data.offset = currentOffset;
            data.pData = mpActivePage->pData + currentOffset;
            data.pResourceHandle = mpActivePage->pResourceHandle;
            mpActivePage->currentOffset = currentOffset + size;
            mpActivePage->allocationsCount++;
        }

        data.fenceValue = mpFence->getCpuValue();
        return data;
    }

    void GpuMemoryHeap::release(Allocation& data)
    {
        assert(data.pResourceHandle);
        mDeferredReleases.push(data);
    }

    void GpuMemoryHeap::executeDeferredReleases()
    {
        uint64_t gpuVal = mpFence->getGpuValue();
        while (mDeferredReleases.size() && mDeferredReleases.top().fenceValue <= gpuVal)
        {
            const Allocation& data = mDeferredReleases.top();
            if (data.pageID == mCurrentPageId)
            {
                mpActivePage->allocationsCount--;
                if (mpActivePage->allocationsCount == 0)
                {
                    mpActivePage->currentOffset = 0;
                }
            }
            else
            {
                if (data.pageID != Allocation::kMegaPageId)
                {
                    auto& pData = mUsedPages[data.pageID];
                    pData->allocationsCount--;
                    if (pData->allocationsCount == 0)
                    {
                        mAvailablePages.push(std::move(pData));
                        mUsedPages.erase(data.pageID);
                    }
                }
                // else it's a mega-page. Popping it will release the resource
            }
            mDeferredReleases.pop();
        }
    }
}
