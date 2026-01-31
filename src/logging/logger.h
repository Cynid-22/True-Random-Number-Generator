#pragma once

namespace Logger {

enum class Level {
    // DEBUG level removed for security hardening
    // DEBUG,
    INFO,
    WARN,
    ERR
};

// Initialize logging (opens file)
void Init(const char* logDir);

// Shutdown logging (closes file)
void Shutdown();

// Enable/Disable logging to file
void SetEnabled(bool enabled);

// Check if logging is enabled
bool IsEnabled();

// Log a message
void Log(Level level, const char* module, const char* format, ...);

// Get current log file path
const char* GetCurrentLogPath();

} // namespace Logger
