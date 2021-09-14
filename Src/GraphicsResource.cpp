#pragma once
#include <memory>
#include <unordered_map>
#include <thread>
#include <mutex>
#include "Common.h"
#include "Formats.h"
#include "GraphicsResView.h"
#include "Vector3.h"
#include "Common/Logger.h"
#include "Vector4.h"
#include "Vector2.h"
#include "GPUMemory.h"
#include "GraphicsResource.h"
#include "Device.h"

namespace WIP3D
{
    // Resources
    Resource::~Resource() = default;

    const std::string to_string(Resource::Type type)
    {
#define type_2_string(a) case Resource::Type::a: return #a;
        switch (type)
        {
            type_2_string(Buffer);
            type_2_string(Texture1D);
            type_2_string(Texture2D);
            type_2_string(Texture3D);
            type_2_string(TextureCube);
            type_2_string(Texture2DMultisample);
        default:
            should_not_get_here();
            return "";
        }
#undef type_2_string
    }

    const std::string to_string(Resource::State state)
    {
        if (state == Resource::State::Common)
        {
            return "Common";
        }
        std::string s;
#define state_to_str(f_) if (state == Resource::State::f_) {return #f_; }

        state_to_str(Common);
        state_to_str(VertexBuffer);
        state_to_str(ConstantBuffer);
        state_to_str(IndexBuffer);
        state_to_str(RenderTarget);
        state_to_str(UnorderedAccess);
        state_to_str(DepthStencil);
        state_to_str(ShaderResource);
        state_to_str(StreamOut);
        state_to_str(IndirectArg);
        state_to_str(CopyDest);
        state_to_str(CopySource);
        state_to_str(ResolveDest);
        state_to_str(ResolveSource);
        state_to_str(Present);
        state_to_str(Predication);
        state_to_str(NonPixelShader);
#ifdef WIP_D3D12
        state_to_str(AccelerationStructure);
#endif
#undef state_to_str
        return s;
    }

    void Resource::invalidateViews() const
    {
        mSrvs.clear();
        mUavs.clear();
        mRtvs.clear();
        mDsvs.clear();
    }

    Resource::State Resource::getGlobalState() const
    {
        if (mState.isGlobal == false)
        {
            LOG_WARN("Resource::getGlobalState() - the resource doesn't have a global state. The subresoruces are in a different state, use getSubResourceState() instead");
            return State::Undefined;
        }
        return mState.global;
    }

    Resource::State Resource::getSubresourceState(uint32_t arraySlice, uint32_t mipLevel) const
    {
        const Texture* pTexture = dynamic_cast<const Texture*>(this);
        if (pTexture)
        {
            uint32_t subResource = pTexture->getSubresourceIndex(arraySlice, mipLevel);
            return (mState.isGlobal) ? mState.global : mState.perSubresource[subResource];
        }
        else
        {
            LOG_WARN("Calling Resource::getSubresourceState() on an object that is not a texture. This call is invalid, use Resource::getGlobalState() instead");
            assert(mState.isGlobal);
            return mState.global;
        }
    }

    void Resource::setGlobalState(State newState) const
    {
        mState.isGlobal = true;
        mState.global = newState;
    }

    void Resource::setSubresourceState(uint32_t arraySlice, uint32_t mipLevel, State newState) const
    {
        const Texture* pTexture = dynamic_cast<const Texture*>(this);
        if (pTexture == nullptr)
        {
            LOG_WARN("Calling Resource::setSubresourceState() on an object that is not a texture. This is invalid. Ignoring call");
            return;
        }

        // If we are transitioning from a global to local state, initialize the subresource array
        if (mState.isGlobal)
        {
            std::fill(mState.perSubresource.begin(), mState.perSubresource.end(), mState.global);
        }
        mState.isGlobal = false;
        mState.perSubresource[pTexture->getSubresourceIndex(arraySlice, mipLevel)] = newState;
    }

