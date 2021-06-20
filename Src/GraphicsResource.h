#pragma once
#include <memory>
#include <unordered_map>
#include "./D3D12/WIPD3D12.h"
#include "Formats.h"
#include "GraphicsResView.h"
#include "Vector3.h"
#include "Common/Logger.h"
#include "Vector4.h"
#include "Vector2.h"
#include "GPUMemory.h"

namespace WIP3D
{
    class Texture;
    class Buffer;
    class ParameterBlock;

    class Resource : public std::enable_shared_from_this<Resource>
    {
    public:
        using ApiHandle = ResourceHandle;
        using BindFlags = ResourceBindFlags;

        /** Resource types. Notice there are no array types. Array are controlled using the array size parameter on texture creation.
        */
        enum class Type
        {
            Buffer,                 ///< Buffer. Can be bound to all shader-stages
            Texture1D,              ///< 1D texture. Can be bound as render-target, shader-resource and UAV
            Texture2D,              ///< 2D texture. Can be bound as render-target, shader-resource and UAV
            Texture3D,              ///< 3D texture. Can be bound as render-target, shader-resource and UAV
            TextureCube,            ///< Texture-cube. Can be bound as render-target, shader-resource and UAV
            Texture2DMultisample,   ///< 2D multi-sampled texture. Can be bound as render-target, shader-resource and UAV
        };

        /** Resource state. Keeps track of how the resource was last used
        */
        enum class State : uint32_t
        {
            Undefined,
            PreInitialized,
            Common,
            VertexBuffer,
            ConstantBuffer,
            IndexBuffer,
            RenderTarget,
            UnorderedAccess,
            DepthStencil,
            ShaderResource,
            StreamOut,
            IndirectArg,
            CopyDest,
            CopySource,
            ResolveDest,
            ResolveSource,
            Present,
            GenericRead,
            Predication,
            PixelShader,
            NonPixelShader,
#ifdef WIP_D3D12
            AccelerationStructure,
#endif
        };

        using SharedPtr = std::shared_ptr<Resource>;
        using SharedConstPtr = std::shared_ptr<const Resource>;

        /** Default value used in create*() methods
        */
        static const uint32_t kMaxPossible = RenderTargetView::kMaxPossible;

        virtual ~Resource() = 0;

        /** Get the bind flags
        */
        BindFlags getBindFlags() const { return mBindFlags; }

        bool isStateGlobal() const { return mState.isGlobal; }

        /** Get the current state. This is only valid if isStateGlobal() returns true
        */
        State getGlobalState() const;

        /** Get a subresource state
        */
        State getSubresourceState(uint32_t arraySlice, uint32_t mipLevel) const;

        /** Get the resource type
        */
        Type getType() const { return mType; }

        /** Get the API handle
        */
        const ApiHandle& getApiHandle() const { return mApiHandle; }

        /** Creates a shared resource API handle.
        */
        SharedResourceApiHandle createSharedApiHandle();

        struct ViewInfoHashFunc
        {
            std::size_t operator()(const ResourceViewInfo& v) const
            {
                return ((std::hash<uint32_t>()(v.firstArraySlice) ^ (std::hash<uint32_t>()(v.arraySize) << 1)) >> 1)
                    ^ (std::hash<uint32_t>()(v.mipCount) << 1)
                    ^ (std::hash<uint32_t>()(v.mostDetailedMip) << 3)
                    ^ (std::hash<uint32_t>()(v.firstElement) << 5)
                    ^ (std::hash<uint32_t>()(v.elementCount) << 7);
            }
        };

        /** Get the size of the resource
        */
        size_t getSize() const { return mSize; }

        /** Invalidate and release all of the resource views
        */
        void invalidateViews() const;

        /** Set the resource name
        */
        void setName(const std::string& name) { mName = name; apiSetName(); }

        /** Get the resource name
        */
        const std::string& getName() const { return mName; }

        /** Get a SRV/UAV for the entire resource.
            Buffer and Texture have overloads which allow you to create a view into part of the resource
        */
        virtual ShaderResourceView::SharedPtr getSRV() = 0;
        virtual UnorderedAccessView::SharedPtr getUAV() = 0;

