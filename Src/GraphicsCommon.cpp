#include "GraphicsCommon.h"
#include "Device.h"

namespace WIP3D
{
    WIP3D::D3D12DescriptorHeap::~D3D12DescriptorHeap()
    {
    }

    D3D12DescriptorHeap::SharedPtr WIP3D::D3D12DescriptorHeap::create(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descCount, bool shaderVisible)
    {
        return SharedPtr();
    }


    WIP3D::D3D12DescriptorHeap::Allocation::~Allocation()
    {
    }

    D3D12DescriptorHeap::Allocation::SharedPtr WIP3D::D3D12DescriptorHeap::Allocation::create(D3D12DescriptorHeap::SharedPtr pHeap, uint32_t baseIndex, uint32_t descCount, std::shared_ptr<Chunk> pChunk)
    {
        return D3D12DescriptorHeap::Allocation::SharedPtr();
    }
    D3D12DescriptorHeap::Allocation::SharedPtr WIP3D::D3D12DescriptorHeap::allocateDescriptors(uint32_t count)
    {
        return Allocation::SharedPtr();
    }

    D3D12DescriptorHeap::CpuHandle WIP3D::D3D12DescriptorHeap::getCpuHandle(uint32_t index) const
    {
        return D3D12DescriptorHeap::CpuHandle();
    }

    D3D12DescriptorHeap::GpuHandle WIP3D::D3D12DescriptorHeap::getGpuHandle(uint32_t index) const
    {
        return D3D12DescriptorHeap::GpuHandle();
    }
	
    DescriptorPool::SharedPtr DescriptorPool::create(const Desc& desc, const GpuFence::SharedPtr& pFence)
    {
        return SharedPtr(new DescriptorPool(desc, pFence));
    }

    DescriptorPool::DescriptorPool(const Desc& desc, const GpuFence::SharedPtr& pFence)
        : mDesc(desc)
        , mpFence(pFence)
    {
        apiInit();
    }

    DescriptorPool::~DescriptorPool() = default;

    void DescriptorPool::executeDeferredReleases()
    {
        uint64_t gpuVal = mpFence->getGpuValue();
        while (mpDeferredReleases.size() && mpDeferredReleases.top().fenceValue <= gpuVal)
        {
            mpDeferredReleases.pop();
        }
    }

    void DescriptorPool::releaseAllocation(std::shared_ptr<DescriptorSetApiData> pData)
    {
        DeferredRelease d;
        d.pData = pData;
        d.fenceValue = mpFence->getCpuValue();
        mpDeferredReleases.push(d);
    }

    RasterizerState::SharedPtr RasterizerState::create(const Desc& desc)
    {
        return SharedPtr(new RasterizerState(desc));
    }
   
    DepthStencilState::SharedPtr DepthStencilState::create(const Desc& desc)
    {
        return SharedPtr(new DepthStencilState(desc));
    }

    DepthStencilState::~DepthStencilState() = default;

    DepthStencilState::Desc& DepthStencilState::Desc::setStencilWriteMask(uint8_t mask)
    {
        mStencilWriteMask = mask;
        return *this;
    }

    DepthStencilState::Desc& DepthStencilState::Desc::setStencilReadMask(uint8_t mask)
    {
        mStencilReadMask = mask;
        return *this;
    }

    DepthStencilState::Desc& DepthStencilState::Desc::setStencilFunc(Face face, Func func)
    {
        if (face == Face::FrontAndBack)
        {
            setStencilFunc(Face::Front, func);
            setStencilFunc(Face::Back, func);
            return *this;
        }
        StencilDesc& Desc = (face == Face::Front) ? mStencilFront : mStencilBack;
        Desc.func = func;
        return *this;
    }

    DepthStencilState::Desc& DepthStencilState::Desc::setStencilOp(Face face, StencilOp stencilFail, StencilOp depthFail, StencilOp depthStencilPass)
    {
        if (face == Face::FrontAndBack)
        {
            setStencilOp(Face::Front, stencilFail, depthFail, depthStencilPass);
            setStencilOp(Face::Back, stencilFail, depthFail, depthStencilPass);
            return *this;
        }
        StencilDesc& Desc = (face == Face::Front) ? mStencilFront : mStencilBack;
        Desc.stencilFailOp = stencilFail;
        Desc.depthFailOp = depthFail;
        Desc.depthStencilPassOp = depthStencilPass;

        return *this;
    }

    const DepthStencilState::StencilDesc& DepthStencilState::getStencilDesc(Face face) const
    {
        assert(face != Face::FrontAndBack);
        return (face == Face::Front) ? mDesc.mStencilFront : mDesc.mStencilBack;
    }

    struct SamplerData
    {
        uint32_t objectCount = 0;
        Sampler::SharedPtr pDefaultSampler;
    };
    SamplerData gSamplerData;

    Sampler::Sampler(const Desc& desc) : mDesc(desc)
    {
        gSamplerData.objectCount++;
    }

    Sampler::~Sampler()
    {
        gSamplerData.objectCount--;
        if (gSamplerData.objectCount <= 1) gSamplerData.pDefaultSampler = nullptr;
    }

    Sampler::Desc& Sampler::Desc::setFilterMode(Filter minFilter, Filter magFilter, Filter mipFilter)
    {
        mMagFilter = magFilter;
        mMinFilter = minFilter;
        mMipFilter = mipFilter;
        return *this;
    }

    Sampler::Desc& Sampler::Desc::setMaxAnisotropy(uint32_t maxAnisotropy)
    {
        mMaxAnisotropy = maxAnisotropy;
        return *this;
    }

    Sampler::Desc& Sampler::Desc::setLodParams(float minLod, float maxLod, float lodBias)
    {
        mMinLod = minLod;
        mMaxLod = maxLod;
        mLodBias = lodBias;
        return *this;
    }

    Sampler::Desc& Sampler::Desc::setComparisonMode(ComparisonMode mode)
    {
        mComparisonMode = mode;
        return *this;
    }

    Sampler::Desc& Sampler::Desc::setReductionMode(ReductionMode mode)
    {
        mReductionMode = mode;
        return *this;
    }

    Sampler::Desc& Sampler::Desc::setAddressingMode(AddressMode modeU, AddressMode modeV, AddressMode modeW)
    {
        mModeU = modeU;
        mModeV = modeV;
        mModeW = modeW;
        return *this;
    }

    Sampler::Desc& Sampler::Desc::setBorderColor(const RBVector4& borderColor)
    {
        mBorderColor = borderColor;
        return *this;
    }

    Sampler::SharedPtr Sampler::getDefault()
    {
        if (gSamplerData.pDefaultSampler == nullptr)
        {
            gSamplerData.pDefaultSampler = create(Desc());
        }
        return gSamplerData.pDefaultSampler;
    }
}
