#include "WIPD3D12.h"
#include "../Common/Logger.h"

void TraceHResult(const std::string& msg, HRESULT hr)
{
    char hr_msg[512];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, hr, 0, hr_msg, ARRAYSIZE(hr_msg), nullptr);

    std::string error_msg = msg + ".\nError! " + hr_msg;
    
    LOG_ERROR(error_msg.c_str());
}