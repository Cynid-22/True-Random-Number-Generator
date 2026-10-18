#include <windows.h>
#include <commdlg.h>

#include "format_options.h"
#include "../../core/app_state.h"
#include "../../logging/logger.h"
#include "../../logic/logic.h"
#include "../widgets.h"
#include "imgui.h"
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

static void RenderTextInputTab() {
  if (ImGui::BeginTabItem("Text Input")) {
    if (g_state.oneTimePadInputMode != ONE_TIME_PAD_TEXT_INPUT)
      Logger::Log(Logger::Level::INFO, "GUI",
                  "Output Config [OTP]: Switched to Text Input");
    g_state.oneTimePadInputMode = ONE_TIME_PAD_TEXT_INPUT;
    ImGui::Spacing();
    ImGui::Text("Enter your message:");
    ImGui::PushStyleColor(ImGuiCol_Text, UiColor::Warning);
    ImGui::Text(
        "(Note: Only supports ASCII characters. Output will be ASCII.)");
    ImGui::PopStyleColor();
    ImGui::TextColored(UiColor::DimText, "(Content is NOT logged)");
    if (ImGui::InputTextMultiline(
            "##OTPMessage", g_state.oneTimePadMessage.data(),
            g_state.oneTimePadMessage.size(), ImVec2(-1, 150))) {
      UpdateTargetEntropy();
    }
    ImGui::EndTabItem();
  }
}

static void RenderFileInputTab() {
  if (ImGui::BeginTabItem("File Input")) {
    if (g_state.oneTimePadInputMode != ONE_TIME_PAD_FILE_INPUT)
      Logger::Log(Logger::Level::INFO, "GUI",
                  "Output Config [OTP]: Switched to File Input");
    g_state.oneTimePadInputMode = ONE_TIME_PAD_FILE_INPUT;
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
    RenderFileInputTab();
    ImGui::EndTabBar();
  }

  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
  ImGui::TextWrapped(
      "Note: This program runs locally. Your message is safe.");
  ImGui::PopStyleColor();
}
