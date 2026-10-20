#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

inline constexpr wchar_t TEXT_FILE_FILTER[] =
    L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
inline constexpr wchar_t BINARY_FILE_FILTER[] =
    L"Binary Files (*.bin)\0*.bin\0All Files (*.*)\0*.*\0";

void ExportWithSaveDialog(const std::wstring &defaultName,
                          const wchar_t *filter,
                          const wchar_t *defaultExtension, const char *data,
                          size_t size, const char *exportName);

enum class OutputFileType { Text, Binary, Csv, Json };

bool ChooseOutputFile(const std::wstring &defaultName, bool textOnly,
                      std::wstring &filePath, OutputFileType &fileType);

void WriteExportFile(const wchar_t *filePath, const char *data, size_t size,
                     const char *exportName);

void WriteAndWipeExportFile(const wchar_t *filePath, std::vector<char> &content,
                            const char *exportName);

void WriteAndWipeExportFile(const wchar_t *filePath,
                            std::vector<uint8_t> &content,
                            const char *exportName);

void LogExportOutOfMemory(const char *exportName);

void SetupNativeStyle();

void RenderMenuBar();

void BeginCollectionPadFrame();
void FocusCollectionPadWhenShown();
void RenderInputTab();

void RenderEntropyPoolBar();

float StatusBarReservedHeight();
void RenderStatusBar();

void RenderOutputConfigSection();
void RenderOutputSection();

void RenderAboutModal();

void RenderClosingModal();
