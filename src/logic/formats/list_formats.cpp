#include "list_formats.h"
#include "../../crypto/secure_mem.h"
#include "cancellation.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace CSPRNG {

static constexpr char LINE_BREAK = '\n';
static constexpr char CARRIAGE_RETURN = '\r';
static constexpr std::string_view LINE_JOINER = "\n";
static constexpr size_t CANCEL_CHECK_MASK = 0x3FF;
static constexpr size_t NOT_FOUND = std::string_view::npos;

static bool IsListSpace(char character) {
  return character == ' ' || character == '\t' || character == '\r' ||
         character == '\v' || character == '\f';
}

static std::string_view TrimListSpaces(std::string_view text) {
  size_t first = 0;
  while (first < text.size() && IsListSpace(text[first]))
    first++;
  size_t last = text.size();
  while (last > first && IsListSpace(text[last - 1]))
    last--;
  return text.substr(first, last - first);
}

static std::string_view SplitSeparator(const ListFormat &format) {
  if (format.oneItemPerLine)
    return {};
  if (!format.trimSpaces)
    return format.separator;
  const std::string_view trimmed = TrimListSpaces(format.separator);
  return trimmed.empty() ? format.separator : trimmed;
}

template <typename FieldVisitor>
static void ForEachListField(std::string_view text, const ListFormat &format,
                             FieldVisitor visitField) {
  const std::string_view separator = SplitSeparator(format);
  size_t nextLineBreak = text.find(LINE_BREAK);
  size_t nextSeparator = separator.empty() ? NOT_FOUND : text.find(separator);
  size_t fieldStart = 0;
  while (true) {
    if (nextLineBreak < fieldStart)
      nextLineBreak = text.find(LINE_BREAK, fieldStart);
    if (nextSeparator < fieldStart)
      nextSeparator = text.find(separator, fieldStart);

    size_t fieldEnd = text.size();
    size_t delimiterEnd = text.size();
    const bool lastField = nextSeparator == NOT_FOUND && nextLineBreak == NOT_FOUND;
    if (nextSeparator < nextLineBreak) {
      fieldEnd = nextSeparator;
      delimiterEnd = nextSeparator + separator.size();
    } else if (nextLineBreak != NOT_FOUND) {
      fieldEnd = nextLineBreak;
      delimiterEnd = nextLineBreak + 1;
      if (fieldEnd > fieldStart && text[fieldEnd - 1] == CARRIAGE_RETURN)
        fieldEnd--;
    }

    const std::string_view field = text.substr(fieldStart, fieldEnd - fieldStart);
    const std::string_view item = format.trimSpaces ? TrimListSpaces(field) : field;
    visitField(ListField{fieldStart, fieldEnd, delimiterEnd,
                         fieldStart + static_cast<size_t>(item.data() - field.data()),
                         item.size()});
    if (lastField)
      break;
    fieldStart = delimiterEnd;
  }
}

std::vector<ListField> SplitList(std::string_view text, const ListFormat &format) {
  std::vector<ListField> fields;
  ForEachListField(text, format, [&fields](const ListField &field) { fields.push_back(field); });
  return fields;
}

size_t CountListItems(std::string_view text, const ListFormat &format) {
  size_t itemCount = 0;
  ForEachListField(text, format, [&itemCount](const ListField &field) {
    if (field.itemLength > 0)
      itemCount++;
  });
  return itemCount;
}

std::vector<size_t> ListItemFieldIndices(const std::vector<ListField> &fields) {
  std::vector<size_t> itemFields;
  for (size_t i = 0; i < fields.size(); i++) {
    if (fields[i].itemLength > 0)
      itemFields.push_back(i);
  }
  return itemFields;
}

std::string_view ListItemText(std::string_view text, const ListField &field) {
  return text.substr(field.itemStart, field.itemLength);
}

bool ListJoinsWithLineBreaks(const ListFormat &format) {
  return format.oneItemPerLine || format.separator.empty();
}

std::string_view ListJoiner(const ListFormat &format) {
  return ListJoinsWithLineBreaks(format) ? LINE_JOINER : format.separator;
}

size_t JoinedListLength(const std::vector<ListField> &fields,
                        const std::vector<size_t> &fieldOrder, std::string_view joiner) {
  size_t length = fieldOrder.empty() ? 0 : (fieldOrder.size() - 1) * joiner.size();
  for (size_t fieldIndex : fieldOrder)
    length += fields[fieldIndex].itemLength;
  return length;
}

std::vector<char> JoinListItems(std::string_view text, const std::vector<ListField> &fields,
                                const std::vector<size_t> &fieldOrder, std::string_view joiner) {
  std::vector<char> joined;
  joined.reserve(JoinedListLength(fields, fieldOrder, joiner));
  for (size_t i = 0; i < fieldOrder.size(); i++) {
    if (i > 0)
      joined.insert(joined.end(), joiner.begin(), joiner.end());
    const std::string_view item = ListItemText(text, fields[fieldOrder[i]]);
    joined.insert(joined.end(), item.begin(), item.end());
  }
  return joined;
}