        /** Conversions to derived classes
        */
        // dynamic用于向派生类转换
        std::shared_ptr<Texture> asTexture() { return this ? std::dynamic_pointer_cast<Texture>(shared_from_this()) : nullptr; }
        std::shared_ptr<Buffer> asBuffer() { return this ? std::dynamic_pointer_cast<Buffer>(shared_from_this()) : nullptr; }

    protected:
        friend class CopyContext;

        Resource(Type type, BindFlags bindFlags, uint64_t size) : mType(type), mBindFlags(bindFlags), mSize(size) {}

        Type mType;
        BindFlags mBindFlags;
        struct
        {
            bool isGlobal = true;
            State global = State::Undefined;
            std::vector<State> perSubresource;
        } mutable mState;

        void setSubresourceState(uint32_t arraySlice, uint32_t mipLevel, State newState) const;
        void setGlobalState(State newState) const;
        void apiSetName();

        ApiHandle mApiHandle;
        size_t mSize = 0;
        GpuAddress mGpuVaOffset = 0;
        std::string mName;

        mutable std::unordered_map<ResourceViewInfo, ShaderResourceView::SharedPtr, ViewInfoHashFunc> mSrvs;
        mutable std::unordered_map<ResourceViewInfo, RenderTargetView::SharedPtr, ViewInfoHashFunc> mRtvs;
        mutable std::unordered_map<ResourceViewInfo, DepthStencilView::SharedPtr, ViewInfoHashFunc> mDsvs;
        mutable std::unordered_map<ResourceViewInfo, UnorderedAccessView::SharedPtr, ViewInfoHashFunc> mUavs;
    };

    const std::string to_string(Resource::Type);
    const std::string to_string(Resource::State);

    class Sampler;
    class Device;
    class RenderContext;

    /** Abstracts the API texture objects
    */
    class Texture : public Resource
    {
    public:
        using SharedPtr = std::shared_ptr<Texture>;
        using SharedConstPtr = std::shared_ptr<const Texture>;
        using WeakPtr = std::weak_ptr<Texture>;
        using WeakConstPtr = std::weak_ptr<const Texture>;

        ~Texture();

        /** Get a mip-level width
        */
        uint32_t getWidth(uint32_t mipLevel = 0) const 
        { 
            return (mipLevel == 0) || (mipLevel < mMipLevels) ? RBMath::get_max(1U, mWidth >> mipLevel) : 0; 
        }

        /** Get a mip-level height
        */
        uint32_t getHeight(uint32_t mipLevel = 0) const 
        { 
            return (mipLevel == 0) || (mipLevel < mMipLevels) ? RBMath::get_max(1U, mHeight >> mipLevel) : 0;
        }

        /** Get a mip-level depth
        */
        uint32_t getDepth(uint32_t mipLevel = 0) const 
        { 
            return (mipLevel == 0) || (mipLevel < mMipLevels) ? RBMath::get_max(1U, mDepth >> mipLevel) : 0;
        }

        /** Get the number of mip-levels
        */
        uint32_t getMipCount() const { return mMipLevels; }

        /** Get the sample count
        */
        uint32_t getSampleCount() const { return mSampleCount; }

        /** Get the array size
        */
        uint32_t getArraySize() const { return mArraySize; }

        /** Get the array index of a subresource
        */
        uint32_t getSubresourceArraySlice(uint32_t subresource) const { return subresource / mMipLevels; }

        /** Get the mip-level of a subresource
        */
        uint32_t getSubresourceMipLevel(uint32_t subresource) const { return subresource % mMipLevels; }

        /** Get the subresource index
        */
        uint32_t getSubresourceIndex(uint32_t arraySlice, uint32_t mipLevel) const { return mipLevel + arraySlice * mMipLevels; }

        /** Get the resource format
        */
        ResourceFormat getFormat() const { return mFormat; }

