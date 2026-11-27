#include "test_framework.h"
#include "test_data.h"
#include "../src/core/app_state.h"
#include "../src/logic/formats/list_formats.h"
#include "../src/logic/list_input.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using CSPRNG::ListField;
using CSPRNG::ListFormat;
using Tests::TextOf;

namespace {

constexpr int PROPERTY_TEXT_COUNT = 400;
constexpr uint64_t PROPERTY_MAX_TOKENS = 24;
constexpr uint64_t PROPERTY_SEED = 41;
constexpr double BITS_TOLERANCE = 1e-9;

const char* const LIST_TOKENS[] = {"a",  "b",    "c1", "10", "2",   " ",   "  ",
                                   "\t", ",",    ", ", "\n", "\r\n", "x y", "Z"};

ListFormat LineFormat(bool trimSpaces = true) {
    ListFormat format;
    format.trimSpaces = trimSpaces;
    return format;
}

ListFormat SeparatedFormat(std::string_view separator, bool trimSpaces = true) {
    ListFormat format;
    format.oneItemPerLine = false;
    format.separator = separator;
    format.trimSpaces = trimSpaces;
    return format;
}

std::vector<ListFormat> PropertyFormats() {
    return {LineFormat(true), LineFormat(false), SeparatedFormat(", ", true),
            SeparatedFormat(",", false), SeparatedFormat(" ", true)};
}

std::vector<std::string> ItemsIn(std::string_view text, const ListFormat& format) {
    const std::vector<ListField> fields = CSPRNG::SplitList(text, format);
    std::vector<std::string> items;
    for (size_t fieldIndex : CSPRNG::ListItemFieldIndices(fields))
        items.emplace_back(CSPRNG::ListItemText(text, fields[fieldIndex]));
    return items;
}

std::string RandomListText(Tests::SplitMix64& random) {
    std::string text;
    const uint64_t tokenCount = random.Next() % (PROPERTY_MAX_TOKENS + 1);
    for (uint64_t i = 0; i < tokenCount; i++)
        text += LIST_TOKENS[random.Next() % std::size(LIST_TOKENS)];
    return text;
}

bool FieldsCoverTheText(std::string_view text, const std::vector<ListField>& fields) {
    if (fields.empty())
        return false;
    size_t expectedStart = 0;
    for (const ListField& field : fields) {
        if (field.start != expectedStart || field.end < field.start ||
            field.delimiterEnd < field.end || field.itemStart < field.start ||
            field.itemStart + field.itemLength > field.end)
            return false;
        expectedStart = field.delimiterEnd;
    }
    return fields.back().end == text.size() && fields.back().delimiterEnd == text.size();
}

std::vector<uint8_t> FieldRemovalOf(const std::vector<ListField>& fields,
                                    const std::vector<size_t>& itemFields,
                                    const std::vector<bool>& removeItem) {
    std::vector<uint8_t> removeField(fields.size(), 0);
    for (size_t item = 0; item < itemFields.size(); item++) {
        if (removeItem[item])
            removeField[itemFields[item]] = 1;
    }
    return removeField;
}

void SetBuffer(std::vector<char>& buffer, const std::string& text) {
    std::fill(buffer.begin(), buffer.end(), '\0');
    std::copy(text.begin(), text.end(), buffer.begin());
}

OutputSettings ListSettings(int format, const std::string& text) {
    OutputSettings settings;
    settings.outputFormat = format;
    SetBuffer(settings.listText, text);
    return settings;
}

void SetWeight(ListItemWeights& weights, const char* item, int weight) {
    weights.emplace(std::piecewise_construct, std::forward_as_tuple(item),
                    std::forward_as_tuple(weight));
}

void AppendDraw(std::vector<uint8_t>& bytes, uint64_t draw) {
    for (int shift = 56; shift >= 0; shift -= 8)
        bytes.push_back(static_cast<uint8_t>(draw >> shift));
}

std::vector<uint8_t> Draws(std::initializer_list<uint64_t> draws) {
    std::vector<uint8_t> bytes;
    for (uint64_t draw : draws)
        AppendDraw(bytes, draw);
    return bytes;
}

bool NearlyEqual(double actual, double expected) {
    return std::fabs(actual - expected) < BITS_TOLERANCE;
}

struct OrderedPair {
    const char* lower;
    const char* higher;
};

const OrderedPair NATURAL_ORDER_PAIRS[] = {
    {"a1b2", "a1b10"},
    {"x", "x1"},
    {"file-2", "file-10"},
    {"item 9", "item 10"},
    {"99999999999999999999", "100000000000000000000"},
    {"007", "7"},
    {"0", "00"},
    {"a", "B"},
    {"A", "a"},
    {"1", "a"},
    {" ", "0"},
    {"z", "\xC3\xA9"},
    {"", "a"},
};

}

