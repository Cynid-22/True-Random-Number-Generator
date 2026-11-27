#include "generation_task.h"
#include "../../core/app_state.h"
#include "../../logic/formats/cancellation.h"
#include "../../logic/list_input.h"
#include "../../logic/output_export.h"
#include "../gui.h"
#include "format_options.h"
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
static bool s_runningListShuffle = false;
static constexpr char LIST_SHUFFLE_FAILED_HEADING[] = "Shuffle failed";

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
  const bool shuffleJoinsItemsOnOneLine =
      g_state.outputFormat == FORMAT_LIST_SHUFFLE &&
      !CSPRNG::ListJoinsWithLineBreaks(CSPRNG::ListFormatOf(g_state));
  details.numberedItems = g_state.numberListItems &&
                          g_state.outputFormat != FORMAT_ONE_TIME_PAD &&
                          !shuffleJoinsItemsOnOneLine;
  return details;
}

static void StartGenerationThread(OutputSettings settings) {
  std::thread([includedSources = g_state.enabledEntropySources(),
               settings = std::move(settings)]() {
    auto startTime = std::chrono::steady_clock::now();
    CSPRNG::GenerationResult result =
        CSPRNG::GenerateOutput(includedSources, settings);
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

static void MarkGenerationStarted(bool isListShuffle) {
  s_generationRunning.store(true);
  s_resultReady.store(false);
  s_runningListShuffle = isListShuffle;
  g_state.cancelGenerationRequested = false;
}

void StartGeneration() {
  MarkGenerationStarted(false);
  g_state.generationTimeText = "";

  try {
    s_runningGenerationDetails = CaptureOutputDetails();
    StartGenerationThread(OutputSettings(g_state));
  } catch (const std::exception &error) {
    s_generationRunning.store(false);
    ShowOutputText(std::string("[ERROR] The generation could not start: ") +
                   error.what());
  }
}

void StartListShuffle() {
  MarkGenerationStarted(true);
  try {
    OutputSettings settings(g_state);
    settings.outputFormat = FORMAT_LIST_SHUFFLE;
    settings.shuffleListInPlace = true;
    StartGenerationThread(std::move(settings));
  } catch (const std::exception &error) {
    s_generationRunning.store(false);
    s_runningListShuffle = false;
    ShowErrorModal(LIST_SHUFFLE_FAILED_HEADING,
                   std::string("The shuffle could not start: ") + error.what());
  }
}

static bool ApplyUpdatedListText(CSPRNG::GenerationResult &result) {
  if (!result.updatesListText)
    return false;
  const bool replaced = ReplaceListText(result.updatedListText);
  if (!replaced)
    ShowErrorModal("List not changed",
                   "The changed list is longer than the list box holds, so the "
                   "list was not changed.");
  Crypto::SecureClearVector(result.updatedListText);
  result.updatesListText = false;
  return replaced;
}

static void CollectFinishedListShuffle() {
  CSPRNG::GenerationResult &result = s_finishedResult;
  if (result.success) {
    if (ApplyUpdatedListText(result))
      g_state.listStatusText =
          std::string("Shuffled in ") + CSPRNG::ModeName(result.mode) + " mode";
  } else if (result.cancelled) {
    g_state.listStatusText = "Shuffle cancelled";
  } else {
    ShowErrorModal(LIST_SHUFFLE_FAILED_HEADING, result.errorMessage);
  }
  Crypto::SecureClearVector(result.output);
  Crypto::SecureClearVector(result.binaryOutput);
}

static void ApplyPickedItemsRemoval() {
  const size_t removedCount = s_finishedResult.removedListItemCount;
  if (ApplyUpdatedListText(s_finishedResult))
    g_state.listStatusText = "Removed " + std::to_string(removedCount) +
                             (removedCount == 1 ? " picked item" : " picked items");
}

static void DiscardResultFinishedAfterCancel(CSPRNG::GenerationResult &result) {
  Crypto::SecureClearVector(result.output);
  Crypto::SecureClearVector(result.binaryOutput);
  Crypto::SecureClearVector(result.oneTimePadKey);
  Crypto::SecureClearVector(result.oneTimePadCiphertext);
  Crypto::SecureClearVector(result.updatedListText);
  result.oneTimePadResultMode = ONE_TIME_PAD_NO_RESULT;
  result.oneTimePadFileName.clear();
  result.updatesListText = false;
  result.removedListItemCount = 0;
  result.joinedItemLengths.clear();
  result.success = false;
  result.cancelled = true;
  result.errorMessage = CSPRNG::GenerationCancelled().what();
}

void CollectFinishedGeneration() {
  if (!s_resultReady.load())
    return;

  std::lock_guard<std::mutex> lock(s_finishedResultMutex);

  if (s_finishedResult.success && g_state.cancelGenerationRequested)
    DiscardResultFinishedAfterCancel(s_finishedResult);

  if (s_runningListShuffle) {
    CollectFinishedListShuffle();
  } else if (s_finishedResult.success) {
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

    const char *modeName = CSPRNG::ModeName(s_finishedResult.mode);
    g_state.generatedOutputDetails = std::move(s_runningGenerationDetails);
    g_state.generatedOutputDetails.securityMode = modeName;
    g_state.generatedOutputDetails.joinedItemLengths =
        std::move(s_finishedResult.joinedItemLengths);
    g_state.generatedOutputDetails.joinedSeparatorLength =
        s_finishedResult.joinedSeparatorLength;
    ApplyPickedItemsRemoval();
  } else if (s_finishedResult.cancelled) {
    ShowOutputText(s_finishedResult.errorMessage);
  } else {
    ShowOutputText("[ERROR] " + s_finishedResult.errorMessage);
  }

  s_runningListShuffle = false;
  s_resultReady.store(false);
  s_generationRunning.store(false);
}