        /** Create a new texture from an existing API handle.
            \param[in] handle Handle of already allocated resource.
            \param[in] type The type of texture.
            \param[in] width The width of the texture.
            \param[in] height The height of the texture.
            \param[in] depth The depth of the texture.
            \param[in] format The format of the texture.
            \param[in] sampleCount The sample count of the texture.
            \param[in] arraySize The array size of the texture.
            \param[in] mipLevels The number of mip levels.
            \param[in] initState The initial resource state.
            \param[in] bindFlags Texture bind flags. Flags must match the bind flags of the original resource.
            \return A pointer to a new texture, or throws an exception if creation failed.
        */
        static SharedPtr createFromApiHandle(ApiHandle handle, Type type, uint32_t width, uint32_t height, uint32_t depth, ResourceFormat format, uint32_t sampleCount, uint32_t arraySize, uint32_t mipLevels, State initState, BindFlags bindFlags);

        /** Create a 1D texture.
            \param[in] width The width of the texture.
            \param[in] format The format of the texture.
            \param[in] arraySize The array size of the texture.
            \param[in] mipLevels If equal to kMaxPossible then an entire mip chain will be generated from mip level 0. If any other value is given then the data for at least that number of miplevels must be provided.
            \param[in] pInitData If different than nullptr, pointer to a buffer containing data to initialize the texture with.
            \param[in] bindFlags The requested bind flags for the resource.
            \return A pointer to a new texture, or throws an exception if creation failed.
        */
        static SharedPtr create1D(uint32_t width, ResourceFormat format, uint32_t arraySize = 1, uint32_t mipLevels = kMaxPossible, const void* pInitData = nullptr, BindFlags bindFlags = BindFlags::ShaderResource);

        /** Create a 2D texture.
            \param[in] width The width of the texture.
            \param[in] height The height of the texture.
            \param[in] format The format of the texture.
            \param[in] arraySize The array size of the texture.
            \param[in] mipLevels If equal to kMaxPossible then an entire mip chain will be generated from mip level 0. If any other value is given then the data for at least that number of miplevels must be provided.
            \param[in] pInitData If different than nullptr, pointer to a buffer containing data to initialize the texture with.
            \param[in] bindFlags The requested bind flags for the resource.
            \return A pointer to a new texture, or throws an exception if creation failed.
        */
        static SharedPtr create2D(uint32_t width, uint32_t height, ResourceFormat format, uint32_t arraySize = 1, uint32_t mipLevels = kMaxPossible, const void* pInitData = nullptr, BindFlags bindFlags = BindFlags::ShaderResource);

        /** Create a 3D texture.
            \param[in] width The width of the texture.
            \param[in] height The height of the texture.
            \param[in] depth The depth of the texture.
            \param[in] format The format of the texture.
            \param[in] mipLevels If equal to kMaxPossible then an entire mip chain will be generated from mip level 0. If any other value is given then the data for at least that number of miplevels must be provided.
            \param[in] pInitData If different than nullptr, pointer to a buffer containing data to initialize the texture with.
            \param[in] bindFlags The requested bind flags for the resource.
            \param[in] isSparse If true, the texture is created using sparse texture options supported by the API.
            \return A pointer to a new texture, or throws an exception if creation failed.
        */
        static SharedPtr create3D(uint32_t width, uint32_t height, uint32_t depth, ResourceFormat format, uint32_t mipLevels = kMaxPossible, const void* pInitData = nullptr, BindFlags bindFlags = BindFlags::ShaderResource, bool isSparse = false);

        /** Create a cube texture.
            \param[in] width The width of the texture.
            \param[in] height The height of the texture.
            \param[in] format The format of the texture.
            \param[in] arraySize The array size of the texture.
            \param[in] mipLevels If equal to kMaxPossible then an entire mip chain will be generated from mip level 0. If any other value is given then the data for at least that number of miplevels must be provided.
            \param[in] pInitData If different than nullptr, pointer to a buffer containing data to initialize the texture with.
            \param[in] bindFlags The requested bind flags for the resource.
            \return A pointer to a new texture, or throws an exception if creation failed.
        */
        static SharedPtr createCube(uint32_t width, uint32_t height, ResourceFormat format, uint32_t arraySize = 1, uint32_t mipLevels = kMaxPossible, const void* pInitData = nullptr, BindFlags bindFlags = BindFlags::ShaderResource);

