#pragma once
#include <string>
#include <d3d12.h>
#include <comdef.h>
#include <dxgi1_4.h>
#include <dxgiformat.h>
#include <memory>
#include "../Formats.h"

void TraceHResult(const std::string& msg, HRESULT hr);

#define MAKE_SMART_COM_PTR(_a) _COM_SMARTPTR_TYPEDEF(_a, __uuidof(_a))
#define d3d_call(a) {HRESULT hr_ = a; if(FAILED(hr_)) { TraceHResult( #a, hr_); }}
#define GET_COM_INTERFACE(base, type, var) MAKE_SMART_COM_PTR(type); concat_strings(type, Ptr) var; d3d_call(base->QueryInterface(IID_PPV_ARGS(&var)));
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")

#define UNSUPPORTED_IN_D3D(msg_) {logWarning(msg_ + std::string(" is not supported in D3D. Ignoring call."));}

namespace WIP3D
{
    class DescriptorSet;
    struct DxgiFormatDesc
    {
        ResourceFormat falcorFormat;
        DXGI_FORMAT dxgiFormat;
    };
#define to_string_case(a) case a: return #a;
    inline std::string to_string(D3D_FEATURE_LEVEL featureLevel)
    {
        switch (featureLevel)
        {
            to_string_case(D3D_FEATURE_LEVEL_9_1)
            to_string_case(D3D_FEATURE_LEVEL_9_2)
            to_string_case(D3D_FEATURE_LEVEL_9_3)
            to_string_case(D3D_FEATURE_LEVEL_10_0)
            to_string_case(D3D_FEATURE_LEVEL_10_1)
            to_string_case(D3D_FEATURE_LEVEL_11_0)
            to_string_case(D3D_FEATURE_LEVEL_11_1)
            to_string_case(D3D_FEATURE_LEVEL_12_0)
            to_string_case(D3D_FEATURE_LEVEL_12_1)
            default: should_not_get_here(); return "";
        }
    }
#undef to_string_case
    struct DxgiFormatDesc
    {
        ResourceFormat falcorFormat;
        DXGI_FORMAT dxgiFormat;
    };

    extern const DxgiFormatDesc kDxgiFormatDesc[];
    /** Convert from Falcor to DXGI format
    */
    inline DXGI_FORMAT getDxgiFormat(ResourceFormat format)
    {
        assert(kDxgiFormatDesc[(uint32_t)format].falcorFormat == format);
        return kDxgiFormatDesc[(uint32_t)format].dxgiFormat;
    }

    // DXGI
    MAKE_SMART_COM_PTR(IDXGISwapChain3);
    MAKE_SMART_COM_PTR(IDXGIDevice);
    MAKE_SMART_COM_PTR(IDXGIAdapter1);
    MAKE_SMART_COM_PTR(IDXGIFactory4);
    MAKE_SMART_COM_PTR(ID3DBlob);

    MAKE_SMART_COM_PTR(ID3D12StateObject);
    MAKE_SMART_COM_PTR(ID3D12Device);
    MAKE_SMART_COM_PTR(ID3D12GraphicsCommandList);
    MAKE_SMART_COM_PTR(ID3D12Debug);
    MAKE_SMART_COM_PTR(ID3D12CommandQueue);
    MAKE_SMART_COM_PTR(ID3D12CommandAllocator);
    MAKE_SMART_COM_PTR(ID3D12DescriptorHeap);
    MAKE_SMART_COM_PTR(ID3D12Resource);
    MAKE_SMART_COM_PTR(ID3D12Fence);
    MAKE_SMART_COM_PTR(ID3D12PipelineState);
    MAKE_SMART_COM_PTR(ID3D12RootSignature);
    MAKE_SMART_COM_PTR(ID3D12QueryHeap);
    MAKE_SMART_COM_PTR(ID3D12CommandSignature);
    MAKE_SMART_COM_PTR(IUnknown);

    using ApiObjectHandle = IUnknownPtr;

    using HeapCpuHandle = D3D12_CPU_DESCRIPTOR_HANDLE;
    using HeapGpuHandle = D3D12_GPU_DESCRIPTOR_HANDLE;

    class DescriptorHeapEntry;

    using WindowHandle = HWND;
    using DeviceHandle = ID3D12DevicePtr;
    using CommandListHandle = ID3D12GraphicsCommandListPtr;
    using CommandQueueHandle = ID3D12CommandQueuePtr;
    using ApiCommandQueueType = D3D12_COMMAND_LIST_TYPE;
    using CommandAllocatorHandle = ID3D12CommandAllocatorPtr;
    using CommandSignatureHandle = ID3D12CommandSignaturePtr;
    using FenceHandle = ID3D12FencePtr;
    using ResourceHandle = ID3D12ResourcePtr;
    using RtvHandle = std::shared_ptr<DescriptorSet>;
    using DsvHandle = std::shared_ptr<DescriptorSet>;
    using SrvHandle = std::shared_ptr<DescriptorSet>;
    using SamplerHandle = std::shared_ptr<DescriptorSet>;
    using UavHandle = std::shared_ptr<DescriptorSet>;
    using CbvHandle = std::shared_ptr<DescriptorSet>;
    using FboHandle = void*;
    using GpuAddress = D3D12_GPU_VIRTUAL_ADDRESS;
    using QueryHeapHandle = ID3D12QueryHeapPtr;
    using SharedResourceApiHandle = HANDLE;

    using GraphicsStateHandle = ID3D12PipelineStatePtr;
    using ComputeStateHandle = ID3D12PipelineStatePtr;
    using ShaderHandle = D3D12_SHADER_BYTECODE;
    using RootSignatureHandle = ID3D12RootSignaturePtr;
    using DescriptorHeapHandle = ID3D12DescriptorHeapPtr;

    using VaoHandle = void*;
    using VertexShaderHandle = void*;
    using FragmentShaderHandle = void*;
    using DomainShaderHandle = void*;
    using HullShaderHandle = void*;
    using GeometryShaderHandle = void*;
    using ComputeShaderHandle = void*;
    using ProgramHandle = void*;
    using DepthStencilStateHandle = void*;
    using RasterizerStateHandle = void*;
    using BlendStateHandle = void*;
    using DescriptorSetApiHandle = void*;
}