#include "generation_task.h"
#include "../../core/app_state.h"
#include "../../entropy/entropy_common.h"
#include "../../logging/logger.h"
#include <atomic>
#include <mutex>
#include <thread>
#include <time.h>
#include <utility>

static std::atomic<bool> s_generationRunning{false};
static std::mutex s_finishedResultMutex;
static CSPRNG::GenerationResult s_finishedResult;
static std::atomic<bool> s_resultReady{false};

bool IsGenerationRunning() { return s_generationRunning.load(); }

void ShowOutputText(const std::string &text) {
  g_state.generatedOutput = std::vector<char>(text.begin(), text.end());
  g_state.generatedOutput.push_back('\0');
}

void StartGeneration(CSPRNG::GenerationMode mode) {
  s_generationRunning.store(true);
  s_resultReady.store(false);
  g_state.cancelGenerationRequested = false;

  g_state.lockedDataTimestamp = Entropy::GetNanosecondTimestamp();
  Logger::Log(Logger::Level::INFO, "GUI",
              "Entropy locked at timestamp: %llu",
              g_state.lockedDataTimestamp);

  std::thread([mode]() {
    CSPRNG::GenerationResult result = CSPRNG::GenerateOutput(mode);
    {
      std::lock_guard<std::mutex> lock(s_finishedResultMutex);
      s_finishedResult = std::move(result);
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
