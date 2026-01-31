// TRNG - GUI Rendering Header
// Contains all GUI-related function declarations

#pragma once

#include "imgui.h"

//=============================================================================
// STYLE SETUP
//=============================================================================
void SetupNativeStyle();

//=============================================================================
// MENU BAR
//=============================================================================
void RenderMenuBar();


//=============================================================================
// ENTROPY SOURCE TABS
//=============================================================================
void RenderUserInputTab();
void RenderSystemInputTab();
void RenderCollectionWindow();

//=============================================================================
// ENTROPY POOL BAR (Always visible)
//=============================================================================
void RenderEntropyPoolBar();

//=============================================================================
// OUTPUT CONFIGURATION & RESULTS
//=============================================================================
void RenderOutputConfigSection();
void RenderOutputSection();
// void RenderLoggingWarningWindow();

//=============================================================================
// SIMULATION (Placeholder)
//=============================================================================
void SimulateEntropyCollection();
