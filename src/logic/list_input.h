#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "../core/output_settings.h"
#include "formats/list_formats.h"

namespace CSPRNG {

std::string_view ListTextOf(const OutputSettings& settings);

ListFormat ListFormatOf(const OutputSettings& settings);

bool ListPicksCanRepeat(const OutputSettings& settings);

int ListWeightOf(const ListItemWeights& weights, std::string_view item);

std::vector<uint64_t> ListPickWeights(const OutputSettings& settings, std::string_view text,
                                      const std::vector<ListField>& fields,
                                      const std::vector<size_t>& itemFields);

size_t ListRandomByteCount(const OutputSettings& settings);

std::string ListInputError(const OutputSettings& settings);

}
