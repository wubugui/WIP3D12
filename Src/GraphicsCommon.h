#pragma once
#include <memory>
#include <vector>
#include <unordered_set>
#include <deque>
#include <queue>
#include "Vector4.h"
#include "Formats.h"
#include "GraphicsResource.h"
#include "GraphicsResView.h"

//#include "GraphicsResource.h"
namespace WIP3D
{
    enum class ShaderType
    {
        Vertex,         ///< Vertex shader
        Pixel,          ///< Pixel shader
        Geometry,       ///< Geometry shader
        Hull,           ///< Hull shader (AKA Tessellation control shader)
        Domain,         ///< Domain shader (AKA Tessellation evaluation shader)
        Compute,        ///< Compute shader

#ifdef WIP_D3D12
        RayGeneration,  ///< Ray generation shader
        Intersection,   ///< Intersection shader
        AnyHit,         ///< Any hit shader
        ClosestHit,     ///< Closest hit shader
        Miss,           ///< Miss shader
        Callable,       ///< Callable shader
#endif
        Count           ///< Shader Type count
    };

    /** Shading languages. Used for shader cross-compilation.
    */
    enum class ShadingLanguage
    {
        Unknown,        ///< Unknown language (e.g., for a plain .h file)
        GLSL,           ///< OpenGL Shading Language (GLSL)
        VulkanGLSL,     ///< GLSL for Vulkan
        HLSL,           ///< High-Level Shading Language
        Slang,          ///< Slang shading language
    };

    /** Framebuffer target flags. Used for clears and copy operations
    */
    enum class FboAttachmentType
    {
        None = 0,    ///< Nothing. Here just for completeness
        Color = 1,    ///< Operate on the color buffer.
        Depth = 2,    ///< Operate on the the depth buffer.
        Stencil = 4,    ///< Operate on the the stencil buffer.

        All = Color | Depth | Stencil ///< Operate on all targets
    };

    enum_class_operators(FboAttachmentType);




    enum class ComparisonFunc
    {
        Disabled,       ///< Comparison is disabled
        Never,          ///< Comparison always fails
        Always,         ///< Comparison always succeeds
        Less,           ///< Passes if source is less than the destination
        Equal,          ///< Passes if source is equal to the destination
        NotEqual,       ///< Passes if source is not equal to the destination
        LessEqual,      ///< Passes if source is less than or equal to the destination
        Greater,        ///< Passes if source is greater than to the destination
        GreaterEqual,   ///< Passes if source is greater than or equal to the destination
    };

    /** Converts ShaderType enum elements to a string.
      \param[in] type Type to convert to string
      \return Shader type as a string
  */
    inline const std::string to_string(ShaderType Type)
    {
        switch (Type)
        {
        case ShaderType::Vertex:
            return "vertex";
        case ShaderType::Pixel:
            return "pixel";
        case ShaderType::Hull:
            return "hull";
        case ShaderType::Domain:
            return "domain";
        case ShaderType::Geometry:
            return "geometry";
        case ShaderType::Compute:
            return "compute";
#ifdef WIP_D3D12
        case ShaderType::RayGeneration:
            return "raygeneration";
        case ShaderType::Intersection:
            return "intersection";
        case ShaderType::AnyHit:
            return "anyhit";
        case ShaderType::ClosestHit:
            return "closesthit";
        case ShaderType::Miss:
            return "miss";
        case ShaderType::Callable:
            return "callable";
#endif
        default:
            assert(false);
            return "";
        }
    }

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
            AccelerationStructureSrv,
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


    // 一个创建ObjectType对象的池
    // 使用fence的值来重分配，当cache对象的值小于等于fence的gpu值，说明对象还在使用
    // 否则就拉出来重用
    template<typename ObjectType>
    class FencedPool : public std::enable_shared_from_this<FencedPool<ObjectType>>
    {
    public:
        using SharedPtr = std::shared_ptr<FencedPool<ObjectType>>;
        using SharedConstPtr = std::shared_ptr<const FencedPool<ObjectType>>;
        using NewObjectFuncType = ObjectType(*)(void*);

        /** Create a new fenced pool.
            \param[in] pFence GPU fence to use for synchronization.
            \param[in] newFunc Ptr to function called to create new objects.
            \param[in] pUserData Optional ptr to user data passed to the object creation function.
            \return A new object, or throws an exception if creation failed.
        */
        static SharedPtr create(GpuFence::SharedConstPtr pFence, NewObjectFuncType newFunc, void* pUserData = nullptr)
        {
            return SharedPtr(new FencedPool(pFence, newFunc, pUserData));
        }

