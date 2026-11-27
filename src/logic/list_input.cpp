#include "list_input.h"
#include <algorithm>
#include <cstring>

namespace CSPRNG {

static std::string_view TextInBuffer(const std::vector<char> &buffer) {
  return std::string_view(buffer.data(), strnlen(buffer.data(), buffer.size()));
}

std::string_view ListTextOf(const OutputSettings &settings) {
  return TextInBuffer(settings.listText);
}

ListFormat ListFormatOf(const OutputSettings &settings) {
  ListFormat format;
  format.oneItemPerLine = settings.listOneItemPerLine;
  format.separator = TextInBuffer(settings.listSeparator);
  format.trimSpaces = settings.listTrimSpaces;
  return format;
}

bool ListPicksCanRepeat(const OutputSettings &settings) {
  return settings.listAllowRepeats && !settings.listRemovePicked;
}

int ListWeightOf(const ListItemWeights &weights, std::string_view item) {
  const auto found = weights.find(item);
  if (found == weights.end())
    return AppConfig::LIST_DEFAULT_WEIGHT;
  return std::clamp(found->second, AppConfig::LIST_MIN_WEIGHT, AppConfig::LIST_MAX_WEIGHT);
}

std::vector<uint64_t> ListPickWeights(const OutputSettings &settings, std::string_view text,
                                      const std::vector<ListField> &fields,
                                      const std::vector<size_t> &itemFields) {
  std::vector<uint64_t> weights(itemFields.size(), AppConfig::LIST_DEFAULT_WEIGHT);
  if (!settings.listUseWeights)
    return weights;
  for (size_t i = 0; i < itemFields.size(); i++)
    weights[i] = static_cast<uint64_t>(
        ListWeightOf(settings.listItemWeights, ListItemText(text, fields[itemFields[i]])));
  return weights;
}

size_t ListRandomByteCount(const OutputSettings &settings) {
  if (settings.outputFormat == FORMAT_LIST_PICK)
    return ListPickByteCount(settings.outputQuantity);
  return ListShuffleByteCount(CountListItems(ListTextOf(settings), ListFormatOf(settings)));
}

static std::string PickCountError(const OutputSettings &settings, size_t pickableCount) {
  std::string error = "Can't pick " + std::to_string(settings.outputQuantity) +
                      " different items: the list has only " + std::to_string(pickableCount);
  if (settings.listUseWeights)
    error += " with a weight above 0";
  error += ". Lower Items to pick";
  error += settings.listRemovePicked ? "." : ", or tick Allow repeats.";
  return error;
}

std::string ListInputError(const OutputSettings &settings) {
  const std::string_view text = ListTextOf(settings);
  const std::vector<ListField> fields = SplitList(text, ListFormatOf(settings));
  const std::vector<size_t> itemFields = ListItemFieldIndices(fields);

  if (settings.outputFormat == FORMAT_LIST_SHUFFLE)
    return itemFields.size() < 2 ? "Enter at least two items to shuffle." : "";
  if (itemFields.empty())
    return "Enter at least one item in the list.";

  const std::vector<uint64_t> weights = ListPickWeights(settings, text, fields, itemFields);
  const size_t pickableCount = static_cast<size_t>(
      std::count_if(weights.begin(), weights.end(), [](uint64_t weight) { return weight > 0; }));
  if (pickableCount == 0)
    return "Every item has a weight of 0. Give at least one item a weight above 0 on the "
           "Advanced tab.";
  if (!ListPicksCanRepeat(settings) &&
      static_cast<size_t>(std::max(settings.outputQuantity, 1)) > pickableCount)
    return PickCountError(settings, pickableCount);
  return "";
}

}
