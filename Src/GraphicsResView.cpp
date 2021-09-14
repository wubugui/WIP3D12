#include "GraphicsResView.h"
#include "Device.h"
#include <array>

namespace WIP3D
{
    namespace
    {
        struct NullResourceViews
        {
            std::array<ShaderResourceView::SharedPtr, (size_t)ShaderResourceView::Dimension::Count> srv;
            std::array<UnorderedAccessView::SharedPtr, (size_t)UnorderedAccessView::Dimension::Count> uav;
            std::array<DepthStencilView::SharedPtr, (size_t)DepthStencilView::Dimension::Count> dsv;
            std::array<RenderTargetView::SharedPtr, (size_t)RenderTargetView::Dimension::Count> rtv;
            ConstantBufferView::SharedPtr cbv;
        };

        NullResourceViews gNullViews;
    }

    void createNullViews()
    {
        gNullViews.srv[(size_t)ShaderResourceView::Dimension::Buffer] = ShaderResourceView::create(ShaderResourceView::Dimension::Buffer);
        gNullViews.srv[(size_t)ShaderResourceView::Dimension::Texture1D] = ShaderResourceView::create(ShaderResourceView::Dimension::Texture1D);
        gNullViews.srv[(size_t)ShaderResourceView::Dimension::Texture1DArray] = ShaderResourceView::create(ShaderResourceView::Dimension::Texture1DArray);
        gNullViews.srv[(size_t)ShaderResourceView::Dimension::Texture2D] = ShaderResourceView::create(ShaderResourceView::Dimension::Texture2D);
        gNullViews.srv[(size_t)ShaderResourceView::Dimension::Texture2DArray] = ShaderResourceView::create(ShaderResourceView::Dimension::Texture2DArray);
        gNullViews.srv[(size_t)ShaderResourceView::Dimension::Texture2DMS] = ShaderResourceView::create(ShaderResourceView::Dimension::Texture2DMS);
        gNullViews.srv[(size_t)ShaderResourceView::Dimension::Texture2DMSArray] = ShaderResourceView::create(ShaderResourceView::Dimension::Texture2DMSArray);
        gNullViews.srv[(size_t)ShaderResourceView::Dimension::Texture3D] = ShaderResourceView::create(ShaderResourceView::Dimension::Texture3D);
        gNullViews.srv[(size_t)ShaderResourceView::Dimension::TextureCube] = ShaderResourceView::create(ShaderResourceView::Dimension::TextureCube);
        gNullViews.srv[(size_t)ShaderResourceView::Dimension::TextureCubeArray] = ShaderResourceView::create(ShaderResourceView::Dimension::TextureCubeArray);

        if (gpDevice->isFeatureSupported(Device::SupportedFeatures::Raytracing))
        {
            gNullViews.srv[(size_t)ShaderResourceView::Dimension::AccelerationStructure] = ShaderResourceView::create(ShaderResourceView::Dimension::AccelerationStructure);
        }

        gNullViews.uav[(size_t)UnorderedAccessView::Dimension::Buffer] = UnorderedAccessView::create(UnorderedAccessView::Dimension::Buffer);
        gNullViews.uav[(size_t)UnorderedAccessView::Dimension::Texture1D] = UnorderedAccessView::create(UnorderedAccessView::Dimension::Texture1D);
        gNullViews.uav[(size_t)UnorderedAccessView::Dimension::Texture1DArray] = UnorderedAccessView::create(UnorderedAccessView::Dimension::Texture1DArray);
        gNullViews.uav[(size_t)UnorderedAccessView::Dimension::Texture2D] = UnorderedAccessView::create(UnorderedAccessView::Dimension::Texture2D);
        gNullViews.uav[(size_t)UnorderedAccessView::Dimension::Texture2DArray] = UnorderedAccessView::create(UnorderedAccessView::Dimension::Texture2DArray);
        gNullViews.uav[(size_t)UnorderedAccessView::Dimension::Texture3D] = UnorderedAccessView::create(UnorderedAccessView::Dimension::Texture3D);

        gNullViews.dsv[(size_t)DepthStencilView::Dimension::Texture1D] = DepthStencilView::create(DepthStencilView::Dimension::Texture1D);
        gNullViews.dsv[(size_t)DepthStencilView::Dimension::Texture1DArray] = DepthStencilView::create(DepthStencilView::Dimension::Texture1DArray);
        gNullViews.dsv[(size_t)DepthStencilView::Dimension::Texture2D] = DepthStencilView::create(DepthStencilView::Dimension::Texture2D);
        gNullViews.dsv[(size_t)DepthStencilView::Dimension::Texture2DArray] = DepthStencilView::create(DepthStencilView::Dimension::Texture2DArray);
        gNullViews.dsv[(size_t)DepthStencilView::Dimension::Texture2DMS] = DepthStencilView::create(DepthStencilView::Dimension::Texture2DMS);
        gNullViews.dsv[(size_t)DepthStencilView::Dimension::Texture2DMSArray] = DepthStencilView::create(DepthStencilView::Dimension::Texture2DMSArray);

        gNullViews.rtv[(size_t)RenderTargetView::Dimension::Buffer] = RenderTargetView::create(RenderTargetView::Dimension::Buffer);
        gNullViews.rtv[(size_t)RenderTargetView::Dimension::Texture1D] = RenderTargetView::create(RenderTargetView::Dimension::Texture1D);
        gNullViews.rtv[(size_t)RenderTargetView::Dimension::Texture1DArray] = RenderTargetView::create(RenderTargetView::Dimension::Texture1DArray);
        gNullViews.rtv[(size_t)RenderTargetView::Dimension::Texture2D] = RenderTargetView::create(RenderTargetView::Dimension::Texture2D);
        gNullViews.rtv[(size_t)RenderTargetView::Dimension::Texture2DArray] = RenderTargetView::create(RenderTargetView::Dimension::Texture2DArray);
        gNullViews.rtv[(size_t)RenderTargetView::Dimension::Texture2DMS] = RenderTargetView::create(RenderTargetView::Dimension::Texture2DMS);
        gNullViews.rtv[(size_t)RenderTargetView::Dimension::Texture2DMSArray] = RenderTargetView::create(RenderTargetView::Dimension::Texture2DMSArray);
        gNullViews.rtv[(size_t)RenderTargetView::Dimension::Texture3D] = RenderTargetView::create(RenderTargetView::Dimension::Texture3D);

        gNullViews.cbv = ConstantBufferView::create();
    }

