#include "../Device.h"
#include "../Common/FileSystem.h"

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


    SharedResourceApiHandle Resource::createSharedApiHandle()
    {
        ID3D12DevicePtr pDevicePtr = gpDevice->getApiHandle();
        auto s = string_to_wstring(mName);
        SharedResourceApiHandle pHandle;

        HRESULT res = pDevicePtr->CreateSharedHandle(mApiHandle, 0, GENERIC_ALL, s.c_str(), &pHandle);
        if (res == S_OK) return pHandle;
        else return nullptr;
    }
}
