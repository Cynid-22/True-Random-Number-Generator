#pragma once

namespace Logger {

enum class Level {
    DEBUG,
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

} // namespace Logger