    namespace
    {
        Buffer::SharedPtr createStructuredFromType(
            const ReflectionType* pType,
            const std::string& varName,
            uint32_t elementCount,
            ResourceBindFlags bindFlags,
            Buffer::CpuAccess cpuAccess,
            const void* pInitData,
            bool createCounter)
        {
            const ReflectionResourceType* pResourceType = pType->unwrapArray()->asResourceType();
            if (!pResourceType || pResourceType->getType() != ReflectionResourceType::Type::StructuredBuffer)
            {
                throw std::exception(("Can't create a structured buffer from the variable '" + varName + "'. The variable is not a structured buffer.").c_str());
            }

            assert(pResourceType->getSize() <= std::numeric_limits<uint32_t>::max());
            return Buffer::createStructured((uint32_t)pResourceType->getSize(), elementCount, bindFlags, cpuAccess, pInitData, createCounter);
        }
    }

    size_t getBufferDataAlignment(const Buffer* pBuffer);
    void* mapBufferApi(const Buffer::ApiHandle& apiHandle, size_t size);

    Buffer::Buffer(size_t size, BindFlags bindFlags, CpuAccess cpuAccess)
        : Resource(Type::Buffer, bindFlags, size)
        , mCpuAccess(cpuAccess)
    {
        // Check that buffer size is within 4GB limit. Larger buffers are currently not well supported in D3D12.
        // TODO: Revisit this check in the future.
        if (size > (1ull << 32))
        {
            logWarning("Creating GPU buffer of size " + std::to_string(size) + " bytes. Buffers above 4GB are not currently well supported.");
        }
    }

    Buffer::SharedPtr Buffer::create(size_t size, BindFlags bindFlags, CpuAccess cpuAccess, const void* pInitData)
    {
        Buffer::SharedPtr pBuffer = SharedPtr(new Buffer(size, bindFlags, cpuAccess));
        pBuffer->apiInit(pInitData != nullptr);
        if (pInitData) pBuffer->setBlob(pInitData, 0, size);
        pBuffer->mElementCount = uint32_t(size);
        return pBuffer;
    }

    Buffer::SharedPtr Buffer::createTyped(ResourceFormat format, uint32_t elementCount, BindFlags bindFlags, CpuAccess cpuAccess, const void* pInitData)
    {
        size_t size = (size_t)elementCount * getFormatBytesPerBlock(format);
        SharedPtr pBuffer = create(size, bindFlags, cpuAccess, pInitData);
        assert(pBuffer);

        pBuffer->mFormat = format;
        pBuffer->mElementCount = elementCount;
        return pBuffer;
    }

    Buffer::SharedPtr Buffer::createStructured(
        uint32_t structSize,
        uint32_t elementCount,
        ResourceBindFlags bindFlags,
        CpuAccess cpuAccess,
        const void* pInitData,
        bool createCounter)
    {
        size_t size = (size_t)structSize * elementCount;
        Buffer::SharedPtr pBuffer = create(size, bindFlags, cpuAccess, pInitData);
        assert(pBuffer);

        pBuffer->mElementCount = elementCount;
        pBuffer->mStructSize = structSize;
        static const uint32_t zero = 0;
        if (createCounter)
        {
            pBuffer->mpUAVCounter = Buffer::create(sizeof(uint32_t), Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, &zero);
        }
        return pBuffer;
    }

    Buffer::SharedPtr Buffer::createStructured(
        const ShaderVar& shaderVar,
        uint32_t elementCount,
        ResourceBindFlags bindFlags,
        CpuAccess cpuAccess,
        const void* pInitData,
        bool createCounter)
    {
        return createStructuredFromType(shaderVar.getType().get(), "<Unknown ShaderVar>", elementCount, bindFlags, cpuAccess, pInitData, createCounter);
    }

