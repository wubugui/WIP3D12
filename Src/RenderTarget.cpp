#include "RenderResource.h"
#include "rendertarget.h"


namespace WIP3D
{
    namespace
    {
        bool checkAttachmentParams(const Texture* pTexture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize, bool isDepthAttachment)
        {
#ifndef _DEBUG
            return true;
#endif
            if (pTexture == nullptr)
            {
                return true;
            }

            if (mipLevel >= pTexture->getMipCount())
            {
                LOG_ERROR("Error when attaching texture to FBO. Requested mip-level is out-of-bound.");
                return false;
            }

            if (arraySize != Fbo::kAttachEntireMipLevel)
            {
                if (arraySize == 0)
                {
                    LOG_ERROR("Error when attaching texture to FBO. Requested to attach zero array slices");
                    return false;
                }

                if (pTexture->getType() == Texture::Type::Texture3D)
                {
                    if (arraySize + firstArraySlice > pTexture->getDepth())
                    {
                        LOG_ERROR("Error when attaching texture to FBO. Requested depth-index is out-of-bound.");
                        return false;
                    }
                }
                else
                {
                    if (arraySize + firstArraySlice > pTexture->getArraySize())
                    {
                        LOG_ERROR("Error when attaching texture to FBO. Requested array index is out-of-bound.");
                        return false;
                    }
                }
            }

            if (isDepthAttachment)
            {
                if (isDepthStencilFormat(pTexture->getFormat()) == false)
                {
                    LOG_ERROR("Error when attaching texture to FBO. Attaching to depth-stencil target, but resource has color format.");
                    return false;
                }

                if ((pTexture->getBindFlags() & Texture::BindFlags::DepthStencil) == Texture::BindFlags::None)
                {
                    LOG_ERROR("Error when attaching texture to FBO. Attaching to depth-stencil target, the texture wasn't create with the DepthStencil bind flag");
                    return false;

                }
            }
            else
            {
                if (isDepthStencilFormat(pTexture->getFormat()))
                {
                    LOG_ERROR("Error when attaching texture to FBO. Attaching to color target, but resource has depth-stencil format.");
                    return false;
                }

                if ((pTexture->getBindFlags() & Texture::BindFlags::RenderTarget) == Texture::BindFlags::None)
                {
                    LOG_ERROR("Error when attaching texture to FBO. Attaching to color target, the texture wasn't create with the RenderTarget bind flag");
                    return false;

                }
            }

            return true;
        }

        bool checkParams(const std::string& Func, uint32_t width, uint32_t height, uint32_t arraySize, uint32_t mipLevels, uint32_t sampleCount)
        {
            std::string msg = "Fbo::" + Func + "() - ";
            std::string param;

            if (mipLevels == 0)      param = "mipLevels";
            else if (width == 0)     param = "width";
            else if (height == 0)    param = "height";
            else if (arraySize == 0) param = "arraySize";
            else
            {
                if (sampleCount > 1 && mipLevels > 1)
                {
                    LOG_ERROR(
                        (msg + "can't create multi-sampled texture with more than one mip-level. sampleCount = " + std::to_string(sampleCount) + ", mipLevels = " + std::to_string(mipLevels) + ".").c_str()
                    );
                    return false;
                }
                return true;
            }

            LOG_ERROR((msg + param + " can't be zero.").c_str());
            return false;
        }

        Texture::SharedPtr createTexture2D(uint32_t w, uint32_t h, ResourceFormat format, uint32_t sampleCount, uint32_t arraySize, uint32_t mipLevels, Texture::BindFlags flags)
        {
            if (format == ResourceFormat::Unknown)
            {
                LOG_ERROR("Can't create Texture2D with an unknown resource format");
                return nullptr;
            }

            Texture::SharedPtr pTex;
            if (sampleCount > 1)
            {
                pTex = Texture::create2DMS(w, h, format, sampleCount, arraySize, flags);
            }
            else
            {
                pTex = Texture::create2D(w, h, format, arraySize, mipLevels, nullptr, flags);
            }

            return pTex;
        }

