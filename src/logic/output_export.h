#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include "../core/app_state.h"

std::vector<OutputSetting> CaptureOutputSettings();

std::vector<char> BuildOutputCsv(const GeneratedOutputDetails &details,
                                 const char *text, size_t length);

std::vector<char> BuildOutputJson(const GeneratedOutputDetails &details,
                                  const std::string &generatedAt,
                                  const char *text, size_t length);

std::vector<char> BuildOneTimePadCsv(const char *key, const char *ciphertext,
                                     size_t length, bool fileMode);

std::vector<char> BuildOneTimePadJson(const GeneratedOutputDetails &details,
                                      const std::string &generatedAt,
                                      const char *key, const char *ciphertext,
                                      size_t length, bool fileMode);
