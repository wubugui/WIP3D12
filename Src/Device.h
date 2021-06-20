#pragma once
#include <array>
#include <memory>

#include "Formats.h"
#include "GraphicsContext.h"
#include "Application.h"
namespace WIP3D
{
#ifdef _DEBUG
#define DEFAULT_ENABLE_DEBUG_LAYER true
#else
#define DEFAULT_ENABLE_DEBUG_LAYER false
#endif

    /** Low level framebuffer object.
        This class abstracts the API's framebuffer creation and management.
    */
    class Fbo : public std::enable_shared_from_this<Fbo>
    {
    public:
        using SharedPtr = std::shared_ptr<Fbo>;
        using SharedConstPtr = std::shared_ptr<const Fbo>;
        using ApiHandle = FboHandle;

        class Desc
        {
        public:
            Desc();

            /** Set a render target to be a color target.
                \param[in] rtIndex Index of render target
                \param[in] format Texture resource format
                \param[in] allowUav Whether the resource can be a UAV
            */
            Desc& setColorTarget(uint32_t rtIndex, ResourceFormat format, bool allowUav = false) { mColorTargets[rtIndex] = TargetDesc(format, allowUav); return *this; }

            /** Set the format of the depth-stencil target.
                \param[in] format Texture resource format
                \param[in] allowUav Whether the resource can be a UAV
            */
            Desc& setDepthStencilTarget(ResourceFormat format, bool allowUav = false) { mDepthStencilTarget = TargetDesc(format, allowUav); return *this; }

            /** Set the resource sample count.
            */
            Desc& setSampleCount(uint32_t sampleCount) { mSampleCount = sampleCount ? sampleCount : 1; return *this; }

            /** Get the resource format of a render target
            */
            ResourceFormat getColorTargetFormat(uint32_t rtIndex) const { return mColorTargets[rtIndex].format; }

            /** Get whether a color target resource can be a UAV.
            */
            bool isColorTargetUav(uint32_t rtIndex) const { return mColorTargets[rtIndex].allowUav; }

            /** Get the depth-stencil resource format.
            */
            ResourceFormat getDepthStencilFormat() const { return mDepthStencilTarget.format; }

            /** Get whether depth-stencil resource can be a UAV.
            */
            bool isDepthStencilUav() const { return mDepthStencilTarget.allowUav; }

            /** Get sample count of the targets.
            */
            uint32_t getSampleCount() const { return mSampleCount; }

            /** Comparison operator
            */
            bool operator==(const Desc& other) const;

        private:
            struct TargetDesc
            {
                TargetDesc() = default;
                TargetDesc(ResourceFormat f, bool uav) : format(f), allowUav(uav) {}
                ResourceFormat format = ResourceFormat::Unknown;
                bool allowUav = false;

                bool operator==(const TargetDesc& other) const { return (format == other.format) && (allowUav == other.allowUav); }

                bool operator!=(const TargetDesc& other) const { return !(*this == other); }
            };

            std::vector<TargetDesc> mColorTargets;
            TargetDesc mDepthStencilTarget;
            uint32_t mSampleCount = 1;
        };

        /** Used to tell some functions to attach all array slices of a specific mip-level.
        */
        static const uint32_t kAttachEntireMipLevel = uint32_t(-1);

        /** Destructor. Releases the API object
        */
        ~Fbo();

        /** Get a FBO representing the default framebuffer object
        */
        static SharedPtr getDefault();

        /** Create a new empty FBO.
            \return A new object, or throws an exception if creation failed.
        */
        static SharedPtr create();

        /** Create an FBO from a list of textures. It will bind mip 0 and the all of the array slices.
            \param[in] colors A vector with color textures. The index in the vector corresponds to the render target index in the shader. You can use nullptr for unused indices.
            \param[in] depth An optional depth buffer texture.
            \return A new object. An exception is thrown if creation failed, for example due to texture size mismatch, bind flags issues, illegal formats, etc.
        */
        static SharedPtr create(const std::vector<Texture::SharedPtr>& colors, const Texture::SharedPtr& pDepth = nullptr);

        /** Create a color-only 2D framebuffer.
            \param[in] width Width of the render targets.
            \param[in] height Height of the render targets.
            \param[in] fboDesc Struct specifying the frame buffer's attachments and formats.
            \param[in] arraySize Optional. The number of array slices in the texture.
            \param[in] mipLevels Optional. The number of mip levels to create. You can use Texture#kMaxPossible to create the entire chain.
            \return A new object. An exception is thrown if creation failed, for example due to invalid parameters.
        */
        static SharedPtr create2D(uint32_t width, uint32_t height, const Desc& fboDesc, uint32_t arraySize = 1, uint32_t mipLevels = 1);

        /** Create a color-only cubemap framebuffer.
            \param[in] width width of the render targets.
            \param[in] height height of the render targets.
            \param[in] fboDesc Struct specifying the frame buffer's attachments and formats.
            \param[in] arraySize Optional. The number of cubes in the texture.
            \param[in] mipLevels Optional. The number of mip levels to create. You can use Texture#kMaxPossible to create the entire chain.
            \return A new object. An exception is thrown if creation failed, for example due to invalid parameters.
        */
        static SharedPtr createCubemap(uint32_t width, uint32_t height, const Desc& fboDesc, uint32_t arraySize = 1, uint32_t mipLevels = 1);

        /** Creates an FBO with a single color texture (single mip, single array slice), and optionally a depth buffer.
            \param[in] width Width of the render targets.
            \param[in] height Height of the render targets.
            \param[in] color The color format.
            \param[in] depth The depth-format. If a depth-buffer is not required, use ResourceFormat::Unknown.
            \return A new object. An exception is thrown if creation failed, for example due to invalid parameters.
        */
        static SharedPtr create2D(uint32_t width, uint32_t height, ResourceFormat color, ResourceFormat depth = ResourceFormat::Unknown);

