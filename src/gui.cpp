// TRNG - GUI Implementation
// Contains all GUI rendering functions

#include "gui.h"
#include "app_state.h"
#include <windows.h>
#include <commdlg.h>
#include <cstdio>
#include <ctime>

//=============================================================================
// STYLE SETUP
//=============================================================================

void SetupNativeStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Minimal rounding for native look
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.TabRounding = 0.0f;
    
    // Tight padding like native apps
    style.WindowPadding = ImVec2(8, 8);
    style.FramePadding = ImVec2(6, 4);
    style.ItemSpacing = ImVec2(8, 4);
    style.ItemInnerSpacing = ImVec2(4, 4);
    style.IndentSpacing = 20.0f;
    
    // Borders
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.TabBorderSize = 1.0f;
    
    // Scrollbar
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 10.0f;
    
    // Native dark theme colors (like WizTree)
    ImVec4* colors = style.Colors;
    
    // Window backgrounds - dark grays
    colors[ImGuiCol_WindowBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    
    // Borders - subtle
    colors[ImGuiCol_Border] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    
    // Frame backgrounds
    colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    
    // Title bar
    colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.14f, 0.14f, 0.14f, 0.50f);
    
    // Menu bar
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    
    // Buttons - blue accent like Windows
    colors[ImGuiCol_Button] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    
    // Headers
    colors[ImGuiCol_Header] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    
    // Tabs
    colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    
    // Checkbox/Radio - blue accent
    colors[ImGuiCol_CheckMark] = ImVec4(0.40f, 0.70f, 1.00f, 1.00f);
    
    // Slider
    colors[ImGuiCol_SliderGrab] = ImVec4(0.40f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.50f, 0.80f, 1.00f, 1.00f);
    
    // Scrollbar
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
    
    // Progress bar - blue
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.40f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.50f, 0.80f, 1.00f, 1.00f);
    
    // Text - crisp white/gray
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    
    // Separator
    colors[ImGuiCol_Separator] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    
    // Table
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.10f, 0.10f, 0.10f, 0.50f);
}

//=============================================================================
// MENU BAR
//=============================================================================

void RenderMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Export Output...", "Ctrl+S")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                PostQuitMessage(0);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Options")) {
            ImGui::MenuItem("Auto-start collection", nullptr, &g_state.isCollecting);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Settings")) {
                g_state = AppState();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About TRNG")) {}
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

//=============================================================================
// HELPER: Render Start/Stop Button (used in each tab)
//=============================================================================

static void RenderStartStopButton() {
    if (g_state.isCollecting) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.60f, 0.20f, 0.20f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.25f, 0.25f, 1.00f));
        if (ImGui::Button("Stop Collection", ImVec2(150, 0))) {
            g_state.isCollecting = false;
        }
        ImGui::PopStyleColor(2);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.50f, 0.20f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.60f, 0.25f, 1.00f));
        if (ImGui::Button("Start Collection", ImVec2(150, 0))) {
            g_state.isCollecting = true;
        }
        ImGui::PopStyleColor(2);
    }
}

//=============================================================================
// ENTROPY SOURCE TABS
//=============================================================================

void RenderMicrophoneTab() {
    ImGui::Text("Microphone Noise Collection");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextWrapped("Captures ambient audio noise from the system microphone. "
                       "Environmental thermal noise provides high-quality entropy.");
    ImGui::Spacing();
    
    ImGui::Checkbox("Enable Microphone Collection", &g_state.microphoneEnabled);
    
    if (g_state.microphoneEnabled && g_state.isCollecting) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Status: Active - Collecting entropy...");
    } else if (g_state.microphoneEnabled) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Status: Ready");
    } else {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Status: Disabled");
    }
    
    ImGui::Spacing();
    ImGui::Spacing();
    RenderStartStopButton();
}

void RenderKeystrokeTab() {
    ImGui::Text("Keystroke Dynamics Collection");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextWrapped("Measures timing intervals between keystrokes with microsecond precision. "
                       "Requires user typing to generate entropy.");
    ImGui::Spacing();
    
    ImGui::Checkbox("Enable Keystroke Collection", &g_state.keystrokeEnabled);
    
    if (g_state.keystrokeEnabled && g_state.isCollecting) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Status: Active - Type to generate entropy...");
    } else if (g_state.keystrokeEnabled) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Status: Ready");
    } else {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Status: Disabled");
    }
    
    ImGui::Spacing();
    ImGui::Spacing();
    RenderStartStopButton();
}

