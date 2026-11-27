#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace CSPRNG {

struct ListFormat {
    bool oneItemPerLine = true;
    std::string_view separator;
    bool trimSpaces = true;
};

struct ListField {
    size_t start;
    size_t end;
    size_t delimiterEnd;
    size_t itemStart;
    size_t itemLength;
};

inline constexpr size_t LIST_DRAW_BYTES = 8;
inline constexpr size_t LIST_SPARE_DRAW_BYTES = 64;

std::vector<ListField> SplitList(std::string_view text, const ListFormat& format);

size_t CountListItems(std::string_view text, const ListFormat& format);

std::vector<size_t> ListItemFieldIndices(const std::vector<ListField>& fields);

std::string_view ListItemText(std::string_view text, const ListField& field);

bool ListJoinsWithLineBreaks(const ListFormat& format);

std::string_view ListJoiner(const ListFormat& format);

size_t JoinedListLength(const std::vector<ListField>& fields,
                        const std::vector<size_t>& fieldOrder, std::string_view joiner);

std::vector<char> JoinListItems(std::string_view text, const std::vector<ListField>& fields,
                                const std::vector<size_t>& fieldOrder, std::string_view joiner);

std::vector<char> RemoveListFields(std::string_view text, const std::vector<ListField>& fields,
                                   const std::vector<uint8_t>& removeField);

int CompareListItemsNaturally(std::string_view a, std::string_view b);

void SortListFields(std::string_view text, const std::vector<ListField>& fields,
                    std::vector<size_t>& fieldOrder, bool descending);

std::vector<uint8_t> DuplicateListFields(std::string_view text,
                                         const std::vector<ListField>& fields);

size_t ListPickByteCount(int quantity);

size_t ListShuffleByteCount(size_t itemCount);

double ListPickBits(size_t itemCount, int quantity, bool allowRepeats);

double ListShuffleBits(size_t itemCount);

std::vector<size_t> PickListItems(const std::vector<uint8_t>& randomBytes,
                                  const std::vector<uint64_t>& weights, size_t quantity,
                                  bool allowRepeats);

void ShuffleListOrder(const std::vector<uint8_t>& randomBytes, std::vector<size_t>& order);

bool StartsLikeSpreadsheetFormula(const char* text, size_t length);

}
