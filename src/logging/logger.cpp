#include "logger.h"
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>
#include <mutex>
#include <filesystem>

namespace Logger {

static std::ofstream g_logFile;
static std::mutex g_logMutex;
static bool g_initialized = false;
static bool g_enabled = false;
static std::string g_logDir;
static std::string g_currentLogPath;

void Init(const char* logDir) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_initialized) return;
    
    // Store directory for later use when enabling
    g_logDir = logDir;
    g_initialized = true;
    
    // Create directory now to ensure permissions/existence
    // MOVED to SetEnabled to prevent empty folder creation if feature unused.
    // std::filesystem::create_directories(g_logDir, ec);
    
    // Do NOT open file yet. Wait for SetEnabled(true).
    Log(Level::INFO, "Logger", "Logger initialized (File logging waiting for enable)");
}

void Shutdown() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logFile.is_open()) {
        g_logFile.close();
    }
    g_initialized = false;
    g_enabled = false;
}

void SetEnabled(bool enabled) {
    // Wrapper to handle locking correctly for the "log state change" requirement
    std::lock_guard<std::mutex> lock(g_logMutex);
    
    if (enabled == g_enabled) return;

    if (enabled) {
        // Enable
        if (!g_logFile.is_open()) {
            time_t now = time(nullptr);
            struct tm t;
            localtime_s(&t, &now);
            // Create directory lazily when first enabled
            std::error_code ec;
            std::filesystem::create_directories(g_logDir, ec);

            char filename[256];
            snprintf(filename, sizeof(filename), "%s/trng_%04d%02d%02d_%02d%02d%02d.log",
                     g_logDir.c_str(), t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                     t.tm_hour, t.tm_min, t.tm_sec);
            g_logFile.open(filename, std::ios::out | std::ios::app);
            // Store relative path for UI
            g_currentLogPath = std::string(filename); // filename contains full relative path e.g. "logs/..."
        }
        
        if (g_logFile.is_open()) {
            g_enabled = true;
            // We can't use LogInternal here easily because we are holding the lock and LogInternal is static/helper
            // Let's just define LogInternal as a proper helper first.
            SYSTEMTIME st;
            GetLocalTime(&st);
            char timestamp[32];
            snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
            char logLine[4096];
            snprintf(logLine, sizeof(logLine), "[%s] [INFO ] [Logger] Logging enabled by user.\n", timestamp);
            if (g_logFile.is_open()) {
                 g_logFile << logLine;
                 g_logFile.flush();
            }
            OutputDebugStringA(logLine);
            printf("%s", logLine);
        }
    } else {
        // Disable
        if (g_enabled) {
            SYSTEMTIME st;
            GetLocalTime(&st);
            char timestamp[32];
            snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
            char logLine[4096];
            snprintf(logLine, sizeof(logLine), "[%s] [INFO ] [Logger] Logging disabled by user.\n", timestamp);
            if (g_logFile.is_open()) {
                 g_logFile << logLine;
                 g_logFile.flush();
            }
            OutputDebugStringA(logLine);
            printf("%s", logLine);

            g_enabled = false;
            // Keep file open? Or close? User said "only start keep keeping logs if this option is turned on".
            // Implies we can close it to save resources/flush fully.
            if (g_logFile.is_open()) {
                g_logFile.close();
            }
            g_currentLogPath = "";
        }
    }
}

// Helper that doesn't lock (assumes caller holds lock)
void LogInternal(Level level, const char* module, const char* message) {
    // 1. Format timestamp
    SYSTEMTIME st;
    GetLocalTime(&st);
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    const char* levelStr = "UNKNOWN";
    switch (level) {
        // case Level::DEBUG: levelStr = "DEBUG"; break;
        case Level::INFO:  levelStr = "INFO "; break;
        case Level::WARN:  levelStr = "WARN "; break;
        case Level::ERR:   levelStr = "ERROR"; break;
    }

    char logLine[4096];
    snprintf(logLine, sizeof(logLine), "[%s] [%s] [%s] %s\n", 
             timestamp, levelStr, module, message);

    if (g_logFile.is_open()) {
        g_logFile << logLine;
        g_logFile.flush();
    }
    // Debug output removed per user request when logging is enabled (LogInternal is only called when enabled)
    // Actually wait, user said "if the keep logs is not on, completely bypass any debug logging function"
    // This implies if keeps logs IS on, we probably still want debug output? 
    // Or maybe they meant "logging function" generally.
    // Let's keep debug output when enabled for developer sanity, but strictly bypass when disabled.
    OutputDebugStringA(logLine);
    printf("%s", logLine);
}

bool IsEnabled() {
    return g_enabled;
}

void Log(Level level, const char* module, const char* format, ...) {
    // Check if logging is enabled BEFORE locking or formatting
    // This makes the "off" state extremely cheap (just a boolean check)
    if (!g_enabled) return;

    std::lock_guard<std::mutex> lock(g_logMutex);
    
    // Safety double-check under lock
    if (!g_enabled) return;

    // Buffer the message first
    char message[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    // If enabled, write to file
    if (g_logFile.is_open()) {
        LogInternal(level, module, message);
    } 
}

const char* GetCurrentLogPath() {
    // Determine path based on state
    if (!g_enabled) return "";
    return g_currentLogPath.c_str();
}

} // namespace Logger}