        /** Return an object.
            \return An object, or throws an exception on failure.
        */
        ObjectType newObject()
        {
            // Retire the active object
            Data data;
            // 已经分配出去的值永远都会在下一次分配的时候重新压回queue，所以不存在泄露
            data.alloc = mActiveObject;
            data.timestamp = mpFence->getCpuValue();
            mQueue.push(data);

            // The queue is sorted based on time. Check if the first object is free
            data = mQueue.front();
            // 这个fence只要一个，只会在flush的时候被改变
            // 一旦这个fence的gpu达到了某个值，那说明这个值之前分配的所有东西都已经使用完成了
            // mQueue的总数应该是flush前所有的分配数
            if (data.timestamp <= mpFence->getGpuValue())
            {
                mQueue.pop();
            }
            else
            {
                // alloc中原来存的对象没有pop就还在queue中，不存在丢失
                data.alloc = createObject();
            }

            mActiveObject = data.alloc;
            return mActiveObject;
        }

    private:
        FencedPool(GpuFence::SharedConstPtr pFence, NewObjectFuncType newFunc, void* pUserData)
            : mpUserData(pUserData)
            , mpFence(pFence)
            , mNewObjFunc(newFunc)
        {
            assert(pFence && newFunc);
            mActiveObject = createObject();
        }

        ObjectType createObject()
        {
            ObjectType pObj = mNewObjFunc(mpUserData);
            if (pObj == nullptr) throw std::exception("Failed to create new object in fenced pool");
            return pObj;
        }

        struct Data
        {
            ObjectType alloc;
            uint64_t timestamp;
        };

        ObjectType mActiveObject;
        NewObjectFuncType mNewObjFunc = nullptr;
        std::queue<Data> mQueue;
        GpuFence::SharedConstPtr mpFence;
        void* mpUserData;
    };

    /** Describes the layout of a vertex buffer that will be bound to a render operation as part of a VAO.
    */
    class VertexBufferLayout : public std::enable_shared_from_this<VertexBufferLayout>
    {
    public:
        using SharedPtr = std::shared_ptr<VertexBufferLayout>;
        using SharedConstPtr = std::shared_ptr<const VertexBufferLayout>;

        enum class InputClass
        {
            PerVertexData,      ///< Buffer elements will represent per-vertex data
            PerInstanceData     ///< Buffer elements will represent per-instance data
        };

        /** Create a new vertex buffer layout object.
            \return New object, or throws an exception on error.
        */
        static SharedPtr create()
        {
            return SharedPtr(new VertexBufferLayout());
        }

        /** Add a new element to the layout.
            \param name The semantic name of the element. In OpenGL this is just a descriptive field. In DX, this is the semantic name used to match the element with the shader input signature.
            \param offset Offset in bytes of the element from the start of the vertex.
            \param format The format of each channel in the element.
            \param arraySize The array size of the input element. Must be at least 1.
            \param shaderLocation The attribute binding location in the shader.
        */
        void addElement(const std::string& name, uint32_t offset, ResourceFormat format, uint32_t arraySize, uint32_t shaderLocation)
        {
            Element Elem;
            Elem.offset = offset;
            Elem.format = format;
            Elem.shaderLocation = shaderLocation;
            Elem.name = name;
            Elem.arraySize = arraySize;
            mElements.push_back(Elem);
            mVertexStride += getFormatBytesPerBlock(Elem.format) * Elem.arraySize;
        }

        /** Return the element offset pointed to by Index
        */
        uint32_t getElementOffset(uint32_t index) const
        {
            return mElements[index].offset;
        }

        /** Return the element format pointed to by Index
        */
        ResourceFormat getElementFormat(uint32_t index) const
        {
            return mElements[index].format;
        }

        /** Return the semantic name of the element
        */
        const std::string& getElementName(uint32_t index) const
        {
            return mElements[index].name;
        }

        /** Return the array size the element
        */
        const uint32_t getElementArraySize(uint32_t index) const
        {
            return mElements[index].arraySize;
        }

        /** Return the element shader binding location pointed to by Index
        */
        uint32_t getElementShaderLocation(uint32_t index) const
        {
            return mElements[index].shaderLocation;
        }

        /** Return the number of elements in the object
        */
        uint32_t getElementCount() const
        {
            return (uint32_t)mElements.size();
        }

        /** Return the total stride of all elements in bytes
        */
        uint32_t getStride() const { return mVertexStride; }

        /** Return the input classification
        */
        InputClass getInputClass() const
        {
            return mClass;
        }

        /** Returns the per-instance data step rate
        */
        uint32_t getInstanceStepRate() const
        {
            return mInstanceStepRate;
        }

        /** Set the input class and the data step rate
            \param inputClass Specifies is this layout object holds per-vertex or per-instance data
            \param instanceStepRate For per-instance data, specifies how many instance to draw using the same per-instance data. If this is zero, it behaves as if the class is PerVertexData
        */
        void setInputClass(InputClass inputClass, uint32_t stepRate) { mClass = inputClass; mInstanceStepRate = stepRate; }

