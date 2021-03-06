#pragma once
#include <array>
#include <memory>

#include "Formats.h"
#include "GraphicsContext.h"
#include "RenderTarget.h"
#include "Application.h"
namespace WIP3D
{
#ifdef _DEBUG
#define DEFAULT_ENABLE_DEBUG_LAYER true
#else
#define DEFAULT_ENABLE_DEBUG_LAYER false
#endif
    struct DeviceApiData;

    class Device
    {
    public:
        using SharedPtr = std::shared_ptr<Device>;
        using SharedConstPtr = std::shared_ptr<const Device>;
        using ApiHandle = DeviceHandle;
        static const uint32_t kQueueTypeCount = (uint32_t)LowLevelContextData::CommandQueueType::Count;

        /** Device configuration
        */
        struct Desc
        {
            ResourceFormat colorFormat = ResourceFormat::BGRA8UnormSrgb;    ///< The color buffer format
            ResourceFormat depthFormat = ResourceFormat::D32Float;          ///< The depth buffer format
            uint32_t apiMajorVersion = 0;                                   ///< Requested API major version. If specified, device creation will fail if not supported. Otherwise, the highest supported version will be automatically selected.
            uint32_t apiMinorVersion = 0;                                   ///< Requested API minor version. If specified, device creation will fail if not supported. Otherwise, the highest supported version will be automatically selected.
            bool enableVsync = false;                                       ///< Controls vertical-sync
            bool enableDebugLayer = DEFAULT_ENABLE_DEBUG_LAYER;             ///< Enable the debug layer. The default for release build is false, for debug build it's true.

            static_assert((uint32_t)LowLevelContextData::CommandQueueType::Direct == 2, "Default initialization of cmdQueues assumes that Direct queue index is 2");
            //用于指定需要创建的command queue类型的数量，分别对应copy，compute，graphics
            //此处默认只创建一个graphics queue
            ///< Command queues to create. If no direct-queues are created, mpRenderContext will not be initialized
            std::array<uint32_t, kQueueTypeCount> cmdQueues = { 0, 0, 1 };  

            // GUID list for experimental features
            std::vector<UUID> experimentalFeatures;
        };

        enum class SupportedFeatures
        {
            None = 0x0,
            ProgrammableSamplePositionsPartialOnly = 0x1, // On D3D12, this means tier 1 support. Allows one sample position to be set.
            ProgrammableSamplePositionsFull = 0x2,        // On D3D12, this means tier 2 support. Allows up to 4 sample positions to be set.
            Barycentrics = 0x4,                           // On D3D12, pixel shader barycentrics are supported.
            Raytracing = 0x8,                             // On D3D12, DirectX Raytracing is supported. It is up to the user to not use raytracing functions when not supported.
            RaytracingTier1_1 = 0x10,                     // On D3D12, DirectX Raytracing Tier 1.1 is supported.
        };

        /** Create a new device.
            \param[in] pWindow a previously-created window object
            \param[in] desc Device configuration descriptor.
            \return nullptr if the function failed, otherwise a new device object
        */
        static SharedPtr create(Window::SharedPtr& pWindow, const Desc& desc);

        /** Acts as the destructor for Device. Some resources use gpDevice in their cleanup.
            Cleaning up the SharedPtr directly would clear gpDevice before calling destructors.
        */
        void cleanup();

        /** Enable/disable vertical sync
        */
        void toggleVSync(bool enable);

        /** Check if the window is occluded
        */
        bool isWindowOccluded() const;

        /** Get the FBO object associated with the swap-chain.
            This can change each frame, depending on the API used
        */
        Fbo::SharedPtr getSwapChainFbo() const;

        /** Get the default render-context.
            The default render-context is managed completely by the device. The user should just queue commands into it, the device will take care of allocation, submission and synchronization
        */
        RenderContext* getRenderContext() const { return mpRenderContext.get(); }

