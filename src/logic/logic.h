#pragma once
#include <vector>
#include <cstdint>

bool LoadWordListForGeneration();

float CalculateRequiredEntropy();

void UpdateTargetEntropy();

float CalculateEntropyFromDeltas(const std::vector<uint64_t>& deltas);
