#include "unicode_path.h"
#include <algorithm>
#include <windows.h>

namespace Platform {

static constexpr DWORD FILE_CHUNK_BYTES = 64u * 1024u * 1024u;

std::wstring Utf8ToWide(const char *text) {
  if (text == nullptr || text[0] == '\0')
    return std::wstring();
  int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1,
                                   nullptr, 0);
  if (length <= 0)
    return std::wstring();
  std::wstring wide(static_cast<size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide.data(),
                      length);
  wide.resize(static_cast<size_t>(length) - 1);
  return wide;
}

std::string WideToUtf8(const wchar_t *text) {
  if (text == nullptr || text[0] == L'\0')
    return std::string();
  int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1,
                                   nullptr, 0, nullptr, nullptr);
  if (length <= 0)
    return std::string();
  std::string utf8(static_cast<size_t>(length), '\0');
  WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, utf8.data(),
                      length, nullptr, nullptr);
  utf8.resize(static_cast<size_t>(length) - 1);
  return utf8;
}

static HANDLE OpenForReadingDenyingWrites(const char *utf8Path) {
  std::wstring widePath = Utf8ToWide(utf8Path);
  if (widePath.empty())
    return INVALID_HANDLE_VALUE;
  return CreateFileW(widePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
}

static bool StampOfHandle(HANDLE file, FileStamp &stamp) {
  LARGE_INTEGER fileSize;
  FILETIME lastWrite;
  if (!GetFileSizeEx(file, &fileSize) ||
      !GetFileTime(file, nullptr, nullptr, &lastWrite))
    return false;
  stamp.size = fileSize.QuadPart;
  stamp.lastWriteTime =
      (static_cast<uint64_t>(lastWrite.dwHighDateTime) << 32) |
      lastWrite.dwLowDateTime;
  return true;
}

bool FileStampOf(const char *utf8Path, FileStamp &stamp) {
  HANDLE file = OpenForReadingDenyingWrites(utf8Path);
  if (file == INVALID_HANDLE_VALUE)
    return false;
  const bool ok = StampOfHandle(file, stamp);
  CloseHandle(file);
  return ok;
}

FileReadStatus ReadWholeFileIfUnchanged(const char *utf8Path,
                                        const FileStamp &expectedStamp,
                                        std::vector<uint8_t> &data) {
  HANDLE file = OpenForReadingDenyingWrites(utf8Path);
  if (file == INVALID_HANDLE_VALUE)
    return FileReadStatus::OpenFailed;

  FileStamp currentStamp;
  if (!StampOfHandle(file, currentStamp)) {
    CloseHandle(file);
    return FileReadStatus::ReadFailed;
  }
  if (currentStamp != expectedStamp) {
    CloseHandle(file);
    return FileReadStatus::Changed;
  }

  data.assign(static_cast<size_t>(expectedStamp.size), 0);
  size_t offset = 0;
  while (offset < data.size()) {
    DWORD chunk = static_cast<DWORD>(
        std::min<size_t>(data.size() - offset, FILE_CHUNK_BYTES));
    DWORD bytesRead = 0;
    if (!ReadFile(file, data.data() + offset, chunk, &bytesRead, nullptr) ||
        bytesRead == 0) {
      CloseHandle(file);
      SecureZeroMemory(data.data(), data.size());
      data.clear();
      return FileReadStatus::ReadFailed;
    }
    offset += bytesRead;
  }

  CloseHandle(file);
  return FileReadStatus::Ok;
}

bool WriteWholeFile(const wchar_t *path, const char *data, size_t size) {
  HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return false;

  size_t offset = 0;
  while (offset < size) {
    DWORD chunk =
        static_cast<DWORD>(std::min<size_t>(size - offset, FILE_CHUNK_BYTES));
    DWORD bytesWritten = 0;
    if (!WriteFile(file, data + offset, chunk, &bytesWritten, nullptr) ||
        bytesWritten == 0) {
      CloseHandle(file);
      return false;
    }
    offset += bytesWritten;
  }

  CloseHandle(file);
  return true;
}

}
