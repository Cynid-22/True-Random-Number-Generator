#pragma once
#include <cstddef>
#include <windows.h>

namespace Platform {

constexpr unsigned SECRET_CLIPBOARD_LIFETIME_SECONDS = 60;

void SetClipboardOwnerWindow(HWND window);

bool CopySecretTextToClipboard(const char *utf8Text, size_t length);

int SecondsUntilSecretClipboardClear();

void ClearSecretClipboardWhenDue();

void ClearSecretClipboardNow();

}
