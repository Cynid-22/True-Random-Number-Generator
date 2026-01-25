#pragma once
#include <vector>
#include <cstdint>
#include <set>
#include "../entropy/entropy_common.h"

// Loads the default wordlist into memory for generation
bool LoadWordListForGeneration();

// Calculates the required entropy bits based on current output configuration
float CalculateRequiredEntropy();

// Updates the global targetBits based on configuration
void UpdateTargetEntropy();

// Estimate entropy from collected deltas (conservative)
float CalculateEntropyFromDeltas(const std::vector<uint64_t>& deltas);

// Get pooled entropy data filtered by included sources (for output generation)
std::vector<Entropy::EntropyDataPoint> GetPooledEntropyForOutput(const std::set<Entropy::EntropySource>& includedSources);

// Check if we have enough entropy for consolidation (Input >= Output)
bool PrepareConsolidation();

// Check if we need expansion (Input < Output)
bool PrepareExpansion();