    void releaseNullViews()
    {
        gNullViews = {};
    }

    ShaderResourceView::SharedPtr ShaderResourceView::getNullView(ShaderResourceView::Dimension dimension)
    {
        assert((size_t)dimension < gNullViews.srv.size() && gNullViews.srv[(size_t)dimension]);
        return gNullViews.srv[(size_t)dimension];
    }

    UnorderedAccessView::SharedPtr UnorderedAccessView::getNullView(UnorderedAccessView::Dimension dimension)
    {
        assert((size_t)dimension < gNullViews.uav.size() && gNullViews.uav[(size_t)dimension]);
        return gNullViews.uav[(size_t)dimension];
    }

    DepthStencilView::SharedPtr DepthStencilView::getNullView(DepthStencilView::Dimension dimension)
    {
        assert((size_t)dimension < gNullViews.dsv.size() && gNullViews.dsv[(size_t)dimension]);
        return gNullViews.dsv[(size_t)dimension];
    }

    RenderTargetView::SharedPtr RenderTargetView::getNullView(RenderTargetView::Dimension dimension)
    {
        assert((size_t)dimension < gNullViews.rtv.size() && gNullViews.rtv[(size_t)dimension]);
        return gNullViews.rtv[(size_t)dimension];
    }

    ConstantBufferView::SharedPtr ConstantBufferView::getNullView()
    {
        return gNullViews.cbv;
    }

}
