#pragma once

namespace Logger {

enum class Level {
    INFO,
    WARN,
    ERR
};

void Init();

void Shutdown();

void Log(Level level, const char* module, const char* format, ...);

}
