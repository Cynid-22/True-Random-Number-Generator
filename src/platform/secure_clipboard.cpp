#include "secure_clipboard.h"
#include <climits>

namespace Platform {

namespace {

constexpr ULONGLONG MILLISECONDS_PER_SECOND = 1000;

HWND s_ownerWindow = nullptr;
bool s_holdsSecret = false;
DWORD s_secretSequenceNumber = 0;
ULONGLONG s_secretClearTick = 0;

bool StillHoldsSecret() {
  if (s_holdsSecret && GetClipboardSequenceNumber() != s_secretSequenceNumber)
    s_holdsSecret = false;
  return s_holdsSecret;
}

void FreeWipedGlobal(HGLOBAL memory, size_t byteCount) {
  if (void *data = GlobalLock(memory)) {
    SecureZeroMemory(data, byteCount);
    GlobalUnlock(memory);
  }
  GlobalFree(memory);
}

HGLOBAL WideTextGlobal(const char *utf8Text, size_t length, size_t &byteCount) {
  if (length > static_cast<size_t>(INT_MAX))
    return nullptr;
  const int textLength = static_cast<int>(length);
  const int wideLength =
      textLength == 0
          ? 0
          : MultiByteToWideChar(CP_UTF8, 0, utf8Text, textLength, nullptr, 0);
  if (textLength > 0 && wideLength == 0)
    return nullptr;

  byteCount = (static_cast<size_t>(wideLength) + 1) * sizeof(wchar_t);
  HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, byteCount);
  if (!memory)
    return nullptr;
  wchar_t *wideText = static_cast<wchar_t *>(GlobalLock(memory));
  if (!wideText) {
    GlobalFree(memory);
    return nullptr;
  }
  if (wideLength > 0)
    MultiByteToWideChar(CP_UTF8, 0, utf8Text, textLength, wideText, wideLength);
  wideText[wideLength] = L'\0';
  GlobalUnlock(memory);
  return memory;
}

void AddDwordFormat(const wchar_t *formatName, DWORD value) {
  const UINT format = RegisterClipboardFormatW(formatName);
  if (format == 0)
    return;
  HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
  if (!memory)
    return;
  DWORD *data = static_cast<DWORD *>(GlobalLock(memory));
  if (!data) {
    GlobalFree(memory);
    return;
  }
  *data = value;
  GlobalUnlock(memory);
  if (!SetClipboardData(format, memory))
    GlobalFree(memory);
}

}

void SetClipboardOwnerWindow(HWND window) { s_ownerWindow = window; }

bool CopySecretTextToClipboard(const char *utf8Text, size_t length) {
  size_t byteCount = 0;
  HGLOBAL text = WideTextGlobal(utf8Text, length, byteCount);
  if (!text)
    return false;
  if (!OpenClipboard(s_ownerWindow)) {
    FreeWipedGlobal(text, byteCount);
    return false;
  }

  EmptyClipboard();
  const bool copied = SetClipboardData(CF_UNICODETEXT, text) != nullptr;
  if (copied) {
    AddDwordFormat(L"ExcludeClipboardContentFromMonitorProcessing", 0);
    AddDwordFormat(L"CanIncludeInClipboardHistory", 0);
    AddDwordFormat(L"CanUploadToCloudClipboard", 0);
  } else {
    FreeWipedGlobal(text, byteCount);
  }
  CloseClipboard();
  if (!copied)
    return false;

  s_holdsSecret = true;
  s_secretSequenceNumber = GetClipboardSequenceNumber();
  s_secretClearTick = GetTickCount64() + SECRET_CLIPBOARD_LIFETIME_SECONDS *
                                             MILLISECONDS_PER_SECOND;
  return true;
}

int SecondsUntilSecretClipboardClear() {
  if (!StillHoldsSecret())
    return -1;
  const ULONGLONG now = GetTickCount64();
  if (now >= s_secretClearTick)
    return 0;
  return static_cast<int>((s_secretClearTick - now + MILLISECONDS_PER_SECOND - 1) /
                          MILLISECONDS_PER_SECOND);
}

void ClearSecretClipboardWhenDue() {
  if (StillHoldsSecret() && GetTickCount64() >= s_secretClearTick)
    ClearSecretClipboardNow();
}

void ClearSecretClipboardNow() {
  if (!StillHoldsSecret() || !OpenClipboard(s_ownerWindow))
    return;
  EmptyClipboard();
  CloseClipboard();
  s_holdsSecret = false;
}

}
