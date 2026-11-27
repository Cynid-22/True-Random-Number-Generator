#pragma once
#include <windows.h>

namespace Platform {

void SetCaptureProtectedWindow(HWND window);

bool SetScreenCaptureAllowed(bool allowed);

}