        /** Attach a depth-stencil texture.
            An exception is thrown if the texture can't be used as a depth-buffer (usually a format or bind flags issue).
            \param pDepthStencil The depth-stencil texture.
            \param mipLevel The selected mip-level to attach.
            \param firstArraySlice The first array-slice to bind
            \param arraySize The number of array sliced to bind, or Fbo#kAttachEntireMipLevel to attach the range [firstArraySlice, pTexture->getArraySize()]
        */
        void attachDepthStencilTarget(const Texture::SharedPtr& pDepthStencil, uint32_t mipLevel = 0, uint32_t firstArraySlice = 0, uint32_t arraySize = kAttachEntireMipLevel);

        /** Attach a color texture.
            An exception is thrown if the texture can't be used as a color-target (usually a format or bind flags issue).
            \param pColorTexture The color texture.
            \param rtIndex The render-target index to attach the texture to.
            \param mipLevel The selected mip-level to attach.
            \param firstArraySlice The first array-slice to bind
            \param arraySize The number of array sliced to bind, or Fbo#kAttachEntireMipLevel to attach the range [firstArraySlice, pTexture->getArraySize()]
        */
        void attachColorTarget(const Texture::SharedPtr& pColorTexture, uint32_t rtIndex, uint32_t mipLevel = 0, uint32_t firstArraySlice = 0, uint32_t arraySize = kAttachEntireMipLevel);

        /** Get the object's API handle.
        */
        const ApiHandle& getApiHandle() const;

        /** Get the maximum number of color targets
        */
        static uint32_t getMaxColorTargetCount();

        /** Get an attached color texture. If no texture is attached will return nullptr.
        */
        Texture::SharedPtr getColorTexture(uint32_t index) const;

        /** Get the attached depth-stencil texture, or nullptr if no texture is attached.
        */
        const Texture::SharedPtr& getDepthStencilTexture() const;

        /** Get the width of the FBO
        */
        uint32_t getWidth() const { finalize(); return mWidth; }

        /** Get the height of the FBO
        */
        uint32_t getHeight() const { finalize(); return mHeight; }

        /** Get the sample-count of the FBO
        */
        uint32_t getSampleCount() const { finalize(); return mpDesc->getSampleCount(); }

        /** Get the FBO format descriptor
        */
        const Desc& getDesc() const { finalize();  return *mpDesc; }

        /** Get a depth-stencil view to the depth-stencil target.
        */
        DepthStencilView::SharedPtr getDepthStencilView() const;

        /** Get a render target view to a color target.
        */
        RenderTargetView::SharedPtr getRenderTargetView(uint32_t rtIndex) const;

        struct SamplePosition
        {
            int8_t xOffset = 0;
            int8_t yOffset = 0;
        };

        /**  Configure the sample positions used by multi-sampled buffers.
            \param[in] samplesPerPixel The number of samples-per-pixel. This value has to match the FBO's sample count
            \param[in] pixelCount the number if pixels the sample pattern is specified for
            \param[in] positions The sample positions. (0,0) is a pixel's center. The size of this array should be samplesPerPixel*pixelCount
            To reset the positions to their original location pass `nullptr` for positions
        */
        void setSamplePositions(uint32_t samplesPerPixel, uint32_t pixelCount, const SamplePosition positions[]);

        /** Get the sample positions
        */
        const std::vector<SamplePosition> getSamplePositions() const { return mSamplePositions; }

        /** Get the number of pixels the sample positions are configured for
        */
        uint32_t getSamplePositionsPixelCount() const { return mSamplePositionsPixelCount; }

        struct Attachment
        {
            Texture::SharedPtr pTexture = nullptr;
            uint32_t mipLevel = 0;
            uint32_t arraySize = 1;
            uint32_t firstArraySlice = 0;
        };

        struct DescHash
        {
            std::size_t operator()(const Desc& d) const;
        };

    private:
        static std::unordered_set<Desc, DescHash> sDescs;

        bool verifyAttachment(const Attachment& attachment) const;
        bool calcAndValidateProperties() const;

        void applyColorAttachment(uint32_t rtIndex);
        void applyDepthAttachment();
        void initApiHandle() const;

        /** Validates that the framebuffer attachments are OK. Throws an exception on error.
            This function causes the actual HW resources to be generated (RTV/DSV).
        */
        void finalize() const;

        Fbo();
        std::vector<Attachment> mColorAttachments;
        std::vector<SamplePosition> mSamplePositions;
        uint32_t mSamplePositionsPixelCount = 0;

        Attachment mDepthStencil;

        mutable Desc mTempDesc;
        mutable const Desc* mpDesc = nullptr;
        mutable uint32_t mWidth = (uint32_t)-1;
        mutable uint32_t mHeight = (uint32_t)-1;
        mutable uint32_t mDepth = (uint32_t)-1;
        mutable bool mHasDepthAttachment = false;
        mutable bool mIsLayered = false;
        mutable bool mIsZeroAttachment = false;

        mutable ApiHandle mApiHandle = {};
        void* mpPrivateData = nullptr;
    };

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
#ifdef FALCOR_VK
        uint32_t getVkMemoryType(GpuMemoryHeap::Type falcorType, uint32_t memoryTypeBits) const;
        const VkPhysicalDeviceLimits& getPhysicalDeviceLimits() const;
        uint32_t  getDeviceVendorID() const;
#endif
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