        /** Create a multi-sampled 2D texture.
            \param[in] width The width of the texture.
            \param[in] height The height of the texture.
            \param[in] format The format of the texture.
            \param[in] sampleCount The sample count of the texture.
            \param[in] arraySize The array size of the texture.
            \param[in] bindFlags The requested bind flags for the resource.
            \return A pointer to a new texture, or throws an exception if creation failed.
        */
        static SharedPtr create2DMS(uint32_t width, uint32_t height, ResourceFormat format, uint32_t sampleCount, uint32_t arraySize = 1, BindFlags bindFlags = BindFlags::ShaderResource);

        /** Create a new texture object from a file.
            \param[in] filename Filename of the image. Can also include a full path or relative path from a data directory.
            \param[in] generateMipLevels Whether the mip-chain should be generated.
            \param[in] loadAsSrgb Load the texture using sRGB format. Only valid for 3 or 4 component textures.
            \param[in] bindFlags The bind flags to create the texture with.
            \return A new texture, or nullptr if the texture failed to load.
        */
        static SharedPtr createFromFile(const std::string& filename, bool generateMipLevels, bool loadAsSrgb, BindFlags bindFlags = BindFlags::ShaderResource);

        /** Get a shader-resource view for the entire resource
        */
        virtual ShaderResourceView::SharedPtr getSRV() override;

        /** Get an unordered access view for the entire resource
        */
        virtual UnorderedAccessView::SharedPtr getUAV() override;

        /** Get a shader-resource view.
            \param[in] mostDetailedMip The most detailed mip level of the view
            \param[in] mipCount The number of mip-levels to bind. If this is equal to Texture#kMaxPossible, will create a view ranging from mostDetailedMip to the texture's mip levels count
            \param[in] firstArraySlice The first array slice of the view
            \param[in] arraySize The array size. If this is equal to Texture#kMaxPossible, will create a view ranging from firstArraySlice to the texture's array size
        */
        ShaderResourceView::SharedPtr getSRV(uint32_t mostDetailedMip, uint32_t mipCount = kMaxPossible, uint32_t firstArraySlice = 0, uint32_t arraySize = kMaxPossible);

        /** Get a render-target view.
            \param[in] mipLevel The requested mip-level
            \param[in] firstArraySlice The first array slice of the view
            \param[in] arraySize The array size. If this is equal to Texture#kMaxPossible, will create a view ranging from firstArraySlice to the texture's array size
        */
        RenderTargetView::SharedPtr getRTV(uint32_t mipLevel = 0, uint32_t firstArraySlice = 0, uint32_t arraySize = kMaxPossible);

        /** Get a depth stencil view.
            \param[in] mipLevel The requested mip-level
            \param[in] firstArraySlice The first array slice of the view
            \param[in] arraySize The array size. If this is equal to Texture#kMaxPossible, will create a view ranging from firstArraySlice to the texture's array size
        */
        DepthStencilView::SharedPtr getDSV(uint32_t mipLevel = 0, uint32_t firstArraySlice = 0, uint32_t arraySize = kMaxPossible);

        /** Get an unordered access view.
            \param[in] mipLevel The requested mip-level
            \param[in] firstArraySlice The first array slice of the view
            \param[in] arraySize The array size. If this is equal to Texture#kMaxPossible, will create a view ranging from firstArraySlice to the texture's array size
        */
        UnorderedAccessView::SharedPtr getUAV(uint32_t mipLevel, uint32_t firstArraySlice = 0, uint32_t arraySize = kMaxPossible);