        static const uint32_t kInvalidShaderLocation = uint32_t(-1);
    private:
        VertexBufferLayout() = default;

        struct Element
        {
            uint32_t offset = 0;
            ResourceFormat format = ResourceFormat::Unknown;
            uint32_t shaderLocation = kInvalidShaderLocation;
            std::string name;
            uint32_t arraySize;
            uint32_t vbIndex;
        };

        std::vector<Element> mElements;
        InputClass mClass = InputClass::PerVertexData;
        uint32_t mInstanceStepRate = 0;
        uint32_t mVertexStride = 0;
    };

    /** Container to hold layouts for every vertex layout that will be bound at once to a VAO.
    */
    class VertexLayout : public std::enable_shared_from_this<VertexLayout>
    {
    public:
        using SharedPtr = std::shared_ptr<VertexLayout>;
        using SharedConstPtr = std::shared_ptr<const VertexLayout>;

        /** Create a new vertex layout object.
            \return New object, or throws an exception on error.
        */
        static SharedPtr create() { return SharedPtr(new VertexLayout()); }

        /** Add a layout description for a buffer.
        */
        void addBufferLayout(uint32_t index, VertexBufferLayout::SharedConstPtr pLayout)
        {
            if (mpBufferLayouts.size() <= index)
            {
                mpBufferLayouts.resize(index + 1);
            }
            mpBufferLayouts[index] = pLayout;
        }

        /** Get a buffer layout.
        */
        const VertexBufferLayout::SharedConstPtr& getBufferLayout(size_t index) const
        {
            return mpBufferLayouts[index];
        }

        /** Get how many buffer descriptions there are.
        */
        size_t getBufferCount() const { return mpBufferLayouts.size(); }

    private:
        VertexLayout() { mpBufferLayouts.reserve(16); }
        std::vector<VertexBufferLayout::SharedConstPtr> mpBufferLayouts;
    };

    /** Abstracts vertex array objects. A VAO must at least specify a primitive topology. You may additionally specify a number of vertex buffer layouts
        corresponding to the number of vertex buffers to be bound. The number of vertex buffers to be bound must match the number described in the layout.
    */
    class Vao : public std::enable_shared_from_this<Vao>
    {
    public:
        using SharedPtr = std::shared_ptr<Vao>;
        using WeakPtr = std::weak_ptr<Vao>;
        using SharedConstPtr = std::shared_ptr<const Vao>;
        ~Vao() = default;

        /** Primitive topology
        */
        enum class Topology
        {
            Undefined,
            PointList,
            LineList,
            LineStrip,
            TriangleList,
            TriangleStrip
        };

        struct ElementDesc
        {
            static const uint32_t kInvalidIndex = -1;
            uint32_t vbIndex = kInvalidIndex;
            uint32_t elementIndex = kInvalidIndex;
        };

        using BufferVec = std::vector<Buffer::SharedPtr>;

        /** Create a new vertex array object.
            \param primTopology The primitive topology.
            \param pLayout The vertex layout description. Can be nullptr.
            \param pVBs Array of pointers to vertex buffers. Number of buffers must match with pLayout.
            \param pIB Pointer to the index buffer. Can be nullptr, in which case no index buffer will be bound.
            \param ibFormat The resource format of the index buffer. Can be either R16Uint or R32Uint.
            \return New object, or throws an exception on error.
        */
        static SharedPtr create(Topology primTopology, const VertexLayout::SharedPtr& pLayout = nullptr, const BufferVec& pVBs = BufferVec(), const Buffer::SharedPtr& pIB = nullptr, ResourceFormat ibFormat = ResourceFormat::Unknown);

        /** Get the API handle
        */
        const VaoHandle& getApiHandle() const;

        /** Get the vertex buffer count
        */
        uint32_t getVertexBuffersCount() const { return (uint32_t)mpVBs.size(); }

        /** Get a vertex buffer
        */
        const Buffer::SharedPtr& getVertexBuffer(uint32_t index) const { assert(index < (uint32_t)mpVBs.size()); return mpVBs[index]; }

        /** Get a vertex buffer layout
        */
        const VertexLayout::SharedPtr& getVertexLayout() const { return mpVertexLayout; }

        /** Return the vertex buffer index and the element index by its location.
            If the element is not found, returns the default ElementDesc
        */
        ElementDesc getElementIndexByLocation(uint32_t elementLocation) const;

        /** Get the index buffer
        */
        const Buffer::SharedPtr& getIndexBuffer() const { return mpIB; }

        /** Get the index buffer format
        */
        ResourceFormat getIndexBufferFormat() const { return mIbFormat; }

        /** Get the primitive topology
        */
        Topology getPrimitiveTopology() const { return mTopology; }

    protected:
        friend class RenderContext;

    private:
        Vao(const BufferVec& pVBs, const VertexLayout::SharedPtr& pLayout, const Buffer::SharedPtr& pIB, ResourceFormat ibFormat, Topology primTopology);

