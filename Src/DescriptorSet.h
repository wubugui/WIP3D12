#pragma once
#include "GraphicsCommon.h"

namespace WIP3D
{
    class ShaderResourceView;
    class UnorderedAccessView;
    class ConstantBufferView;
    class Sampler;
    class CopyContext;
    class RootSignature;

    enum class ShaderVisibility
    {
        None = 0,
        Vertex = (1 << (uint32_t)ShaderType::Vertex),
        Pixel = (1 << (uint32_t)ShaderType::Pixel),
        Hull = (1 << (uint32_t)ShaderType::Hull),
        Domain = (1 << (uint32_t)ShaderType::Domain),
        Geometry = (1 << (uint32_t)ShaderType::Geometry),
        Compute = (1 << (uint32_t)ShaderType::Compute),

        All = (1 << (uint32_t)ShaderType::Count) - 1,
    };

    enum_class_operators(ShaderVisibility);

    class DescriptorSet
    {
    public:
        using SharedPtr = std::shared_ptr<DescriptorSet>;
        using Type = DescriptorPool::Type;
        using CpuHandle = DescriptorPool::CpuHandle;
        using GpuHandle = DescriptorPool::GpuHandle;
        using ApiHandle = DescriptorSetApiHandle;
        using ApiData = DescriptorSetApiData;

        ~DescriptorSet();

        class Layout
        {
        public:
            struct Range
            {
                Type type;
                uint32_t baseRegIndex;
                uint32_t descCount;
                uint32_t regSpace;
            };

            Layout(ShaderVisibility visibility = ShaderVisibility::All) : mVisibility(visibility) {}
            Layout& addRange(Type type, uint32_t baseRegIndex, uint32_t descriptorCount, uint32_t regSpace = 0);
            size_t getRangeCount() const { return mRanges.size(); }
            const Range& getRange(size_t index) const { return mRanges[index]; }
            ShaderVisibility getVisibility() const { return mVisibility; }
        private:
            std::vector<Range> mRanges;
            ShaderVisibility mVisibility;
        };

        /** Create a new descriptor set.
            \param[in] pPool The descriptor pool.
            \param[in] layout The layout.
            \return A new object, or throws an exception if creation failed.
        */
        static SharedPtr create(const DescriptorPool::SharedPtr& pPool, const Layout& layout);

        size_t getRangeCount() const { return mLayout.getRangeCount(); }
        const Layout::Range& getRange(uint32_t range) const { return mLayout.getRange(range); }
        ShaderVisibility getVisibility() const { return mLayout.getVisibility(); }

        CpuHandle getCpuHandle(uint32_t rangeIndex, uint32_t descInRange = 0) const;
        GpuHandle getGpuHandle(uint32_t rangeIndex, uint32_t descInRange = 0) const;
        const ApiHandle& getApiHandle() const { return mApiHandle; }
        const ApiData* getApiData() const { return mpApiData.get(); }

        void setSrv(uint32_t rangeIndex, uint32_t descIndex, const ShaderResourceView* pSrv);
        void setUav(uint32_t rangeIndex, uint32_t descIndex, const UnorderedAccessView* pUav);
        void setSampler(uint32_t rangeIndex, uint32_t descIndex, const Sampler* pSampler);
        void setCbv(uint32_t rangeIndex, uint32_t descIndex, ConstantBufferView* pView);

        void bindForGraphics(CopyContext* pCtx, const RootSignature* pRootSig, uint32_t rootIndex);
        void bindForCompute(CopyContext* pCtx, const RootSignature* pRootSig, uint32_t rootIndex);

    private:
        DescriptorSet(DescriptorPool::SharedPtr pPool, const Layout& layout);
        void apiInit();

        Layout mLayout;
        std::shared_ptr<ApiData> mpApiData;
        DescriptorPool::SharedPtr mpPool;
        ApiHandle mApiHandle = {};
    };
}