    Buffer::SharedPtr Buffer::createStructured(
        const Program* pProgram,
        const std::string& name,
        uint32_t elementCount,
        ResourceBindFlags bindFlags,
        CpuAccess cpuAccess,
        const void* pInitData,
        bool createCounter)
    {
        const auto& pDefaultBlock = pProgram->getReflector()->getDefaultParameterBlock();
        const ReflectionVar* pVar = pDefaultBlock ? pDefaultBlock->getResource(name).get() : nullptr;
        if (pVar == nullptr)
        {
            throw std::exception(("Can't find a structured buffer named '" + name + "' in the program").c_str());
        }
        return createStructuredFromType(pVar->getType().get(), name, elementCount, bindFlags, cpuAccess, pInitData, createCounter);
    }

    Buffer::SharedPtr Buffer::aliasResource(Resource::SharedPtr pBaseResource, GpuAddress offset, size_t size, Resource::BindFlags bindFlags)
    {
        assert(pBaseResource->asBuffer()); // Only aliasing buffers for now
        CpuAccess cpuAccess = pBaseResource->asBuffer() ? pBaseResource->asBuffer()->getCpuAccess() : CpuAccess::None;
        if (cpuAccess != CpuAccess::None)
        {
            LOG_ERROR(("Buffer::aliasResource() - trying to alias a buffer with CpuAccess::" + to_string(cpuAccess) + " which is illegal. Aliased resource must have CpuAccess::None").c_str());
            return nullptr;
        }

        if ((pBaseResource->getBindFlags() & bindFlags) != bindFlags)
        {
            LOG_ERROR(
                ("Buffer::aliasResource() - requested buffer bind-flags don't match the aliased resource bind flags.\nRequested = " + to_string(bindFlags) + "\nAliased = " + to_string(pBaseResource->getBindFlags())).c_str()
            );
            return nullptr;
        }

        if (offset >= pBaseResource->getSize() || (offset + size) >= pBaseResource->getSize())
        {
            LOG_ERROR(("Buffer::aliasResource() - requested offset and size don't fit inside the alias resource dimensions. Requested size = " +
                std::to_string(size) + ", offset = " + std::to_string(offset) + ". Aliased resource size = " + std::to_string(pBaseResource->getSize())).c_str());
            return nullptr;
        }

        SharedPtr pBuffer = SharedPtr(new Buffer(size, bindFlags, CpuAccess::None));
        pBuffer->mpAliasedResource = pBaseResource;
        pBuffer->mApiHandle = pBaseResource->getApiHandle();
        pBuffer->mGpuVaOffset = offset;
        return pBuffer;
    }

    Buffer::SharedPtr Buffer::createFromApiHandle(ApiHandle handle, size_t size, Resource::BindFlags bindFlags, CpuAccess cpuAccess)
    {
        assert(handle);
        Buffer::SharedPtr pBuffer = SharedPtr(new Buffer(size, bindFlags, cpuAccess));
        pBuffer->mApiHandle = handle;
        return pBuffer;
    }

    Buffer::~Buffer()
    {
        if (mpAliasedResource) return;

        if (mDynamicData.pResourceHandle)
        {
            gpDevice->getUploadHeap()->release(mDynamicData);
        }
        else
        {
            gpDevice->releaseResource(mApiHandle);
        }
    }

#if 1
    namespace
    {
        Texture::BindFlags updateBindFlags(Texture::BindFlags flags, bool hasInitData, uint32_t mipLevels, ResourceFormat format, const std::string& texType)
        {
            if ((mipLevels == Texture::kMaxPossible) && hasInitData)
            {
                flags |= Texture::BindFlags::RenderTarget;
            }

            Texture::BindFlags supported = getFormatBindFlags(format);
            supported |= ResourceBindFlags::Shared;
            if ((flags & supported) != flags)
            {
                LOG_ERROR(("Error when creating " + texType + " of format " + to_string(format) + ". The requested bind-flags are not supported.\n"
                    "Requested = (" + to_string(flags) + "), supported = (" + to_string(supported) + ").\n\n"
                    "The texture will be created only with the supported bind flags, which may result in a crash or a rendering error.").c_str());
                flags = flags & supported;
            }

            return flags;
        }
    }
    Texture::SharedPtr Texture::createFromApiHandle(ApiHandle handle, Type type, uint32_t width, uint32_t height, uint32_t depth, ResourceFormat format, uint32_t sampleCount, uint32_t arraySize, uint32_t mipLevels, State initState, BindFlags bindFlags)
    {
        assert(handle);
        switch (type)
        {
        case Resource::Type::Texture1D:
            assert(height == 1 && depth == 1 && sampleCount == 1);
            break;
        case Resource::Type::Texture2D:
            assert(depth == 1 && sampleCount == 1);
            break;
        case Resource::Type::Texture2DMultisample:
            assert(depth == 1);
            break;
        case Resource::Type::Texture3D:
            assert(sampleCount == 1);
            break;
        case Resource::Type::TextureCube:
            assert(depth == 1 && sampleCount == 1);
            break;
        default:
            should_not_get_here();
            break;
        }
        Texture::SharedPtr pTexture = SharedPtr(new Texture(width, height, depth, arraySize, mipLevels, sampleCount, format, type, bindFlags));
        pTexture->mApiHandle = handle;
        pTexture->mState.global = initState;
        pTexture->mState.isGlobal = true;
        return pTexture;
    }