        VaoHandle mApiHandle;
        VertexLayout::SharedPtr mpVertexLayout;
        BufferVec mpVBs;
        Buffer::SharedPtr mpIB;
        void* mpPrivateData = nullptr;
        ResourceFormat mIbFormat;
        Topology mTopology;
    };

    /** Blend state
    */
    class BlendState : public std::enable_shared_from_this<BlendState>
    {
    public:
        using SharedPtr = std::shared_ptr<BlendState>;
        using SharedConstPtr = std::shared_ptr<const BlendState>;

        /** Defines how to combine the blend inputs
        */
        enum class BlendOp
        {
            Add,                ///< Add src1 and src2
            Subtract,           ///< Subtract src1 from src2
            ReverseSubtract,    ///< Subtract src2 from src1
            Min,                ///< Find the minimum between the sources (per-channel)
            Max                 ///< Find the maximum between the sources (per-channel)
        };

        /** Defines how to modulate the fragment-shader and render-target pixel values
        */
        enum class BlendFunc
        {
            Zero,                   ///< (0, 0, 0, 0)
            One,                    ///< (1, 1, 1, 1)
            SrcColor,               ///< The fragment-shader output color
            OneMinusSrcColor,       ///< One minus the fragment-shader output color
            DstColor,               ///< The render-target color
            OneMinusDstColor,       ///< One minus the render-target color
            SrcAlpha,               ///< The fragment-shader output alpha value
            OneMinusSrcAlpha,       ///< One minus the fragment-shader output alpha value
            DstAlpha,               ///< The render-target alpha value
            OneMinusDstAlpha,       ///< One minus the render-target alpha value
            BlendFactor,            ///< Constant color, set using Desc#SetBlendFactor()
            OneMinusBlendFactor,    ///< One minus constant color, set using Desc#SetBlendFactor()
            SrcAlphaSaturate,       ///< (f, f, f, 1), where f = min(fragment shader output alpha, 1 - render-target pixel alpha)
            Src1Color,              ///< Fragment-shader output color 1
            OneMinusSrc1Color,      ///< One minus fragment-shader output color 1
            Src1Alpha,              ///< Fragment-shader output alpha 1
            OneMinusSrc1Alpha       ///< One minus fragment-shader output alpha 1
        };

        /** Descriptor used to create new blend-state
        */
        class Desc
        {
        public:
            Desc();
            friend class BlendState;

            /** Set the constant blend factor
                \param[in] factor Blend factor
            */
            Desc& setBlendFactor(const RBVector4& factor) { mBlendFactor = factor; return *this; }

            /** Enable/disable independent blend modes for different render target. Only used when multiple render-targets are bound.
                \param[in] enabled True If false, will use RenderTargetDesc[0] for all the bound render-targets. Otherwise, will use the entire RenderTargetDesc[] array.
            */
            Desc& setIndependentBlend(bool enabled) { mEnableIndependentBlend = enabled; return *this; }

            /** Set the blend parameters
                \param[in] rtIndex The RT index to set the parameters into. If independent blending is disabled, only the index 0 is used.
                \param[in] rgbOp Blend operation for the RGB channels
                \param[in] alphaOp Blend operation for the alpha channels
                \param[in] srcRgbFunc Blend function for the fragment-shader output RGB channels
                \param[in] dstRgbFunc Blend function for the render-target RGB channels
                \param[in] srcAlphaFunc Blend function for the fragment-shader output alpha channel
                \param[in] dstAlphaFunc Blend function for the render-target alpha channel
            */
            Desc& setRtParams(uint32_t rtIndex, BlendOp rgbOp, BlendOp alphaOp, BlendFunc srcRgbFunc, BlendFunc dstRgbFunc, BlendFunc srcAlphaFunc, BlendFunc dstAlphaFunc);

            /** Enable/disable blending for a specific render-target. If independent blending is disabled, only the index 0 is used.
            */
            Desc& setRtBlend(uint32_t rtIndex, bool enable) { mRtDesc[rtIndex].blendEnabled = enable; return *this; }

            /** Enable/disable alpha-to-coverage
                \param[in] enabled True to enable alpha-to-coverage, false to disable it
            */
            Desc& setAlphaToCoverage(bool enabled) { mAlphaToCoverageEnabled = enabled; return *this; }

            /** Set color write-mask
            */
            Desc& setRenderTargetWriteMask(uint32_t rtIndex, bool writeRed, bool writeGreen, bool writeBlue, bool writeAlpha);

