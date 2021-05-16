#pragma once
#include <memory>

#include <queue>
#include <unordered_map>
#include "D3D12/WIPD3D12.h"
#include "GraphicsCommon.h"

namespace WIP3D
{
    class GpuMemoryHeap
    {
    public:
        using SharedPtr = std::shared_ptr<GpuMemoryHeap>;
        using SharedConstPtr = std::shared_ptr<const GpuMemoryHeap>;

        enum class Type
        {
            Default,
            Upload,
            Readback
        };

        struct BaseData
        {
            ResourceHandle pResourceHandle;
            GpuAddress offset = 0;
            uint8_t* pData = nullptr;
        };

        struct Allocation : public BaseData
        {
            uint64_t pageID = 0;
            uint64_t fenceValue = 0;

            static const uint64_t kMegaPageId = -1;
            bool operator<(const Allocation& other)  const { return fenceValue > other.fenceValue; }
        };

        ~GpuMemoryHeap();

        /** Create a new GPU memory heap.
            \param[in] type The type of heap.
            \param[in] pageSize Page size in bytes.
            \param[in] pFence Fence to use for synchronization.
            \return A new object, or throws an exception if creation failed.
        */
        static SharedPtr create(Type type, size_t pageSize, const GpuFence::SharedPtr& pFence);

        Allocation allocate(size_t size, size_t alignment = 1);
        void release(Allocation& data);
        size_t getPageSize() const { return mPageSize; }
        void executeDeferredReleases();

    private:
        GpuMemoryHeap(Type type, size_t pageSize, const GpuFence::SharedPtr& pFence);

        struct PageData : public BaseData
        {
            uint32_t allocationsCount = 0;
            size_t currentOffset = 0;

            using UniquePtr = std::unique_ptr<PageData>;
        };

        Type mType;
        GpuFence::SharedPtr mpFence;
        size_t mPageSize = 0;
        size_t mCurrentPageId = 0;
        PageData::UniquePtr mpActivePage;

        std::priority_queue<Allocation> mDeferredReleases;
        std::unordered_map<size_t, PageData::UniquePtr> mUsedPages;
        std::queue<PageData::UniquePtr> mAvailablePages;

        void allocateNewPage();
        void initBasePageData(BaseData& data, size_t size);
    };
}