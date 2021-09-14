#include "WIPD3D12.h"
#include "../Common/Logger.h"
#include "../GraphicsCommon.h"
#include "../Device.h"
#include "../GPUMemory.h"
#include "../Util.h"


void TraceHResult(const std::string& msg, HRESULT hr)
{
    char hr_msg[512];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, hr, 0, hr_msg, ARRAYSIZE(hr_msg), nullptr);

    std::string error_msg = msg + ".\nError! " + hr_msg;
    
    LOG_ERROR(error_msg.c_str());
}

namespace WIP3D
{
	struct FenceApiData
	{
		HANDLE eventHandle = INVALID_HANDLE_VALUE;
	};
	GpuFence::~GpuFence()
	{
		CloseHandle(mpApiData->eventHandle);
		safe_delete(mpApiData);
	}
	GpuFence::SharedPtr GpuFence::create()
	{
		SharedPtr pFence = SharedPtr(new GpuFence());
		pFence->mpApiData = new FenceApiData;
		pFence->mpApiData->eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (pFence->mpApiData->eventHandle == nullptr) throw std::exception("Failed to create an event object");

		assert(gpDevice);
		ID3D12Device* pDevice = gpDevice->getApiHandle().GetInterfacePtr();
		HRESULT hr = pDevice->CreateFence(pFence->mCpuValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence->mApiHandle));
		if (FAILED(hr))
		{
			TraceHResult("Failed to create a fence object", hr);
			throw std::exception("Failed to create GPU fence");
		}

		pFence->mCpuValue++;
		return pFence;
	}

	uint64_t GpuFence::getGpuValue() const
	{
		//Returns the current value of the fence. 
		//If the device has been removed, the return value will be UINT64_MAX.
		return mApiHandle->GetCompletedValue();
	}
	void GpuFence::syncGpu(CommandQueueHandle pQueue)
	{
		// Queues a GPU-side wait, and returns immediately. A GPU-side wait is where the GPU waits 
		// until the specified fence reaches or exceeds the specified value.
		// Second Prama:
		// The value that the command queue is waiting for the fence 
		// to reach or exceed. 
		// So when ID3D12Fence::GetCompletedValue is greater than or equal to Value, \
		// the wait is terminated.
		d3d_call(pQueue->Wait(mApiHandle, mCpuValue - 1));
	}
	void GpuFence::syncCpu(uint64_t val)
	{
		uint64_t syncVal = val ? val : mCpuValue - 1;
		assert(syncVal <= mCpuValue - 1);

		uint64_t gpuVal = getGpuValue();
		if (gpuVal < syncVal)
		{
			// Specifies an event that should be fired when the fence reaches a certain value
			d3d_call(mApiHandle->SetEventOnCompletion(syncVal, mpApiData->eventHandle));
			WaitForSingleObject(mpApiData->eventHandle, INFINITE);
		}
	}
	uint64_t GpuFence::gpuSignal(CommandQueueHandle pQueue)
	{
		assert(pQueue);
		// Updates a fence to a specified value.
		d3d_call(pQueue->Signal(mApiHandle, mCpuValue));
		mCpuValue++;
		return mCpuValue - 1;
	}


	D3D12_DESCRIPTOR_HEAP_TYPE falcorToDxDescType(DescriptorPool::Type t)
	{
		switch (t)
		{
		case DescriptorPool::Type::TextureSrv:
		case DescriptorPool::Type::TextureUav:
		case DescriptorPool::Type::RawBufferSrv:
		case DescriptorPool::Type::RawBufferUav:
		case DescriptorPool::Type::TypedBufferSrv:
		case DescriptorPool::Type::TypedBufferUav:
		case DescriptorPool::Type::StructuredBufferSrv:
		case DescriptorPool::Type::StructuredBufferUav:
		case DescriptorPool::Type::Cbv:
			return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		case DescriptorPool::Type::Dsv:
			return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		case DescriptorPool::Type::Rtv:
			return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		case DescriptorPool::Type::Sampler:
			return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		default:
			should_not_get_here();
			return D3D12_DESCRIPTOR_HEAP_TYPE(-1);
		}
	}

	void DescriptorPool::apiInit()
	{
		// Find out how many heaps we need
		static_assert(DescriptorPool::kTypeCount == 12, "Unexpected desc count, make sure all desc types are supported");
		uint32_t descCount[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = { 0 };

		descCount[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] = mDesc.mDescCount[(uint32_t)Type::Rtv];
		descCount[D3D12_DESCRIPTOR_HEAP_TYPE_DSV] = mDesc.mDescCount[(uint32_t)Type::Dsv];
		descCount[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = mDesc.mDescCount[(uint32_t)Type::Sampler];
		descCount[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = mDesc.mDescCount[(uint32_t)Type::Cbv];
		descCount[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] += mDesc.mDescCount[(uint32_t)Type::TextureSrv] + mDesc.mDescCount[(uint32_t)Type::RawBufferSrv] + mDesc.mDescCount[(uint32_t)Type::TypedBufferSrv] + mDesc.mDescCount[(uint32_t)Type::StructuredBufferSrv];
		descCount[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] += mDesc.mDescCount[(uint32_t)Type::TextureUav] + mDesc.mDescCount[(uint32_t)Type::RawBufferUav] + mDesc.mDescCount[(uint32_t)Type::TypedBufferUav] + mDesc.mDescCount[(uint32_t)Type::StructuredBufferUav];

		mpApiData = std::make_shared<DescriptorPoolApiData>();
		for (uint32_t i = 0; i < ARRAY_COUNT(mpApiData->pHeaps); i++)
		{
			if (descCount[i])
			{
				mpApiData->pHeaps[i] = D3D12DescriptorHeap::create(D3D12_DESCRIPTOR_HEAP_TYPE(i), descCount[i], mDesc.mShaderVisible);
			}
		}
	}

	const DescriptorPool::ApiHandle& DescriptorPool::getApiHandle(uint32_t heapIndex) const
	{
		assert(heapIndex < ARRAY_COUNT(mpApiData->pHeaps));
		return mpApiData->pHeaps[heapIndex]->getApiHandle();
	}
}