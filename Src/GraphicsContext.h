#pragma once
#include <memory>
#include <vector>


#define def_shared_ptr(T) \
using SharedPtr = std::shared_ptr<T>;\
using SharedConstPtr = std::shared_ptr<const T>;



namespace WIP3D
{
    class LowLevelContextData : public std::enable_shared_from_this<LowLevelContextData>
    {
    public:
        using SharedPtr = std::shared_ptr<LowLevelContextData>;
        using SharedConstPtr = std::shared_ptr<const LowLevelContextData>;

        enum class CommandQueueType
        {
            Copy,
            Compute,
            Direct,
            Count
        };

        ~LowLevelContextData();

        /** Create a new low-level context data object.
            \param[in] type Command queue type.
            \param[in] queue Command queue handle. Can be nullptr.
            \return A new object, or throws an exception if creation failed.
        */
        static SharedPtr create(CommandQueueType type, CommandQueueHandle queue);

        void flush();

        const CommandListHandle& getCommandList() const { return mpList; }
        const CommandQueueHandle& getCommandQueue() const { return mpQueue; }
        const CommandAllocatorHandle& getCommandAllocator() const { return mpAllocator; }
        const GpuFence::SharedPtr& getFence() const { return mpFence; }
        LowLevelContextApiData* getApiData() const { return mpApiData; }
        void setCommandList(CommandListHandle pList) { mpList = pList; }
    protected:
        LowLevelContextData(CommandQueueType type, CommandQueueHandle queue);

        LowLevelContextApiData* mpApiData = nullptr;
        CommandQueueType mType;
        CommandListHandle mpList;
        CommandQueueHandle mpQueue; // Can be nullptr
        CommandAllocatorHandle mpAllocator;
        GpuFence::SharedPtr mpFence;
    };

	class Texture;

	class CopyContext
	{
	public:
		def_shared_ptr(CopyContext);
		~CopyContext();
        class ReadTextureTask
        {
        public:
            using SharedPtr = std::shared_ptr<ReadTextureTask>;
            static SharedPtr create(CopyContext* pCtx, const Texture* pTexture, uint32_t subresourceIndex);
            std::vector<uint8_t> getData();
        private:
            ReadTextureTask() = default;
            GpuFence::SharedPtr mpFence;
            Buffer::SharedPtr mpBuffer;
            CopyContext* mpContext;
#ifdef FALCOR_D3D12
            uint32_t mRowCount;
            ResourceFormat mTextureFormat;
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT mFootprint;
#elif defined(FALCOR_VK)
            size_t mDataSize;
#endif
        };
	};
}