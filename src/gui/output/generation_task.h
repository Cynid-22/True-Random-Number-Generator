#pragma once

#include <string>
#include "../../logic/output_generator.h"

bool IsGenerationRunning();

void StartGeneration();

void StartListShuffle();

void CollectFinishedGeneration();

void ShowOutputText(const std::string &text);
