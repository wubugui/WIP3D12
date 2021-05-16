#include "Application.h"
#include "Device.h"
#include "Common/Logger.h"

namespace WIP3D
{
	void WindowCallback::HandleWindowInit(Window::SharedPtr WindowPointer)
	{
#if 1
		Device::Desc desc;
		//ʹ��Ĭ��Device����
		gpDevice = Device::create(WindowPointer, desc);
		if (gpDevice == nullptr)
		{
			LOG_ERROR("Failed to create device");
			return;
		}
#endif	

	}
	void WindowCallback::HandleWindowResize()
	{
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