void RenderClockDriftTab() {
    ImGui::Text("CPU/System Clock Drift Collection");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextWrapped("Exploits timing inconsistencies between the system clock and "
                       "high-resolution performance counter. Passive collection.");
    ImGui::Spacing();
    
    ImGui::Checkbox("Enable Clock Drift Collection", &g_state.clockDriftEnabled);
    
    if (g_state.clockDriftEnabled && g_state.isCollecting) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Status: Active - Collecting entropy...");
    } else if (g_state.clockDriftEnabled) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Status: Ready");
    } else {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Status: Disabled");
    }
    
    ImGui::Spacing();
    ImGui::Spacing();
    RenderStartStopButton();
}

void RenderCPUJitterTab() {
    ImGui::Text("CPU Execution Jitter Collection");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextWrapped("Measures variations in CPU instruction execution times caused by "
                       "thermal noise, cache effects, and interrupts. Passive collection.");
    ImGui::Spacing();
    
    ImGui::Checkbox("Enable CPU Jitter Collection", &g_state.cpuJitterEnabled);
    
    if (g_state.cpuJitterEnabled && g_state.isCollecting) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Status: Active - Collecting entropy...");
    } else if (g_state.cpuJitterEnabled) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Status: Ready");
    } else {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Status: Disabled");
    }
    
    ImGui::Spacing();
    ImGui::Spacing();
    RenderStartStopButton();
}

void RenderMouseMovementTab() {
    ImGui::Text("Mouse Movement Collection");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextWrapped("Captures mouse position, velocity, and timing. "
                       "Requires user mouse movement to generate entropy.");
    ImGui::Spacing();
    
    ImGui::Checkbox("Enable Mouse Collection", &g_state.mouseMovementEnabled);
    
    if (g_state.mouseMovementEnabled && g_state.isCollecting) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Status: Active - Move mouse to generate entropy...");
    } else if (g_state.mouseMovementEnabled) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Status: Ready");
    } else {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Status: Disabled");
    }
    
    ImGui::Spacing();
    ImGui::Spacing();
    RenderStartStopButton();
}

//=============================================================================
// ENTROPY POOL BAR
//=============================================================================

void RenderEntropyPoolBar() {
    // Entropy progress bar - always visible at top
    float progress = g_state.collectedBits / g_state.targetBits;
    if (progress > 1.0f) progress = 1.0f;
    
    char overlay[64];
    snprintf(overlay, sizeof(overlay), "Entropy: %.0f / %.0f bits", g_state.collectedBits, g_state.targetBits);
    ImGui::ProgressBar(progress, ImVec2(-1, 30), overlay);  // Taller bar for larger font
    
    // Quality indicator
    ImGui::Text("Quality:");
    ImGui::SameLine();
    if (g_state.collectedBits < 128) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Insufficient");
    } else if (g_state.collectedBits < 256) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Minimal");
    } else if (g_state.collectedBits < 512) {
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Good");
    } else {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Excellent");
    }
    
    // Show collection status indicator
    ImGui::SameLine(ImGui::GetWindowWidth() - 120);
    if (g_state.isCollecting) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "[Collecting...]");
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[Stopped]");
    }
    
    ImGui::Separator();
}

//=============================================================================
// OUTPUT CONFIGURATION
//=============================================================================

