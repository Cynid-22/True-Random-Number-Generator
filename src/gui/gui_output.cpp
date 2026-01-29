#include "../../external/imgui/imgui.h"
#include <windows.h>
#include <commdlg.h>

#include "../core/app_state.h"
#include "../entropy/entropy_common.h"
#include "../logging/logger.h"
#include "../logic/logic.h"
#include "../logic/csprng.h"
#include "gui.h"

#include <cstdio>
#include <string>
#include <time.h>

//=============================================================================
// OUTPUT CONFIGURATION
//=============================================================================

void RenderOutputConfigSection() {
  ImGui::Text("Output Configuration");
  ImGui::Separator();
  ImGui::Spacing();

  const char *formats[] = {"Decimal Number (0.0 - 1.0)",
                           "Integer Range",
                           "Binary String",
                           "Custom String",
                           "Bit/Byte Output",
                           "Passphrase",
                           "One-Time Pad"};

  ImGui::SetNextItemWidth(300);
  int prevFormat = g_state.outputFormat;
  if (ImGui::Combo("##Format", &g_state.outputFormat, formats,
                   IM_ARRAYSIZE(formats))) {
    Logger::Log(Logger::Level::INFO, "GUI",
                "Output Format changed from '%s' to '%s'", formats[prevFormat],
                formats[g_state.outputFormat]);
    UpdateTargetEntropy();
  }

  ImGui::Spacing();

  // Format-specific parameters
  if (ImGui::BeginTable("OutputConfigTable", 2,
                        ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Labels", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    ImGui::TableSetupColumn("Inputs", ImGuiTableColumnFlags_None);

    bool parametersChanged = false;

    switch (g_state.outputFormat) {
    case 0: // Decimal
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Decimal digits:");
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(230);
      if (ImGui::InputInt("##Digits", &g_state.decimalDigits)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Decimal]: Digits set to %d",
                    g_state.decimalDigits);
      }
      if (g_state.decimalDigits < 1)
        g_state.decimalDigits = 1;
      if (g_state.decimalDigits > 10000)
        g_state.decimalDigits = 10000;
      break;

    case 1: // Integer
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Minimum:");
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(230);
      if (ImGui::InputInt("##Min", &g_state.integerMin)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Integer]: Min set to %d",
                    g_state.integerMin);
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Maximum:");
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(230);
      if (ImGui::InputInt("##Max", &g_state.integerMax)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Integer]: Max set to %d",
                    g_state.integerMax);
      }
      break;

    case 2: // Binary
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Length:");
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(230);
      if (ImGui::InputInt("##BinLen", &g_state.binaryLength)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Binary]: Length set to %d",
                    g_state.binaryLength);
      }
      if (g_state.binaryLength < 1)
        g_state.binaryLength = 1;
      if (g_state.binaryLength > 100000)
        g_state.binaryLength = 100000;
      break;

    case 3: // Custom
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Length:");
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(230);
      if (ImGui::InputInt("##CustomLen", &g_state.customLength)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Custom]: Length set to %d",
                    g_state.customLength);
      }
      if (g_state.customLength < 1)
        g_state.customLength = 1;
      if (g_state.customLength > 100000)
        g_state.customLength = 100000;

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Include:");
      ImGui::TableSetColumnIndex(1);
      if (ImGui::Checkbox("0-9", &g_state.includeNumbers)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Custom]: 0-9 toggled %s",
                    g_state.includeNumbers ? "ON" : "OFF");
      }
      ImGui::SameLine();
      if (ImGui::Checkbox("A-Z", &g_state.includeUppercase)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Custom]: A-Z toggled %s",
                    g_state.includeUppercase ? "ON" : "OFF");
      }
      ImGui::SameLine();
      if (ImGui::Checkbox("a-z", &g_state.includeLowercase)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Custom]: a-z toggled %s",
                    g_state.includeLowercase ? "ON" : "OFF");
      }
      ImGui::SameLine();
      if (ImGui::Checkbox("Special", &g_state.includeSpecial)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Custom]: Special toggled %s",
                    g_state.includeSpecial ? "ON" : "OFF");
      }
      ImGui::SameLine();
      ImGui::TextDisabled("(!@#$%^&*()_+-=[]{}|;':\",./<>?)");
      break;

    case 4: { // Bit/Byte
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Amount:");
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(230);
      if (ImGui::InputInt("##Amount", &g_state.bitByteAmount)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Bit/Byte]: Amount set to %d",
                    g_state.bitByteAmount);
      }
      if (g_state.bitByteAmount < 1)
        g_state.bitByteAmount = 1;
      if (g_state.bitByteAmount > 1000000)
        g_state.bitByteAmount = 1000000;

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Unit:");
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(150);
      const char *units[] = {"Bits", "Bytes"};
      if (ImGui::Combo("##Unit", &g_state.bitByteUnit, units, 2)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Bit/Byte]: Unit set to %s",
                    units[g_state.bitByteUnit]);
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Format:");
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(180);
      const char *outFmts[] = {"Hexadecimal", "Base64", "Binary"};
      if (ImGui::Combo("##OutFmt", &g_state.bitByteFormat, outFmts, 3)) {
        // Note: bitByteFormat logic might need update in core if used elsewhere
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Bit/Byte]: Format set to %s",
                    outFmts[g_state.bitByteFormat]);
      }
      
      // Inline Binary Separator Config
      if (g_state.bitByteFormat == 2) { // Binary only
        ImGui::SameLine();
        ImGui::Text("|");
        ImGui::SameLine();
        if (ImGui::Checkbox("Separator", &g_state.binarySeparatorEnabled)) {
             Logger::Log(Logger::Level::INFO, "GUI", "Output Config [Binary]: Separator toggled %s", g_state.binarySeparatorEnabled ? "ON" : "OFF");
        }
        if (g_state.binarySeparatorEnabled) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(150); 
            if (ImGui::InputInt("bits", &g_state.binarySeparatorInterval)) {
                if (g_state.binarySeparatorInterval < 1) g_state.binarySeparatorInterval = 1;
            }
        }
      }
      break;
    }

    case 5: // Passphrase
    {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Word count:");
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(230);
      if (ImGui::InputInt("##WordCount", &g_state.passphraseWordCount)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Passphrase]: Word count set to %d",
                    g_state.passphraseWordCount);
      }
      if (g_state.passphraseWordCount < 1)
        g_state.passphraseWordCount = 1;
      if (g_state.passphraseWordCount > 100)
        g_state.passphraseWordCount = 100;

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Separator:");
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(150);
      if (ImGui::InputText("##Separator", g_state.passphraseSeparator,
                           sizeof(g_state.passphraseSeparator))) {
        // InputText returns true on every char update, might be spammy?
        // User asked for "what did they change it to", so logging final state
        // or every char? Let's log but maybe user should press enter? relying
        // on standard behavior. To avoid spam, we could checking
        // ImGui::IsItemDeactivatedAfterEdit() but standard return is fine for
        // now if not typing fast.
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Passphrase]: Separator changed");
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TableSetColumnIndex(1);
      ImGui::TextColored(
          ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
          "(Using built-in wordlist: 123,565 words, ~16.5 bits/word)");
      break;
    }

    case 6: // One-Time Pad
    {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Input Data:");

      ImGui::TableSetColumnIndex(1);

      // Tab bar for input method
      if (ImGui::BeginTabBar("OTPInputTabs")) {
        if (ImGui::BeginTabItem("Text Input")) {
          if (g_state.otpInputMode != 0)
            Logger::Log(Logger::Level::INFO, "GUI",
                        "Output Config [OTP]: Switched to Text Input");
          g_state.otpInputMode = 0;
          ImGui::Spacing();
          ImGui::Text("Enter your message:");
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
          ImGui::Text("(Note: Only supports ASCII characters. Output will be ASCII.)");
          ImGui::PopStyleColor();
          ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                             "(Content is NOT logged)");
          if (ImGui::InputTextMultiline("##OTPMessage", g_state.otpMessage,
                                        sizeof(g_state.otpMessage),
                                        ImVec2(-1, 150))) {
            UpdateTargetEntropy();
            // SECURITY: DO NOT LOG MESSAGE CONTENT
            Logger::Log(
                Logger::Level::DEBUG, "GUI",
                "Output Config [OTP]: Message content updated (length: %zu)",
                strlen(g_state.otpMessage));
          }
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("File Input")) {
          if (g_state.otpInputMode != 1)
            Logger::Log(Logger::Level::INFO, "GUI",
                        "Output Config [OTP]: Switched to File Input");
          g_state.otpInputMode = 1;
          ImGui::Spacing();

          ImGui::Text("File to process:");
          ImGui::SetNextItemWidth(-1);
          ImGui::InputText("##OTPFilePath", g_state.otpFilePath,
                           sizeof(g_state.otpFilePath),
                           ImGuiInputTextFlags_ReadOnly);

          if (ImGui::Button("Browse File...")) {
            OPENFILENAMEA ofn;
            char szFile[512] = {0};
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = NULL;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = "All Files\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrFileTitle = NULL;
            ofn.nMaxFileTitle = 0;
            ofn.lpstrInitialDir = NULL;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileNameA(&ofn) == TRUE) {
              strncpy(g_state.otpFilePath, ofn.lpstrFile,
                      sizeof(g_state.otpFilePath) - 1);

              HANDLE hFile = CreateFileA(g_state.otpFilePath, GENERIC_READ,
                                         FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                         FILE_ATTRIBUTE_NORMAL, NULL);
              if (hFile != INVALID_HANDLE_VALUE) {
                LARGE_INTEGER size;
                if (GetFileSizeEx(hFile, &size)) {
                  g_state.otpFileSize = size.QuadPart;
                  Logger::Log(Logger::Level::INFO, "GUI",
                              "Output Config [OTP]: File loaded successfully. "
                              "Size: %lld bytes",
                              g_state.otpFileSize);
                }
                CloseHandle(hFile);
              } else {
                Logger::Log(
                    Logger::Level::ERR, "GUI",
                    "Output Config [OTP]: Failed to load file. Error code: %d",
                    GetLastError());
              }
              UpdateTargetEntropy();
            }
          }

          ImGui::Spacing();
          if (g_state.otpFileSize > 0) {
            ImGui::Text("File Size: %lld bytes", g_state.otpFileSize);
          }

          ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
      }

      ImGui::Spacing();
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
      ImGui::TextWrapped(
          "Note: This program runs locally. Your message is safe.");
      ImGui::PopStyleColor();
      break;
    }
    }

    ImGui::EndTable();

    if (parametersChanged) {
      UpdateTargetEntropy();
    }
  }

  ImGui::Spacing();
  ImGui::Separator();

  // Display required entropy
  ImGui::Text("Current Entropy Requirement:");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "%.0f bits",
                     g_state.targetBits);

  if (g_state.targetBits <= 512.0f) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                       "(Minimum Base Security)");
  }
}

