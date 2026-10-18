#include "logger.h"
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <fstream>
#include <mutex>

namespace Logger {

static std::ofstream s_logFile;
static std::mutex s_logMutex;
static bool s_initialized = false;
static bool s_enabled = false;

static void WriteLogLine(Level level, const char* module, const char* message) {
    SYSTEMTIME localTime;
    GetLocalTime(&localTime);
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
             localTime.wYear, localTime.wMonth, localTime.wDay, localTime.wHour,
             localTime.wMinute, localTime.wSecond, localTime.wMilliseconds);

    const char* levelText = "UNKNOWN";
    switch (level) {
        case Level::INFO:  levelText = "INFO "; break;
        case Level::WARN:  levelText = "WARN "; break;
        case Level::ERR:   levelText = "ERROR"; break;
    }

    char logLine[4096];
    snprintf(logLine, sizeof(logLine), "[%s] [%s] [%s] %s\n",
             timestamp, levelText, module, message);

    if (s_logFile.is_open()) {
        s_logFile << logLine;
        s_logFile.flush();
    }
    OutputDebugStringA(logLine);
    printf("%s", logLine);
}

void Init() {
    std::lock_guard<std::mutex> lock(s_logMutex);
    if (s_initialized) return;

    s_initialized = true;

    Log(Level::INFO, "Logger", "Logger initialized (File logging waiting for enable)");
}

void Shutdown() {
    std::lock_guard<std::mutex> lock(s_logMutex);
    if (s_logFile.is_open()) {
        s_logFile.close();
    }
    s_initialized = false;
    s_enabled = false;
}

void Log(Level level, const char* module, const char* format, ...) {
    if (!s_enabled) return;

    std::lock_guard<std::mutex> lock(s_logMutex);

    if (!s_enabled) return;

    char message[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if (s_logFile.is_open()) {
        WriteLogLine(level, module, message);
    }
}

}