            struct RenderTargetDesc
            {
                bool blendEnabled = false;
                BlendOp rgbBlendOp = BlendOp::Add;
                BlendOp alphaBlendOp = BlendOp::Add;
                BlendFunc srcRgbFunc = BlendFunc::One;
                BlendFunc srcAlphaFunc = BlendFunc::One;
                BlendFunc dstRgbFunc = BlendFunc::Zero;
                BlendFunc dstAlphaFunc = BlendFunc::Zero;
                struct WriteMask
                {
                    bool writeRed = true;
                    bool writeGreen = true;
                    bool writeBlue = true;
                    bool writeAlpha = true;
                };
                WriteMask writeMask;
            };

        protected:
            std::vector<RenderTargetDesc> mRtDesc;
            bool mEnableIndependentBlend = false;
            bool mAlphaToCoverageEnabled = false;
            RBVector4 mBlendFactor = RBVector4(0, 0, 0, 0);
        };

        ~BlendState();

        /** Create a new blend state object.
            \param[in] Desc Blend state descriptor.
            \return A new object, or throws an exception if creation failed.
        */
        static BlendState::SharedPtr create(const Desc& desc);

        /** Get the constant blend factor color
        */
        const RBVector4& getBlendFactor() const { return mDesc.mBlendFactor; }

        /** Get the RGB blend operation
        */
        BlendOp getRgbBlendOp(uint32_t rtIndex) const { return mDesc.mRtDesc[rtIndex].rgbBlendOp; }

        /** Get the alpha blend operation
        */
        BlendOp getAlphaBlendOp(uint32_t rtIndex) const { return mDesc.mRtDesc[rtIndex].alphaBlendOp; }

        /** Get the fragment-shader RGB blend func
        */
        BlendFunc getSrcRgbFunc(uint32_t rtIndex)   const { return mDesc.mRtDesc[rtIndex].srcRgbFunc; }

        /** Get the fragment-shader alpha blend func
        */
        BlendFunc getSrcAlphaFunc(uint32_t rtIndex) const { return mDesc.mRtDesc[rtIndex].srcAlphaFunc; }

        /** Get the render-target RGB blend func
        */
        BlendFunc getDstRgbFunc(uint32_t rtIndex)   const { return mDesc.mRtDesc[rtIndex].dstRgbFunc; }

        /** Get the render-target alpha blend func
        */
        BlendFunc getDstAlphaFunc(uint32_t rtIndex) const { return mDesc.mRtDesc[rtIndex].dstAlphaFunc; }

        /** Check if blend is enabled
        */
        bool isBlendEnabled(uint32_t rtIndex) const { return mDesc.mRtDesc[rtIndex].blendEnabled; }

        /** Check if alpha-to-coverage is enabled
        */
        bool isAlphaToCoverageEnabled() const { return mDesc.mAlphaToCoverageEnabled; }

        /** Check if independent blending is enabled
        */
        bool isIndependentBlendEnabled() const { return mDesc.mEnableIndependentBlend; }

        /** Get a render-target descriptor
        */
        const Desc::RenderTargetDesc& getRtDesc(size_t rtIndex) const { return mDesc.mRtDesc[rtIndex]; }

        /** Get the render-target array size
        */
        uint32_t getRtCount() const { return (uint32_t)mDesc.mRtDesc.size(); }

        /** Get the API handle
        */
        const BlendStateHandle& getApiHandle() const;

    private:
        BlendState(const Desc& Desc) : mDesc(Desc) {}
        const Desc mDesc;
        BlendStateHandle mApiHandle;
    };
    

    /** Rasterizer state
    */
    class RasterizerState : public std::enable_shared_from_this<RasterizerState>
    {
    public:
        using SharedPtr = std::shared_ptr<RasterizerState>;
        using SharedConstPtr = std::shared_ptr<const RasterizerState>;

        /** Cull mode
        */
        enum class CullMode : uint32_t
        {
            None,   ///< No culling
            Front,  ///< Cull front-facing primitives
            Back,   ///< Cull back-facing primitives
        };

        /** Polygon fill mode
        */
        enum class FillMode
        {
            Wireframe,   ///< Wireframe
            Solid        ///< Solid
        };

        /** Rasterizer state descriptor
        */
        class Desc
        {
        public:
            friend class RasterizerState;

            /** Set the cull mode
            */
            Desc& setCullMode(CullMode mode) { mCullMode = mode; return *this; }

            /** Set the fill mode
            */
            Desc& setFillMode(FillMode mode) { mFillMode = mode; return *this; }

            /** Determines how to interpret triangle direction.
                \param isFrontCCW If true, a triangle is front-facing if is vertices are counter-clockwise. If false, the opposite.
            */
            Desc& setFrontCounterCW(bool isFrontCCW) { mIsFrontCcw = isFrontCCW; return *this; }