void RenderOutputConfigSection() {
    ImGui::Text("Output Format:");
    ImGui::Separator();
    
    const char* formats[] = { 
        "Decimal (0 to 1)", 
        "Integer Range", 
        "Binary String", 
        "Custom String", 
        "Bit/Byte Output",
        "Passphrase"
    };
    
    ImGui::SetNextItemWidth(250);
    ImGui::Combo("##Format", &g_state.outputFormat, formats, IM_ARRAYSIZE(formats));
    
    ImGui::Spacing();
    
    // Format-specific parameters
    if (ImGui::BeginTable("OutputConfigTable", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Labels", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Inputs", ImGuiTableColumnFlags_None);

        switch (g_state.outputFormat) {
            case 0: // Decimal
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Decimal digits:");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(230);
                ImGui::InputInt("##Digits", &g_state.decimalDigits);
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
                ImGui::InputInt("##Min", &g_state.integerMin);
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Maximum:");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(230);
                ImGui::InputInt("##Max", &g_state.integerMax);
                break;
                
            case 2: // Binary
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Length:");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(230);
                ImGui::InputInt("##BinLen", &g_state.binaryLength);
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
                ImGui::InputInt("##CustomLen", &g_state.customLength);
                if (g_state.customLength < 1) g_state.customLength = 1;
                if (g_state.customLength > 100000) g_state.customLength = 100000;
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Include:");
                ImGui::TableSetColumnIndex(1);
                ImGui::Checkbox("0-9", &g_state.includeNumbers); ImGui::SameLine();
                ImGui::Checkbox("A-Z", &g_state.includeUppercase); ImGui::SameLine();
                ImGui::Checkbox("a-z", &g_state.includeLowercase); ImGui::SameLine();
                ImGui::Checkbox("Special", &g_state.includeSpecial); ImGui::SameLine();
                ImGui::TextDisabled("(!@#$%^&*()_+-=[]{}|;':\",./<>?)");
                break;
                
            case 4: { // Bit/Byte
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Amount:");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(230);
                ImGui::InputInt("##Amount", &g_state.bitByteAmount);
                if (g_state.bitByteAmount < 1) g_state.bitByteAmount = 1;
                if (g_state.bitByteAmount > 1000000) g_state.bitByteAmount = 1000000;
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Unit:");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(150);
                const char* units[] = { "Bits", "Bytes" };
                ImGui::Combo("##Unit", &g_state.bitByteUnit, units, 2);
                
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
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Word count:");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(230);
                ImGui::InputInt("##WordCount", &g_state.passphraseWordCount);
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
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Word list file:");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextWrapped("%s", g_state.wordListPath);
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(One word per line in text file)");
                
                if (ImGui::Button("Browse...")) {
                    OPENFILENAMEA ofn;
                    char szFile[260] = { 0 };
                    ZeroMemory(&ofn, sizeof(ofn));
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = NULL;
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = sizeof(szFile);
                    ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFileTitle = NULL;
                    ofn.nMaxFileTitle = 0;
                    ofn.lpstrInitialDir = NULL;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                    if (GetOpenFileNameA(&ofn) == TRUE) {
                        strncpy(g_state.wordListPath, ofn.lpstrFile, sizeof(g_state.wordListPath) - 1);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Default")) {
                    strncpy(g_state.wordListPath, "assets/default_wordlist.txt", sizeof(g_state.wordListPath) - 1);
                }
                break;
        }
        ImGui::EndTable();
    }
}

//=============================================================================
// OUTPUT RESULTS
//=============================================================================

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
    bool canGenerate = g_state.collectedBits >= 128;
    
    if (!canGenerate) ImGui::BeginDisabled();
    if (ImGui::Button("Generate", ImVec2(100, 0))) {
        const char* formatNames[] = { "Decimal", "Integer", "Binary", "Custom", "BitByte", "Passphrase" };
        g_state.generatedOutput = "[Placeholder] Format: " + std::string(formatNames[g_state.outputFormat]);
        time_t now = time(nullptr);
        char timeStr[64];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localtime(&now));
        g_state.timestamp = timeStr;
        g_state.entropyConsumed = 64.0f;
    }
    if (!canGenerate) ImGui::EndDisabled();
    
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
        g_state.generatedOutput.clear();
        g_state.timestamp.clear();
    }
    if (!canCopy) ImGui::EndDisabled();
    
    // Status bar
    if (!g_state.timestamp.empty()) {
        ImGui::Text("Generated: %s  |  Entropy used: %.0f bits", 
                    g_state.timestamp.c_str(), g_state.entropyConsumed);
    }
}

//=============================================================================
// SIMULATION (Placeholder - will be replaced with real entropy collection)
//=============================================================================

void SimulateEntropyCollection() {
    if (!g_state.isCollecting) return;
    
    float bitsPerFrame = 0.0f;
    if (g_state.cpuJitterEnabled) bitsPerFrame += 0.15f;
    if (g_state.clockDriftEnabled) bitsPerFrame += 0.08f;
    if (g_state.mouseMovementEnabled) bitsPerFrame += 0.20f;
    if (g_state.keystrokeEnabled) bitsPerFrame += 0.30f;
    if (g_state.microphoneEnabled) bitsPerFrame += 0.40f;
    
    g_state.collectedBits += bitsPerFrame;
}
