#pragma once
#include <string>
#include <d3d12.h>
#include <comdef.h>
#include <dxgi1_4.h>
#include <dxgiformat.h>
#include <memory>
#include <vector>
#include <set>
#include "../Formats.h"
#include "../GraphicsResource.h"


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


	class D3D12DescriptorHeap : public std::enable_shared_from_this<D3D12DescriptorHeap>
	{
	public:
		using SharedPtr = std::shared_ptr<D3D12DescriptorHeap>;
		using SharedConstPtr = std::shared_ptr<const D3D12DescriptorHeap>;
		using ApiHandle = DescriptorHeapHandle;
		using CpuHandle = HeapCpuHandle;
		using GpuHandle = HeapGpuHandle;

		~D3D12DescriptorHeap();
		static const uint32_t kDescPerChunk = 64;

		/** Create a new descriptor heap.
			\param[in] type Descriptor heap type.
			\param[in] descCount Descriptor count.
			\param[in] shaderVisible True if the descriptor heap should be shader visible.
			\return A new object, or throws an exception if creation failed.
		*/
		static SharedPtr create(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descCount, bool shaderVisible = true);

		GpuHandle getBaseGpuHandle() const { return mGpuHeapStart; }
		CpuHandle getBaseCpuHandle() const { return mCpuHeapStart; }

	private:
		struct Chunk;

	public:
		class Allocation
		{
		public:
			using SharedPtr = std::shared_ptr<Allocation>;
			~Allocation();

			uint32_t getHeapEntryIndex(uint32_t index) const 
			{ assert(index < mDescCount); return index + mBaseIndex; }
			// Index is relative to the allocation
			CpuHandle getCpuHandle(uint32_t index) const 
			{ return mpHeap->getCpuHandle(getHeapEntryIndex(index)); } 
			// Index is relative to the allocation
			GpuHandle getGpuHandle(uint32_t index) const 
			{ return mpHeap->getGpuHandle(getHeapEntryIndex(index)); } 

		private:
			friend D3D12DescriptorHeap;
			static SharedPtr create(D3D12DescriptorHeap::SharedPtr pHeap, uint32_t baseIndex, uint32_t descCount, std::shared_ptr<Chunk> pChunk);
			Allocation(D3D12DescriptorHeap::SharedPtr pHeap, uint32_t baseIndex, uint32_t descCount, std::shared_ptr<Chunk> pChunk);
			D3D12DescriptorHeap::SharedPtr mpHeap;
			uint32_t mBaseIndex;
			uint32_t mDescCount;
			std::shared_ptr<Chunk> mpChunk;
		};

		Allocation::SharedPtr allocateDescriptors(uint32_t count);
		const ApiHandle& getApiHandle() const { return mApiHandle; }
		D3D12_DESCRIPTOR_HEAP_TYPE getType() const { return mType; }

		uint32_t getReservedChunkCount() const { return mMaxChunkCount; }
		uint32_t getDescriptorSize() const { return mDescriptorSize; }

	private:
		friend Allocation;
		D3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t chunkCount);


		CpuHandle getCpuHandle(uint32_t index) const;
		GpuHandle getGpuHandle(uint32_t index) const;

		CpuHandle mCpuHeapStart = {};
		GpuHandle mGpuHeapStart = {};
		uint32_t mDescriptorSize;
		const uint32_t mMaxChunkCount = 0;
		uint32_t mAllocatedChunks = 0;
		ApiHandle mApiHandle;
		D3D12_DESCRIPTOR_HEAP_TYPE mType;

		struct Chunk
		{
		public:
			using SharedPtr = std::shared_ptr<Chunk>;
			Chunk(uint32_t index, uint32_t count) : chunkIndex(index), chunkCount(count) {}

			void reset() { allocCount = 0; currentDesc = 0; }
			uint32_t getCurrentAbsoluteIndex() const { return chunkIndex * kDescPerChunk + currentDesc; }
			uint32_t getRemainingDescs() const { return chunkCount * kDescPerChunk - currentDesc; }

			uint32_t chunkIndex = 0;
			uint32_t chunkCount = 1; // For outstanding requests we can allocate more then a single chunk. This is the number of chunks we actually allocated
			uint32_t allocCount = 0;
			uint32_t currentDesc = 0;
		};

		// Helper to compare Chunk::SharedPtr types
		struct ChunkComparator
		{
			bool operator()(const Chunk::SharedPtr& lhs, const Chunk::SharedPtr& rhs) const
			{
				return lhs->chunkCount < rhs->chunkCount;
			}

			bool operator()(const Chunk::SharedPtr& lhs, uint32_t rhs) const
			{
				return lhs->chunkCount < rhs;
			};
		};

		bool setupCurrentChunk(uint32_t descCount);
		void releaseChunk(Chunk::SharedPtr pChunk);

		Chunk::SharedPtr mpCurrentChunk;
		std::vector<Chunk::SharedPtr> mFreeChunks; // Free list for standard sized chunks (1 chunk * kDescPerChunk)
		std::multiset<Chunk::SharedPtr, ChunkComparator> mFreeLargeChunks; // Free list for large chunks with the capacity of multiple chunks (>1 chunk * kDescPerChunk)
	};

	struct DescriptorPoolApiData
	{
		D3D12DescriptorHeap::SharedPtr pHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	};

	struct DescriptorSetApiData
	{
		D3D12DescriptorHeap::Allocation::SharedPtr pAllocation; // The heap-allocation. We always allocate a single contiguous block, even if there are multiple ranges.
		std::vector<uint32_t> rangeBaseOffset;                  // For each range, we store the base offset into the allocation. We need it because many set calls accept a range index.
	};

	D3D12_RESOURCE_FLAGS getD3D12ResourceFlags(Resource::BindFlags flags);
	D3D12_RESOURCE_STATES getD3D12ResourceState(Resource::State s);

	extern const D3D12_HEAP_PROPERTIES kDefaultHeapProps;
	extern const D3D12_HEAP_PROPERTIES kUploadHeapProps;
	extern const D3D12_HEAP_PROPERTIES kReadbackHeapProps;
}