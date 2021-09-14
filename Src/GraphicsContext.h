#pragma once
#include <memory>
#include <vector>
#include "GraphicsCommon.h"
#include "GraphicsResource.h"

#define def_shared_ptr(T) \
using SharedPtr = std::shared_ptr<T>;\
using SharedConstPtr = std::shared_ptr<const T>;

namespace WIP3D
{
    struct LowLevelContextApiData;
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
        using SharedPtr = std::shared_ptr<CopyContext>;
        using SharedConstPtr = std::shared_ptr<const CopyContext>;

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
            uint32_t mRowCount;
            ResourceFormat mTextureFormat;
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT mFootprint;
        };

        virtual ~CopyContext();

        /** Create a copy context.
            \param[in] queue Command queue handle.
            \return A new object, or throws an exception if creation failed.
        */
        static SharedPtr create(CommandQueueHandle queue);

        /** Flush the command list. This doesn't reset the command allocator, just submits the commands
            \param[in] wait If true, will block execution until the GPU finished processing the commands
        */
        virtual void flush(bool wait = false);

        /** Check if we have pending commands
        */
        bool hasPendingCommands() const { return mCommandsPending; }

        /** Signal the context that we have pending commands. Useful in case you make raw API calls
        */
        void setPendingCommands(bool commandsPending) { mCommandsPending = commandsPending; }

        /** Insert a resource barrier
            if pViewInfo is nullptr, will transition the entire resource. Otherwise, it will only transition the subresource in the view
            \return true if a barrier commands were recorded for the entire resource-view, otherwise false (for example, when the current resource state is the same as the new state or when only some subresources were transitioned)
        */
        virtual bool resourceBarrier(const Resource* pResource, Resource::State newState, const ResourceViewInfo* pViewInfo = nullptr);

        /** Insert a UAV barrier
        */
        virtual void uavBarrier(const Resource* pResource);

        /** Copy an entire resource
        */
        void copyResource(const Resource* pDst, const Resource* pSrc);

        /** Copy a subresource
        */
        void copySubresource(const Texture* pDst, uint32_t dstSubresourceIdx, const Texture* pSrc, uint32_t srcSubresourceIdx);

        /** Copy part of a buffer
        */
        void copyBufferRegion(const Buffer* pDst, uint64_t dstOffset, const Buffer* pSrc, uint64_t srcOffset, uint64_t numBytes);

        /** Copy a region of a subresource from one texture to another
            `srcOffset`, `dstOffset` and `size` describe the source and destination regions. For any channel of `extent` that is -1, the source texture dimension will be used
        */
        void copySubresourceRegion(const Texture* pDst, uint32_t dstSubresource, const Texture* pSrc, uint32_t srcSubresource, const RBVector3IU& dstOffset = RBVector3IU(0), const RBVector3IU& srcOffset = RBVector3IU(0), const RBVector3IU& size = RBVector3IU(-1));

        /** Update a texture's subresource data
            `offset` and `size` describe a region to update. For any channel of `extent` that is -1, the texture dimension will be used.
            pData can't be null. The size of the pointed buffer must be equal to a single texel size times the size of the region we are updating
        */
        void updateSubresourceData(const Texture* pDst, uint32_t subresource, const void* pData, const RBVector3IU& offset = RBVector3IU(0), const RBVector3IU& size = RBVector3IU(-1));

        /** Update an entire texture
        */
        void updateTextureData(const Texture* pTexture, const void* pData);

        /** Update a buffer
        */
        void updateBuffer(const Buffer* pBuffer, const void* pData, size_t offset = 0, size_t numBytes = 0);

        /** Read texture data synchronously. Calling this command will flush the pipeline and wait for the GPU to finish execution
        */
        std::vector<uint8_t> readTextureSubresource(const Texture* pTexture, uint32_t subresourceIndex);

        /** Read texture data Asynchronously
        */
        ReadTextureTask::SharedPtr asyncReadTextureSubresource(const Texture* pTexture, uint32_t subresourceIndex);

        /** Get the low-level context data
        */
        virtual const LowLevelContextData::SharedPtr& getLowLevelData() const { return mpLowLevelData; }

        /** Override the low-level context data with a user provided object
        */
        void setLowLevelContextData(LowLevelContextData::SharedPtr pLowLevelData) { mpLowLevelData = pLowLevelData; }

        /** Bind the descriptor heaps from the device into the command list.
        */
        void bindDescriptorHeaps();

    protected:
        CopyContext(LowLevelContextData::CommandQueueType type, CommandQueueHandle queue);

        bool textureBarrier(const Texture* pTexture, Resource::State newState);
        bool bufferBarrier(const Buffer* pBuffer, Resource::State newState);
        bool subresourceBarriers(const Texture* pTexture, Resource::State newState, const ResourceViewInfo* pViewInfo);
        void apiSubresourceBarrier(const Texture* pTexture, Resource::State newState, Resource::State oldState, uint32_t arraySlice, uint32_t mipLevel);
        void updateTextureSubresources(const Texture* pTexture, uint32_t firstSubresource, uint32_t subresourceCount, const void* pData, const RBVector3IU& offset = RBVector3IU(0), const RBVector3IU& size = RBVector3IU(-1));

        bool mCommandsPending = false;
        LowLevelContextData::SharedPtr mpLowLevelData;
    };

    class ComputeContext : public CopyContext
    {
    public:
        using SharedPtr = std::shared_ptr<ComputeContext>;
        using SharedConstPtr = std::shared_ptr<const ComputeContext>;

        ~ComputeContext();

        /** Create a new compute context.
            \param[in] queue Command queue handle.
            \return A new object, or throws an exception if creation failed.
        */
        static SharedPtr create(CommandQueueHandle queue);

#if 1
        /** Dispatch a compute task
            \param[in] dispatchSize 3D dispatch group size
        */
        void dispatch(ComputeState* pState, ComputeVars* pVars, const uint3& dispatchSize);

        /** Executes a dispatch call. Args to the dispatch call are contained in pArgBuffer
        */
        void dispatchIndirect(ComputeState* pState, ComputeVars* pVars, const Buffer* pArgBuffer, uint64_t argBufferOffset);

        /** Clear an unordered-access view
            \param[in] pUav The UAV to clear
            \param[in] value The clear value
        */
        void clearUAV(const UnorderedAccessView* pUav, const RBVector4& value);

        /** Clear an unordered-access view
            \param[in] pUav The UAV to clear
            \param[in] value The clear value
        */
        void clearUAV(const UnorderedAccessView* pUav, const uint4& value);

        /** Clear a structured buffer's UAV counter
            \param[in] pBuffer Structured Buffer containing UAV counter
            \param[in] value Value to clear counter to
        */
        void clearUAVCounter(const Buffer::SharedPtr& pBuffer, uint32_t value);

        /** Submit the command list
        */
        virtual void flush(bool wait = false) override;

    protected:
        ComputeContext(LowLevelContextData::CommandQueueType type, CommandQueueHandle queue);
        bool prepareForDispatch(ComputeState* pState, ComputeVars* pVars);
        bool applyComputeVars(ComputeVars* pVars, RootSignature* pRootSignature);

        const ComputeVars* mpLastBoundComputeVars = nullptr;
#endif
    };

    class FullScreenPass;

    /** The rendering context. Use it to bind state and dispatch calls to the GPU
    */
    class RenderContext : public ComputeContext
    {
    public:
        using SharedPtr = std::shared_ptr<RenderContext>;
        using SharedConstPtr = std::shared_ptr<const RenderContext>;

        /**
            This flag control which aspects of the GraphicState will be bound into the pipeline before drawing.
            It is useful in cases where the user wants to set a specific object using a raw-API call before calling one of the draw functions
        */
        enum class StateBindFlags : uint32_t
        {
            None = 0x0,              ///<Bind Nothing
            Vars = 0x1,              ///<Bind Graphics Vars (root signature and sets)
            Topology = 0x2,              ///<Bind Primitive Topology
            Vao = 0x4,              ///<Bind Vao
            Fbo = 0x8,              ///<Bind Fbo
            Viewports = 0x10,             ///<Bind Viewport
            Scissors = 0x20,             ///<Bind scissors
            PipelineState = 0x40,             ///<Bind Pipeline State Object
            SamplePositions = 0x80,             ///<Set the programmable sample positions
            All = uint32_t(-1)
        };

        ~RenderContext();

        /** Create a new render context.
            \param[in] queue The command queue.
            \return A new object, or throws an exception if creation failed.
        */
        static SharedPtr create(CommandQueueHandle queue);
#if 1
        /** Clear an FBO.
            \param[in] pFbo The FBO to clear
            \param[in] color The clear color for the bound render-targets
            \param[in] depth The depth clear value
            \param[in] stencil The stencil clear value
            \param[in] flags Optional. Which components of the FBO to clear. By default will clear all attached resource.
            If you'd like to clear a specific color target, you can use RenderContext#clearFboColorTarget().
        */
        void clearFbo(const Fbo* pFbo, const RBVector4& color, float depth, uint8_t stencil, FboAttachmentType flags = FboAttachmentType::All);

        /** Clear a render-target view.
            \param[in] pRtv The RTV to clear
            \param[in] color The clear color
        */
        void clearRtv(const RenderTargetView* pRtv, const RBVector4& color);

        /** Clear a depth-stencil view.
            \param[in] pDsv The DSV to clear
            \param[in] depth The depth clear value
            \param[in] stencil The stencil clear value
            \param[in] clearDepth Optional. Controls whether or not to clear the depth channel
            \param[in] clearStencil Optional. Controls whether or not to clear the stencil channel
        */
        void clearDsv(const DepthStencilView* pDsv, float depth, uint8_t stencil, bool clearDepth = true, bool clearStencil = true);

        /** Clear a texture. The function will use the bind-flags to find the optimal API call to make
            \param[in] pTexture The texture to clear
            \param[in] clearColor The clear color
            The function only support floating-point and normalized color-formats and depth. For depth buffers, `clearColor.x` will be used. If there's a stencil-channel, `clearColor.y` must be zero
        */
        void clearTexture(Texture* pTexture, const RBVector4& clearColor = RBVector4(0, 0, 0, 1));


        /** Ordered draw call.
            \param[in] vertexCount Number of vertices to draw
            \param[in] startVertexLocation The location of the first vertex to read from the vertex buffers (offset in vertices)
        */
        void draw(GraphicsState* pState, GraphicsVars* pVars, uint32_t vertexCount, uint32_t startVertexLocation);

        /** Ordered instanced draw call.
            \param[in] vertexCount Number of vertices to draw
            \param[in] instanceCount Number of instances to draw
            \param[in] startVertexLocation The location of the first vertex to read from the vertex buffers (offset in vertices)
            \param[in] startInstanceLocation A value which is added to each index before reading per-instance data from the vertex buffer
        */
        void drawInstanced(GraphicsState* pState, GraphicsVars* pVars, uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation);

        /** Indexed draw call.
            \param[in] indexCount Number of indices to draw
            \param[in] startIndexLocation The location of the first index to read from the index buffer (offset in indices)
            \param[in] baseVertexLocation A value which is added to each index before reading a vertex from the vertex buffer
        */
        void drawIndexed(GraphicsState* pState, GraphicsVars* pVars, uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation);

        /** Indexed instanced draw call.
            \param[in] indexCount Number of indices to draw per instance
            \param[in] instanceCount Number of instances to draw
            \param[in] startIndexLocation The location of the first index to read from the index buffer (offset in indices)
            \param[in] baseVertexLocation A value which is added to each index before reading a vertex from the vertex buffer
            \param[in] startInstanceLocation A value which is added to each index before reading per-instance data from the vertex buffer
        */
        void drawIndexedInstanced(GraphicsState* pState, GraphicsVars* pVars, uint32_t indexCount, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation);

        /** Executes an indirect draw call.
            \param[in] maxCommandCount If pCountBuffer is null, this specifies the command count. Otherwise, command count is minimum of maxCommandCount and the value contained in pCountBuffer
            \param[in] pArgBuffer Buffer containing draw arguments
            \param[in] argBufferOffset Offset into buffer to read arguments from
            \param[in] pCountBuffer Optional. A GPU buffer that contains a uint32 value specifying the command count. This can, but does not have to be a dedicated buffer
            \param[in] countBufferOffset Offset into pCountBuffer to read the value from
        */
        void drawIndirect(GraphicsState* pState, GraphicsVars* pVars, uint32_t maxCommandCount, const Buffer* pArgBuffer, uint64_t argBufferOffset, const Buffer* pCountBuffer, uint64_t countBufferOffset);

        /** Executes an indirect draw-indexed call.
            \param[in] maxCommandCount If pCountBuffer is null, this specifies the command count. Otherwise, command count is minimum of maxCommandCount and the value contained in pCountBuffer
            \param[in] pArgBuffer Buffer containing draw arguments
            \param[in] argBufferOffset Offset into buffer to read arguments from
            \param[in] pCountBuffer Optional. A GPU buffer that contains a uint32 value specifying the command count. This can, but does not have to be a dedicated buffer
            \param[in] countBufferOffset Offset into pCountBuffer to read the value from
        */
        void drawIndexedIndirect(GraphicsState* pState, GraphicsVars* pVars, uint32_t maxCommandCount, const Buffer* pArgBuffer, uint64_t argBufferOffset, const Buffer* pCountBuffer, uint64_t countBufferOffset);

        /** Blits (low-level copy) an SRV into an RTV.
            \param[in] pSrc Source view to copy from
            \param[in] pDst Target view to copy to
            \param[in] srcRect Source rectangle to blit from, specified by [left, up, right, down]
            \param[in] dstRect Target rectangle to blit to, specified by [left, up, right, down]
        */
        void blit(ShaderResourceView::SharedPtr pSrc, RenderTargetView::SharedPtr pDst, const RBVector4IU& srcRect = RBVector4IU(-1), const RBVector4IU& dstRect = RBVector4IU(-1), Sampler::Filter = Sampler::Filter::Linear);

        /** Submit the command list
        */
        void flush(bool wait = false) override;

        /** Tell the render context what it should and shouldn't bind before drawing
        */
        void setBindFlags(StateBindFlags flags) { mBindFlags = flags; }

        /** Get the render context bind flags so the user can restore the state after setBindFlags()
        */
        StateBindFlags getBindFlags() const { return mBindFlags; }

        /** Resolve an entire multi-sampled resource. The dst and src resources must have the same dimensions, array-size, mip-count and format.
            If any of these properties don't match, you'll have to use `resolveSubresource`
        */
        void resolveResource(const Texture::SharedPtr& pSrc, const Texture::SharedPtr& pDst);

        /** Resolve a multi-sampled sub-resource
        */
        void resolveSubresource(const Texture::SharedPtr& pSrc, uint32_t srcSubresource, const Texture::SharedPtr& pDst, uint32_t dstSubresource);

#ifdef WIP_D3D12
        /** Submit a raytrace command. This function doesn't change the state of the render-context. Graphics/compute vars and state will stay the same.
        */
        void raytrace(RtProgram* pProgram, RtProgramVars* pVars, uint32_t width, uint32_t height, uint32_t depth);
#endif

    private:
        RenderContext(CommandQueueHandle queue);
        bool applyGraphicsVars(GraphicsVars* pVars, RootSignature* pRootSignature);
        bool prepareForDraw(GraphicsState* pState, GraphicsVars* pVars);

        StateBindFlags mBindFlags = StateBindFlags::All;
        GraphicsVars* mpLastBoundGraphicsVars = nullptr;
#endif
    };

}