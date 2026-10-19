#include "generation_task.h"
#include "../../core/app_state.h"
#include "../../logging/logger.h"
#include "../../logic/output_export.h"
#include "output_types.h"
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
static time_t s_finishedAt = 0;
static std::atomic<bool> s_resultReady{false};
static GeneratedOutputDetails s_runningGenerationDetails;

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
  g_state.wipeGeneratedOutput();
  g_state.wipeOneTimePadResult();
  g_state.generatedOutput = std::vector<char>(text.begin(), text.end());
  g_state.generatedOutput.push_back('\0');
  g_state.generatedAtText = "";
  g_state.generationTimeText = "";
}

static GeneratedOutputDetails CaptureOutputDetails() {
  int category = 0;
  int type = 0;
  FindCurrentOutputType(category, type);
  const OutputCategory &outputCategory = GetOutputCategory(category);

  GeneratedOutputDetails details;
  details.format = g_state.outputFormat;
  details.category = outputCategory.name;
  details.type = outputCategory.types[type].name;
  details.settings = CaptureOutputSettings();
  details.numberedItems =
      g_state.numberListItems && g_state.outputFormat != FORMAT_ONE_TIME_PAD;
  return details;
}

void StartGeneration(CSPRNG::GenerationMode mode) {
  s_runningGenerationDetails = CaptureOutputDetails();
  s_generationRunning.store(true);
  s_resultReady.store(false);
  g_state.cancelGenerationRequested = false;
  g_state.generationTimeText = "";

  std::thread([mode, includedSources = g_state.enabledEntropySources()]() {
    auto startTime = std::chrono::steady_clock::now();
    CSPRNG::GenerationResult result =
        CSPRNG::GenerateOutput(mode, includedSources);
    long long elapsedMilliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime)
            .count();
    time_t finishedAt = time(nullptr);
    {
      std::lock_guard<std::mutex> lock(s_finishedResultMutex);
      s_finishedResult = std::move(result);
      s_finishedElapsedMs = elapsedMilliseconds;
      s_finishedAt = finishedAt;
    }
    s_resultReady.store(true);
  }).detach();
}

void CollectFinishedGeneration() {
  if (!s_resultReady.load())
    return;

  std::lock_guard<std::mutex> lock(s_finishedResultMutex);

  if (s_finishedResult.success) {
    g_state.wipeGeneratedOutput();
    g_state.wipeOneTimePadResult();
    if (s_finishedResult.oneTimePadResultMode != ONE_TIME_PAD_NO_RESULT) {
      g_state.oneTimePadResultMode = s_finishedResult.oneTimePadResultMode;
      g_state.oneTimePadKey = std::move(s_finishedResult.oneTimePadKey);
      g_state.oneTimePadCiphertext =
          std::move(s_finishedResult.oneTimePadCiphertext);
      g_state.oneTimePadResultFileName =
          std::move(s_finishedResult.oneTimePadFileName);
    } else {
      g_state.generatedOutput = std::move(s_finishedResult.output);
      g_state.generatedBinaryOutput = std::move(s_finishedResult.binaryOutput);
    }

    char generatedAt[64];
    strftime(generatedAt, sizeof(generatedAt), "%Y-%m-%d %H:%M:%S (UTC%z)",
             localtime(&s_finishedAt));
    g_state.generatedAtText = generatedAt;
    g_state.generationTimeText = FormatElapsedTime(s_finishedElapsedMs);

    const char *modeName =
        (s_finishedResult.mode == CSPRNG::GenerationMode::Consolidation)
            ? "TRUE RANDOMNESS"
            : "PSEUDO-RANDOM";
    g_state.generatedOutputDetails = std::move(s_runningGenerationDetails);
    g_state.generatedOutputDetails.securityMode = modeName;
    Logger::Log(Logger::Level::INFO, "GUI",
                "Generated output using %s mode, consumed %.1f bits",
                modeName, s_finishedResult.entropyConsumed);
  } else if (s_finishedResult.cancelled) {
    ShowOutputText(s_finishedResult.errorMessage);
    Logger::Log(Logger::Level::INFO, "GUI", "Generation cancelled");
  } else {
    ShowOutputText("[ERROR] " + s_finishedResult.errorMessage);
    Logger::Log(Logger::Level::ERR, "GUI", "Generation failed: %s",
                s_finishedResult.errorMessage.c_str());
  }

  s_resultReady.store(false);
  s_generationRunning.store(false);
}
