#include <windows.h>
#include <commdlg.h>

#include "format_options.h"
#include "../../core/app_state.h"
#include "../../logging/logger.h"
#include "../../logic/formats/one_time_pad_format.h"
#include "../../logic/logic.h"
#include "../widgets.h"
#include "imgui.h"
#include <algorithm>
#include <cstring>

static void BrowseForOneTimePadFile() {
  OPENFILENAMEA dialog;
  char selectedPath[512] = {0};
  ZeroMemory(&dialog, sizeof(dialog));
  dialog.lStructSize = sizeof(dialog);
  dialog.lpstrFile = selectedPath;
  dialog.nMaxFile = sizeof(selectedPath);
  dialog.lpstrFilter = "All Files\0*.*\0";
  dialog.nFilterIndex = 1;
  dialog.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

  if (GetOpenFileNameA(&dialog) == TRUE) {
    strncpy(g_state.oneTimePadFilePath.data(), dialog.lpstrFile,
            g_state.oneTimePadFilePath.size() - 1);
    g_state.oneTimePadFilePath[g_state.oneTimePadFilePath.size() - 1] = '\0';

    HANDLE fileHandle = CreateFileA(
        g_state.oneTimePadFilePath.data(), GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle != INVALID_HANDLE_VALUE) {
      LARGE_INTEGER fileSize;
      if (GetFileSizeEx(fileHandle, &fileSize)) {
        g_state.oneTimePadFileSize = fileSize.QuadPart;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [OTP]: File loaded successfully. "
                    "Size: %lld bytes",
                    g_state.oneTimePadFileSize);
      }
      CloseHandle(fileHandle);
    } else {
      Logger::Log(
          Logger::Level::ERR, "GUI",
          "Output Config [OTP]: Failed to load file. Error code: %d",
          GetLastError());
    }
    UpdateTargetEntropy();
  }
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