            /** Set the depth-bias. The depth bias is calculated as
                \code
                bias = (float)depthBias * r + slopeScaledBias * maxDepthSlope
                \endcode
                where r is the minimum representable value in the depth buffer and maxDepthSlope is the maximum of the horizontal and vertical slopes of the depth value in the pixel.\n
                See <a href="https://msdn.microsoft.com/en-us/library/windows/desktop/cc308048%28v=vs.85%29.aspx">the DX documentation</a> for depth bias explanation.
            */
            Desc& setDepthBias(int32_t depthBias, float slopeScaledBias) { mSlopeScaledDepthBias = slopeScaledBias; mDepthBias = depthBias; return *this; }

            /** Determines weather to clip or cull the vertices based on depth
                \param clampDepth If true, clamp depth value to the viewport extent. If false, clip primitives to near/far Z-planes
            */
            Desc& setDepthClamp(bool clampDepth) { mClampDepth = clampDepth; return *this; }

            /** Enable/disable anti-aliased lines. Actual anti-aliasing algorithm is implementation dependent, but usually uses quadrilateral lines.
            */
            Desc& setLineAntiAliasing(bool enableLineAA) { mEnableLinesAA = enableLineAA; return *this; };

            /** Enable/disable scissor test
            */
            Desc& setScissorTest(bool enabled) { mScissorEnabled = enabled; return *this; }

            /** Enable/disable conservative rasterization
            */
            Desc& setConservativeRasterization(bool enabled) { mConservativeRaster = enabled; return *this; }

            /** Set the forced sample count. Useful when using UAV
            */
            Desc& setForcedSampleCount(uint32_t samples) { mForcedSampleCount = samples; return *this; }

        protected:
            CullMode mCullMode = CullMode::Back;
            FillMode mFillMode = FillMode::Solid;
            bool     mIsFrontCcw = true;
            float    mSlopeScaledDepthBias = 0;
            int32_t  mDepthBias = 0;
            bool     mClampDepth = false;
            bool     mScissorEnabled = false;
            bool     mEnableLinesAA = true;
            uint32_t mForcedSampleCount = 0;
            bool     mConservativeRaster = false;
        };

        ~RasterizerState() {}

        /** Create a new rasterizer state.
            \param[in] desc Rasterizer state descriptor.
            \return A new object, or throws an exception if creation failed.
        */
        static SharedPtr create(const Desc& desc);

        /** Get the cull mode
        */
        CullMode getCullMode() const { return mDesc.mCullMode; }
        /** Get the fill mode
        */
        FillMode getFillMode() const { return mDesc.mFillMode; }
        /** Check what is the winding order for triangles to be considered front-facing
        */
        bool isFrontCounterCW() const { return mDesc.mIsFrontCcw; }
        /** Get the slope-scaled depth bias
        */
        float getSlopeScaledDepthBias() const { return mDesc.mSlopeScaledDepthBias; }
        /** Get the depth bias
        */
        int32_t getDepthBias() const { return mDesc.mDepthBias; }
        /** Check if depth clamp is enabled
        */
        bool isDepthClampEnabled() const { return mDesc.mClampDepth; }
        /** Check if scissor test is enabled
        */
        bool isScissorTestEnabled() const { return mDesc.mScissorEnabled; }
        /** Check if anti-aliased lines are enabled
        */
        bool isLineAntiAliasingEnabled() const { return mDesc.mEnableLinesAA; }

        /** Check if conservative rasterization is enabled
        */
        bool isConservativeRasterizationEnabled() const { return mDesc.mConservativeRaster; }

        /** Get the forced sample count
        */
        uint32_t getForcedSampleCount() const { return mDesc.mForcedSampleCount; }

        /** Get the API handle
        */
        const RasterizerStateHandle& getApiHandle() const { return mApiHandle; };
    private:
        RasterizerStateHandle mApiHandle;
        RasterizerState(const Desc& Desc) : mDesc(Desc) {}
        Desc mDesc;
    };

    /** Depth-Stencil state
    */
    class DepthStencilState : public std::enable_shared_from_this<DepthStencilState>
    {
    public:
        using SharedPtr = std::shared_ptr<DepthStencilState>;
        using SharedConstPtr = std::shared_ptr<const DepthStencilState>;

        /** Used for stencil control.
        */
        enum class Face
        {
            Front,          ///< Front-facing primitives
            Back,           ///< Back-facing primitives
            FrontAndBack    ///< Front and back-facing primitives
        };

        /** Comparison function
        */
        using Func = ComparisonFunc;

        /** Stencil operation
        */
        enum class StencilOp
        {
            Keep,               ///< Keep the stencil value
            Zero,               ///< Set the stencil value to zero
            Replace,            ///< Replace the stencil value with the reference value
            Increase,           ///< Increase the stencil value by one, wrap if necessary
            IncreaseSaturate,   ///< Increase the stencil value by one, clamp if necessary
            Decrease,           ///< Decrease the stencil value by one, wrap if necessary
            DecreaseSaturate,   ///< Decrease the stencil value by one, clamp if necessary
            Invert              ///< Invert the stencil data (bitwise not)
        };

