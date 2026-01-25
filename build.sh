#!/bin/bash
# TRNG Build Script for MSYS2/MinGW-w64
#
# Source Files:
#   src/main.cpp              - Application entry point and main loop
#   src/gui/gui.cpp           - GUI rendering functions
#   src/gui/gui_sources.cpp   - Entropy source tabs
#   src/gui/gui_output.cpp    - Output configuration
#   src/logic/logic.cpp       - Entropy calculation
#   src/platform/dx11.cpp     - DirectX 11 helpers
#
# Header Files:
#   src/core/app_state.h      - Application state definition
#   src/gui/gui.h             - GUI function declarations
#   src/logic/logic.h         - Logic declarations
#   src/platform/dx11.h       - DirectX helper declarations

echo "========================================"
echo "TRNG Build Script (MinGW)"
echo "========================================"

if ! command -v g++ &> /dev/null; then
    echo "ERROR: g++ not found"
    exit 1
fi

# Download ImGui if needed
if [ ! -d "external/imgui" ]; then
    echo "Downloading Dear ImGui..."
    mkdir -p external/imgui/backends
    curl -L -o external/imgui/imgui.h https://raw.githubusercontent.com/ocornut/imgui/master/imgui.h
    curl -L -o external/imgui/imgui.cpp https://raw.githubusercontent.com/ocornut/imgui/master/imgui.cpp
    curl -L -o external/imgui/imgui_internal.h https://raw.githubusercontent.com/ocornut/imgui/master/imgui_internal.h
    curl -L -o external/imgui/imgui_draw.cpp https://raw.githubusercontent.com/ocornut/imgui/master/imgui_draw.cpp
    curl -L -o external/imgui/imgui_tables.cpp https://raw.githubusercontent.com/ocornut/imgui/master/imgui_tables.cpp
    curl -L -o external/imgui/imgui_widgets.cpp https://raw.githubusercontent.com/ocornut/imgui/master/imgui_widgets.cpp
    curl -L -o external/imgui/imstb_rectpack.h https://raw.githubusercontent.com/ocornut/imgui/master/imstb_rectpack.h
    curl -L -o external/imgui/imstb_textedit.h https://raw.githubusercontent.com/ocornut/imgui/master/imstb_textedit.h
    curl -L -o external/imgui/imstb_truetype.h https://raw.githubusercontent.com/ocornut/imgui/master/imstb_truetype.h
    curl -L -o external/imgui/imconfig.h https://raw.githubusercontent.com/ocornut/imgui/master/imconfig.h
    curl -L -o external/imgui/backends/imgui_impl_win32.h https://raw.githubusercontent.com/ocornut/imgui/master/backends/imgui_impl_win32.h
    curl -L -o external/imgui/backends/imgui_impl_win32.cpp https://raw.githubusercontent.com/ocornut/imgui/master/backends/imgui_impl_win32.cpp
    curl -L -o external/imgui/backends/imgui_impl_dx11.h https://raw.githubusercontent.com/ocornut/imgui/master/backends/imgui_impl_dx11.h
    curl -L -o external/imgui/backends/imgui_impl_dx11.cpp https://raw.githubusercontent.com/ocornut/imgui/master/backends/imgui_impl_dx11.cpp
    echo "ImGui downloaded!"
fi

mkdir -p build

echo "Compiling..."

# Compile all source files
g++ -std=c++17 -O2 -mwindows \
    -I"src" -I"src/core" -I"src/gui" -I"src/logic" -I"src/platform" -I"src/logging" -I"src/entropy" \
    -I"src/entropy/clock_drift" \
    -I"external/imgui" -I"external/imgui/backends" \
    -o build/TRNG.exe \
    src/main.cpp \
    src/gui/gui.cpp \
    src/gui/gui_sources.cpp \
    src/gui/gui_output.cpp \
    src/logic/logic.cpp \
    src/platform/dx11.cpp \
    src/logging/logger.cpp \
    src/entropy/clock_drift/clock_drift.cpp \
    src/entropy/pool.cpp \
    external/imgui/imgui.cpp \
    external/imgui/imgui_draw.cpp \
    external/imgui/imgui_tables.cpp \
    external/imgui/imgui_widgets.cpp \
    external/imgui/backends/imgui_impl_win32.cpp \
    external/imgui/backends/imgui_impl_dx11.cpp \
    -static \
    -ld3d11 -ld3dcompiler -ldxgi -ldwmapi -lshcore -luser32 -lgdi32 -limm32

if [ $? -eq 0 ]; then
    echo "========================================"
    echo "Build successful!"
    echo "Executable: build/TRNG.exe"
    echo "========================================"
else
    echo "========================================"
    echo "Build FAILED!"
    echo "========================================"
fi
