#if 1
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
	//class dlldecl Sample : public Window::ICallbacks, public IFramework
	//Sample.run(Render)
	//Callback:
	//	窗口尺寸变化
	//	渲染Frame
	//	处理键盘事件
	//	处理鼠标事件
	//	处理文件拖入事件
	//Framework:
	//	
	WIP3D::WindowCallback WindowCallback_;
	WIP3D::Window::SharedPtr Window_ = WIP3D::Window::Create(WindowConfig, &WindowCallback_);
	Window_->MsgLoop();
	g_logger->shutdown();
	g_logger->release();
	return 0;
}
#else
#include "Common.h"
#include <iostream>
using namespace std;
int main(int argc, char** argv)
{

	int j = bitScanReverse(0x80);
	cout << j << endl;
	j = bitScanReverse(0x40);
	cout << j << endl;

	j++;
}
#endif