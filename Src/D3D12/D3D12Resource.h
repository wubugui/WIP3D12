#include <d3d12.h>
#include <comdef.h>
#include <dxgi1_4.h>
#include <dxgiformat.h>

namespace WIP3D
{
    D3D12_RESOURCE_FLAGS getD3D12ResourceFlags(Resource::BindFlags flags);
    D3D12_RESOURCE_STATES getD3D12ResourceState(Resource::State s);

    extern const D3D12_HEAP_PROPERTIES kDefaultHeapProps;
    extern const D3D12_HEAP_PROPERTIES kUploadHeapProps;
    extern const D3D12_HEAP_PROPERTIES kReadbackHeapProps;
}
