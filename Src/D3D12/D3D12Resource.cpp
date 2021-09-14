#include "../D3D12/WIPD3D12.h"
#include "../Device.h"
#include "../Common/FileSystem.h"
#include "D3D12Resource.h"

namespace WIP3D
{
    const D3D12_HEAP_PROPERTIES kDefaultHeapProps =
    {
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        0,
        0
    };

    const D3D12_HEAP_PROPERTIES kUploadHeapProps =
    {
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        0,
        0,
    };

    const D3D12_HEAP_PROPERTIES kReadbackHeapProps =
    {
        D3D12_HEAP_TYPE_READBACK,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        0,
        0
    };

    D3D12_RESOURCE_FLAGS getD3D12ResourceFlags(Resource::BindFlags flags)
    {
        D3D12_RESOURCE_FLAGS d3d = D3D12_RESOURCE_FLAG_NONE;

        bool uavRequired = is_set(flags, Resource::BindFlags::UnorderedAccess) || is_set(flags, Resource::BindFlags::AccelerationStructure);

        if (uavRequired)
        {
            d3d |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }

        if (is_set(flags, Resource::BindFlags::DepthStencil))
        {
            if (is_set(flags, Resource::BindFlags::ShaderResource) == false)
            {
                d3d |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
            }
            d3d |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        }

        if (is_set(flags, Resource::BindFlags::RenderTarget))
        {
            d3d |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }
        
        return d3d;
    }

    D3D12_RESOURCE_STATES getD3D12ResourceState(Resource::State s)
    {
        switch (s)
        {
        case Resource::State::Undefined:
        case Resource::State::Common:
            return D3D12_RESOURCE_STATE_COMMON;
        case Resource::State::ConstantBuffer:
        case Resource::State::VertexBuffer:
            return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        case Resource::State::CopyDest:
            return D3D12_RESOURCE_STATE_COPY_DEST;
        case Resource::State::CopySource:
            return D3D12_RESOURCE_STATE_COPY_SOURCE;
        case Resource::State::DepthStencil:
            return D3D12_RESOURCE_STATE_DEPTH_WRITE; // If depth-writes are disabled, return D3D12_RESOURCE_STATE_DEPTH_WRITE
        case Resource::State::IndexBuffer:
            return D3D12_RESOURCE_STATE_INDEX_BUFFER;
        case Resource::State::IndirectArg:
            return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
        case Resource::State::Predication:
            return D3D12_RESOURCE_STATE_PREDICATION;
        case Resource::State::Present:
            return D3D12_RESOURCE_STATE_PRESENT;
        case Resource::State::RenderTarget:
            return D3D12_RESOURCE_STATE_RENDER_TARGET;
        case Resource::State::ResolveDest:
            return D3D12_RESOURCE_STATE_RESOLVE_DEST;
        case Resource::State::ResolveSource:
            return D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
        case Resource::State::ShaderResource:
            return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE; // TODO: Need the shader usage mask to set state more optimally
        case Resource::State::StreamOut:
            return D3D12_RESOURCE_STATE_STREAM_OUT;
        case Resource::State::UnorderedAccess:
            return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        case Resource::State::GenericRead:
            return D3D12_RESOURCE_STATE_GENERIC_READ;
        case Resource::State::PixelShader:
            return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        case Resource::State::NonPixelShader:
            return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        case Resource::State::AccelerationStructure:
            return D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        default:
            should_not_get_here();
            return D3D12_RESOURCE_STATE_GENERIC_READ;
        }
    }

    void Resource::apiSetName()
    {
        std::wstring ws = string_to_wstring(mName);
        mApiHandle->SetName(ws.c_str());
    }


    

    Texture::~Texture()
    {
      gpDevice->releaseResource(mApiHandle);
    }

   // ID3D12ResourcePtr createBuffer(Buffer::State initState, size_t size, const D3D12_HEAP_PROPERTIES& heapProps, Buffer::BindFlags bindFlags);

    ID3D12ResourcePtr createBuffer(Buffer::State initState, size_t size, const D3D12_HEAP_PROPERTIES& heapProps, Buffer::BindFlags bindFlags)
    {
        assert(gpDevice);
        ID3D12Device* pDevice = gpDevice->getApiHandle();

        // Create the buffer
        D3D12_RESOURCE_DESC bufDesc = {};
        bufDesc.Alignment = 0;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufDesc.Flags = getD3D12ResourceFlags(bindFlags);
        bufDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufDesc.Height = 1;
        bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufDesc.MipLevels = 1;
        bufDesc.SampleDesc.Count = 1;
        bufDesc.SampleDesc.Quality = 0;
        bufDesc.Width = size;
        assert(bufDesc.Width > 0);

        D3D12_RESOURCE_STATES d3dState = getD3D12ResourceState(initState);
        ID3D12ResourcePtr pApiHandle;
        D3D12_HEAP_FLAGS heapFlags = is_set(bindFlags, ResourceBindFlags::Shared) ? D3D12_HEAP_FLAG_SHARED : D3D12_HEAP_FLAG_NONE;
        d3d_call(pDevice->CreateCommittedResource(&heapProps, heapFlags, &bufDesc, d3dState, nullptr, IID_PPV_ARGS(&pApiHandle)));
        assert(pApiHandle);

        return pApiHandle;
    }

