#pragma once

// Calculates the estimated entropy of the wordlist file
int EstimateWordListEntropy(const char* path);

// Calculates the required entropy bits based on current output configuration
float CalculateRequiredEntropy();

// Updates the global targetBits based on configuration
void UpdateTargetEntropy();
