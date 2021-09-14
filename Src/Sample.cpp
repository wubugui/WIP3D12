#include "Application.h"
#include "Device.h"
#include "GraphicsCommon.h"
#include "GraphicsContext.h"
#include "GraphicsResource.h"
#include "GraphicsResView.h"
#include "Common/Logger.h"


namespace WIP3D
{
	RasterizerState::SharedPtr mpWireframeRS;
	DepthStencilState::SharedPtr mpNoDepthDS;
	DepthStencilState::SharedPtr mpDepthTestDS;
	Sampler::SharedPtr mpPointSampler;
	Sampler::SharedPtr mpLinearSampler;
	void WindowCallback::HandleWindowInit(Window::SharedPtr WindowPointer)
	{
#if 1
		Device::Desc desc;
		//使用默认Device设置
		gpDevice = Device::create(WindowPointer, desc);
		if (gpDevice == nullptr)
		{
			LOG_ERROR("Failed to create device");
			return;
		}

		// create rasterizer state
		RasterizerState::Desc wireframeDesc;
		wireframeDesc.setFillMode(RasterizerState::FillMode::Wireframe);
		wireframeDesc.setCullMode(RasterizerState::CullMode::None);
		mpWireframeRS = RasterizerState::create(wireframeDesc);


		// Depth test
		DepthStencilState::Desc dsDesc;
		dsDesc.setDepthEnabled(false);
		mpNoDepthDS = DepthStencilState::create(dsDesc);
		dsDesc.setDepthFunc(ComparisonFunc::Less).setDepthEnabled(true);
		mpDepthTestDS = DepthStencilState::create(dsDesc);

		Sampler::Desc samplerDesc;
		samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point);
		mpPointSampler = Sampler::create(samplerDesc);
		samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
		mpLinearSampler = Sampler::create(samplerDesc);
#endif	

	}
	void WindowCallback::HandleWindowResize(Window::SharedPtr mpWindow, int w, int h)
	{
		if (!gpDevice) return;
		// Tell the device to resize the swap chain
		// auto winSize = mpWindow->getClientAreaSize();
		auto pBackBufferFBO = gpDevice->resizeSwapChain(w, h);
		auto width = pBackBufferFBO->getWidth();
		auto height = pBackBufferFBO->getHeight();

		// Recreate target fbo
		auto pCurrentFbo = mpTargetFBO;
		mpTargetFBO = Fbo::create2D(width, height, pBackBufferFBO->getDesc());
		gpDevice->getRenderContext()->blit(pCurrentFbo->getColorTexture(0)->getSRV(), mpTargetFBO->getRenderTargetView(0));

		// Tell the GUI the swap-chain size changed
		// if (mpGui) mpGui->onWindowResize(width, height);

		// Resize the pixel zoom
		// if (mpPixelZoom) mpPixelZoom->onResizeSwapChain(gpDevice->getSwapChainFbo().get());

		// Call the user callback
		// if (mpRenderer) mpRenderer->onResizeSwapChain(width, height);

	}
	void WindowCallback::HandleRenderFrame()
	{
	}
	void WindowCallback::HandleKeyboardEvent(const KeyboardEvent& keyEvent)
	{
	}
	void WindowCallback::HandleMouseEvent(const MouseEvent& mouseEvent)
	{
	}
	void WindowCallback::HandleDroppedFile(const std::string& filename)
	{
	}
}