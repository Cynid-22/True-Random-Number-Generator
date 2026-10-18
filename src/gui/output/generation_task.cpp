#include "generation_task.h"
#include "../../core/app_state.h"
#include "../../entropy/entropy_common.h"
#include "../../logging/logger.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>
#include <time.h>
#include <utility>

static std::atomic<bool> s_generationRunning{false};
static std::mutex s_finishedResultMutex;
static CSPRNG::GenerationResult s_finishedResult;
static long long s_finishedElapsedMs = 0;
static std::atomic<bool> s_resultReady{false};

bool IsGenerationRunning() { return s_generationRunning.load(); }

static std::string FormatElapsedTime(long long totalMilliseconds) {
  long long milliseconds = totalMilliseconds % 1000;
  long long seconds = (totalMilliseconds / 1000) % 60;
  long long minutes = (totalMilliseconds / 60000) % 60;
  long long hours = totalMilliseconds / 3600000;

  char text[64];
  if (hours > 0)
    snprintf(text, sizeof(text), "%lldh %lldm %lld.%03llds", hours, minutes,
             seconds, milliseconds);
  else if (minutes > 0)
    snprintf(text, sizeof(text), "%lldm %lld.%03llds", minutes, seconds,
             milliseconds);
  else
    snprintf(text, sizeof(text), "%lld.%03llds", seconds, milliseconds);
  return text;
}

void ShowOutputText(const std::string &text) {
  g_state.generatedOutput = std::vector<char>(text.begin(), text.end());
  g_state.generatedOutput.push_back('\0');
  g_state.generationTimeText = "";
}

void StartGeneration(CSPRNG::GenerationMode mode) {
  s_generationRunning.store(true);
  s_resultReady.store(false);
  g_state.cancelGenerationRequested = false;
  g_state.generationTimeText = "";

  g_state.lockedDataTimestamp = Entropy::GetNanosecondTimestamp();
  Logger::Log(Logger::Level::INFO, "GUI",
              "Entropy locked at timestamp: %llu",
              g_state.lockedDataTimestamp);

  std::thread([mode]() {
    auto startTime = std::chrono::steady_clock::now();
    CSPRNG::GenerationResult result = CSPRNG::GenerateOutput(mode);
    long long elapsedMilliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime)
            .count();
    {
      std::lock_guard<std::mutex> lock(s_finishedResultMutex);
      s_finishedResult = std::move(result);
      s_finishedElapsedMs = elapsedMilliseconds;
    }
    s_resultReady.store(true);
  }).detach();
}

void CollectFinishedGeneration() {
  if (!s_resultReady.load())
    return;

  std::lock_guard<std::mutex> lock(s_finishedResultMutex);

  if (s_finishedResult.success) {
    g_state.generatedOutput = std::move(s_finishedResult.output);

    time_t now = time(nullptr);
    char generatedAt[64];
    strftime(generatedAt, sizeof(generatedAt), "%Y-%m-%d %H:%M:%S (UTC%z)",
             localtime(&now));
    g_state.generatedAtText = generatedAt;
    g_state.generationTimeText = FormatElapsedTime(s_finishedElapsedMs);

    const char *modeName =
        (s_finishedResult.mode == CSPRNG::GenerationMode::Consolidation)
            ? "TRUE RANDOMNESS"
            : "PSEUDO-RANDOM";
    Logger::Log(Logger::Level::INFO, "GUI",
                "Generated output using %s mode, consumed %.1f bits",
                modeName, s_finishedResult.entropyConsumed);
  } else {
    ShowOutputText("[ERROR] " + s_finishedResult.errorMessage);
    Logger::Log(Logger::Level::ERR, "GUI", "Generation failed: %s",
                s_finishedResult.errorMessage.c_str());
  }

  s_resultReady.store(false);
  s_generationRunning.store(false);
}