        /** Stencil descriptor
        */
        struct StencilDesc
        {
            Func func = Func::Disabled;                     ///< Stencil comparison function
            StencilOp stencilFailOp = StencilOp::Keep;      ///< Stencil operation in case stencil test fails
            StencilOp depthFailOp = StencilOp::Keep;        ///< Stencil operation in case stencil test passes but depth test fails
            StencilOp depthStencilPassOp = StencilOp::Keep; ///< Stencil operation in case stencil and depth tests pass
        };

        /** Depth-stencil descriptor
        */
        class Desc
        {
        public:
            friend class DepthStencilState;

            /** Enable/disable depth-test
            */
            Desc& setDepthEnabled(bool enabled) { mDepthEnabled = enabled; return *this; }

            /** Set the depth-function
            */
            Desc& setDepthFunc(Func depthFunc) { mDepthFunc = depthFunc; return *this; }

            /** Enable or disable depth writes into the depth buffer
            */
            Desc& setDepthWriteMask(bool writeDepth) { mWriteDepth = writeDepth; return *this; }

            /** Enable/disable stencil-test
            */
            Desc& setStencilEnabled(bool enabled) { mStencilEnabled = enabled; return *this; }

            /** Set the stencil write-mask
            */
            Desc& setStencilWriteMask(uint8_t mask);

            /** Set the stencil read-mask
            */
            Desc& setStencilReadMask(uint8_t mask);

            /** Set the stencil comparison function
                \param face Chooses the face to apply the function to
                \param func Comparison function
            */
            Desc& setStencilFunc(Face face, Func func);

            /** Set the stencil operation
                \param face Chooses the face to apply the operation to
                \param stencilFail Stencil operation in case stencil test fails
                \param depthFail Stencil operation in case stencil test passes but depth test fails
                \param depthStencilPass Stencil operation in case stencil and depth tests pass
            */
            Desc& setStencilOp(Face face, StencilOp stencilFail, StencilOp depthFail, StencilOp depthStencilPass);

            /** Set the stencil ref value
            */
            Desc& setStencilRef(uint8_t value) { mStencilRef = value; return *this; };

        protected:
            bool mDepthEnabled = true;
            bool mStencilEnabled = false;
            bool mWriteDepth = true;
            Func mDepthFunc = Func::Less;
            StencilDesc mStencilFront;
            StencilDesc mStencilBack;
            uint8_t mStencilReadMask = (uint8_t)-1;
            uint8_t mStencilWriteMask = (uint8_t)-1;
            uint8_t mStencilRef = 0;
        };

        ~DepthStencilState();

        /** Create a new depth-stencil state object.
            \param desc Depth-stencil descriptor.
            \return New object, or throws an exception if an error occurred.
        */
        static SharedPtr create(const Desc& desc);

        /** Check if depth test is enabled or disabled
        */
        bool isDepthTestEnabled() const { return mDesc.mDepthEnabled; }

        /** Check if depth write is enabled or disabled
        */
        bool isDepthWriteEnabled() const { return mDesc.mWriteDepth; }

        /** Get the depth comparison function
        */
        Func getDepthFunc() const { return mDesc.mDepthFunc; }

        /** Check if stencil is enabled or disabled
        */
        bool isStencilTestEnabled() const { return mDesc.mStencilEnabled; }

        /** Get the stencil descriptor for the selected face
        */
        const StencilDesc& getStencilDesc(Face face) const;

        /** Get the stencil read mask
        */
        uint8_t getStencilReadMask() const { return mDesc.mStencilReadMask; }

        /** Get the stencil write mask
        */
        uint8_t getStencilWriteMask() const { return mDesc.mStencilWriteMask; }

        /** Get the stencil ref value
        */
        uint8_t getStencilRef() const { return mDesc.mStencilRef; }

        /** Get the API handle
        */
        const DepthStencilStateHandle& getApiHandle() const;

    private:
        DepthStencilStateHandle mApiHandle;
        DepthStencilState(const Desc& Desc) : mDesc(Desc) {}
        Desc mDesc;
    };


    /** Abstract the API sampler state object
    */
    class Sampler : public std::enable_shared_from_this<Sampler>
    {
    public:
        using SharedPtr = std::shared_ptr<Sampler>;
        using SharedConstPtr = std::shared_ptr<const Sampler>;
        using ApiHandle = SamplerHandle;

        /** Filter mode
        */
        enum class Filter
        {
            Point,
            Linear,
        };

        /** Addressing mode in case the texture coordinates are out of [0, 1] range
        */
        enum class AddressMode
        {
            Wrap,               ///< Wrap around
            Mirror,             ///< Wrap around and mirror on every integer junction
            Clamp,              ///< Clamp the normalized coordinates to [0, 1]
            Border,             ///< If out-of-bound, use the sampler's border color
            MirrorOnce          ///< Same as Mirror, but mirrors only once around 0
        };