    Texture::SharedPtr Texture::create1D(uint32_t width, ResourceFormat format, uint32_t arraySize, uint32_t mipLevels, const void* pData, BindFlags bindFlags)
    {
        bindFlags = updateBindFlags(bindFlags, pData != nullptr, mipLevels, format, "Texture1D");
        Texture::SharedPtr pTexture = SharedPtr(new Texture(width, 1, 1, arraySize, mipLevels, 1, format, Type::Texture1D, bindFlags));
        pTexture->apiInit(pData, (mipLevels == kMaxPossible));
        return pTexture;
    }

    Texture::SharedPtr Texture::create2D(uint32_t width, uint32_t height, ResourceFormat format, uint32_t arraySize, uint32_t mipLevels, const void* pData, BindFlags bindFlags)
    {
        bindFlags = updateBindFlags(bindFlags, pData != nullptr, mipLevels, format, "Texture2D");
        Texture::SharedPtr pTexture = SharedPtr(new Texture(width, height, 1, arraySize, mipLevels, 1, format, Type::Texture2D, bindFlags));
        pTexture->apiInit(pData, (mipLevels == kMaxPossible));
        return pTexture;
    }

    Texture::SharedPtr Texture::create3D(uint32_t width, uint32_t height, uint32_t depth, ResourceFormat format, uint32_t mipLevels, const void* pData, BindFlags bindFlags, bool isSparse)
    {
        bindFlags = updateBindFlags(bindFlags, pData != nullptr, mipLevels, format, "Texture3D");
        Texture::SharedPtr pTexture = SharedPtr(new Texture(width, height, depth, 1, mipLevels, 1, format, Type::Texture3D, bindFlags));
        pTexture->apiInit(pData, (mipLevels == kMaxPossible));
        return pTexture;
    }

    Texture::SharedPtr Texture::createCube(uint32_t width, uint32_t height, ResourceFormat format, uint32_t arraySize, uint32_t mipLevels, const void* pData, BindFlags bindFlags)
    {
        bindFlags = updateBindFlags(bindFlags, pData != nullptr, mipLevels, format, "TextureCube");
        Texture::SharedPtr pTexture = SharedPtr(new Texture(width, height, 1, arraySize, mipLevels, 1, format, Type::TextureCube, bindFlags));
        pTexture->apiInit(pData, (mipLevels == kMaxPossible));
        return pTexture;
    }

    Texture::SharedPtr Texture::create2DMS(uint32_t width, uint32_t height, ResourceFormat format, uint32_t sampleCount, uint32_t arraySize, BindFlags bindFlags)
    {
        bindFlags = updateBindFlags(bindFlags, false, 1, format, "Texture2DMultisample");
        Texture::SharedPtr pTexture = SharedPtr(new Texture(width, height, 1, arraySize, 1, sampleCount, format, Type::Texture2DMultisample, bindFlags));
        pTexture->apiInit(nullptr, false);
        return pTexture;
    }

#include "Common/FileSystem.h"

