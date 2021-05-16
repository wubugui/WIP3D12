#include "GraphicsCommon.h"
#include "Device.h"

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
		return uint64_t();
	}
	void GpuFence::syncGpu(CommandQueueHandle pQueue)
	{
	}
	void GpuFence::syncCpu(uint64_t val)
	{
	}
	uint64_t GpuFence::gpuSignal(CommandQueueHandle pQueue)
	{
		return uint64_t();
	}
	DescriptorPool::SharedPtr DescriptorPool::create(const Desc& desc, const GpuFence::SharedPtr& pFence)
	{
		return SharedPtr();
	}
}