TEST(ListSplitEdgeCases) {
    const std::vector<std::string> ab = {"a", "b"};
    CHECK(ItemsIn(",a,,b,", SeparatedFormat(",")) == ab);
    CHECK_EQ(CSPRNG::SplitList(",a,,b,", SeparatedFormat(",")).size(), size_t(5));
    CHECK(ItemsIn("a, b, ", SeparatedFormat(", ")) == ab);
    CHECK(ItemsIn("\ta\t\n b", LineFormat()) == ab);

    const std::vector<std::string> whole = {"abc"};
    CHECK(ItemsIn("abc", SeparatedFormat("abcd")) == whole);

    const std::vector<std::string> arrows = {"x", "y", "z"};
    CHECK(ItemsIn("x\xE2\x86\x92y\xE2\x86\x92z", SeparatedFormat("\xE2\x86\x92")) == arrows);

    const std::vector<std::string> unicode = {"caf\xC3\xA9", "\xE6\x97\xA5\xE6\x9C\xAC"};
    CHECK(ItemsIn("caf\xC3\xA9\n\xE6\x97\xA5\xE6\x9C\xAC\n", LineFormat()) == unicode);

    const std::vector<std::string> spaceSeparator = {"a", "b c"};
    CHECK(ItemsIn("a   b c", SeparatedFormat("   ")) == spaceSeparator);
    CHECK(ItemsIn("a b", SeparatedFormat(" ")) == ab);

    CHECK(ItemsIn("\n\n\r\n", LineFormat()).empty());
    CHECK_EQ(CSPRNG::CountListItems("\n\n\r\n", LineFormat()), size_t(0));

    const std::vector<std::string> carriageReturnInside = {"a\rb"};
    CHECK(ItemsIn("a\rb", LineFormat(false)) == carriageReturnInside);
    CHECK(ItemsIn("a\rb", LineFormat(true)) == carriageReturnInside);
    const std::vector<std::string> crlfDropped = {"a"};
    CHECK(ItemsIn("a\r\n", LineFormat(false)) == crlfDropped);

    const std::vector<std::string> exactSeparator = {"a ", "b"};
    CHECK(ItemsIn("a , b", SeparatedFormat(", ", false)) == exactSeparator);
}

TEST(ListFieldsAlwaysCoverTheWholeText) {
    Tests::SplitMix64 random(PROPERTY_SEED);
    size_t failures = 0;
    for (int textIndex = 0; textIndex < PROPERTY_TEXT_COUNT; textIndex++) {
        const std::string text = RandomListText(random);
        for (const ListFormat& format : PropertyFormats()) {
            const std::vector<ListField> fields = CSPRNG::SplitList(text, format);
            if (!FieldsCoverTheText(text, fields))
                failures++;
            if (CSPRNG::CountListItems(text, format) != ItemsIn(text, format).size())
                failures++;
            for (const std::string& item : ItemsIn(text, format)) {
                if (item.empty() || item.find('\n') != std::string::npos)
                    failures++;
                if (format.trimSpaces && (item.front() == ' ' || item.back() == ' ' ||
                                          item.front() == '\t' || item.back() == '\t'))
                    failures++;
            }
        }
    }
    CHECK_EQ(failures, size_t(0));
}

TEST(ListRemovingNothingKeepsTheTextExactly) {
    Tests::SplitMix64 random(PROPERTY_SEED + 1);
    size_t failures = 0;
    for (int textIndex = 0; textIndex < PROPERTY_TEXT_COUNT; textIndex++) {
        const std::string text = RandomListText(random);
        for (const ListFormat& format : PropertyFormats()) {
            const std::vector<ListField> fields = CSPRNG::SplitList(text, format);
            const std::vector<uint8_t> removeNothing(fields.size(), 0);
            if (TextOf(CSPRNG::RemoveListFields(text, fields, removeNothing)) != text)
                failures++;
        }
    }
    CHECK_EQ(failures, size_t(0));
}