        Texture::BindFlags getBindFlags(bool isDepth, bool allowUav)
        {
            Texture::BindFlags flags = Texture::BindFlags::ShaderResource;
            flags |= isDepth ? Texture::BindFlags::DepthStencil : Texture::BindFlags::RenderTarget;

            if (allowUav)
            {
                flags |= Texture::BindFlags::UnorderedAccess;
            }
            return flags;
        }

    };

    std::unordered_set<Fbo::Desc, Fbo::DescHash> Fbo::sDescs;

    size_t Fbo::DescHash::operator()(const Fbo::Desc& d) const
    {
        size_t hash = 0;
        std::hash<uint32_t> u32hash;
        std::hash<bool> bhash;
        for (uint32_t i = 0; i < getMaxColorTargetCount(); i++)
        {
            uint32_t format = (uint32_t)d.getColorTargetFormat(i);
            format <<= i;
            hash |= u32hash(format) >> i;
            hash |= bhash(d.isColorTargetUav(i)) << i;
        }

        uint32_t format = (uint32_t)d.getDepthStencilFormat();
        hash |= u32hash(format);
        hash |= bhash(d.isDepthStencilUav());
        hash |= u32hash(d.getSampleCount());

        return hash;
    }

    bool Fbo::Desc::operator==(const Fbo::Desc& other) const
    {
        if (mColorTargets.size() != other.mColorTargets.size()) return false;

        for (size_t i = 0; i < mColorTargets.size(); i++)
        {
            if (mColorTargets[i] != other.mColorTargets[i]) return false;
        }
        if (mDepthStencilTarget != other.mDepthStencilTarget) return false;
        if (mSampleCount != other.mSampleCount) return false;

        return true;
    }

    Fbo::Desc::Desc()
    {
        mColorTargets.resize(Fbo::getMaxColorTargetCount());
    }

    Fbo::SharedPtr Fbo::create()
    {
        return SharedPtr(new Fbo());
    }

    Fbo::SharedPtr Fbo::create(const std::vector<Texture::SharedPtr>& colors, const Texture::SharedPtr& pDepth)
    {
        auto pFbo = create();
        for (uint32_t i = 0; i < colors.size(); i++)
        {
            pFbo->attachColorTarget(colors[i], i);
        }
        if (pDepth)
        {
            pFbo->attachDepthStencilTarget(pDepth);
        }
        pFbo->finalize();
        return pFbo;
    }

    Fbo::SharedPtr Fbo::getDefault()
    {
        static Fbo::SharedPtr pDefault;
        if (pDefault == nullptr)
        {
            pDefault = Fbo::SharedPtr(new Fbo());
        }
        return pDefault;
    }

    void Fbo::attachDepthStencilTarget(const Texture::SharedPtr& pDepthStencil, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize)
    {
        if (checkAttachmentParams(pDepthStencil.get(), mipLevel, firstArraySlice, arraySize, true) == false)
        {
            throw std::exception("Can't attach depth-stencil texture to FBO. Invalid parameters.");
        }

        mpDesc = nullptr;
        mDepthStencil.pTexture = pDepthStencil;
        mDepthStencil.mipLevel = mipLevel;
        mDepthStencil.firstArraySlice = firstArraySlice;
        mDepthStencil.arraySize = arraySize;
        bool allowUav = false;
        if (pDepthStencil)
        {
            allowUav = ((pDepthStencil->getBindFlags() & Texture::BindFlags::UnorderedAccess) != Texture::BindFlags::None);
        }

        mTempDesc.setDepthStencilTarget(pDepthStencil ? pDepthStencil->getFormat() : ResourceFormat::Unknown, allowUav);
        applyDepthAttachment();
    }

