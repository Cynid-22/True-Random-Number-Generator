#include "gui.h"
#include "../core/app_state.h"
#include "imgui.h"
#include "../logic/logic.h"
#include <windows.h>
#include <commdlg.h>
#include <string>
#include <ctime>
#include <cstdio>

//=============================================================================
// OUTPUT CONFIGURATION
//=============================================================================

void RenderOutputConfigSection() {
    ImGui::Text("Output Configuration");
    ImGui::Separator();
    ImGui::Spacing();
    
    const char* formats[] = { 
        "Decimal Number (0.0 - 1.0)", 
        "Integer Range", 
        "Binary String", 
        "Custom String", 
        "Bit/Byte Output",
        "Passphrase",
        "One-Time Pad"
    };
    
    ImGui::SetNextItemWidth(300);
    if (ImGui::Combo("##Format", &g_state.outputFormat, formats, IM_ARRAYSIZE(formats))) {
        UpdateTargetEntropy();
    }
    
    ImGui::Spacing();
    
    // Format-specific parameters
    if (ImGui::BeginTable("OutputConfigTable", 2, ImGuiTableFlags_SizingStretchProp)) {
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
                if (ImGui::InputInt("##Digits", &g_state.decimalDigits)) parametersChanged = true;
                if (g_state.decimalDigits < 1) g_state.decimalDigits = 1;
                if (g_state.decimalDigits > 10000) g_state.decimalDigits = 10000;
                break;
                
            case 1: // Integer
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Minimum:");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(230);
                if (ImGui::InputInt("##Min", &g_state.integerMin)) parametersChanged = true;
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Maximum:");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(230);
                if (ImGui::InputInt("##Max", &g_state.integerMax)) parametersChanged = true;
                break;
                
            case 2: // Binary
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Length:");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(230);
                if (ImGui::InputInt("##BinLen", &g_state.binaryLength)) parametersChanged = true;
                if (g_state.binaryLength < 1) g_state.binaryLength = 1;
                if (g_state.binaryLength > 100000) g_state.binaryLength = 100000;
                break;
                
            case 3: // Custom
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Length:");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(230);
                if (ImGui::InputInt("##CustomLen", &g_state.customLength)) parametersChanged = true;
                if (g_state.customLength < 1) g_state.customLength = 1;
                if (g_state.customLength > 100000) g_state.customLength = 100000;
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Include:");
                ImGui::TableSetColumnIndex(1);
                if (ImGui::Checkbox("0-9", &g_state.includeNumbers)) parametersChanged = true; ImGui::SameLine();
                if (ImGui::Checkbox("A-Z", &g_state.includeUppercase)) parametersChanged = true; ImGui::SameLine();
                if (ImGui::Checkbox("a-z", &g_state.includeLowercase)) parametersChanged = true; ImGui::SameLine();
                if (ImGui::Checkbox("Special", &g_state.includeSpecial)) parametersChanged = true; ImGui::SameLine();
                ImGui::TextDisabled("(!@#$%^&*()_+-=[]{}|;':\",./<>?)");
                break;
                
            case 4: { // Bit/Byte
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Amount:");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(230);
                if (ImGui::InputInt("##Amount", &g_state.bitByteAmount)) parametersChanged = true;
                if (g_state.bitByteAmount < 1) g_state.bitByteAmount = 1;
                if (g_state.bitByteAmount > 1000000) g_state.bitByteAmount = 1000000;
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Unit:");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(150);
                const char* units[] = { "Bits", "Bytes" };
                if (ImGui::Combo("##Unit", &g_state.bitByteUnit, units, 2)) parametersChanged = true;
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Format:");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(180);
                const char* outFmts[] = { "Hexadecimal", "Base64", "Binary" };
                ImGui::Combo("##OutFmt", &g_state.bitByteFormat, outFmts, 3);
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
                if (ImGui::InputInt("##WordCount", &g_state.passphraseWordCount)) parametersChanged = true;
                if (g_state.passphraseWordCount < 1) g_state.passphraseWordCount = 1;
                if (g_state.passphraseWordCount > 100) g_state.passphraseWordCount = 100;
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Separator:");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(150);
                ImGui::InputText("##Separator", g_state.passphraseSeparator, sizeof(g_state.passphraseSeparator));
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(Using built-in wordlist: 123,565 words, ~16.5 bits/word)");
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
                        g_state.otpInputMode = 0;
                        ImGui::Spacing();
                        ImGui::Text("Enter your message:");
                        if (ImGui::InputTextMultiline("##OTPMessage", g_state.otpMessage, sizeof(g_state.otpMessage), 
                                                      ImVec2(-1, 150))) {
                             UpdateTargetEntropy();
                        }
                        ImGui::EndTabItem();
                    }
                    
                    if (ImGui::BeginTabItem("File Input")) {
                        g_state.otpInputMode = 1;
                        ImGui::Spacing();
                        
                        ImGui::Text("File to process:");
                        ImGui::SetNextItemWidth(-1);
                        ImGui::InputText("##OTPFilePath", g_state.otpFilePath, sizeof(g_state.otpFilePath), ImGuiInputTextFlags_ReadOnly);
                        
                        if (ImGui::Button("Browse File...")) {
                             OPENFILENAMEA ofn;
                             char szFile[512] = { 0 };
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
                                 strncpy(g_state.otpFilePath, ofn.lpstrFile, sizeof(g_state.otpFilePath) - 1);
                                 
                                 // Get file size
                                 HANDLE hFile = CreateFileA(g_state.otpFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                                 if (hFile != INVALID_HANDLE_VALUE) {
                                     LARGE_INTEGER size;
                                     if (GetFileSizeEx(hFile, &size)) {
                                         g_state.otpFileSize = size.QuadPart;
                                     }
                                     CloseHandle(hFile);
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
                ImGui::TextWrapped("Note: This program runs locally. Your message is safe.");
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
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "%.0f bits", g_state.targetBits);
    
    if (g_state.targetBits <= 512.0f) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(Minimum Base Security)");
    }
}

void RenderOutputSection() {
    ImGui::Spacing();
    ImGui::Text("Generated Output:");
    ImGui::Separator();
    
    // Output text box
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.10f, 0.10f, 1.00f));
    char buffer[4096] = {0};
    strncpy(buffer, g_state.generatedOutput.c_str(), sizeof(buffer) - 1);
    ImGui::InputTextMultiline("##Output", buffer, sizeof(buffer), 
                              ImVec2(-1, 100), ImGuiInputTextFlags_ReadOnly);
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
        ImGui::Text("WARNING: Using PSEUDO-RANDOM mode (CSPRNG expansion). Collect more for TRUE RANDOMNESS.");
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }
    
    // Only disable if below 512 bits (minimum base security)
    if (!hasMinimumEntropy) ImGui::BeginDisabled();
    if (ImGui::Button("Generate", ImVec2(100, 0))) {
        const char* formatNames[] = { "Decimal", "Integer", "Binary", "Custom", "BitByte", "Passphrase", "OneTimePad" };
        g_state.generatedOutput = "[Placeholder] Format: " + std::string(formatNames[g_state.outputFormat]);
        time_t now = time(nullptr);
        char timeStr[64];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S (UTC%z)", localtime(&now));
        g_state.timestamp = timeStr;
        g_state.entropyConsumed = 64.0f;
    }
    if (!hasMinimumEntropy) ImGui::EndDisabled();
    
    ImGui::SameLine();
    
    bool canCopy = !g_state.generatedOutput.empty();
    if (!canCopy) ImGui::BeginDisabled();
    if (ImGui::Button("Copy", ImVec2(80, 0))) {
        ImGui::SetClipboardText(g_state.generatedOutput.c_str());
    }
    if (!canCopy) ImGui::EndDisabled();
    
    ImGui::SameLine();
    
    if (!canCopy) ImGui::BeginDisabled();
    if (ImGui::Button("Clear", ImVec2(80, 0))) {
        g_state.generatedOutput = "";
        g_state.entropyConsumed = 0.0f;
        g_state.timestamp = "";
    }
    if (!canCopy) ImGui::EndDisabled();
    
    // Info footer
    if (!g_state.timestamp.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Generated at: %s", g_state.timestamp.c_str());
        ImGui::SameLine(ImGui::GetWindowWidth() - 280);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Entropy Consumed: %.1f bits", g_state.entropyConsumed);
    }
}