        /** Reduction mode
        */
        enum class ReductionMode
        {
            Standard,
            Comparison,
            Min,
            Max,
        };

        /** Comparison mode for the sampler.
        */
        using ComparisonMode = ComparisonFunc;

        /** Descriptor used to create a new Sampler object
        */
        class Desc
        {
        public:
            friend class Sampler;

            /** Set the filter mode
                \param[in] minFilter Filter mode in case of minification.
                \param[in] magFilter Filter mode in case of magnification.
                \param[in] mipFilter Mip-level sampling mode
            */
            Desc& setFilterMode(Filter minFilter, Filter magFilter, Filter mipFilter);

            /** Set the maximum anisotropic filtering value. If MaxAnisotropy > 1, min/mag/mip filter modes are ignored
            */
            Desc& setMaxAnisotropy(uint32_t maxAnisotropy);

            /** Set the lod clamp parameters
                \param[in] minLod Minimum LOD that will be used when sampling
                \param[in] maxLod Maximum LOD that will be used when sampling
                \param[in] lodBias Bias to apply to the LOD
            */
            Desc& setLodParams(float minLod, float maxLod, float lodBias);

            /** Set the sampler comparison mode.
            */
            Desc& setComparisonMode(ComparisonMode mode);

            /** Set the sampler reduction mode.
            */
            Desc& setReductionMode(ReductionMode mode);

            /** Set the sampler addressing mode
                \param[in] modeU Addressing mode for U texcoord channel
                \param[in] modeV Addressing mode for V texcoord channel
                \param[in] modeW Addressing mode for W texcoord channel
            */
            Desc& setAddressingMode(AddressMode modeU, AddressMode modeV, AddressMode modeW);

            /** Set the border color. Only applies when the addressing mode is ClampToBorder
            */
            Desc& setBorderColor(const RBVector4& borderColor);

        protected:
            Filter mMagFilter = Filter::Linear;
            Filter mMinFilter = Filter::Linear;
            Filter mMipFilter = Filter::Linear;
            uint32_t mMaxAnisotropy = 1;
            float mMaxLod = 1000;
            float mMinLod = -1000;
            float mLodBias = 0;
            ComparisonMode mComparisonMode = ComparisonMode::Disabled;
            ReductionMode mReductionMode = ReductionMode::Standard;
            AddressMode mModeU = AddressMode::Wrap;
            AddressMode mModeV = AddressMode::Wrap;
            AddressMode mModeW = AddressMode::Wrap;
            RBVector4 mBorderColor = RBVector4(0, 0, 0, 0);
        };

        ~Sampler();

        /** Create a new sampler object.
            \param[in] desc Describes sampler settings.
            \return A new object, or throws an exception if creation failed.
        */
        static SharedPtr create(const Desc& desc);

        /** Get the API handle
        */
        const ApiHandle& getApiHandle() const { return mApiHandle; }

        /** Get the magnification filter
        */
        Filter getMagFilter() const { return mDesc.mMagFilter; }

        /** Get the minification filter
        */
        Filter getMinFilter() const { return mDesc.mMinFilter; }

        /** Get the mip-levels filter
        */
        Filter getMipFilter() const { return mDesc.mMipFilter; }

        /** Get the maximum anisotropy
        */
        uint32_t getMaxAnisotropy() const { return mDesc.mMaxAnisotropy; }

        /** Get the minimum LOD value
        */
        float getMinLod() const { return mDesc.mMinLod; }

        /** Get the maximum LOD value
        */
        float getMaxLod() const { return mDesc.mMaxLod; }

        /** Get the LOD bias
        */
        float getLodBias() const { return mDesc.mLodBias; }

        /** Get the comparison mode
        */
        ComparisonMode getComparisonMode() const { return mDesc.mComparisonMode; }

        /** Get the reduction mode
        */
        ReductionMode getReductionMode() const { return mDesc.mReductionMode; }

        /** Get the addressing mode for the U texcoord
        */
        AddressMode getAddressModeU() const { return mDesc.mModeU; }

        /** Get the addressing mode for the V texcoord
        */
        AddressMode getAddressModeV() const { return mDesc.mModeV; }

        /** Get the addressing mode for the W texcoord
        */
        AddressMode getAddressModeW() const { return mDesc.mModeW; }

        /** Get the border color
        */
        const RBVector4& getBorderColor() const { return mDesc.mBorderColor; }

        /** Get the descriptor that was used to create the sampler.
        */
        const Desc& getDesc() const { return mDesc; }

        /** Get an object that represents a default sampler
        */
        static Sampler::SharedPtr getDefault();

    private:
        Sampler(const Desc& desc);
        Desc mDesc;
        ApiHandle mApiHandle = {};
        static uint32_t getApiMaxAnisotropy();
    };

}