void RenderOutputSection() {
  ImGui::Spacing();
  ImGui::Text("Generated Output:");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(Result is NOT logged)");
  ImGui::Separator();

  // Output text box (with text wrapping for long output)
  // Use a child window with TextWrapped for proper wrapping
  // Size.y = -160.0f leaves space for the buttons and footer at the bottom
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 1.00f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
  if (ImGui::BeginChild("##OutputChild", ImVec2(-1, -160.0f), true)) {
    if (!g_state.generatedOutput.empty()) {
       // Using TextWrapped for native wrapping behavior (non-selectable)
      ImGui::TextWrapped("%s", g_state.generatedOutput.c_str());
    }
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();

  // Buttons
  // Check entropy conditions
  bool hasMinimumEntropy = g_state.collectedBits >= 512.0f;
  bool hasFullEntropy = g_state.collectedBits >= g_state.targetBits;

  // Warning messages based on entropy level
  if (!hasMinimumEntropy) {
    // Block: Not enough entropy at all
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
    ImGui::Text("BLOCKED: Collect at least 512 bits before generating.");
    ImGui::PopStyleColor();
    ImGui::Spacing();
  } else if (!hasFullEntropy) {
    // Warning: Can generate but using PSEUDO-RANDOM (expansion mode)
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
    ImGui::Text("WARNING: Using PSEUDO-RANDOM mode (CSPRNG expansion). Collect "
                "more for TRUE RANDOMNESS.");
    ImGui::PopStyleColor();
    ImGui::Spacing();
  }

  // Only disable if below 512 bits (minimum base security)
  if (!hasMinimumEntropy)
    ImGui::BeginDisabled();
  if (ImGui::Button("Generate", ImVec2(100, 0))) {
    // Check if logging was ever enabled during this session
    if (g_state.loggingWasEverEnabled) {
      // Show warning window instead of generating immediately
      g_state.showLoggingWarningWindow = true;
      g_state.loggingWarningCountdown = 5.0f; // Reset countdown
    } else {
      // Check OTP mode consolidation requirement
      if (g_state.outputFormat == 6 && !hasFullEntropy) {
        // OTP requires TRUE RANDOMNESS (consolidation mode)
        g_state.generatedOutput = "[ERROR] One-Time Pad requires TRUE RANDOMNESS mode.\n"
                                  "Collect more entropy until the bar reaches 100%.";
        g_state.entropyConsumed = 0.0f;
      } else {
        // Proceed with actual CSPRNG generation
        CSPRNG::GenerationResult result = CSPRNG::GenerateOutput();
        
        if (result.success) {
          g_state.generatedOutput = result.output;
          g_state.entropyConsumed = result.entropyConsumed;
          
          // Generate timestamp for output
          time_t now = time(nullptr);
          char timeStr[64];
          strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S (UTC%z)",
                   localtime(&now));
          g_state.timestamp = timeStr;
          
          // Add mode indicator to log
          const char* modeStr = (result.mode == CSPRNG::GenerationMode::Consolidation)
                                ? "TRUE RANDOMNESS" : "PSEUDO-RANDOM";
          Logger::Log(Logger::Level::INFO, "GUI",
                      "Generated output using %s mode, consumed %.1f bits",
                      modeStr, result.entropyConsumed);
        } else {
          g_state.generatedOutput = "[ERROR] " + result.errorMessage;
          g_state.entropyConsumed = 0.0f;
          Logger::Log(Logger::Level::ERR, "GUI",
                      "Generation failed: %s", result.errorMessage.c_str());
        }
      }

      // LOCK-IN: Lock all currently used entropy
      g_state.lockedDataTimestamp = Entropy::GetNanosecondTimestamp();
      Logger::Log(Logger::Level::INFO, "GUI",
                  "Entropy locked at timestamp: %llu",
                  g_state.lockedDataTimestamp);
    }
  }
  if (!hasMinimumEntropy)
    ImGui::EndDisabled();

  ImGui::SameLine();

  bool canCopy = !g_state.generatedOutput.empty();
  if (!canCopy)
    ImGui::BeginDisabled();
  if (ImGui::Button("Copy", ImVec2(80, 0))) {
    ImGui::SetClipboardText(g_state.generatedOutput.c_str());
  }
  if (!canCopy)
    ImGui::EndDisabled();

  ImGui::SameLine();

  if (!canCopy)
    ImGui::BeginDisabled();
  if (ImGui::Button("Clear", ImVec2(80, 0))) {
    g_state.generatedOutput = "";
    g_state.entropyConsumed = 0.0f;
    g_state.timestamp = "";
  }
  if (!canCopy)
    ImGui::EndDisabled();

  // Note about time-based uniqueness
  ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                     "Note: The current time is mixed into the seed for uniqueness.");

  // Info footer
  if (!g_state.timestamp.empty()) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Generated at: %s",
                       g_state.timestamp.c_str());
  }
}