TEST(ListRemovalLeavesExactlyTheOtherItemsInOrder) {
    Tests::SplitMix64 random(PROPERTY_SEED + 2);
    size_t failures = 0;
    for (int textIndex = 0; textIndex < PROPERTY_TEXT_COUNT; textIndex++) {
        const std::string text = RandomListText(random);
        for (const ListFormat& format : PropertyFormats()) {
            const std::vector<ListField> fields = CSPRNG::SplitList(text, format);
            const std::vector<size_t> itemFields = CSPRNG::ListItemFieldIndices(fields);
            const std::vector<std::string> items = ItemsIn(text, format);

            std::vector<bool> removeItem(items.size());
            std::vector<std::string> expected;
            for (size_t item = 0; item < items.size(); item++) {
                removeItem[item] = (random.Next() & 1) != 0;
                if (!removeItem[item])
                    expected.push_back(items[item]);
            }
            const std::string kept = TextOf(CSPRNG::RemoveListFields(
                text, fields, FieldRemovalOf(fields, itemFields, removeItem)));
            if (ItemsIn(kept, format) != expected)
                failures++;
        }
    }
    CHECK_EQ(failures, size_t(0));
}

TEST(ListDuplicateRemovalKeepsTheFirstCopyOfEachItem) {
    Tests::SplitMix64 random(PROPERTY_SEED + 3);
    size_t failures = 0;
    for (int textIndex = 0; textIndex < PROPERTY_TEXT_COUNT; textIndex++) {
        const std::string text = RandomListText(random);
        for (const ListFormat& format : PropertyFormats()) {
            const std::vector<ListField> fields = CSPRNG::SplitList(text, format);
            std::vector<std::string> expected;
            std::set<std::string> seen;
            for (const std::string& item : ItemsIn(text, format)) {
                if (seen.insert(item).second)
                    expected.push_back(item);
            }
            const std::vector<uint8_t> duplicates = CSPRNG::DuplicateListFields(text, fields);
            const std::string withoutDuplicates =
                TextOf(CSPRNG::RemoveListFields(text, fields, duplicates));
            if (ItemsIn(withoutDuplicates, format) != expected)
                failures++;
        }
    }
    CHECK_EQ(failures, size_t(0));
}

TEST(ListJoiningAndSplittingAgainGivesTheSameItems) {
    Tests::SplitMix64 random(PROPERTY_SEED + 4);
    size_t failures = 0;
    for (int textIndex = 0; textIndex < PROPERTY_TEXT_COUNT; textIndex++) {
        const std::string text = RandomListText(random);
        for (const ListFormat& format : PropertyFormats()) {
            const std::vector<ListField> fields = CSPRNG::SplitList(text, format);
            const std::vector<size_t> itemFields = CSPRNG::ListItemFieldIndices(fields);
            const std::vector<char> joined = CSPRNG::JoinListItems(text, fields, itemFields, "\n");
            if (joined.size() != CSPRNG::JoinedListLength(fields, itemFields, "\n"))
                failures++;
            if (ItemsIn(TextOf(joined), LineFormat(format.trimSpaces)) != ItemsIn(text, format))
                failures++;
        }
    }
    CHECK_EQ(failures, size_t(0));

    const std::vector<ListField> noFields;
    CHECK(CSPRNG::JoinListItems("", noFields, {}, ", ").empty());
    CHECK_EQ(CSPRNG::JoinedListLength(noFields, {}, ", "), size_t(0));
}

TEST(ListNaturalOrderKnownPairs) {
    for (const OrderedPair& pair : NATURAL_ORDER_PAIRS) {
        CHECK_EQ(CSPRNG::CompareListItemsNaturally(pair.lower, pair.higher), -1);
        CHECK_EQ(CSPRNG::CompareListItemsNaturally(pair.higher, pair.lower), 1);
        CHECK_EQ(CSPRNG::CompareListItemsNaturally(pair.lower, pair.lower), 0);
    }
}

TEST(ListSortIsAStablePermutation) {
    Tests::SplitMix64 random(PROPERTY_SEED + 5);
    size_t failures = 0;
    for (int textIndex = 0; textIndex < PROPERTY_TEXT_COUNT; textIndex++) {
        const std::string text = RandomListText(random) + "\na\na\nA";
        const ListFormat format = LineFormat();
        const std::vector<ListField> fields = CSPRNG::SplitList(text, format);
        const std::vector<size_t> itemFields = CSPRNG::ListItemFieldIndices(fields);

        for (bool descending : {false, true}) {
            std::vector<size_t> order = itemFields;
            CSPRNG::SortListFields(text, fields, order, descending);
            std::vector<size_t> sortedOrder = order;
            std::sort(sortedOrder.begin(), sortedOrder.end());
            if (sortedOrder != itemFields)
                failures++;
            for (size_t i = 1; i < order.size(); i++) {
                const int comparison =
                    CSPRNG::CompareListItemsNaturally(CSPRNG::ListItemText(text, fields[order[i - 1]]),
                                                      CSPRNG::ListItemText(text, fields[order[i]]));
                if (descending ? comparison < 0 : comparison > 0)
                    failures++;
                if (comparison == 0 && order[i - 1] > order[i])
                    failures++;
            }
        }
    }
    CHECK_EQ(failures, size_t(0));
}