    void Fbo::attachColorTarget(const Texture::SharedPtr& pTexture, uint32_t rtIndex, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize)
    {
        if (rtIndex >= mColorAttachments.size())
        {
            throw std::exception(("Error when attaching texture to FBO. Requested color index " + std::to_string(rtIndex) + ", but context only supports " + std::to_string(mColorAttachments.size()) + " targets").c_str());
        }
        if (checkAttachmentParams(pTexture.get(), mipLevel, firstArraySlice, arraySize, false) == false)
        {
            throw std::exception("Can't attach texture to FBO. Invalid parameters.");
        }

        mpDesc = nullptr;
        mColorAttachments[rtIndex].pTexture = pTexture;
        mColorAttachments[rtIndex].mipLevel = mipLevel;
        mColorAttachments[rtIndex].firstArraySlice = firstArraySlice;
        mColorAttachments[rtIndex].arraySize = arraySize;
        bool allowUav = false;
        if (pTexture)
        {
            allowUav = ((pTexture->getBindFlags() & Texture::BindFlags::UnorderedAccess) != Texture::BindFlags::None);
        }

        mTempDesc.setColorTarget(rtIndex, pTexture ? pTexture->getFormat() : ResourceFormat::Unknown, allowUav);
        applyColorAttachment(rtIndex);
    }

    bool Fbo::verifyAttachment(const Attachment& attachment) const
    {
        const Texture* pTexture = attachment.pTexture.get();
        if (pTexture)
        {
            // Calculate size
            if (mWidth == uint32_t(-1))
            {
                // First attachment in the FBO
                mTempDesc.setSampleCount(pTexture->getSampleCount());
                mIsLayered = (attachment.arraySize > 1);
            }
            
            mWidth = RBMath::get_min(mWidth, pTexture->getWidth(attachment.mipLevel));
            mHeight = RBMath::get_min(mHeight, pTexture->getHeight(attachment.mipLevel));
            mDepth = RBMath::get_min(mDepth, pTexture->getDepth(attachment.mipLevel));

            {
                if ((pTexture->getSampleCount() > mTempDesc.getSampleCount()) && isDepthStencilFormat(pTexture->getFormat()))
                {
                    // We're using target-independent raster (more depth samples than color samples).  This is OK.
                    mTempDesc.setSampleCount(pTexture->getSampleCount());
                    return true;
                }

                if (mTempDesc.getSampleCount() != pTexture->getSampleCount())
                {
                    LOG_ERROR("Error when validating FBO. Different sample counts in attachments\n");
                    return false;
                }


                if (mIsLayered != (attachment.arraySize > 1))
                {
                    LOG_ERROR("Error when validating FBO. Can't bind both layered and non-layered textures\n");
                    return false;
                }
            }
        }
        return true;
    }

    bool Fbo::calcAndValidateProperties() const
    {
        mWidth = (uint32_t)-1;
        mHeight = (uint32_t)-1;
        mDepth = (uint32_t)-1;
        mTempDesc.setSampleCount(uint32_t(-1));
        mIsLayered = false;

        // Check color
        for (const auto& attachment : mColorAttachments)
        {
            if (verifyAttachment(attachment) == false)
            {
                return false;
            }
        }

        // Check depth
        if (verifyAttachment(mDepthStencil) == false) return false;

        // In case there are sample positions, make sure they are valid
        if (mSamplePositions.size())
        {
            uint32_t expectedCount = mSamplePositionsPixelCount * mTempDesc.getSampleCount();
            if (expectedCount != mSamplePositions.size())
            {
                LOG_ERROR("Error when validating FBO. The sample-positions array-size has the wrong size.\n");
                return false;
            }
        }

        // Insert the attachment into the static array and initialize the address
        mpDesc = &(*(sDescs.insert(mTempDesc).first));

        return true;
    }

    Texture::SharedPtr Fbo::getColorTexture(uint32_t index) const
    {
        if (index >= mColorAttachments.size())
        {
            throw std::exception(("Can't get texture from FBO. Index is out of range. Requested " + std::to_string(index) + " but only " + std::to_string(mColorAttachments.size()) + " color slots are available.").c_str());
        }
        return mColorAttachments[index].pTexture;
    }

    const Texture::SharedPtr& Fbo::getDepthStencilTexture() const
    {
        return mDepthStencil.pTexture;
    }

