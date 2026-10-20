#pragma once
#include <vector>
#include <cstdint>

float CalculateRequiredEntropy();

void UpdateTargetEntropy();

float CalculateEntropyFromDeltas(const std::vector<uint64_t>& deltas);