    Texture::SharedPtr Texture::createFromFile(const std::string& filename, bool generateMipLevels, bool loadAsSrgb, Texture::BindFlags bindFlags)
    {
        std::string fullpath = g_filesystem->get_full_path(filename);
        if (!g_filesystem->file_exists(filename))
        {
            LOG_WARN(("Error when loading image file. Can't find image file '" + filename + "'").c_str());
            return nullptr;
        }
        string extension = g_filesystem->get_extension(filename);
        

        Texture::SharedPtr pTex;
        if (extension == ".dds")
        {
            LOG_WARN(("now cannt read dds '" + filename + "'").c_str());
            return nullptr;
            // TODO:Load DDS
            // pTex = ImageIO::loadTextureFromDDS(filename, loadAsSrgb);
        }
        else
        {
            LOG_WARN(("now cannt read any picture '" + filename + "'").c_str());
            return nullptr;
#if 0
            Bitmap::UniqueConstPtr pBitmap = Bitmap::createFromFile(fullpath, kTopDown);
            if (pBitmap)
            {
                ResourceFormat texFormat = pBitmap->getFormat();
                if (loadAsSrgb)
                {
                    texFormat = linearToSrgbFormat(texFormat);
                }

                pTex = Texture::create2D(pBitmap->getWidth(), pBitmap->getHeight(), texFormat, 1, generateMipLevels ? Texture::kMaxPossible : 1, pBitmap->getData(), bindFlags);
            }
#endif
        }

        if (pTex != nullptr)
        {
            pTex->setSourceFilename(fullpath);
        }

        return pTex;
    }

    Texture::Texture(uint32_t width, uint32_t height, uint32_t depth, uint32_t arraySize, uint32_t mipLevels, uint32_t sampleCount, ResourceFormat format, Type type, BindFlags bindFlags)
        : Resource(type, bindFlags, 0), mWidth(width), mHeight(height), mDepth(depth), mMipLevels(mipLevels), mSampleCount(sampleCount), mArraySize(arraySize), mFormat(format)
    {
        assert(width > 0 && height > 0 && depth > 0);
        assert(arraySize > 0 && mipLevels > 0 && sampleCount > 0);
        assert(format != ResourceFormat::Unknown);

        if (mMipLevels == kMaxPossible)
        {
            uint32_t dims = width | height | depth;
            mMipLevels = bitScanReverse(dims) + 1;
        }
        mState.perSubresource.resize(mMipLevels * mArraySize, mState.global);
    }

    template<typename ViewClass>
    using CreateFuncType = std::function<typename ViewClass::SharedPtr(Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize)>;

    template<typename ViewClass, typename ViewMapType>
    typename ViewClass::SharedPtr findViewCommon(Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize, ViewMapType& viewMap, CreateFuncType<ViewClass> createFunc)
    {
        uint32_t resMipCount = 1;
        uint32_t resArraySize = 1;

        resArraySize = pTexture->getArraySize();
        resMipCount = pTexture->getMipCount();

        if (firstArraySlice >= resArraySize)
        {
            logWarning("First array slice is OOB when creating resource view. Clamping");
            firstArraySlice = resArraySize - 1;
        }

        if (mostDetailedMip >= resMipCount)
        {
            logWarning("Most detailed mip is OOB when creating resource view. Clamping");
            mostDetailedMip = resMipCount - 1;
        }

        if (mipCount == Resource::kMaxPossible)
        {
            mipCount = resMipCount - mostDetailedMip;
        }
        else if (mipCount + mostDetailedMip > resMipCount)
        {
            logWarning("Mip count is OOB when creating resource view. Clamping");
            mipCount = resMipCount - mostDetailedMip;
        }

        if (arraySize == Resource::kMaxPossible)
        {
            arraySize = resArraySize - firstArraySlice;
        }
        else if (arraySize + firstArraySlice > resArraySize)
        {
            logWarning("Array size is OOB when creating resource view. Clamping");
            arraySize = resArraySize - firstArraySlice;
        }

        ResourceViewInfo view = ResourceViewInfo(mostDetailedMip, mipCount, firstArraySlice, arraySize);

        if (viewMap.find(view) == viewMap.end())
        {
            viewMap[view] = createFunc(pTexture, mostDetailedMip, mipCount, firstArraySlice, arraySize);
        }

        return viewMap[view];
    }

