#pragma once
#include <vector>
#include <cstdint>

// Loads the default wordlist into memory for generation
bool LoadWordListForGeneration();

// Calculates the required entropy bits based on current output configuration
float CalculateRequiredEntropy();

// Updates the global targetBits based on configuration
void UpdateTargetEntropy();

// Estimate entropy from collected deltas (conservative)
float CalculateEntropyFromDeltas(const std::vector<uint64_t>& deltas);
