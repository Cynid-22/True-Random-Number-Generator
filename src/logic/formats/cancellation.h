#pragma once
#include <stdexcept>
#include "../../core/app_state.h"

namespace CSPRNG {

class GenerationCancelled : public std::runtime_error {
public:
    GenerationCancelled() : std::runtime_error("Generation cancelled.") {}
};

inline void ThrowIfGenerationCancelled() {
    if (g_state.cancelGenerationRequested)
        throw GenerationCancelled();
}

}
