#pragma once
#include <stdexcept>
#include "../../core/app_state.h"

namespace CSPRNG {

inline void ThrowIfGenerationCancelled() {
    if (g_state.cancelGenerationRequested)
        throw std::runtime_error("Generation cancelled by user.");
}

}
