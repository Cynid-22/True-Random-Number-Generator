#pragma once

#include <string>
#include "../../logic/output_generator.h"

bool IsGenerationRunning();

void StartGeneration();

void CollectFinishedGeneration();

void ShowOutputText(const std::string &text);