//=============================================================================
// LOGGING WARNING WINDOW
//=============================================================================

void RenderLoggingWarningWindow() {
  if (!g_state.showLoggingWarningWindow)
    return;

  // Update countdown timer
  ImGuiIO &io = ImGui::GetIO();
  if (g_state.loggingWarningCountdown > 0.0f) {
    g_state.loggingWarningCountdown -= io.DeltaTime;
    if (g_state.loggingWarningCountdown < 0.0f) {
      g_state.loggingWarningCountdown = 0.0f;
    }
  }

  // Center the window
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(600, 580), ImGuiCond_FirstUseEver);

  // Make it modal (blocking) - OpenPopup must be called before BeginPopupModal
  if (!ImGui::IsPopupOpen("Logging Warning")) {
    ImGui::OpenPopup("Logging Warning");
  }

  if (ImGui::BeginPopupModal(
          "Logging Warning", &g_state.showLoggingWarningWindow,
          ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {

    // Title with warning icon
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
    ImGui::Text("SECURITY WARNING: Logging Mode Detected");
    ImGui::PopStyleColor();

    ImGui::Separator();
    ImGui::Spacing();

    // Risks section
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "RISKS:");
    ImGui::Spacing();
    ImGui::BulletText("Entropy data may have been compromised");
    ImGui::BulletText("Randomness cannot be fully ensured");
    ImGui::BulletText("Log files may contain sensitive entropy data");
    ImGui::BulletText("Security of generated output is compromised");
    ImGui::BulletText("Log files persist on disk and may be recoverable");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Recommendations section
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "RECOMMENDATIONS:");
    ImGui::Spacing();
    ImGui::BulletText("Turn off logging mode if it is not already off");
    ImGui::BulletText("Delete all recorded entropy data");
    ImGui::BulletText("Record entropy data again with logging disabled");
    ImGui::BulletText("Only use logging mode for debugging purposes");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Generate Anyway button with countdown
    bool canGenerate = (g_state.loggingWarningCountdown <= 0.0f);

    // Center the buttons
    float buttonWidth1 =
        220.0f; // Width for "Generate Anyway (X.X)" with padding
    float buttonWidth2 = 100.0f; // Width for Cancel button
    float totalButtonWidth =
        buttonWidth1 + 20.0f + buttonWidth2; // 20px spacing between buttons
    float startX = (ImGui::GetWindowWidth() - totalButtonWidth) * 0.5f;

    ImGui::SetCursorPosX(startX);

    if (!canGenerate) {
      ImGui::BeginDisabled();
    }

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(0.7f, 0.25f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          ImVec4(0.8f, 0.3f, 0.3f, 1.0f));

    char buttonText[64];
    if (canGenerate) {
      snprintf(buttonText, sizeof(buttonText), "Generate Anyway");
    } else {
      snprintf(buttonText, sizeof(buttonText), "Generate Anyway (%.1f)",
               g_state.loggingWarningCountdown);
    }

    if (ImGui::Button(buttonText, ImVec2(buttonWidth1, 40))) {
      // Check OTP consolidation requirement
      bool hasFullEntropy = g_state.collectedBits >= g_state.targetBits;
      
      if (g_state.outputFormat == 6 && !hasFullEntropy) {
        // OTP requires TRUE RANDOMNESS (consolidation mode)
        g_state.generatedOutput = "[ERROR] One-Time Pad requires TRUE RANDOMNESS mode.\n"
                                  "Collect more entropy until the bar reaches 100%.";
        g_state.entropyConsumed = 0.0f;
      } else {
        // Proceed with actual CSPRNG generation
        CSPRNG::GenerationResult result = CSPRNG::GenerateOutput();
        
        if (result.success) {
          g_state.generatedOutput = result.output;
          g_state.entropyConsumed = result.entropyConsumed;
          
          time_t now = time(nullptr);
          char timeStr[64];
          strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S (UTC%z)",
                   localtime(&now));
          g_state.timestamp = timeStr;
          
          const char* modeStr = (result.mode == CSPRNG::GenerationMode::Consolidation)
                                ? "TRUE RANDOMNESS" : "PSEUDO-RANDOM";
          Logger::Log(Logger::Level::INFO, "GUI",
                      "Generated output using %s mode, consumed %.1f bits",
                      modeStr, result.entropyConsumed);
        } else {
          g_state.generatedOutput = "[ERROR] " + result.errorMessage;
          g_state.entropyConsumed = 0.0f;
          Logger::Log(Logger::Level::ERR, "GUI",
                      "Generation failed: %s", result.errorMessage.c_str());
        }
      }

      // LOCK-IN: Lock all currently used entropy
      g_state.lockedDataTimestamp = Entropy::GetNanosecondTimestamp();
      Logger::Log(Logger::Level::INFO, "GUI",
                  "Entropy locked at timestamp: %llu",
                  g_state.lockedDataTimestamp);

      // Close the warning window
      g_state.showLoggingWarningWindow = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::PopStyleColor(3);

    if (!canGenerate) {
      ImGui::EndDisabled();
    }

    ImGui::SameLine();

    // Cancel button
    if (ImGui::Button("Cancel", ImVec2(buttonWidth2, 40))) {
      g_state.showLoggingWarningWindow = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  } else {
    // Popup was closed, reset the flag
    g_state.showLoggingWarningWindow = false;
  }
}