static bool DelimiterHasLineBreak(std::string_view text, const ListField &field) {
  return text.substr(field.end, field.delimiterEnd - field.end).find(LINE_BREAK) != NOT_FOUND;
}

static size_t FieldWithJoiningDelimiter(std::string_view text,
                                        const std::vector<ListField> &fields,
                                        size_t previousKept, size_t nextKept) {
  for (size_t i = previousKept; i < nextKept; i++) {
    if (DelimiterHasLineBreak(text, fields[i]))
      return i;
  }
  return previousKept;
}

static size_t LeadingSpacesEnd(const ListField &field) {
  return field.itemLength > 0 ? field.itemStart : field.end;
}

std::vector<char> RemoveListFields(std::string_view text, const std::vector<ListField> &fields,
                                   const std::vector<uint8_t> &removeField) {
  std::vector<char> kept;
  kept.reserve(text.size());
  bool keptAField = false;
  size_t previousKept = 0;
  for (size_t i = 0; i < fields.size(); i++) {
    if (removeField[i])
      continue;
    if (keptAField) {
      const size_t joining = FieldWithJoiningDelimiter(text, fields, previousKept, i);
      kept.insert(kept.end(), text.begin() + fields[joining].end,
                  text.begin() + LeadingSpacesEnd(fields[joining + 1]));
    } else {
      kept.insert(kept.end(), text.begin() + fields[0].start,
                  text.begin() + LeadingSpacesEnd(fields[0]));
    }
    const ListField &field = fields[i];
    kept.insert(kept.end(), text.begin() + LeadingSpacesEnd(field), text.begin() + field.end);
    keptAField = true;
    previousKept = i;
  }
  return kept;
}

static bool IsAsciiDigit(unsigned char character) {
  return character >= '0' && character <= '9';
}

static unsigned char FoldAsciiCase(unsigned char character) {
  return (character >= 'A' && character <= 'Z')
             ? static_cast<unsigned char>(character - 'A' + 'a')
             : character;
}

static size_t DigitRunEnd(std::string_view text, size_t start) {
  while (start < text.size() && IsAsciiDigit(static_cast<unsigned char>(text[start])))
    start++;
  return start;
}

static std::string_view WithoutLeadingZeros(std::string_view digits) {
  size_t first = 0;
  while (first < digits.size() && digits[first] == '0')
    first++;
  return digits.substr(first);
}

static int Sign(int value) { return (value > 0) - (value < 0); }

static int CompareDigitRuns(std::string_view a, std::string_view b) {
  const std::string_view aValue = WithoutLeadingZeros(a);
  const std::string_view bValue = WithoutLeadingZeros(b);
  if (aValue.size() != bValue.size())
    return aValue.size() < bValue.size() ? -1 : 1;
  return Sign(aValue.compare(bValue));
}

int CompareListItemsNaturally(std::string_view a, std::string_view b) {
  size_t i = 0;
  size_t j = 0;
  while (i < a.size() && j < b.size()) {
    const unsigned char aCharacter = static_cast<unsigned char>(a[i]);
    const unsigned char bCharacter = static_cast<unsigned char>(b[j]);
    if (IsAsciiDigit(aCharacter) && IsAsciiDigit(bCharacter)) {
      const size_t aRunEnd = DigitRunEnd(a, i);
      const size_t bRunEnd = DigitRunEnd(b, j);
      const int order = CompareDigitRuns(a.substr(i, aRunEnd - i), b.substr(j, bRunEnd - j));
      if (order != 0)
        return order;
      i = aRunEnd;
      j = bRunEnd;
      continue;
    }
    const unsigned char aFolded = FoldAsciiCase(aCharacter);
    const unsigned char bFolded = FoldAsciiCase(bCharacter);
    if (aFolded != bFolded)
      return aFolded < bFolded ? -1 : 1;
    i++;
    j++;
  }
  const bool aEnded = i == a.size();
  const bool bEnded = j == b.size();
  if (aEnded != bEnded)
    return aEnded ? -1 : 1;
  return Sign(a.compare(b));
}

void SortListFields(std::string_view text, const std::vector<ListField> &fields,
                    std::vector<size_t> &fieldOrder, bool descending) {
  std::stable_sort(fieldOrder.begin(), fieldOrder.end(),
                   [&](size_t left, size_t right) {
                     const int order = CompareListItemsNaturally(
                         ListItemText(text, fields[left]), ListItemText(text, fields[right]));
                     return descending ? order > 0 : order < 0;
                   });
}

std::vector<uint8_t> DuplicateListFields(std::string_view text,
                                         const std::vector<ListField> &fields) {
  std::vector<uint8_t> duplicate(fields.size(), 0);
  std::unordered_set<std::string_view> seenItems;
  for (size_t i = 0; i < fields.size(); i++) {
    if (fields[i].itemLength == 0)
      continue;
    duplicate[i] = !seenItems.insert(ListItemText(text, fields[i])).second;
  }
  return duplicate;
}

