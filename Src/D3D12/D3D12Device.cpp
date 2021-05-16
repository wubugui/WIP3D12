#include "../Device.h"
#include "WIPD3D12.h"
#include "../Util.h"
#include "../Common/FileSystem.h"

namespace WIP3D
{
	namespace
	{
		const uint32_t kDefaultVendorId = 0x10DE; ///< NVIDIA GPUs
	}
	struct DeviceApiData
	{
		IDXGIFactory4Ptr pDxgiFactory = nullptr;
		IDXGISwapChain3Ptr pSwapChain = nullptr;
		//??
		bool isWindowOccluded = false;
	};

	D3D_FEATURE_LEVEL getD3DFeatureLevel(uint32_t majorVersion, uint32_t minorVersion)
	{
		if (majorVersion == 12)
		{
			switch (minorVersion)
			{
			case 0:
				return D3D_FEATURE_LEVEL_12_0;
			case 1:
				return D3D_FEATURE_LEVEL_12_1;
			}
		}
		else if (majorVersion == 11)
		{
			switch (minorVersion)
			{
			case 0:
				return D3D_FEATURE_LEVEL_11_0;
			case 1:
				return D3D_FEATURE_LEVEL_11_1;
			}
		}
		else if (majorVersion == 10)
		{
			switch (minorVersion)
			{
			case 0:
				return D3D_FEATURE_LEVEL_10_0;
			case 1:
				return D3D_FEATURE_LEVEL_10_1;
			}
		}
		else if (majorVersion == 9)
		{
			switch (minorVersion)
			{
			case 1:
				return D3D_FEATURE_LEVEL_9_1;
			case 2:
				return D3D_FEATURE_LEVEL_9_2;
			case 3:
				return D3D_FEATURE_LEVEL_9_3;
			}
		}
		return (D3D_FEATURE_LEVEL)0;
	}
	Device::SupportedFeatures getSupportedFeatures(DeviceHandle pDevice)
	{
		Device::SupportedFeatures supported = Device::SupportedFeatures::None;

		D3D12_FEATURE_DATA_D3D12_OPTIONS2 features2;
		HRESULT hr = pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS2, &features2, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS2));
		if (FAILED(hr) || features2.ProgrammableSamplePositionsTier == D3D12_PROGRAMMABLE_SAMPLE_POSITIONS_TIER_NOT_SUPPORTED)
		{
			LOG_WARN("Programmable sample positions is not supported on this device.");
		}
		else
		{
			if (features2.ProgrammableSamplePositionsTier == D3D12_PROGRAMMABLE_SAMPLE_POSITIONS_TIER_1) supported |= Device::SupportedFeatures::ProgrammableSamplePositionsPartialOnly;
			else if (features2.ProgrammableSamplePositionsTier == D3D12_PROGRAMMABLE_SAMPLE_POSITIONS_TIER_2) supported |= Device::SupportedFeatures::ProgrammableSamplePositionsFull;
		}

		D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5;
		hr = pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5));
		if (FAILED(hr) || features5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
		{
			LOG_WARN("Raytracing is not supported on this device.");
		}
		else
		{
			supported |= Device::SupportedFeatures::Raytracing;
		}

		return supported;
	}
	DeviceHandle createDevice(IDXGIFactory4* pFactory, D3D_FEATURE_LEVEL requestedFeatureLevel, const std::vector<UUID>& experimentalFeatures)
	{
		// Feature levels to try creating devices. Listed in descending order so the highest supported level is used.
		const static D3D_FEATURE_LEVEL kFeatureLevels[] =
		{
			D3D_FEATURE_LEVEL_12_1,
			D3D_FEATURE_LEVEL_12_0,
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
			D3D_FEATURE_LEVEL_9_3,
			D3D_FEATURE_LEVEL_9_2,
			D3D_FEATURE_LEVEL_9_1
		};

		const static uint32_t kUnspecified = uint32_t(-1);
		const uint32_t preferredGpuVendorId = kUnspecified;
		const uint32_t preferredGpuIndex = kUnspecified;
		/*
		// Read FALCOR_GPU_VENDOR_ID environment variable.
		const uint32_t preferredGpuVendorId = ([](){
			std::string str;
			// Use base = 0 in stoi to autodetect octal/hex/decimal strings
			return getEnvironmentVariable("FALCOR_GPU_VENDOR_ID", str) ? std::stoi(str, nullptr, 0) : kUnspecified;
		})();

		// Read FALCOR_GPU_DEVICE_ID environment variable.
		const uint32_t preferredGpuIndex = ([](){
			std::string str;
			return getEnvironmentVariable("FALCOR_GPU_DEVICE_ID", str) ? std::stoi(str) : kUnspecified;
		})();
		*/
		IDXGIAdapter1Ptr pAdapter;
		DeviceHandle pDevice;
		D3D_FEATURE_LEVEL selectedFeatureLevel;

		auto createMaxFeatureLevel = [&](const D3D_FEATURE_LEVEL* pFeatureLevels, uint32_t featureLevelCount) -> bool
		{
			//从高版本到低版本选择第一个创建成功的featurelevel
			for (uint32_t i = 0; i < featureLevelCount; i++)
			{
				if (SUCCEEDED(D3D12CreateDevice(pAdapter, pFeatureLevels[i], IID_PPV_ARGS(&pDevice))))
				{
					selectedFeatureLevel = pFeatureLevels[i];
					return true;
				}
			}

			return false;
		};

		// Properties to search for
		//上面的环境变量没得到，直接设置为默认ID
		const uint32_t vendorId = (preferredGpuVendorId != kUnspecified) ? preferredGpuVendorId : kDefaultVendorId;//Nvdia GPU
		const uint32_t gpuIdx = (preferredGpuIndex != kUnspecified) ? preferredGpuIndex : 0;

		// Select adapter
		uint32_t vendorDeviceIndex = 0; // Tracks device index within adapters matching a specific vendor ID
		uint32_t selectedAdapterIndex = uint32_t(-1); // The final adapter chosen to create the device object from
		for (uint32_t i = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(i, &pAdapter); i++)
		{
			DXGI_ADAPTER_DESC1 desc;
			pAdapter->GetDesc1(&desc);

			// Skip SW adapters
			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

			// Skip if vendorId doesn't match requested
			if (desc.VendorId != vendorId) continue;

			// When a vendor match is found above, count to the specified device index of that vendor (e.g. the i-th NVIDIA GPU)
			//检测到的第一个GPU
			if (vendorDeviceIndex++ < gpuIdx) continue;

			// Select the first adapter satisfying the conditions
			selectedAdapterIndex = i;
			break;
		}

		if (selectedAdapterIndex == uint32_t(-1))
		{
			// If no GPU was found, just select the first
			selectedAdapterIndex = 0;

			// Log a warning if an adapter matching user specifications wasn't found.
			// Selection could have failed based on the default settings, but that isn't an error.
			if (preferredGpuVendorId != kUnspecified || preferredGpuIndex != kUnspecified)
			{
				LOG_WARN("Could not find a GPU matching conditions specified in environment variables.");
			}
		}

		// Retrieve the adapter that's been selected
		HRESULT result = pFactory->EnumAdapters1(selectedAdapterIndex, &pAdapter);
		//输出Adapter的信息
		DXGI_ADAPTER_DESC1 descSelected;
		pAdapter->GetDesc1(&descSelected);
		LOG_INFO("%s | ID: %d | System Memory:%lld MB| Video Memory:%lld MB| Shared Memory:%lld MB",
			wstring_to_string(descSelected.Description).c_str(),
			descSelected.DeviceId,
			descSelected.DedicatedSystemMemory/(1024ll*1024ll),
			descSelected.DedicatedVideoMemory/ (1024ll * 1024ll),
			descSelected.SharedSystemMemory/ (1024ll * 1024ll));
		assert(SUCCEEDED(result));

		if (requestedFeatureLevel == 0) createMaxFeatureLevel(kFeatureLevels, ARRAY_COUNT(kFeatureLevels));
		else createMaxFeatureLevel(&requestedFeatureLevel, 1);

		if (pDevice != nullptr)
		{
			LOG_INFO(("Successfully created device with feature level: " + to_string(selectedFeatureLevel)).c_str());
			return pDevice;
		}

		LOG_ERROR("Could not find a GPU that supports D3D12 device");
		return nullptr;
	}
	IDXGISwapChain3Ptr createDxgiSwapChain(IDXGIFactory4* pFactory, const Window* pWindow, ID3D12CommandQueue* pCommandQueue, ResourceFormat colorFormat, uint32_t bufferCount)
	{
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		//3??
		swapChainDesc.BufferCount = bufferCount;
		swapChainDesc.Width = pWindow->GetClientAreaSize().x;
		swapChainDesc.Height = pWindow->GetClientAreaSize().y;
		// Flip mode doesn't support SRGB formats, so we strip them down when creating the resource. We will create the RTV as SRGB instead.
		// More details at the end of https://msdn.microsoft.com/en-us/library/windows/desktop/bb173064.aspx
		// 创建非SRGB backbuffer，使用SRGB RTV
		//https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/converting-data-color-space
		//https://blog.csdn.net/wubuguiohmygod/article/details/116898311
		swapChainDesc.Format = getDxgiFormat(srgbToLinearFormat(colorFormat));
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc.Count = 1;

		// CreateSwapChainForHwnd() doesn't accept IDXGISwapChain3 (Why MS? Why?)
		MAKE_SMART_COM_PTR(IDXGISwapChain1);
		IDXGISwapChain1Ptr pSwapChain;

		HRESULT hr = pFactory->CreateSwapChainForHwnd(pCommandQueue, pWindow->GetApiHandle(), &swapChainDesc, nullptr, nullptr, &pSwapChain);
		if (FAILED(hr))
		{
			TraceHResult("Failed to create the swap-chain", hr);
			return false;
		}

		IDXGISwapChain3Ptr pSwapChain3;
		d3d_call(pSwapChain->QueryInterface(IID_PPV_ARGS(&pSwapChain3)));
		return pSwapChain3;
	}
	bool Device::getApiFboData(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat, ResourceHandle apiHandles[kSwapChainBuffersCount], uint32_t& currentBackBufferIndex)
	{
		return false;
	}
	void Device::destroyApiObjects()
	{
	}
	void Device::apiPresent()
	{
	}
	bool Device::apiInit()
	{
		DeviceApiData* pData = new DeviceApiData;
		mpApiData = pData;
		UINT dxgiFlags = 0;
		if (mDesc.enableDebugLayer)
		{
			ID3D12DebugPtr pDx12Debug;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDx12Debug))))
			{
				pDx12Debug->EnableDebugLayer();
				dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
			else
			{
				LOG_WARN("The D3D12 debug layer is not available. Please install Graphics Tools.");
				mDesc.enableDebugLayer = false;
			}
		}

		// Create the DXGI factory
		d3d_call(CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&mpApiData->pDxgiFactory)));

		// Create the device
		mApiHandle = createDevice(mpApiData->pDxgiFactory, getD3DFeatureLevel(mDesc.apiMajorVersion, mDesc.apiMinorVersion), mDesc.experimentalFeatures);
		if (mApiHandle == nullptr) return false;

		//获取当前设备支持的特性
		mSupportedFeatures = getSupportedFeatures(mApiHandle);

		if (mDesc.enableDebugLayer)
		{
			MAKE_SMART_COM_PTR(ID3D12InfoQueue);
			ID3D12InfoQueuePtr pInfoQueue;
			mApiHandle->QueryInterface(IID_PPV_ARGS(&pInfoQueue));
			D3D12_MESSAGE_ID hideMessages[] =
			{
				D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
				D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
				D3D12_MESSAGE_ID_COPY_DESCRIPTORS_INVALID_RANGES
			};
			D3D12_INFO_QUEUE_FILTER f = {};
			f.DenyList.NumIDs = ARRAY_COUNT(hideMessages);
			f.DenyList.pIDList = hideMessages;
			pInfoQueue->AddStorageFilterEntries(&f);

			// Break on DEVICE_REMOVAL_PROCESS_AT_FAULT
			pInfoQueue->SetBreakOnID(D3D12_MESSAGE_ID_DEVICE_REMOVAL_PROCESS_AT_FAULT, true);
		}

		for (uint32_t i = 0; i < kQueueTypeCount; i++)
		{
			//创建所有指定的command queue
			for (uint32_t j = 0; j < mDesc.cmdQueues[i]; j++)
			{
				// Create the command queue
				D3D12_COMMAND_QUEUE_DESC cqDesc = {};
				cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
				cqDesc.Type = getApiCommandQueueType((LowLevelContextData::CommandQueueType)i);

				ID3D12CommandQueuePtr pQueue;
				if (FAILED(mApiHandle->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&pQueue))))
				{
					LOG_ERROR("Failed to create command queue");
					return false;
				}

				mCmdQueues[i].push_back(pQueue);
			}
		}

		//used in timestamp queries
		//将查询结果转换为实际时间
		uint64_t freq;
		d3d_call(getCommandQueueHandle(LowLevelContextData::CommandQueueType::Direct, 0)->GetTimestampFrequency(&freq));
		mGpuTimestampFrequency = 1000.0 / (double)freq;
		return createSwapChain(mDesc.colorFormat);
	}
	bool Device::createSwapChain(ResourceFormat colorFormat)
	{
		mpApiData->pSwapChain = createDxgiSwapChain(mpApiData->pDxgiFactory, mpWindow.get(), getCommandQueueHandle(LowLevelContextData::CommandQueueType::Direct, 0), colorFormat, kSwapChainBuffersCount);
		if (mpApiData->pSwapChain == nullptr) return false;
		return true;
	}
	void Device::apiResizeSwapChain(uint32_t width, uint32_t height, ResourceFormat colorFormat)
	{
	}
	void Device::toggleFullScreen(bool fullscreen)
	{
	}
	bool Device::isWindowOccluded() const
	{
		return false;
	}
	CommandQueueHandle Device::getCommandQueueHandle(LowLevelContextData::CommandQueueType type, uint32_t index) const
	{
		return mCmdQueues[(uint32_t)type][index];
	}

	ApiCommandQueueType Device::getApiCommandQueueType(LowLevelContextData::CommandQueueType type) const
	{
		switch (type)
		{
		case LowLevelContextData::CommandQueueType::Copy:
			return D3D12_COMMAND_LIST_TYPE_COPY;
		case LowLevelContextData::CommandQueueType::Compute:
			return D3D12_COMMAND_LIST_TYPE_COMPUTE;
		case LowLevelContextData::CommandQueueType::Direct:
			return D3D12_COMMAND_LIST_TYPE_DIRECT;
		default:
			throw std::exception("Unknown command queue type");
		}
	}
}