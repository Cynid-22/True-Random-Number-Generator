#pragma once

#include <string>
#include "../../logic/output_generator.h"

bool IsGenerationRunning();

void StartGeneration(CSPRNG::GenerationMode mode);

void CollectFinishedGeneration();

void ShowOutputText(const std::string &text);