        /** Get the command queue handle
        */
        CommandQueueHandle getCommandQueueHandle(LowLevelContextData::CommandQueueType type, uint32_t index) const;

        /** Get the API queue type.
            \return API queue type, or throws an exception if type is unknown.
        */
        ApiCommandQueueType getApiCommandQueueType(LowLevelContextData::CommandQueueType type) const;

        /** Get the native API handle
        */
        const DeviceHandle& getApiHandle() { return mApiHandle; }

        /** Present the back-buffer to the window
        */
        void present();

        /** Flushes pipeline, releases resources, and blocks until completion
        */
        void flushAndSync();

        /** Check if vertical sync is enabled
        */
        bool isVsyncEnabled() const { return mDesc.enableVsync; }

        /** Resize the swap-chain
            \return A new FBO object
        */
        Fbo::SharedPtr resizeSwapChain(uint32_t width, uint32_t height);

        /** Get the desc
        */
        const Desc& getDesc() const { return mDesc; }

        /** Create a new query heap.
            \param[in] type Type of queries.
            \param[in] count Number of queries.
            \return New query heap.
        */
        std::weak_ptr<QueryHeap> createQueryHeap(QueryHeap::Type type, uint32_t count);

        const DescriptorPool::SharedPtr& getCpuDescriptorPool() const { return mpCpuDescPool; }
        const DescriptorPool::SharedPtr& getGpuDescriptorPool() const { return mpGpuDescPool; }
        const GpuMemoryHeap::SharedPtr& getUploadHeap() const { return mpUploadHeap; }
        void releaseResource(ApiObjectHandle pResource);
        double getGpuTimestampFrequency() const { return mGpuTimestampFrequency; } // ms/tick

        /** Check if features are supported by the device
        */
        bool isFeatureSupported(SupportedFeatures flags) const;

    private:
        static constexpr uint32_t kSwapChainBuffersCount = 3;
        struct ResourceRelease
        {
            size_t frameID;
            ApiObjectHandle pApiObject;
        };
        std::queue<ResourceRelease> mDeferredReleases;

        uint32_t mCurrentBackBufferIndex;
        Fbo::SharedPtr mpSwapChainFbos[kSwapChainBuffersCount];

        Device(Window::SharedPtr pWindow, const Desc& desc) : mpWindow(pWindow), mDesc(desc) {}
        bool init();
        void executeDeferredReleases();
        void releaseFboData();
        bool updateDefaultFBO(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat);

        Desc mDesc;
        ApiHandle mApiHandle;
        GpuMemoryHeap::SharedPtr mpUploadHeap;
        DescriptorPool::SharedPtr mpCpuDescPool;
        DescriptorPool::SharedPtr mpGpuDescPool;
        bool mIsWindowOccluded = false;
        GpuFence::SharedPtr mpFrameFence;

        Window::SharedPtr mpWindow;
        DeviceApiData* mpApiData;
        RenderContext::SharedPtr mpRenderContext;
        size_t mFrameID = 0;
        std::list<QueryHeap::SharedPtr> mTimestampQueryHeaps;
        double mGpuTimestampFrequency;
        std::vector<CommandQueueHandle> mCmdQueues[kQueueTypeCount];

        SupportedFeatures mSupportedFeatures = SupportedFeatures::None;

        // API specific functions
        bool getApiFboData(uint32_t width, uint32_t height, ResourceFormat colorFormat, ResourceFormat depthFormat, ResourceHandle apiHandles[kSwapChainBuffersCount], uint32_t& currentBackBufferIndex);
        void destroyApiObjects();
        void apiPresent();
        bool apiInit();
        bool createSwapChain(ResourceFormat colorFormat);
        void apiResizeSwapChain(uint32_t width, uint32_t height, ResourceFormat colorFormat);
        void toggleFullScreen(bool fullscreen);
    };

    extern Device::SharedPtr gpDevice;

    enum_class_operators(Device::SupportedFeatures);
}
