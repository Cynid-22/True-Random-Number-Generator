#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Platform {

std::wstring Utf8ToWide(const char *text);

std::string WideToUtf8(const wchar_t *text);

struct FileStamp {
  long long size = 0;
  uint64_t lastWriteTime = 0;

  bool operator==(const FileStamp &other) const {
    return size == other.size && lastWriteTime == other.lastWriteTime;
  }
  bool operator!=(const FileStamp &other) const { return !(*this == other); }
};

bool FileStampOf(const char *utf8Path, FileStamp &stamp);

enum class FileReadStatus { Ok, OpenFailed, Changed, ReadFailed };

FileReadStatus ReadWholeFileIfUnchanged(const char *utf8Path,
                                        const FileStamp &expectedStamp,
                                        std::vector<uint8_t> &data);

bool WriteWholeFile(const wchar_t *path, const char *data, size_t size);

}
