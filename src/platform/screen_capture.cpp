#include "screen_capture.h"

namespace Platform {

namespace {

HWND s_protectedWindow = nullptr;

}

void SetCaptureProtectedWindow(HWND window) { s_protectedWindow = window; }

bool SetScreenCaptureAllowed(bool allowed) {
  if (!s_protectedWindow)
    return false;
  if (allowed)
    return SetWindowDisplayAffinity(s_protectedWindow, WDA_NONE) != FALSE;
  return SetWindowDisplayAffinity(s_protectedWindow, WDA_EXCLUDEFROMCAPTURE) ||
         SetWindowDisplayAffinity(s_protectedWindow, WDA_MONITOR);
}

}
