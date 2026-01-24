#pragma once

// Loads the default wordlist into memory for generation
bool LoadWordListForGeneration();

// Calculates the required entropy bits based on current output configuration
float CalculateRequiredEntropy();

// Updates the global targetBits based on configuration
void UpdateTargetEntropy();