    size_t getBufferDataAlignment(const Buffer* pBuffer)
    {
        // This in order of the alignment size
        const auto& bindFlags = pBuffer->getBindFlags();
        if (is_set(bindFlags, Buffer::BindFlags::Constant)) return D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
        if (is_set(bindFlags, Buffer::BindFlags::Index)) return sizeof(uint32_t); // This actually depends on the size of the index, but we can handle losing 2 bytes

        return D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
    }

    void* mapBufferApi(const Buffer::ApiHandle& apiHandle, size_t size)
    {
        D3D12_RANGE r{ 0, size };
        void* pData;
        d3d_call(apiHandle->Map(0, &r, &pData));
        return pData;
    }

    void Buffer::apiInit(bool hasInitData)
    {
        if (mCpuAccess != CpuAccess::None && is_set(mBindFlags, BindFlags::Shared))
        {
            throw std::exception("Can't create shared resource with CPU access other than 'None'.");
        }

        if (mBindFlags == BindFlags::Constant)
        {
            mSize = align_to(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, mSize);
        }

        if (mCpuAccess == CpuAccess::Write)
        {
            mState.global = Resource::State::GenericRead;
            if (hasInitData == false) // Else the allocation will happen when updating the data
            {
                assert(gpDevice);
                mDynamicData = gpDevice->getUploadHeap()->allocate(mSize, getBufferDataAlignment(this));
                mApiHandle = mDynamicData.pResourceHandle;
                mGpuVaOffset = mDynamicData.offset;
            }
        }
        else if (mCpuAccess == CpuAccess::Read && mBindFlags == BindFlags::None)
        {
            mState.global = Resource::State::CopyDest;
            mApiHandle = createBuffer(mState.global, mSize, kReadbackHeapProps, mBindFlags);
        }
        else
        {
            mState.global = Resource::State::Common;
            if (is_set(mBindFlags, BindFlags::AccelerationStructure)) mState.global = Resource::State::AccelerationStructure;
            mApiHandle = createBuffer(mState.global, mSize, kDefaultHeapProps, mBindFlags);
        }
    }

    uint64_t Buffer::getGpuAddress() const
    {
        return mGpuVaOffset + mApiHandle->GetGPUVirtualAddress();
    }

    void Buffer::unmap()
    {
        // Only unmap read buffers, write buffers are persistently mapped
        D3D12_RANGE r{};
        if (mpStagingResource)
        {
            mpStagingResource->mApiHandle->Unmap(0, &r);
        }
        else if (mCpuAccess == CpuAccess::Read)
        {
            mApiHandle->Unmap(0, &r);
        }
    }

    namespace
    {
        D3D12_HEAP_PROPERTIES getHeapProps(GpuMemoryHeap::Type t)
        {
            switch (t)
            {
            case GpuMemoryHeap::Type::Default:
                return kDefaultHeapProps;
            case GpuMemoryHeap::Type::Upload:
                return kUploadHeapProps;
            case GpuMemoryHeap::Type::Readback:
                return kReadbackHeapProps;
            default:
                should_not_get_here();
                return D3D12_HEAP_PROPERTIES();
            }
        }

        Buffer::State getInitState(GpuMemoryHeap::Type t)
        {
            switch (t)
            {
            case GpuMemoryHeap::Type::Default:
                return Buffer::State::Common;
            case GpuMemoryHeap::Type::Upload:
                return Buffer::State::GenericRead;
            case GpuMemoryHeap::Type::Readback:
                return Buffer::State::CopyDest;
            default:
                should_not_get_here();
                return Buffer::State::Undefined;
            }
        }
    }

    void GpuMemoryHeap::initBasePageData(BaseData& data, size_t size)
    {
        data.pResourceHandle = createBuffer(getInitState(mType), size, getHeapProps(mType), Buffer::BindFlags::None);
        data.offset = 0;
        D3D12_RANGE readRange = {};
        d3d_call(data.pResourceHandle->Map(0, &readRange, (void**)&data.pData));
    }

    D3D12_RESOURCE_DIMENSION getResourceDimension(Texture::Type type)
    {
        switch (type)
        {
        case Texture::Type::Texture1D:
            return D3D12_RESOURCE_DIMENSION_TEXTURE1D;

        case Texture::Type::Texture2D:
        case Texture::Type::Texture2DMultisample:
        case Texture::Type::TextureCube:
            return D3D12_RESOURCE_DIMENSION_TEXTURE2D;

        case Texture::Type::Texture3D:
            return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        default:
            should_not_get_here();
            return D3D12_RESOURCE_DIMENSION_UNKNOWN;
        }
    }