    void Fbo::finalize() const
    {
        if (mpDesc == nullptr)
        {
            if (calcAndValidateProperties() == false)
            {
                throw std::exception("Can't finalize FBO. Invalid frame buffer object.");
            }
            initApiHandle();
        }
    }

    void Fbo::setSamplePositions(uint32_t samplesPerPixel, uint32_t pixelCount, const SamplePosition positions[])
    {
        if (positions)
        {
            mSamplePositions = std::vector<SamplePosition>(positions, positions + (samplesPerPixel * pixelCount));
            mSamplePositionsPixelCount = pixelCount;
        }
        else
        {
            mSamplePositionsPixelCount = 0;
            mSamplePositions.clear();
        }
    }

    Fbo::SharedPtr Fbo::create2D(uint32_t width, uint32_t height, const Fbo::Desc& fboDesc, uint32_t arraySize, uint32_t mipLevels)
    {
        uint32_t sampleCount = fboDesc.getSampleCount();
        if (checkParams("Create2D", width, height, arraySize, mipLevels, sampleCount) == false)
        {
            throw std::exception("Can't create 2D FBO. Invalid parameters.");
        }

        Fbo::SharedPtr pFbo = create();

        // Create the color targets
        for (uint32_t i = 0; i < Fbo::getMaxColorTargetCount(); i++)
        {
            if (fboDesc.getColorTargetFormat(i) != ResourceFormat::Unknown)
            {
                Texture::BindFlags flags = getBindFlags(false, fboDesc.isColorTargetUav(i));
                Texture::SharedPtr pTex = createTexture2D(width, height, fboDesc.getColorTargetFormat(i), sampleCount, arraySize, mipLevels, flags);
                pFbo->attachColorTarget(pTex, i, 0, 0, kAttachEntireMipLevel);
            }
        }

        if (fboDesc.getDepthStencilFormat() != ResourceFormat::Unknown)
        {
            Texture::BindFlags flags = getBindFlags(true, fboDesc.isDepthStencilUav());
            Texture::SharedPtr pDepth = createTexture2D(width, height, fboDesc.getDepthStencilFormat(), sampleCount, arraySize, mipLevels, flags);
            pFbo->attachDepthStencilTarget(pDepth, 0, 0, kAttachEntireMipLevel);
        }

        return pFbo;
    }

    Fbo::SharedPtr Fbo::createCubemap(uint32_t width, uint32_t height, const Desc& fboDesc, uint32_t arraySize, uint32_t mipLevels)
    {
        if (fboDesc.getSampleCount() > 1)
        {
            throw std::exception("Can't create cubemap FBO. Multisampled cubemap is not supported.");
        }
        if (checkParams("CreateCubemap", width, height, arraySize, mipLevels, 0) == false)
        {
            throw std::exception("Can't create cubemap FBO. Invalid parameters.");
        }

        Fbo::SharedPtr pFbo = create();

        // Create the color targets
        for (uint32_t i = 0; i < getMaxColorTargetCount(); i++)
        {
            Texture::BindFlags flags = getBindFlags(false, fboDesc.isColorTargetUav(i));
            auto pTex = Texture::createCube(width, height, fboDesc.getColorTargetFormat(i), arraySize, mipLevels, nullptr, flags);
            pFbo->attachColorTarget(pTex, i, 0, kAttachEntireMipLevel);
        }

        if (fboDesc.getDepthStencilFormat() != ResourceFormat::Unknown)
        {
            Texture::BindFlags flags = getBindFlags(true, fboDesc.isDepthStencilUav());
            auto pDepth = Texture::createCube(width, height, fboDesc.getDepthStencilFormat(), arraySize, mipLevels, nullptr, flags);
            pFbo->attachDepthStencilTarget(pDepth, 0, kAttachEntireMipLevel);
        }

        return pFbo;
    }

    Fbo::SharedPtr Fbo::create2D(uint32_t width, uint32_t height, ResourceFormat color, ResourceFormat depth)
    {
        Desc d;
        d.setColorTarget(0, color).setDepthStencilTarget(depth);
        return create2D(width, height, d);
    }
}