        /** Capture the texture to an image file.
            \param[in] mipLevel Requested mip-level
            \param[in] arraySlice Requested array-slice
            \param[in] filename Name of the file to save.
            \param[in] fileFormat Destination image file format (e.g., PNG, PFM, etc.)
            \param[in] exportFlags Save flags, see Bitmap::ExportFlags
        */
       // void captureToFile(uint32_t mipLevel, uint32_t arraySlice, const std::string& filename, Bitmap::FileFormat format = Bitmap::FileFormat::PngFile, Bitmap::ExportFlags exportFlags = Bitmap::ExportFlags::None);

        /** Generates mipmaps for a specified texture object.
        */
        void generateMips(RenderContext* pContext);

        /** In case the texture was loaded from a file, use this to set the file path
        */
        void setSourceFilename(const std::string& filename) { mSourceFilename = filename; }

        /** In case the texture was loaded from a file, get the source file path
        */
        const std::string& getSourceFilename() const { return mSourceFilename; }

        /** Returns the size of the texture in bytes as allocated in GPU memory.
        */
        uint32_t getTextureSizeInBytes();

    protected:
        Texture(uint32_t width, uint32_t height, uint32_t depth, uint32_t arraySize, uint32_t mipLevels, uint32_t sampleCount, ResourceFormat format, Type Type, BindFlags bindFlags);
        void apiInit(const void* pData, bool autoGenMips);
        void uploadInitData(const void* pData, bool autoGenMips);

        bool mReleaseRtvsAfterGenMips = true;
        std::string mSourceFilename;

        uint32_t mWidth = 0;
        uint32_t mHeight = 0;
        uint32_t mDepth = 0;
        uint32_t mMipLevels = 0;
        uint32_t mSampleCount = 0;
        uint32_t mArraySize = 0;
        ResourceFormat mFormat = ResourceFormat::Unknown;
        bool mIsSparse = false;

        RBVector3I mSparsePageRes = RBVector3I(0);

        friend class Device;
    };


    class Program;
    struct ShaderVar;

    /** Low-level buffer object
        This class abstracts the API's buffer creation and management
    */
    class Buffer : public Resource
    {
    public:
        using SharedPtr = std::shared_ptr<Buffer>;
        using WeakPtr = std::weak_ptr<Buffer>;
        using SharedConstPtr = std::shared_ptr<const Buffer>;

        /** Buffer access flags.
            These flags are hints the driver how the buffer will be used.
        */
        enum class CpuAccess
        {
            None,    ///< The CPU can't access the buffer's content. The buffer can be updated using Buffer#updateData()
            Write,   ///< The buffer can be mapped for CPU writes
            Read,    ///< The buffer can be mapped for CPU reads
        };

        enum class MapType
        {
            Read,           ///< Map the buffer for read access.
            Write,          ///< Map the buffer for write access. Buffer had to be created with CpuAccess::Write flag.
            WriteDiscard,   ///< Map the buffer for write access, discarding the previous content of the entire buffer. Buffer had to be created with CpuAccess::Write flag.
        };

        ~Buffer();

        /** Create a new buffer.
            \param[in] size Size of the buffer in bytes.
            \param[in] bindFlags Buffer bind flags.
            \param[in] cpuAccess Flags indicating how the buffer can be updated.
            \param[in] pInitData Optional parameter. Initial buffer data. Pointed buffer size should be at least 'size' bytes.
            \return A pointer to a new buffer object, or throws an exception if creation failed.
        */
        static SharedPtr create(
            size_t size,
            Resource::BindFlags bindFlags = Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess,
            CpuAccess cpuAccess = Buffer::CpuAccess::None,
            const void* pInitData = nullptr);

        /** Create a new typed buffer.
            \param[in] format Typed buffer format.
            \param[in] elementCount Number of elements.
            \param[in] bindFlags Buffer bind flags.
            \param[in] cpuAccess Flags indicating how the buffer can be updated.
            \param[in] pInitData Optional parameter. Initial buffer data. Pointed buffer should hold at least 'elementCount' elements.
            \return A pointer to a new buffer object, or throws an exception if creation failed.
        */
        static SharedPtr createTyped(
            ResourceFormat format,
            uint32_t elementCount,
            Resource::BindFlags bindFlags = Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess,
            CpuAccess cpuAccess = Buffer::CpuAccess::None,
            const void* pInitData = nullptr);