TEST(ListPickMapsDrawsAroundZeroWeights) {
    const std::vector<size_t> skipsTheZero = {0, 0, 2, 2, 2};
    CHECK(CSPRNG::PickListItems(Draws({5, 6, 7, 8, 9}), {2, 0, 3}, 5, true) == skipsTheZero);

    const std::vector<size_t> eachOnce = {0, 2, 1};
    CHECK(CSPRNG::PickListItems(Draws({3, 1, 0}), {1, 1, 1}, 3, false) == eachOnce);

    const std::vector<size_t> sixth = {6};
    CHECK(CSPRNG::PickListItems(Draws({5, 6}), std::vector<uint64_t>(10, 1), 1, true) == sixth);

    const std::vector<size_t> onlyItem = {0, 0, 0};
    CHECK(CSPRNG::PickListItems(Draws({UINT64_MAX, 12345, 9}), {7}, 3, true) == onlyItem);

    CHECK_THROWS(CSPRNG::PickListItems(Draws({3}), {1, 1, 1}, 2, true));
    CHECK_THROWS(CSPRNG::PickListItems(Draws({3}), {UINT64_MAX, 1}, 1, true));
    CHECK(CSPRNG::PickListItems({}, {1, 1}, 0, true).empty());
}

TEST(ListShuffleEdgeCases) {
    std::vector<size_t> empty;
    CSPRNG::ShuffleListOrder({}, empty);
    CHECK(empty.empty());

    std::vector<size_t> single = {0};
    CSPRNG::ShuffleListOrder({}, single);
    CHECK(single == std::vector<size_t>{0});

    std::vector<size_t> pair = {0, 1};
    CHECK_THROWS(CSPRNG::ShuffleListOrder({}, pair));

    std::vector<size_t> four = {0, 1, 2, 3};
    CSPRNG::ShuffleListOrder(Draws({4, 3, 1}), four);
    const std::vector<size_t> expected = {2, 1, 3, 0};
    CHECK(four == expected);
}

TEST(ListDrawCountsAndBits) {
    CHECK_EQ(CSPRNG::ListPickByteCount(0), size_t(64));
    CHECK_EQ(CSPRNG::ListPickByteCount(-3), size_t(64));
    CHECK_EQ(CSPRNG::ListPickByteCount(5), size_t(104));
    CHECK_EQ(CSPRNG::ListShuffleByteCount(0), size_t(64));
    CHECK_EQ(CSPRNG::ListShuffleByteCount(1), size_t(64));
    CHECK_EQ(CSPRNG::ListShuffleByteCount(2), size_t(72));

    CHECK(NearlyEqual(CSPRNG::ListPickBits(4, 2, false), std::log2(12.0)));
    CHECK(NearlyEqual(CSPRNG::ListPickBits(4, 2, true), 4.0));
    CHECK(NearlyEqual(CSPRNG::ListPickBits(3, 5, false), std::log2(6.0)));
    CHECK(NearlyEqual(CSPRNG::ListPickBits(1, 5, true), 0.0));
    CHECK(NearlyEqual(CSPRNG::ListPickBits(10, 0, false), 0.0));
    CHECK(NearlyEqual(CSPRNG::ListShuffleBits(3), std::log2(6.0)));
    CHECK(NearlyEqual(CSPRNG::ListShuffleBits(1), 0.0));
    CHECK(NearlyEqual(CSPRNG::ListShuffleBits(0), 0.0));

    OutputSettings joined = ListSettings(FORMAT_LIST_SHUFFLE, "a, b, c");
    joined.listOneItemPerLine = false;
    CHECK_EQ(CSPRNG::ListRandomByteCount(joined), size_t(2 * 8 + 64));
}