    DepthStencilView::SharedPtr Texture::getDSV(uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize)
    {
        auto createFunc = [](Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize)
        {
            return DepthStencilView::create(std::static_pointer_cast<Texture>(pTexture->shared_from_this()), mostDetailedMip, firstArraySlice, arraySize);
        };

        return findViewCommon<DepthStencilView>(this, mipLevel, 1, firstArraySlice, arraySize, mDsvs, createFunc);
    }

    UnorderedAccessView::SharedPtr Texture::getUAV(uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize)
    {
        auto createFunc = [](Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize)
        {
            return UnorderedAccessView::create(std::static_pointer_cast<Texture>(pTexture->shared_from_this()), mostDetailedMip, firstArraySlice, arraySize);
        };

        return findViewCommon<UnorderedAccessView>(this, mipLevel, 1, firstArraySlice, arraySize, mUavs, createFunc);
    }

    ShaderResourceView::SharedPtr Texture::getSRV()
    {
        return getSRV(0);
    }

    UnorderedAccessView::SharedPtr Texture::getUAV()
    {
        return getUAV(0);
    }

    RenderTargetView::SharedPtr Texture::getRTV(uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize)
    {
        auto createFunc = [](Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize)
        {
            return RenderTargetView::create(std::static_pointer_cast<Texture>(pTexture->shared_from_this()), mostDetailedMip, firstArraySlice, arraySize);
        };

        return findViewCommon<RenderTargetView>(this, mipLevel, 1, firstArraySlice, arraySize, mRtvs, createFunc);
    }

    ShaderResourceView::SharedPtr Texture::getSRV(uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize)
    {
        auto createFunc = [](Texture* pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize)
        {
            return ShaderResourceView::create(std::static_pointer_cast<Texture>(pTexture->shared_from_this()), mostDetailedMip, mipCount, firstArraySlice, arraySize);
        };

        return findViewCommon<ShaderResourceView>(this, mostDetailedMip, mipCount, firstArraySlice, arraySize, mSrvs, createFunc);
    }

    //void Texture::captureToFile(uint32_t mipLevel, uint32_t arraySlice, const std::string& filename, Bitmap::FileFormat format, Bitmap::ExportFlags exportFlags)

    void Texture::captureToFile(uint32_t mipLevel, uint32_t arraySlice, const std::string& filename)
    {
#if 0
        if (format == Bitmap::FileFormat::DdsFile)
        {
            throw std::exception("Texture::captureToFile does not yet support saving to DDS.");
        }

        assert(mType == Type::Texture2D);
        RenderContext* pContext = gpDevice->getRenderContext();
        // Handle the special case where we have an HDR texture with less then 3 channels
        FormatType type = getFormatType(mFormat);
        uint32_t channels = getFormatChannelCount(mFormat);
        std::vector<uint8_t> textureData;
        ResourceFormat resourceFormat = mFormat;

        if (type == FormatType::Float && channels < 3)
        {
            Texture::SharedPtr pOther = Texture::create2D(getWidth(mipLevel), getHeight(mipLevel), ResourceFormat::RGBA32Float, 1, 1, nullptr, ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource);
            pContext->blit(getSRV(mipLevel, 1, arraySlice, 1), pOther->getRTV(0, 0, 1));
            textureData = pContext->readTextureSubresource(pOther.get(), 0);
            resourceFormat = ResourceFormat::RGBA32Float;
        }
        else
        {
            uint32_t subresource = getSubresourceIndex(arraySlice, mipLevel);
            textureData = pContext->readTextureSubresource(this, subresource);
        }

        uint32_t width = getWidth(mipLevel);
        uint32_t height = getHeight(mipLevel);
        auto func = [=]()
        {
            Bitmap::saveImage(filename, width, height, format, exportFlags, resourceFormat, true, (void*)textureData.data());
        };

        Threading::dispatchTask(func);
#endif
    }

