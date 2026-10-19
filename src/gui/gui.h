#pragma once

#include <cstddef>
#include <string>

inline constexpr wchar_t TEXT_FILE_FILTER[] =
    L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
inline constexpr wchar_t BINARY_FILE_FILTER[] =
    L"Binary Files (*.bin)\0*.bin\0All Files (*.*)\0*.*\0";

void ExportWithSaveDialog(const std::wstring &defaultName,
                          const wchar_t *filter,
                          const wchar_t *defaultExtension, const char *data,
                          size_t size, const char *exportName);

enum class OutputFileType { Text, Binary };

bool ChooseOutputFile(const std::wstring &defaultName, bool textOnly,
                      std::wstring &filePath, OutputFileType &fileType);

void WriteExportFile(const wchar_t *filePath, const char *data, size_t size,
                     const char *exportName);

void SetupNativeStyle();

void RenderMenuBar();

void RenderUserInputTab();
void RenderSystemInputTab();
void RenderCollectionWindow();

void RenderEntropyPoolBar();

void RenderOutputConfigSection();
void RenderOutputSection();

void RenderAboutModal();

void RenderClosingModal();