TEST(ListInputErrorMessages) {
    OutputSettings tooMany = ListSettings(FORMAT_LIST_PICK, "a\nb\nc");
    tooMany.outputQuantity = 4;
    CHECK_EQ(CSPRNG::ListInputError(tooMany),
             std::string("Can't pick 4 different items: the list has only 3. Lower Items to "
                         "pick, or tick Allow repeats."));

    tooMany.listRemovePicked = true;
    CHECK_EQ(CSPRNG::ListInputError(tooMany),
             std::string("Can't pick 4 different items: the list has only 3. Lower Items to pick."));

    tooMany.listAllowRepeats = true;
    CHECK(!CSPRNG::ListInputError(tooMany).empty());
    tooMany.listRemovePicked = false;
    tooMany.outputQuantity = 10;
    CHECK_EQ(CSPRNG::ListInputError(tooMany), std::string(""));

    OutputSettings weighted = ListSettings(FORMAT_LIST_PICK, "a\nb\nc");
    weighted.outputQuantity = 3;
    weighted.listUseWeights = true;
    SetWeight(weighted.listItemWeights, "c", 0);
    CHECK_EQ(CSPRNG::ListInputError(weighted),
             std::string("Can't pick 3 different items: the list has only 2 with a weight above "
                         "0. Lower Items to pick, or tick Allow repeats."));
    weighted.listUseWeights = false;
    CHECK_EQ(CSPRNG::ListInputError(weighted), std::string(""));

    CHECK_EQ(CSPRNG::ListInputError(ListSettings(FORMAT_LIST_SHUFFLE, "a\nb")), std::string(""));
    OutputSettings joinedShuffle = ListSettings(FORMAT_LIST_SHUFFLE, "a, b");
    joinedShuffle.listOneItemPerLine = false;
    CHECK_EQ(CSPRNG::ListInputError(joinedShuffle), std::string(""));
    joinedShuffle.listOneItemPerLine = true;
    CHECK(!CSPRNG::ListInputError(joinedShuffle).empty());
}

TEST(ListSettingsHelpers) {
    OutputSettings settings = ListSettings(FORMAT_LIST_PICK, "one\ntwo");
    CHECK(CSPRNG::ListTextOf(settings) == std::string_view("one\ntwo"));
    CHECK(CSPRNG::ListFormatOf(settings).oneItemPerLine);
    CHECK(CSPRNG::ListFormatOf(settings).separator == std::string_view(", "));
    CHECK(CSPRNG::ListFormatOf(settings).trimSpaces);

    CHECK(!CSPRNG::ListPicksCanRepeat(settings));
    settings.listAllowRepeats = true;
    CHECK(CSPRNG::ListPicksCanRepeat(settings));
    settings.listRemovePicked = true;
    CHECK(!CSPRNG::ListPicksCanRepeat(settings));

    ListItemWeights weights;
    SetWeight(weights, "big", INT_MAX);
    SetWeight(weights, "negative", -5);
    SetWeight(weights, "zero", 0);
    CHECK_EQ(CSPRNG::ListWeightOf(weights, "missing"), AppConfig::LIST_DEFAULT_WEIGHT);
    CHECK_EQ(CSPRNG::ListWeightOf(weights, "big"), AppConfig::LIST_MAX_WEIGHT);
    CHECK_EQ(CSPRNG::ListWeightOf(weights, "negative"), AppConfig::LIST_MIN_WEIGHT);
    CHECK_EQ(CSPRNG::ListWeightOf(weights, "zero"), 0);

    OutputSettings weighted = ListSettings(FORMAT_LIST_PICK, "zero\nother\nzero");
    SetWeight(weighted.listItemWeights, "zero", 0);
    const std::string_view text = CSPRNG::ListTextOf(weighted);
    const std::vector<ListField> fields = CSPRNG::SplitList(text, CSPRNG::ListFormatOf(weighted));
    const std::vector<size_t> itemFields = CSPRNG::ListItemFieldIndices(fields);
    CHECK(CSPRNG::ListPickWeights(weighted, text, fields, itemFields) ==
          (std::vector<uint64_t>{1, 1, 1}));
    weighted.listUseWeights = true;
    CHECK(CSPRNG::ListPickWeights(weighted, text, fields, itemFields) ==
          (std::vector<uint64_t>{0, 1, 0}));
}

TEST(ListSpreadsheetFormulaStarts) {
    const char* const formulaStarts[] = {"=1", "+1", "-1", "@A", "\tx", "\rx"};
    for (const char* text : formulaStarts)
        CHECK(CSPRNG::StartsLikeSpreadsheetFormula(text, strlen(text)));
    const char* const plainStarts[] = {"a=1", " =1", "#1", "'1", "1"};
    for (const char* text : plainStarts)
        CHECK(!CSPRNG::StartsLikeSpreadsheetFormula(text, strlen(text)));
    CHECK(!CSPRNG::StartsLikeSpreadsheetFormula("=", 0));
}
