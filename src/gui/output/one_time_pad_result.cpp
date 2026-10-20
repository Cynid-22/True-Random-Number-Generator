#include "one_time_pad_result.h"
#include "../../core/app_state.h"
#include "../../logic/formats/one_time_pad_format.h"
#include "../../logic/output_export.h"
#include "../../platform/unicode_path.h"
#include "../gui.h"
#include "../widgets.h"
#include "imgui.h"
#include <algorithm>
#include <new>
#include <string>
#include <vector>

static const char *const ONE_TIME_PAD_METHOD_TEXT =
    "Method: Vernam cipher with a key from the ChaCha20 pipeline";

static bool IsFileResult() {
  return g_state.oneTimePadResultMode == ONE_TIME_PAD_FILE_INPUT;
}

static size_t KeyAndCiphertextLength() {
  size_t length = g_state.oneTimePadKey.size();
  if (!IsFileResult() && length > 0 && g_state.oneTimePadKey.back() == '\0')
    length--;
  return length;
}

static std::wstring ResultFileNameWithoutExtension() {
  std::wstring name =
      Platform::Utf8ToWide(g_state.oneTimePadResultFileName.c_str());
  size_t dot = name.find_last_of(L'.');
  if (dot != std::wstring::npos && dot > 0)
    name.resize(dot);
  return name.empty() ? std::wstring(L"file") : name;
}

static void RenderKeyWarning() {
  ImGui::PushStyleColor(ImGuiCol_Text, UiColor::Warning);
  ImGui::TextWrapped("Save the key. Without it the ciphertext can't be "
                     "decrypted in practice, and this program can't recover "
                     "it either. The key is ChaCha20 output from the "
                     "cryptographic pipeline, so this security is "
                     "computational, not the information-theoretic security "
                     "of a true one-time pad.");
  ImGui::PopStyleColor();
}

static void RenderKeyOrCiphertextBox(const char *title,
                                     const char *copyButtonLabel,
                                     const char *boxId,
                                     const std::vector<char> &text,
                                     size_t length, const char *countSuffix,
                                     float boxHeight) {
  ImGui::AlignTextToFramePadding();
  ImGui::Text("%s", title);
  ImGui::SameLine();
  ImGui::TextColored(UiColor::DimText, "%llu characters%s",
                     static_cast<unsigned long long>(length), countSuffix);
  ImGui::SameLine();
  if (ImGui::Button(copyButtonLabel))
    CopySecretText(text.data(), length);
  RenderSecretCopyTooltip();

  if (ImGui::BeginChild(boxId, ImVec2(-1, boxHeight), true))
    RenderTruncatableText(text.data(), length, g_state.truncateOutputView);
  ImGui::EndChild();
}

static void RenderTextResult() {
  ImGui::Text("%s", ONE_TIME_PAD_METHOD_TEXT);
  ImGui::TextColored(UiColor::LabelText,
                     "ciphertext = (message + key) mod 63 over 0-9 A-Z a-z "
                     "space");
  ImGui::TextColored(UiColor::LabelText,
                     "(0-9 = 0-9, A-Z = 10-35, a-z = 36-61, space = 62)");
  ImGui::TextColored(UiColor::LabelText,
                     "Decrypt: message = (ciphertext - key) mod 63");
  RenderKeyWarning();
  ImGui::Spacing();

  const ImGuiStyle &style = ImGui::GetStyle();
  const float titleRowsHeight = ImGui::GetFrameHeightWithSpacing() * 2.0f;
  const float boxHeight = std::max(
      ImGui::GetTextLineHeightWithSpacing() * 2.0f,
      (ImGui::GetContentRegionAvail().y - titleRowsHeight -
       style.ItemSpacing.y * 2.0f) /
          2.0f);
  const size_t length = KeyAndCiphertextLength();

  RenderKeyOrCiphertextBox("Key", "Copy##OTPKey", "##OTPKeyBox",
                           g_state.oneTimePadKey, length,
                           " (spaces are part of the key)", boxHeight);
  RenderKeyOrCiphertextBox("Ciphertext", "Copy##OTPCiphertext",
                           "##OTPCiphertextBox", g_state.oneTimePadCiphertext,
                           length, "", boxHeight);
}

