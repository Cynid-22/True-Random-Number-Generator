#include <windows.h>
#include <commdlg.h>

#include "format_options.h"
#include "../../core/app_state.h"
#include "../../logging/logger.h"
#include "../../logic/formats/one_time_pad_format.h"
#include "../../logic/logic.h"
#include "../../platform/unicode_path.h"
#include "../widgets.h"
#include "imgui.h"
#include <algorithm>
#include <cstring>
#include <string>

static void ClearSelectedOneTimePadFile() {
  SecureZeroMemory(g_state.oneTimePadFilePath.data(),
                   g_state.oneTimePadFilePath.size());
  g_state.oneTimePadFileSize = 0;
  g_state.oneTimePadFileLastWriteTime = 0;
}

static void BrowseForOneTimePadFile() {
  OPENFILENAMEW dialog;
  wchar_t selectedPath[AppConfig::ONE_TIME_PAD_FILE_PATH_MAX_CHARS] = {0};
  ZeroMemory(&dialog, sizeof(dialog));
  dialog.lStructSize = sizeof(dialog);
  dialog.lpstrFile = selectedPath;
  dialog.nMaxFile = AppConfig::ONE_TIME_PAD_FILE_PATH_MAX_CHARS;
  dialog.lpstrFilter = L"All Files\0*.*\0";
  dialog.nFilterIndex = 1;
  dialog.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

  if (GetOpenFileNameW(&dialog) != TRUE) {
    DWORD dialogError = CommDlgExtendedError();
    if (dialogError != 0) {
      Logger::Log(Logger::Level::ERR, "GUI",
                  "Output Config [OTP]: File dialog failed. Error code: %lu",
                  dialogError);
      ClearSelectedOneTimePadFile();
      UpdateTargetEntropy();
    }
    return;
  }

  std::string utf8Path = Platform::WideToUtf8(selectedPath);
  Platform::FileStamp stamp;
  ClearSelectedOneTimePadFile();
  if (utf8Path.empty() || utf8Path.size() >= g_state.oneTimePadFilePath.size()) {
    Logger::Log(Logger::Level::ERR, "GUI",
                "Output Config [OTP]: Failed to load file. The path could not "
                "be converted.");
  } else {
    memcpy(g_state.oneTimePadFilePath.data(), utf8Path.c_str(),
           utf8Path.size() + 1);
    if (Platform::FileStampOf(g_state.oneTimePadFilePath.data(), stamp)) {
      g_state.oneTimePadFileSize = stamp.size;
      g_state.oneTimePadFileLastWriteTime = stamp.lastWriteTime;
      Logger::Log(Logger::Level::INFO, "GUI",
                  "Output Config [OTP]: File loaded successfully. "
                  "Size: %lld bytes",
                  g_state.oneTimePadFileSize);
    } else {
      Logger::Log(Logger::Level::ERR, "GUI",
                  "Output Config [OTP]: Failed to load file. Error code: %lu",
                  GetLastError());
      ClearSelectedOneTimePadFile();
    }
  }
  UpdateTargetEntropy();
}

static void SelectOneTimePadInputMode(int inputMode, const char *tabName) {
  if (g_state.oneTimePadInputMode == inputMode)
    return;
  Logger::Log(Logger::Level::INFO, "GUI",
              "Output Config [OTP]: Switched to %s", tabName);
  g_state.oneTimePadInputMode = inputMode;
  UpdateTargetEntropy();
}

static int DiscardNonOneTimePadCharacter(ImGuiInputTextCallbackData *data) {
  return CSPRNG::IsOneTimePadCharacter(data->EventChar) ? 0 : 1;
}

static void RenderTextInputTab() {
  if (ImGui::BeginTabItem("Text Input")) {
    SelectOneTimePadInputMode(ONE_TIME_PAD_TEXT_INPUT, "Text Input");
    ImGui::Spacing();
    ImGui::Text("Enter your message:");
    HelpMarker("Only letters (A-Z, a-z), numbers (0-9) and spaces are\n"
               "accepted. Punctuation, symbols and line breaks are blocked\n"
               "when typing or pasting.\n"
               "Spaces are encrypted too. The output uses the same\n"
               "characters, so it can contain spaces at random positions.");
    ImGui::PushStyleColor(ImGuiCol_Text, UiColor::Warning);
    ImGui::TextWrapped("(Note: Only letters A-Z, a-z, numbers 0-9 and spaces. "
                       "No punctuation.)");
    ImGui::PopStyleColor();
    ImGui::TextColored(UiColor::DimText, "(Content is NOT logged)");
    if (ImGui::InputTextMultiline(
            "##OTPMessage", g_state.oneTimePadMessage.data(),
            g_state.oneTimePadMessage.size(), ImVec2(-1, 150),
            ImGuiInputTextFlags_CallbackCharFilter,
            DiscardNonOneTimePadCharacter)) {
      UpdateTargetEntropy();
    }
    ImGui::EndTabItem();
  }
}

static void RenderMessageLengthInputTab() {
  if (ImGui::BeginTabItem("Message Length")) {
    SelectOneTimePadInputMode(ONE_TIME_PAD_MESSAGE_LENGTH_INPUT,
                              "Message Length");
    ImGui::Spacing();
    ImGui::Text("Length of your message:");
    HelpMarker("Generates a pad of this many random characters\n"
               "(A-Z, a-z, 0-9 and space) without typing the message here.\n"
               "Maximum: 2,147,483,647 characters.");
    ImGui::PushStyleColor(ImGuiCol_Text, UiColor::Warning);
    ImGui::TextWrapped("(Note: Enter the total number of characters in your "
                       "message, including spaces.)");
    ImGui::PopStyleColor();
    ImGui::TextColored(UiColor::DimText, "(Length is NOT logged)");
    ImGui::SetNextItemWidth(230);
    if (ImGui::InputInt("##OTPLength", &g_state.oneTimePadMessageLength)) {
      g_state.oneTimePadMessageLength =
          std::clamp(g_state.oneTimePadMessageLength,
                     AppConfig::ONE_TIME_PAD_MIN_MESSAGE_LENGTH,
                     AppConfig::ONE_TIME_PAD_MAX_MESSAGE_LENGTH);
      UpdateTargetEntropy();
    }
    ImGui::EndTabItem();
  }
}

static void RenderFileInputTab() {
  if (ImGui::BeginTabItem("File Input")) {
    SelectOneTimePadInputMode(ONE_TIME_PAD_FILE_INPUT, "File Input");
    ImGui::Spacing();

    ImGui::Text("File to process:");
    ImGui::TextColored(UiColor::DimText,
                       "File names in any language are supported.");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##OTPFilePath", g_state.oneTimePadFilePath.data(),
                     g_state.oneTimePadFilePath.size(),
                     ImGuiInputTextFlags_ReadOnly);

    if (ImGui::Button("Browse File...")) {
      BrowseForOneTimePadFile();
    }

    ImGui::Spacing();
    if (g_state.oneTimePadFileSize > 0) {
      ImGui::Text("File Size: %lld bytes", g_state.oneTimePadFileSize);
    }

    ImGui::EndTabItem();
  }
}

void RenderOneTimePadOptions() {
  BeginOptionRow("Input Data:");

  if (ImGui::BeginTabBar("OTPInputTabs")) {
    RenderTextInputTab();
    RenderMessageLengthInputTab();
    RenderFileInputTab();
    ImGui::EndTabBar();
  }

  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
  ImGui::TextWrapped(
      "Note: This program runs locally. Your message is safe.");
  ImGui::PopStyleColor();
}
