#include "Application.h"
#include "Common/Logger.h"
WIPLogger* g_logger = nullptr;
int main(int argc,char** argv)
{
	g_logger = WIPLogger::get_instance();
	g_logger->startup("./");
	WIP3D::Window::Desc  WindowConfig;
	WindowConfig.width = 1280;
	WindowConfig.height = 720;
	WindowConfig.resizableWindow = false;
	WindowConfig.title = "Fuck";
	WIP3D::WindowCallback WindowCallback_;
	WIP3D::Window::SharedPtr Window_ = WIP3D::Window::Create(WindowConfig, &WindowCallback_);
	Window_->MsgLoop();
	g_logger->shutdown();
	g_logger->release();
	return 0;
}