static void RenderFileResult() {
  const size_t length = KeyAndCiphertextLength();
  ImGui::Text("File: %s", g_state.oneTimePadResultFileName.c_str());
  ImGui::Text("Size: %llu bytes", static_cast<unsigned long long>(length));
  ImGui::Spacing();
  ImGui::Text("%s", ONE_TIME_PAD_METHOD_TEXT);
  ImGui::TextColored(UiColor::LabelText,
                     "Each byte: ciphertext = file XOR key");
  ImGui::TextColored(UiColor::LabelText,
                     "Decrypt: file = ciphertext XOR key (the same XOR)");
  RenderKeyWarning();
  ImGui::Spacing();

  const std::wstring fileBaseName = ResultFileNameWithoutExtension();
  if (ImGui::Button("Export Key...")) {
    ExportWithSaveDialog(fileBaseName + L"_otp_key.bin", BINARY_FILE_FILTER,
                         L"bin", g_state.oneTimePadKey.data(), length,
                         "One-Time Pad key");
  }
  ImGui::SameLine();
  if (ImGui::Button("Export Ciphertext...")) {
    ExportWithSaveDialog(fileBaseName + L"_otp_ciphertext.bin",
                         BINARY_FILE_FILTER, L"bin",
                         g_state.oneTimePadCiphertext.data(), length,
                         "One-Time Pad ciphertext");
  }
  ImGui::PushStyleColor(ImGuiCol_Text, UiColor::DimText);
  ImGui::TextWrapped("Both are raw binary files of %llu bytes, the same size "
                     "as the original. Ctrl+S saves both in one file: .txt, "
                     ".csv or .json as hex, or .bin as the key followed by "
                     "the ciphertext.",
                     static_cast<unsigned long long>(length));
  ImGui::PopStyleColor();
}

void RenderOneTimePadResult() {
  if (IsFileResult())
    RenderFileResult();
  else
    RenderTextResult();
}

static void SaveOneTimePadAs(OutputFileType fileType,
                             const std::wstring &filePath, bool fileMode) {
  const char *key = g_state.oneTimePadKey.data();
  const char *ciphertext = g_state.oneTimePadCiphertext.data();
  const size_t length = KeyAndCiphertextLength();

  switch (fileType) {
  case OutputFileType::Text: {
    std::vector<char> text = CSPRNG::BuildOneTimePadExportText(
        key, ciphertext, length, fileMode, g_state.oneTimePadResultFileName);
    WriteAndWipeExportFile(filePath.c_str(), text,
                           "One-Time Pad key and ciphertext");
    break;
  }
  case OutputFileType::Binary: {
    std::vector<uint8_t> binary = CSPRNG::BuildOneTimePadExportBinary(
        key, ciphertext, length, fileMode);
    WriteAndWipeExportFile(filePath.c_str(), binary,
                           "One-Time Pad key and ciphertext as binary");
    break;
  }
  case OutputFileType::Csv: {
    std::vector<char> csv =
        BuildOneTimePadCsv(key, ciphertext, length, fileMode);
    WriteAndWipeExportFile(filePath.c_str(), csv,
                           "One-Time Pad key and ciphertext as CSV");
    break;
  }
  case OutputFileType::Json: {
    std::vector<char> json =
        BuildOneTimePadJson(g_state.generatedOutputDetails,
                            g_state.generatedAtText, key, ciphertext, length,
                            fileMode);
    WriteAndWipeExportFile(filePath.c_str(), json,
                           "One-Time Pad key and ciphertext as JSON");
    break;
  }
  }
}

void ExportOneTimePadKeyAndCiphertext() {
  if (!g_state.hasOneTimePadResult())
    return;

  const bool fileMode = IsFileResult();
  const std::wstring defaultName =
      fileMode ? ResultFileNameWithoutExtension() +
                     L"_otp_key_and_ciphertext.txt"
               : std::wstring(L"otp_key_and_ciphertext.txt");
  std::wstring filePath;
  OutputFileType fileType = OutputFileType::Text;
  if (!ChooseOutputFile(defaultName, false, filePath, fileType))
    return;

  try {
    SaveOneTimePadAs(fileType, filePath, fileMode);
  } catch (const std::bad_alloc &) {
    ReportExportOutOfMemory("the One-Time Pad key and ciphertext");
  }
}