        /** Create a new typed buffer. The format is deduced from the template parameter.
            \param[in] elementCount Number of elements.
            \param[in] bindFlags Buffer bind flags.
            \param[in] cpuAccess Flags indicating how the buffer can be updated.
            \param[in] pInitData Optional parameter. Initial buffer data. Pointed buffer should hold at least 'elementCount' elements.
            \return A pointer to a new buffer object, or throws an exception if creation failed.
        */
        template<typename T>
        static SharedPtr createTyped(
            uint32_t elementCount,
            Resource::BindFlags bindFlags = Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess,
            CpuAccess cpuAccess = Buffer::CpuAccess::None,
            const T* pInitData = nullptr)
        {
            return createTyped(FormatForElementType<T>::kFormat, elementCount, bindFlags, cpuAccess, pInitData);
        }

        /** Create a new structured buffer.
            \param[in] structSize Size of the struct in bytes.
            \param[in] elementCount Number of elements.
            \param[in] bindFlags Buffer bind flags.
            \param[in] cpuAccess Flags indicating how the buffer can be updated.
            \param[in] pInitData Optional parameter. Initial buffer data. Pointed buffer should hold at least 'elementCount' elements.
            \param[in] createCounter True if the associated UAV counter should be created.
            \return A pointer to a new buffer object, or throws an exception if creation failed.
        */
        static SharedPtr createStructured(
            uint32_t structSize,
            uint32_t elementCount,
            ResourceBindFlags bindFlags = Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess,
            CpuAccess cpuAccess = Buffer::CpuAccess::None,
            const void* pInitData = nullptr,
            bool createCounter = true);

        /** Create a new structured buffer.
            \param[in] pProgram Program declaring the buffer.
            \param[in] name Variable name in the program.
            \param[in] elementCount Number of elements.
            \param[in] bindFlags Buffer bind flags.
            \param[in] cpuAccess Flags indicating how the buffer can be updated.
            \param[in] pInitData Optional parameter. Initial buffer data. Pointed buffer should hold at least 'elementCount' elements.
            \param[in] createCounter True if the associated UAV counter should be created.
            \return A pointer to a new buffer object, or throws an exception if creation failed.
        */
        static SharedPtr createStructured(
            const Program* pProgram,
            const std::string& name,
            uint32_t elementCount,
            ResourceBindFlags bindFlags = Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess,
            CpuAccess cpuAccess = Buffer::CpuAccess::None,
            const void* pInitData = nullptr,
            bool createCounter = true);

        /** Create a new structured buffer.
            \param[in] shaderVar ShaderVar pointing to the buffer variable.
            \param[in] elementCount Number of elements.
            \param[in] bindFlags Buffer bind flags.
            \param[in] cpuAccess Flags indicating how the buffer can be updated.
            \param[in] pInitData Optional parameter. Initial buffer data. Pointed buffer should hold at least 'elementCount' elements.
            \param[in] createCounter True if the associated UAV counter should be created.
            \return A pointer to a new buffer object, or throws an exception if creation failed.
        */
        static SharedPtr createStructured(
            const ShaderVar& shaderVar,
            uint32_t elementCount,
            ResourceBindFlags bindFlags = Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess,
            CpuAccess cpuAccess = Buffer::CpuAccess::None,
            const void* pInitData = nullptr,
            bool createCounter = true);

        static SharedPtr aliasResource(Resource::SharedPtr pBaseResource, GpuAddress offset, size_t size, Resource::BindFlags bindFlags);

        /** Create a new buffer from an existing API handle.
            \param[in] handle Handle of already allocated resource.
            \param[in] size The size of the buffer in bytes.
            \param[in] bindFlags Buffer bind flags. Flags must match the bind flags of the original resource.
            \param[in] cpuAccess Flags indicating how the buffer can be updated. Flags must match those of the heap the original resource is allocated on.
            \return A pointer to a new buffer object, or throws an exception if creation failed.
        */
        static SharedPtr createFromApiHandle(ApiHandle handle, size_t size, Resource::BindFlags bindFlags, CpuAccess cpuAccess);

