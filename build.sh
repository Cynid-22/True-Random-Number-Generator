#!/bin/bash

echo "========================================"
echo "TRNG Build Script (MinGW)"
echo "========================================"

if ! command -v g++ &> /dev/null; then
    echo "ERROR: g++ not found"
    exit 1
fi

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

echo "Compiling resources..."
windres src/resource.rc -O coff -o build/resource.res

echo "Compiling..."

BUILD_VERSION="${VERSION:-dev}"

g++ -std=c++17 -O2 -s -mwindows \
    -DAPP_VERSION="\"${BUILD_VERSION}\"" \
    -D_WIN32_WINNT=0x0A00 \
    -DIMGUI_USE_WCHAR32 \
    -I"src" -I"src/core" -I"src/gui" -I"src/logic" -I"src/platform" -I"src/logging" -I"src/entropy" \
    -I"src/entropy/clock_drift" -I"src/entropy/cpu_jitter" -I"src/entropy/microphone" -I"src/crypto" \
    -I"external/imgui" -I"external/imgui/backends" \
    -o build/TRNG.exe \
    src/main.cpp \
    src/gui/gui.cpp \
    src/gui/widgets.cpp \
    src/gui/sources/source_widgets.cpp \
    src/gui/sources/input_tab.cpp \
    src/gui/sources/collection_pad.cpp \
    src/gui/output/output_types.cpp \
    src/gui/output/output_config.cpp \
    src/gui/output/number_options.cpp \
    src/gui/output/text_options.cpp \
    src/gui/output/raw_data_options.cpp \
    src/gui/output/uuid_options.cpp \
    src/gui/output/one_time_pad_options.cpp \
    src/gui/output/generation_task.cpp \
    src/gui/output/output_result.cpp \
    src/gui/output/one_time_pad_result.cpp \
    src/logic/logic.cpp \
    src/logic/csprng.cpp \
    src/logic/output_generator.cpp \
    src/logic/output_export.cpp \
    src/logic/formats/number_formats.cpp \
    src/logic/formats/text_formats.cpp \
    src/logic/formats/raw_data_format.cpp \
    src/logic/formats/uuid_formats.cpp \
    src/logic/formats/one_time_pad_format.cpp \
    src/platform/dx11.cpp \
    src/platform/unicode_path.cpp \
    src/logging/logger.cpp \
    src/entropy/clock_drift/clock_drift.cpp \
    src/entropy/cpu_jitter/cpu_jitter.cpp \
    src/entropy/keystroke/keystroke.cpp \
    src/entropy/mouse/mouse.cpp \
    src/entropy/microphone/microphone.cpp \
    src/entropy/pool.cpp \
    src/crypto/sha512.cpp \
    src/crypto/hkdf.cpp \
    src/crypto/chacha20.cpp \
    src/crypto/aes.cpp \
    src/crypto/quad_layer.cpp \
    external/imgui/imgui.cpp \
    external/imgui/imgui_draw.cpp \
    external/imgui/imgui_tables.cpp \
    external/imgui/imgui_widgets.cpp \
    external/imgui/backends/imgui_impl_win32.cpp \
    external/imgui/backends/imgui_impl_dx11.cpp \
    build/resource.res \
    -static \
    -ld3d11 -ld3dcompiler -ldxgi -ldwmapi -lshcore -luser32 -lgdi32 -limm32 -lcomdlg32 -lole32 -loleaut32 -luuid -lmmdevapi -lksuser

if [ $? -eq 0 ]; then
    echo "========================================"
    echo "Build successful!"
    echo "Executable: build/TRNG.exe"
    echo "========================================"
else
    echo "========================================"
    echo "Build FAILED!"
    echo "========================================"
    exit 1
fi
