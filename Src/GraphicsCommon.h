#pragma once
#include <memory>
#include <vector>
#include <unordered_set>
#include <deque>
#include <queue>

#include "Formats.h"
#include "GraphicsResView.h"

//#include "GraphicsResource.h"
namespace WIP3D
{



    struct FenceApiData;

    /** This class can be used to synchronize GPU and CPU execution
        It's value monotonically increasing - every time a signal is sent, it will change the value first
        A fence is an integer that represents the current unit of work being processed. 
        When the app advances the fence, by calling ID3D12CommandQueue::Signal, the integer is updated. 
        Apps can check the value of a fence and determine if a unit of work has been completed in order to decide whether a subsequent operation can be started.
    */
    class GpuFence : public std::enable_shared_from_this<GpuFence>
    {
    public:
        using SharedPtr = std::shared_ptr<GpuFence>;
        using SharedConstPtr = std::shared_ptr<const GpuFence>;
        using ApiHandle = FenceHandle;
        ~GpuFence();

        /** Create a new GPU fence.
            \return A new object, or throws an exception if creation failed.
        */
        static SharedPtr create();

        /** Get the internal API handle
        */
        const ApiHandle& getApiHandle() const { return mApiHandle; }

        /** Get the last value the GPU has signaled
        */
        uint64_t getGpuValue() const;

        /** Get the current CPU value
        */
        uint64_t getCpuValue() const { return mCpuValue; }

        /** Tell the GPU to wait until the fence reaches the last GPU-value signaled (which is (mCpuValue - 1))
        */
        void syncGpu(CommandQueueHandle pQueue);

        /** Tell the CPU to wait until the fence reaches the current value
        */
        //void syncCpu(std::optional<uint64_t> val = {});
        void syncCpu(uint64_t val);


        /** Insert a signal command into the command queue. This will increase the internal value
        */
        uint64_t gpuSignal(CommandQueueHandle pQueue);
    private:
        GpuFence() : mCpuValue(0) {}
        uint64_t mCpuValue;

        ApiHandle mApiHandle;
        FenceApiData* mpApiData = nullptr;
    };


    

    class QueryHeap : public std::enable_shared_from_this<QueryHeap>
    {
    public:
        using SharedPtr = std::shared_ptr<QueryHeap>;
        using ApiHandle = QueryHeapHandle;

        enum class Type
        {
            Timestamp,
            Occlusion,
            PipelineStats
        };

        static const uint32_t kInvalidIndex = 0xffffffff;

        /** Create a new query heap.
            \param[in] type Type of queries.
            \param[in] count Number of queries.
            \return New object, or throws an exception if creation failed.
        */
        static SharedPtr create(Type type, uint32_t count) { return SharedPtr(new QueryHeap(type, count)); }

        const ApiHandle& getApiHandle() const { return mApiHandle; }
        uint32_t getQueryCount() const { return mCount; }
        Type getType() const { return mType; }

        /** Allocates a new query.
            \return Query index, or kInvalidIndex if out of queries.
        */
        uint32_t allocate()
        {
            if (mFreeQueries.size())
            {
                uint32_t entry = mFreeQueries.front();
                mFreeQueries.pop_front();
                return entry;
            }
            if (mCurrentObject < mCount) return mCurrentObject++;
            else return kInvalidIndex;
        }

        void release(uint32_t entry)
        {
            assert(entry != kInvalidIndex);
                mFreeQueries.push_back(entry);
        }

    private:
        QueryHeap(Type type, uint32_t count);
        ApiHandle mApiHandle;
        uint32_t mCount = 0;
        uint32_t mCurrentObject = 0;
        std::deque<uint32_t> mFreeQueries;
        Type mType;
    };

    struct DescriptorPoolApiData;
    struct DescriptorSetApiData;

    // A descriptor is a relatively small block of data that fully describes an object to the GPU, in a GPU-specific opaque format. 
    // There are several different types of descriptors—render target views (RTVs), depth stencil views (DSVs), shader resource views (SRVs), 
    // unordered access views (UAVs), constant buffer views (CBVs), and samplers.
    // Object descriptors do not need to be freed or released. Drivers do not attach any allocations to the creation of a descriptor. 
    // The handle is unique across descriptor heaps(唯一即使来自不同的heap), so, for example, an array of handles can reference descriptors in multiple heaps.

    // CBV, UAV and SRV entries can be in the same descriptor heap. However, Samplers entries cannot share 
    // a heap with CBV, UAV or SRV entries. Typically, there are two sets of descriptor heaps, one for the common resources and the second for Samplers.
    class DescriptorPool : public std::enable_shared_from_this<DescriptorPool>
    {
    public:
        using SharedPtr = std::shared_ptr<DescriptorPool>;
        using SharedConstPtr = std::shared_ptr<const DescriptorPool>;
        using ApiHandle = DescriptorHeapHandle;
        using CpuHandle = HeapCpuHandle;
        using GpuHandle = HeapGpuHandle;
        using ApiData = DescriptorPoolApiData;

        ~DescriptorPool();

        enum class Type
        {
            TextureSrv,
            TextureUav,
            RawBufferSrv,
            RawBufferUav,
            TypedBufferSrv,
            TypedBufferUav,
            Cbv,
            StructuredBufferUav,
            StructuredBufferSrv,
            Dsv,
            Rtv,
            Sampler,

            Count
        };

        static const uint32_t kTypeCount = uint32_t(Type::Count);

        class Desc
        {
        public:
            Desc& setDescCount(Type type, uint32_t count)
            {
                uint32_t t = (uint32_t)type;
                mTotalDescCount -= mDescCount[t];
                mTotalDescCount += count;
                mDescCount[t] = count;
                return *this;
            }

            Desc& setShaderVisible(bool visible) { mShaderVisible = visible; return *this; }
        private:
            friend DescriptorPool;
            uint32_t mDescCount[kTypeCount] = { 0 };
            uint32_t mTotalDescCount = 0;
            bool mShaderVisible = false;
        };

        /** Create a new descriptor pool.
            \param[in] desc Description of the descriptor type and count.
            \param[in] pFence Fence object for synchronization.
            \return A new object, or throws an exception if creation failed.
        */
        static SharedPtr create(const Desc& desc, const GpuFence::SharedPtr& pFence);

        uint32_t getDescCount(Type type) const { return mDesc.mDescCount[(uint32_t)type]; }
        uint32_t getTotalDescCount() const { return mDesc.mTotalDescCount; }
        bool isShaderVisible() const { return mDesc.mShaderVisible; }
        const ApiHandle& getApiHandle(uint32_t heapIndex) const;
        const ApiData* getApiData() const { return mpApiData.get(); }
        void executeDeferredReleases();

    private:
        friend DescriptorSet;
        DescriptorPool(const Desc& desc, const GpuFence::SharedPtr& pFence);
        void apiInit();
        void releaseAllocation(std::shared_ptr<DescriptorSetApiData> pData);
        Desc mDesc;
        std::shared_ptr<ApiData> mpApiData;
        GpuFence::SharedPtr mpFence;

        struct DeferredRelease
        {
            std::shared_ptr<DescriptorSetApiData> pData;
            uint64_t fenceValue;
            bool operator>(const DeferredRelease& other) const { return fenceValue > other.fenceValue; }
        };

        std::priority_queue<DeferredRelease, std::vector<DeferredRelease>, std::greater<DeferredRelease>> mpDeferredReleases;
    };



    
}