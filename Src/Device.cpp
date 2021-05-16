#include "Device.h"
#include "Application.h"

namespace WIP3D
{
    Device::SharedPtr gpDevice;

    void createNullViews()
    {
    }

	Device::SharedPtr Device::create(Window::SharedPtr& pWindow, const Desc& desc)
	{
        if (gpDevice)
        {
            LOG_WARN("Only supports a single device");
            return nullptr;
        }
        gpDevice = SharedPtr(new Device(pWindow, desc));
        if (gpDevice->init() == false) { gpDevice = nullptr; }
        return gpDevice;
	}

    

    bool Device::init()
    {
        //至少要创建Graphics Contex
        const uint32_t kDirectQueueIndex = (uint32_t)LowLevelContextData::CommandQueueType::Direct;
        assert(mDesc.cmdQueues[kDirectQueueIndex] > 0);

        if (apiInit() == false) return false;

        // Create the descriptor pools
        DescriptorPool::Desc poolDesc;
        // For DX12 there is no difference between the different SRV/UAV types. 
        // For Vulkan it matters, hence the #ifdef
        // DX12 guarantees at least 1,000,000 descriptors
        poolDesc.setDescCount(DescriptorPool::Type::TextureSrv, 1000000)
            .setDescCount(DescriptorPool::Type::Sampler, 2048)
            .setShaderVisible(true);
#ifdef NO_D3D12
        poolDesc.setDescCount(DescriptorPool::Type::Cbv, 16 * 1024)
            .setDescCount(DescriptorPool::Type::TextureUav, 16 * 1024);
        poolDesc.setDescCount(DescriptorPool::Type::StructuredBufferSrv, 2 * 1024)
            .setDescCount(DescriptorPool::Type::StructuredBufferUav, 2 * 1024)
            .setDescCount(DescriptorPool::Type::TypedBufferSrv, 2 * 1024)
            .setDescCount(DescriptorPool::Type::TypedBufferUav, 2 * 1024)
            .setDescCount(DescriptorPool::Type::RawBufferSrv, 2 * 1024)
            .setDescCount(DescriptorPool::Type::RawBufferUav, 2 * 1024);
#endif
        mpFrameFence = GpuFence::create();
        mpGpuDescPool = DescriptorPool::create(poolDesc, mpFrameFence);
        poolDesc.setShaderVisible(false).setDescCount(DescriptorPool::Type::Rtv, 16 * 1024).setDescCount(DescriptorPool::Type::Dsv, 1024);
        mpCpuDescPool = DescriptorPool::create(poolDesc, mpFrameFence);

        mpUploadHeap = GpuMemoryHeap::create(GpuMemoryHeap::Type::Upload, 1024 * 1024 * 2, mpFrameFence);
        createNullViews();
        mpRenderContext = RenderContext::create(mCmdQueues[(uint32_t)LowLevelContextData::CommandQueueType::Direct][0]);

        assert(mpRenderContext);
        mpRenderContext->flush();  // This will bind the descriptor heaps.
        // TODO: Do we need to flush here or should RenderContext::create() bind the descriptor heaps automatically without flush? See #749.

        // Update the FBOs
        if (updateDefaultFBO(mpWindow->GetClientAreaSize().x, mpWindow->GetClientAreaSize().y, mDesc.colorFormat, mDesc.depthFormat) == false)
        {
            return false;
        }

        return true;
    }

    
    bool Device::updateDefaultFBO(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat)
    {
        /*
        ResourceHandle apiHandles[kSwapChainBuffersCount] = {};
        getApiFboData(width, height, colorFormat, depthFormat, apiHandles, mCurrentBackBufferIndex);

        for (uint32_t i = 0; i < kSwapChainBuffersCount; i++)
        {
            // Create a texture object
            auto pColorTex = Texture::SharedPtr(new Texture(width, height, 1, 1, 1, 1, colorFormat, Texture::Type::Texture2D, Texture::BindFlags::RenderTarget));
            pColorTex->mApiHandle = apiHandles[i];
            // Create the FBO if it's required
            if (mpSwapChainFbos[i] == nullptr) mpSwapChainFbos[i] = Fbo::create();
            mpSwapChainFbos[i]->attachColorTarget(pColorTex, 0);

            // Create a depth texture
            if (depthFormat != ResourceFormat::Unknown)
            {
                auto pDepth = Texture::create2D(width, height, depthFormat, 1, 1, nullptr, Texture::BindFlags::DepthStencil);
                mpSwapChainFbos[i]->attachDepthStencilTarget(pDepth);
            }
        }
        */
        return true;
    }

}