    void Texture::uploadInitData(const void* pData, bool autoGenMips)
    {
        // TODO: This is a hack to allow multi-threaded texture loading using AsyncTextureLoader.
        // Replace with something better.
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);

        assert(gpDevice);
        auto pRenderContext = gpDevice->getRenderContext();
        if (autoGenMips)
        {
            // Upload just the first mip-level
            size_t arraySliceSize = mWidth * mHeight * getFormatBytesPerBlock(mFormat);
            const uint8_t* pSrc = (uint8_t*)pData;
            uint32_t numFaces = (mType == Texture::Type::TextureCube) ? 6 : 1;
            for (uint32_t i = 0; i < mArraySize * numFaces; i++)
            {
                uint32_t subresource = getSubresourceIndex(i, 0);
                pRenderContext->updateSubresourceData(this, subresource, pSrc);
                pSrc += arraySliceSize;
            }
        }
        else
        {
            pRenderContext->updateTextureData(this, pData);
        }

        if (autoGenMips)
        {
            generateMips(gpDevice->getRenderContext());
            invalidateViews();
        }
    }

    void Texture::generateMips(RenderContext* pContext, bool minMaxMips)
    {
        if (mType != Type::Texture2D)
        {
            LOG_WARN("Texture::generateMips() was only tested with Texture2Ds");
        }
        // #OPTME: should blit support arrays?
        for (uint32_t m = 0; m < mMipLevels - 1; m++)
        {
            for (uint32_t a = 0; a < mArraySize; a++)
            {
                auto srv = getSRV(m, 1, a, 1);
                auto rtv = getRTV(m + 1, a, 1);
                if (!minMaxMips)
                {
                    pContext->blit(srv, rtv, uint4(-1), uint4(-1), Sampler::Filter::Linear);
                }
                else
                {
                    const Sampler::ReductionMode redModes[] = { Sampler::ReductionMode::Standard, Sampler::ReductionMode::Min, Sampler::ReductionMode::Max, Sampler::ReductionMode::Standard };
                    const float4 componentsTransform[] = { float4(1.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 1.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 1.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 1.0f) };
                    pContext->blit(srv, rtv, uint4(-1), uint4(-1), Sampler::Filter::Linear, redModes, componentsTransform);
                }
            }
        }

        if (mReleaseRtvsAfterGenMips)
        {
            // Releasing RTVs to free space on the heap.
            // We only do it once to handle the case that generateMips() was called during load.
            // If it was called more then once, the texture is probably dynamic and it's better to keep the RTVs around
            mRtvs.clear();
            mReleaseRtvsAfterGenMips = false;
        }
    }

    uint64_t Texture::getTexelCount() const
    {
        uint64_t count = 0;
        for (uint32_t i = 0; i < getMipCount(); i++)
        {
            uint64_t texelsInMip = (uint64_t)getWidth(i) * getHeight(i) * getDepth(i);
            assert(texelsInMip > 0);
            count += texelsInMip;
        }
        count *= getArraySize();
        assert(count > 0);
        return count;
    }

    uint64_t Texture::getTextureSizeInBytes() const
    {
        ID3D12DevicePtr pDevicePtr = gpDevice->getApiHandle();
        ID3D12ResourcePtr pTexResource = this->getApiHandle();

        D3D12_RESOURCE_ALLOCATION_INFO d3d12ResourceAllocationInfo;
        D3D12_RESOURCE_DESC desc = pTexResource->GetDesc();

        assert(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);

        d3d12ResourceAllocationInfo = pDevicePtr->GetResourceAllocationInfo(0, 1, &desc);
        assert(d3d12ResourceAllocationInfo.SizeInBytes > 0);
        return d3d12ResourceAllocationInfo.SizeInBytes;
    }
#endif
}