size_t ListPickByteCount(int quantity) {
  return static_cast<size_t>(std::max(quantity, 0)) * LIST_DRAW_BYTES + LIST_SPARE_DRAW_BYTES;
}

size_t ListShuffleByteCount(size_t itemCount) {
  const size_t drawCount = itemCount > 1 ? itemCount - 1 : 0;
  return drawCount * LIST_DRAW_BYTES + LIST_SPARE_DRAW_BYTES;
}

static double Log2Factorial(double value) { return std::lgamma(value + 1.0) / std::log(2.0); }

double ListPickBits(size_t itemCount, int quantity, bool allowRepeats) {
  if (itemCount <= 1 || quantity <= 0)
    return 0.0;
  const double items = static_cast<double>(itemCount);
  if (allowRepeats)
    return quantity * std::log2(items);
  const double picks = std::min(static_cast<double>(quantity), items);
  return Log2Factorial(items) - Log2Factorial(items - picks);
}

double ListShuffleBits(size_t itemCount) {
  return itemCount <= 1 ? 0.0 : Log2Factorial(static_cast<double>(itemCount));
}

static uint64_t DrawBelow(const std::vector<uint8_t> &randomBytes, size_t &byteIndex,
                          uint64_t range) {
  const uint64_t rejectBelow = (0 - range) % range;
  while (byteIndex + LIST_DRAW_BYTES <= randomBytes.size()) {
    uint64_t draw = 0;
    for (size_t i = 0; i < LIST_DRAW_BYTES; i++)
      draw = (draw << 8) | randomBytes[byteIndex++];
    if (draw >= rejectBelow)
      return draw % range;
  }
  throw std::runtime_error("List: too many random draws were rejected. Please try again.");
}

class WeightTree {
public:
  explicit WeightTree(const std::vector<uint64_t> &weights) : m_tree(weights.size() + 1, 0) {
    for (size_t node = 1; node < m_tree.size(); node++) {
      m_tree[node] += weights[node - 1];
      const size_t parent = node + LowestBit(node);
      if (parent < m_tree.size())
        m_tree[parent] += m_tree[node];
    }
  }

  ~WeightTree() { Crypto::SecureClearVector(m_tree); }

  void Subtract(size_t index, uint64_t amount) {
    for (size_t node = index + 1; node < m_tree.size(); node += LowestBit(node))
      m_tree[node] -= amount;
  }

  size_t IndexHolding(uint64_t target) const {
    size_t position = 0;
    size_t step = 1;
    while (step * 2 < m_tree.size())
      step *= 2;
    for (; step > 0; step /= 2) {
      const size_t next = position + step;
      if (next < m_tree.size() && m_tree[next] <= target) {
        position = next;
        target -= m_tree[next];
      }
    }
    return position;
  }

private:
  static size_t LowestBit(size_t node) { return node & (0 - node); }

  std::vector<uint64_t> m_tree;
};

std::vector<size_t> PickListItems(const std::vector<uint8_t> &randomBytes,
                                  const std::vector<uint64_t> &weights, size_t quantity,
                                  bool allowRepeats) {
  uint64_t totalWeight = 0;
  size_t pickableCount = 0;
  for (uint64_t weight : weights) {
    if (weight > UINT64_MAX - totalWeight)
      throw std::runtime_error("List: the weights add up to too much.");
    totalWeight += weight;
    if (weight > 0)
      pickableCount++;
  }
  if (totalWeight == 0)
    throw std::runtime_error("List: no item has a weight above 0.");
  if (!allowRepeats && quantity > pickableCount)
    throw std::runtime_error("List: not enough items to pick without repeats.");

  WeightTree tree(weights);
  std::vector<size_t> picks;
  try {
    picks.reserve(quantity);
    size_t byteIndex = 0;
    for (size_t i = 0; i < quantity; i++) {
      if ((i & CANCEL_CHECK_MASK) == 0)
        ThrowIfGenerationCancelled();
      const size_t picked = tree.IndexHolding(DrawBelow(randomBytes, byteIndex, totalWeight));
      picks.push_back(picked);
      if (!allowRepeats) {
        tree.Subtract(picked, weights[picked]);
        totalWeight -= weights[picked];
      }
    }
  } catch (...) {
    Crypto::SecureClearVector(picks);
    throw;
  }
  return picks;
}

void ShuffleListOrder(const std::vector<uint8_t> &randomBytes, std::vector<size_t> &order) {
  size_t byteIndex = 0;
  for (size_t remaining = order.size(); remaining > 1; remaining--) {
    if ((remaining & CANCEL_CHECK_MASK) == 0)
      ThrowIfGenerationCancelled();
    const size_t swapWith = static_cast<size_t>(DrawBelow(randomBytes, byteIndex, remaining));
    std::swap(order[remaining - 1], order[swapWith]);
  }
}

bool StartsLikeSpreadsheetFormula(const char *text, size_t length) {
  if (length == 0)
    return false;
  switch (text[0]) {
  case '=':
  case '+':
  case '-':
  case '@':
  case '\t':
  case '\r':
    return true;
  default:
    return false;
  }
}

}