    void Texture::apiInit(const void* pData, bool autoGenMips)
    {
        D3D12_RESOURCE_DESC desc = {};

        desc.MipLevels = mMipLevels;
        desc.Format = getDxgiFormat(mFormat);
        desc.Width = align_to(getFormatWidthCompressionRatio(mFormat), mWidth);
        desc.Height = align_to(getFormatHeightCompressionRatio(mFormat), mHeight);
        desc.Flags = getD3D12ResourceFlags(mBindFlags);
        desc.SampleDesc.Count = mSampleCount;
        desc.SampleDesc.Quality = 0;
        desc.Dimension = getResourceDimension(mType);
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Alignment = 0;

        if (mType == Texture::Type::TextureCube)
        {
            desc.DepthOrArraySize = mArraySize * 6;
        }
        else if (mType == Texture::Type::Texture3D)
        {
            desc.DepthOrArraySize = mDepth;
        }
        else
        {
            desc.DepthOrArraySize = mArraySize;
        }
        assert(desc.Width > 0 && desc.Height > 0);
        assert(desc.MipLevels > 0 && desc.DepthOrArraySize > 0 && desc.SampleDesc.Count > 0);

        D3D12_CLEAR_VALUE clearValue = {};
        D3D12_CLEAR_VALUE* pClearVal = nullptr;
        if ((mBindFlags & (Texture::BindFlags::RenderTarget | Texture::BindFlags::DepthStencil)) != Texture::BindFlags::None)
        {
            clearValue.Format = desc.Format;
            if ((mBindFlags & Texture::BindFlags::DepthStencil) != Texture::BindFlags::None)
            {
                clearValue.DepthStencil.Depth = 1.0f;
            }
            pClearVal = &clearValue;
        }

        //If depth and either ua or sr, set to typeless
        if (isDepthFormat(mFormat) && is_set(mBindFlags, Texture::BindFlags::ShaderResource | Texture::BindFlags::UnorderedAccess))
        {
            desc.Format = getTypelessFormatFromDepthFormat(mFormat);
            pClearVal = nullptr;
        }

        D3D12_HEAP_FLAGS heapFlags = is_set(mBindFlags, ResourceBindFlags::Shared) ? D3D12_HEAP_FLAG_SHARED : D3D12_HEAP_FLAG_NONE;
        d3d_call(gpDevice->getApiHandle()->CreateCommittedResource(&kDefaultHeapProps, heapFlags, &desc, D3D12_RESOURCE_STATE_COMMON, pClearVal, IID_PPV_ARGS(&mApiHandle)));
        assert(mApiHandle);

        if (pData)
        {
            uploadInitData(pData, autoGenMips);
        }
    }

    Texture::~Texture()
    {
        gpDevice->releaseResource(mApiHandle);
    }
    SharedResourceApiHandle Resource::createSharedApiHandle()
    {
        ID3D12DevicePtr pDevicePtr = gpDevice->getApiHandle();
        auto s = string_to_wstring(mName);
        SharedResourceApiHandle pHandle;

        HRESULT res = pDevicePtr->CreateSharedHandle(mApiHandle, 0, GENERIC_ALL, s.c_str(), &pHandle);
        if (res == S_OK) return pHandle;
        else return nullptr;
    }
    SharedResourceApiHandle Resource::getSharedApiHandle() const
    {
        if (!mSharedApiHandle)
        {
            ID3D12DevicePtr pDevicePtr = gpDevice->getApiHandle();
            auto s = string_to_wstring(mName);
            SharedResourceApiHandle pHandle;

            HRESULT res = pDevicePtr->CreateSharedHandle(mApiHandle, 0, GENERIC_ALL, s.c_str(), &pHandle);
            if (res == S_OK)
            {
                mSharedApiHandle = pHandle;
            }
            else
            {
                throw std::exception("Resource::getSharedApiHandle(): failed to create shared handle");
            }
        }
        return mSharedApiHandle;
    }

    uint64_t Texture::getTextureSizeInBytes() const
    {
        ID3D12DevicePtr pDevicePtr = gpDevice->getApiHandle();
        ID3D12ResourcePtr pTexResource = this->getApiHandle();

        D3D12_RESOURCE_ALLOCATION_INFO d3d12ResourceAllocationInfo;
        D3D12_RESOURCE_DESC desc = pTexResource->GetDesc();

        assert(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D);

        d3d12ResourceAllocationInfo = pDevicePtr->GetResourceAllocationInfo(0, 1, &desc);
        assert(d3d12ResourceAllocationInfo.SizeInBytes > 0);
        return d3d12ResourceAllocationInfo.SizeInBytes;
    }
}