        /** Get a shader-resource view.
            \param[in] firstElement The first element of the view. For raw buffers, an element is a single float
            \param[in] elementCount The number of elements to bind
        */
        ShaderResourceView::SharedPtr getSRV(uint32_t firstElement, uint32_t elementCount = kMaxPossible);

        /** Get an unordered access view.
            \param[in] firstElement The first element of the view. For raw buffers, an element is a single float
            \param[in] elementCount The number of elements to bind
        */
        UnorderedAccessView::SharedPtr getUAV(uint32_t firstElement, uint32_t elementCount = kMaxPossible);

        /** Get a shader-resource view for the entire resource
        */
        virtual ShaderResourceView::SharedPtr getSRV() override;

        /** Get an unordered access view for the entire resource
        */
        virtual UnorderedAccessView::SharedPtr getUAV() override;

        /** Get a constant buffer view
        */
        ConstantBufferView::SharedPtr getCBV();

        /** Update the buffer's data
            \param[in] pData Pointer to the source data.
            \param[in] offset Byte offset into the destination buffer, indicating where to start copy into.
            \param[in] size Number of bytes to copy.
            If offset and size will cause an out-of-bound access to the buffer, an error will be logged and the update will fail.
        */
        virtual bool setBlob(const void* pData, size_t offset, size_t size);

        /** Get the offset from the beginning of the GPU resource
        */
        uint64_t getGpuAddressOffset() const { return mGpuVaOffset; };

        /** Get the GPU address (this includes the offset)
        */
        uint64_t getGpuAddress() const;

        /** Get the size of the buffer
        */
        size_t getSize() const { return mSize; }

        /** Get the element count. For structured-buffers, this is the number of structs. For typed-buffers, this is the number of elements. For other buffer, will return 0
        */
        uint32_t getElementCount() const { return mElementCount; }

        /** Get the size of a single struct. This call is only valid for structured-buffer. For other buffer types, it will return 0
        */
        uint32_t getStructSize() const { return mStructSize; }

        /** Get the buffer format. This call is only valid for typed-buffers, for other buffer types it will return ResourceFormat::Unknown
        */
        ResourceFormat getFormat() const { return mFormat; }

        /** Get the UAV counter buffer
        */
        const Buffer::SharedPtr& getUAVCounter() const { return mpUAVCounter; }

        /** Map the buffer.

            The low-level behavior depends on MapType and the CpuAccess flags of the buffer:
            - For CPU accessible buffers, the caller should ensure CPU/GPU memory accesses do not conflict.
            - For GPU-only buffers, map for read will create an internal staging buffer that is safe to read.
            - Mapping a CPU write buffer for WriteDiscard will cause the buffer to be internally re-allocated,
              causing its address range to change and invalidating all previous views to the buffer.
        */
        void* map(MapType Type);

        /** Unmap the buffer
        */
        void unmap();

        /** Get safe offset and size values
        */
        bool adjustSizeOffsetParams(size_t& size, size_t& offset) const
        {
            if (offset >= mSize)
            {
                LOG_WARN("Buffer::adjustSizeOffsetParams() - offset is larger than the buffer size.");
                return false;
            }

            if (offset + size > mSize)
            {
                LOG_WARN("Buffer::adjustSizeOffsetParams() - offset + size will cause an OOB access. Clamping size");
                size = mSize - offset;
            }
            return true;
        }

        /** Get the CPU access flags
        */
        CpuAccess getCpuAccess() const { return mCpuAccess; }

        /** Check if this is a typed buffer
        */
        bool isTyped() const { return mFormat != ResourceFormat::Unknown; }

        /** Check if this is a structured-buffer
        */
        bool isStructured() const { return mStructSize != 0; }

        template<typename T>
        void setElement(uint32_t index, T const& value)
        {
            setBlob(&value, sizeof(T) * index, sizeof(T));
        }

    protected:
        Buffer(size_t size, BindFlags bindFlags, CpuAccess cpuAccess);
        void apiInit(bool hasInitData);

        CpuAccess mCpuAccess;
        GpuMemoryHeap::Allocation mDynamicData;
        Buffer::SharedPtr mpStagingResource; // For buffers that have both CPU read flag and can be used by the GPU
        Resource::SharedPtr mpAliasedResource;
        uint32_t mElementCount = 0;
        ResourceFormat mFormat = ResourceFormat::Unknown;
        uint32_t mStructSize = 0;
        ConstantBufferView::SharedPtr mpCBV; // For constant-buffers
        Buffer::SharedPtr mpUAVCounter; // For structured-buffers

        /** Helper for converting host type to resource format for typed buffers.
            See list of supported formats for typed UAV loads:
            https://docs.microsoft.com/en-us/windows/win32/direct3d12/typed-unordered-access-view-loads
        */
        template<typename T>
        struct FormatForElementType {};

#define CASE(TYPE, FORMAT) \
        template<> struct FormatForElementType<TYPE> { static const ResourceFormat kFormat = FORMAT; }

        // Guaranteed supported formats on D3D12.
        CASE(float, ResourceFormat::R32Float);
        CASE(uint32_t, ResourceFormat::R32Uint);
        CASE(int32_t, ResourceFormat::R32Int);

        // Optionally supported formats as a set on D3D12. If one is supported all are supported.
        CASE(RBVector4, ResourceFormat::RGBA32Float);
        CASE(RBVector4IU, ResourceFormat::RGBA32Uint);
        CASE(RBVector4I, ResourceFormat::RGBA32Int);
        //R16G16B16A16_FLOAT
        //R16G16B16A16_UINT
        //R16G16B16A16_SINT
        //R8G8B8A8_UNORM
        //R8G8B8A8_UINT
        //R8G8B8A8_SINT
        //R16_FLOAT
        CASE(uint16_t, ResourceFormat::R16Uint);
        CASE(int16_t, ResourceFormat::R16Int);
        //R8_UNORM
        CASE(uint8_t, ResourceFormat::R8Uint);
        CASE(int8_t, ResourceFormat::R8Int);

        // Optionally and individually supported formats on D3D12. Query for support individually.
        //R16G16B16A16_UNORM
        //R16G16B16A16_SNORM
        CASE(RBVector2, ResourceFormat::RG32Float);
        CASE(RBVector2IU, ResourceFormat::RG32Uint);
        CASE(RBVector2I, ResourceFormat::RG32Int);
        //R10G10B10A2_UNORM
        //R10G10B10A2_UINT
        //R11G11B10_FLOAT
        //R8G8B8A8_SNORM
        //R16G16_FLOAT
        //R16G16_UNORM
        //R16G16_UINT
        //R16G16_SNORM
        //R16G16_SINT
        //R8G8_UNORM
        //R8G8_UINT
        //R8G8_SNORM
        //8G8_SINT
        //R16_UNORM
        //R16_SNORM
        //R8_SNORM
        //A8_UNORM
        //B5G6R5_UNORM
        //B5G5R5A1_UNORM
        //B4G4R4A4_UNORM

        // Additional formats that may be supported on some hardware.
        CASE(RBVector3, ResourceFormat::RGB32Float);

#undef CASE
    };

    inline std::string to_string(Buffer::CpuAccess c)
    {
#define a2s(ca_) case Buffer::CpuAccess::ca_: return #ca_;
        switch (c)
        {
            a2s(None);
            a2s(Write);
            a2s(Read);
        default:
            should_not_get_here();
            return "";
        }
#undef a2s
    }

    inline std::string to_string(Buffer::MapType mt)
    {
#define t2s(t_) case Buffer::MapType::t_: return #t_;
        switch (mt)
        {
            t2s(Read);
            t2s(Write);
            t2s(WriteDiscard);
        default:
            should_not_get_here();
            return "";
        }
#undef t2s
    }



}
