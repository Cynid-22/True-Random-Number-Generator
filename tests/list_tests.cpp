#include "test_framework.h"
#include "test_data.h"
#include "../src/core/app_state.h"
#include "../src/logic/formats/list_formats.h"
#include "../src/logic/list_input.h"
#include "../src/logic/output_export.h"
#include "../src/logic/output_generator.h"
#include <algorithm>
#include <numeric>
#include <set>

using CSPRNG::GenerationResult;
using CSPRNG::ListFormat;
using Entropy::EntropySource;
using Tests::FIRST_FULL_CREDIT_SEED;
using Tests::MICROPHONE_ONLY;
using Tests::TextOf;

namespace {

constexpr int SIGN_SAMPLE_STRINGS = 300;
constexpr int MAX_SAMPLE_LENGTH = 5;

ListFormat LinesFormat() {
    return ListFormat{};
}

ListFormat SeparatorFormat(std::string_view separator, bool trimSpaces = true) {
    ListFormat format;
    format.oneItemPerLine = false;
    format.separator = separator;
    format.trimSpaces = trimSpaces;
    return format;
}

std::vector<std::string> ItemsOf(std::string_view text, const ListFormat& format) {
    const std::vector<CSPRNG::ListField> fields = CSPRNG::SplitList(text, format);
    std::vector<std::string> items;
    for (size_t fieldIndex : CSPRNG::ListItemFieldIndices(fields))
        items.emplace_back(CSPRNG::ListItemText(text, fields[fieldIndex]));
    return items;
}

std::string WithFieldsRemoved(std::string_view text, const ListFormat& format,
                              const std::vector<size_t>& itemsToRemove) {
    const std::vector<CSPRNG::ListField> fields = CSPRNG::SplitList(text, format);
    const std::vector<size_t> itemFields = CSPRNG::ListItemFieldIndices(fields);
    std::vector<uint8_t> removeField(fields.size(), 0);
    for (size_t item : itemsToRemove)
        removeField[itemFields[item]] = 1;
    return TextOf(CSPRNG::RemoveListFields(text, fields, removeField));
}

std::vector<std::string> Sorted(std::string_view text, bool descending) {
    const ListFormat format = LinesFormat();
    const std::vector<CSPRNG::ListField> fields = CSPRNG::SplitList(text, format);
    std::vector<size_t> order = CSPRNG::ListItemFieldIndices(fields);
    CSPRNG::SortListFields(text, fields, order, descending);
    std::vector<std::string> items;
    for (size_t fieldIndex : order)
        items.emplace_back(CSPRNG::ListItemText(text, fields[fieldIndex]));
    return items;
}

void AppendDraw(std::vector<uint8_t>& bytes, uint64_t draw) {
    for (int shift = 56; shift >= 0; shift -= 8)
        bytes.push_back(static_cast<uint8_t>(draw >> shift));
}

int Sign(int value) {
    return (value > 0) - (value < 0);
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

Entropy::EntropyPool& Pool() {
    return g_state.entropyPool;
}

void FeedFullMicrophoneStartup() {
    Pool().SecureWipe();
    Pool().AddDataPoints(Tests::SamplePoints(EntropySource::Microphone, FIRST_FULL_CREDIT_SEED, 0,
                                             Entropy::STARTUP_SAMPLE_COUNT),
                         MICROPHONE_ONLY);
}

std::string TextBeforeNull(const std::vector<char>& text) {
    return std::string(text.begin(), std::find(text.begin(), text.end(), '\0'));
}

std::vector<std::string> LinesOf(const std::string& text) {
    return ItemsOf(text, LinesFormat());
}

bool Contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

}

TEST(ListSplitsLinesTrimsSpacesAndSkipsEmptyItems) {
    const std::vector<std::string> expected = {"apple", "banana", "cherry"};
    CHECK(ItemsOf("apple\n  banana \n\n \r\ncherry\r\n", LinesFormat()) == expected);
    CHECK_EQ(CSPRNG::CountListItems("apple\n  banana \n\n \r\ncherry\r\n", LinesFormat()),
             size_t(3));

    ListFormat untrimmed = LinesFormat();
    untrimmed.trimSpaces = false;
    const std::vector<std::string> keptSpaces = {" a ", "b"};
    CHECK(ItemsOf(" a \r\nb", untrimmed) == keptSpaces);
    CHECK(ItemsOf("", LinesFormat()).empty());
}

TEST(ListSplitsAtTheSeparatorAndAtLineBreaks) {
    const std::vector<std::string> expected = {"a", "b", "c", "d"};
    CHECK(ItemsOf("a, b,c\nd", SeparatorFormat(", ")) == expected);
    CHECK(ItemsOf("a | b|c\n d", SeparatorFormat(" | ")) == expected);
    CHECK(ItemsOf("a  b c\nd", SeparatorFormat(" ")) == expected);

    const std::vector<std::string> exact = {"a", "b,c", "d"};
    CHECK(ItemsOf("a, b,c, d", SeparatorFormat(", ", false)) == exact);

    const std::vector<std::string> linesOnly = {"a, b", "c"};
    CHECK(ItemsOf("a, b\nc", SeparatorFormat("")) == linesOnly);
    CHECK(CSPRNG::ListJoinsWithLineBreaks(SeparatorFormat("")));
    CHECK(CSPRNG::ListJoiner(SeparatorFormat(", ")) == std::string_view(", "));
    CHECK(CSPRNG::ListJoiner(LinesFormat()) == std::string_view("\n"));
}

TEST(ListSortsInNaturalAlphanumericOrder) {
    const std::string text = "item10\nitem2\nItem1\nbanana\nApple\nitem02\n10\n9";
    const std::vector<std::string> ascending = {"9",      "10",     "Apple", "banana",
                                                "Item1",  "item02", "item2", "item10"};
    CHECK(Sorted(text, false) == ascending);
    std::vector<std::string> descending = ascending;
    std::reverse(descending.begin(), descending.end());
    CHECK(Sorted(text, true) == descending);
}

TEST(ListNaturalOrderIsATotalOrder) {
    const char alphabet[] = {'a', 'A', 'b', '0', '1', '9', ' ', '-', '~', '\xC3'};
    Tests::SplitMix64 random(7);
    std::vector<std::string> samples;
    for (int i = 0; i < SIGN_SAMPLE_STRINGS; i++) {
        std::string sample;
        const int length = static_cast<int>(random.Next() % (MAX_SAMPLE_LENGTH + 1));
        for (int c = 0; c < length; c++)
            sample.push_back(alphabet[random.Next() % sizeof(alphabet)]);
        samples.push_back(sample);
    }

    size_t violations = 0;
    for (const std::string& a : samples) {
        for (const std::string& b : samples) {
            const int ab = CSPRNG::CompareListItemsNaturally(a, b);
            if (Sign(ab) != -Sign(CSPRNG::CompareListItemsNaturally(b, a)) ||
                (ab == 0) != (a == b))
                violations++;
        }
    }
    std::vector<std::string> ordered = samples;
    std::sort(ordered.begin(), ordered.end(), [](const std::string& a, const std::string& b) {
        return CSPRNG::CompareListItemsNaturally(a, b) < 0;
    });
    for (size_t i = 0; i < ordered.size(); i++) {
        for (size_t j = i + 1; j < ordered.size(); j++) {
            if (CSPRNG::CompareListItemsNaturally(ordered[i], ordered[j]) > 0)
                violations++;
        }
    }
    CHECK_EQ(violations, size_t(0));
}

TEST(ListRemovalKeepsTheRestOfTheText) {
    const ListFormat commas = SeparatorFormat(", ");
    CHECK_EQ(WithFieldsRemoved("a, b, c", commas, {1}), std::string("a, c"));
    CHECK_EQ(WithFieldsRemoved("a, b, c", commas, {2}), std::string("a, b"));
    CHECK_EQ(WithFieldsRemoved("a, b, c", commas, {1, 2}), std::string("a"));
    CHECK_EQ(WithFieldsRemoved("a\r\nb\n\nc\n", LinesFormat(), {1}), std::string("a\r\n\nc\n"));
    CHECK_EQ(WithFieldsRemoved("a\nb", LinesFormat(), {0, 1}), std::string(""));
}

TEST(ListRemovalKeepsLineBreaksBetweenLines) {
    const ListFormat commas = SeparatorFormat(", ");
    CHECK_EQ(WithFieldsRemoved("a, b, c\nd, e", commas, {2}), std::string("a, b\nd, e"));
    CHECK_EQ(WithFieldsRemoved("a, b\nc, d", commas, {2}), std::string("a, b\nd"));
    CHECK_EQ(WithFieldsRemoved("a, b\r\nc, d", commas, {1, 2}), std::string("a\r\nd"));
    CHECK_EQ(WithFieldsRemoved("a, b\nc", commas, {0}), std::string("b\nc"));
}

TEST(ListFindsDuplicatesAfterTrimming) {
    const std::string text = "a\nb\na\n b \nc";
    const std::vector<CSPRNG::ListField> fields = CSPRNG::SplitList(text, LinesFormat());
    const std::vector<uint8_t> duplicates = CSPRNG::DuplicateListFields(text, fields);
    const std::vector<uint8_t> expected = {0, 0, 1, 1, 0};
    CHECK(duplicates == expected);
    CHECK_EQ(TextOf(CSPRNG::RemoveListFields(text, fields, duplicates)), std::string("a\nb\nc"));
}

TEST(ListJoinsItemsInTheGivenOrder) {
    const std::string text = " x ,y,  z";
    const std::vector<CSPRNG::ListField> fields = CSPRNG::SplitList(text, SeparatorFormat(","));
    const std::vector<size_t> order = {2, 0, 1};
    CHECK_EQ(TextOf(CSPRNG::JoinListItems(text, fields, order, " | ")), std::string("z | x | y"));
    CHECK_EQ(CSPRNG::JoinedListLength(fields, order, " | "), size_t(9));
}

TEST(ListPickRejectsBiasedDrawsAndFollowsWeights) {
    std::vector<uint8_t> bytes;
    AppendDraw(bytes, 0);
    AppendDraw(bytes, 5);
    const std::vector<size_t> rejectedThenLast = {2};
    CHECK(CSPRNG::PickListItems(bytes, {1, 1, 1}, 1, false) == rejectedThenLast);

    bytes.clear();
    const uint64_t RANGE = 10;
    for (uint64_t target = 0; target < RANGE; target++)
        AppendDraw(bytes, target + RANGE);
    const std::vector<size_t> byWeight = {0, 1, 1, 2, 2, 2, 3, 3, 3, 3};
    CHECK(CSPRNG::PickListItems(bytes, {1, 2, 3, 4}, RANGE, true) == byWeight);

    const std::vector<uint8_t> random = Tests::RandomBytes(11, CSPRNG::ListPickByteCount(20));
    for (size_t pick : CSPRNG::PickListItems(random, {0, 5, 0}, 20, true))
        CHECK_EQ(pick, size_t(1));

    CHECK_THROWS(CSPRNG::PickListItems(std::vector<uint8_t>(7, 0xFF), {1, 1}, 1, true));
    CHECK_THROWS(CSPRNG::PickListItems(random, {0, 0}, 1, true));
    CHECK_THROWS(CSPRNG::PickListItems(random, {1, 0, 1}, 3, false));
}

TEST(ListPickWithoutRepeatsPicksEachItemOnce) {
    const size_t ITEM_COUNT = 40;
    const std::vector<uint8_t> random =
        Tests::RandomBytes(3, CSPRNG::ListPickByteCount(static_cast<int>(ITEM_COUNT)));
    const std::vector<size_t> picks =
        CSPRNG::PickListItems(random, std::vector<uint64_t>(ITEM_COUNT, 1), ITEM_COUNT, false);
    CHECK_EQ(std::set<size_t>(picks.begin(), picks.end()).size(), ITEM_COUNT);
}

TEST(ListShuffleIsFisherYates) {
    std::vector<uint8_t> bytes;
    AppendDraw(bytes, 3);
    AppendDraw(bytes, 2);
    std::vector<size_t> order = {0, 1, 2};
    CSPRNG::ShuffleListOrder(bytes, order);
    const std::vector<size_t> expected = {1, 2, 0};
    CHECK(order == expected);

    const size_t ITEM_COUNT = 50;
    std::vector<size_t> large(ITEM_COUNT);
    std::iota(large.begin(), large.end(), size_t(0));
    CSPRNG::ShuffleListOrder(Tests::RandomBytes(5, CSPRNG::ListShuffleByteCount(ITEM_COUNT)), large);
    std::vector<size_t> sorted = large;
    std::sort(sorted.begin(), sorted.end());
    std::vector<size_t> identity(ITEM_COUNT);
    std::iota(identity.begin(), identity.end(), size_t(0));
    CHECK(sorted == identity);
    CHECK(large != identity);
}

TEST(ListRandomByteCountCoversEveryDraw) {
    OutputSettings pick = ListSettings(FORMAT_LIST_PICK, "a\nb\nc");
    pick.outputQuantity = 3;
    CHECK_EQ(CSPRNG::RandomByteCount(pick), size_t(3 * 8 + 64));

    const OutputSettings shuffle = ListSettings(FORMAT_LIST_SHUFFLE, "a\nb\nc\nd\ne");
    CHECK_EQ(CSPRNG::RandomByteCount(shuffle), size_t(4 * 8 + 64));
}

TEST(ListPickGeneratesItemsFromTheList) {
    FeedFullMicrophoneStartup();
    OutputSettings settings = ListSettings(FORMAT_LIST_PICK, "red\ngreen\nblue");
    settings.outputQuantity = 2;

    const GenerationResult result = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    CHECK(result.success);
    const std::vector<std::string> picks = LinesOf(TextBeforeNull(result.output));
    CHECK_EQ(picks.size(), size_t(2));
    const std::set<std::string> allowed = {"red", "green", "blue"};
    for (const std::string& pick : picks)
        CHECK(allowed.count(pick) == 1);
    CHECK(picks.size() == 2 && picks[0] != picks[1]);
    CHECK_EQ(result.binaryOutput.size(), size_t(8));
    CHECK(!result.updatesListText);
}

TEST(ListPickCanRemovePickedItems) {
    FeedFullMicrophoneStartup();
    OutputSettings settings = ListSettings(FORMAT_LIST_PICK, "red, green, blue");
    settings.listOneItemPerLine = false;
    SetBuffer(settings.listSeparator, ", ");
    settings.listRemovePicked = true;
    settings.listAllowRepeats = true;

    const GenerationResult result = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    CHECK(result.success);
    CHECK(result.updatesListText);
    CHECK_EQ(result.removedListItemCount, size_t(1));
    const std::string picked = TextBeforeNull(result.output);
    const std::vector<std::string> left =
        ItemsOf(TextOf(result.updatedListText), SeparatorFormat(", "));
    CHECK_EQ(left.size(), size_t(2));
    CHECK(std::find(left.begin(), left.end(), picked) == left.end());
}

TEST(ListPickWeightZeroIsNeverPicked) {
    FeedFullMicrophoneStartup();
    OutputSettings settings = ListSettings(FORMAT_LIST_PICK, "never\nalways\nnever too");
    settings.outputQuantity = 10;
    settings.listAllowRepeats = true;
    settings.listUseWeights = true;
    settings.listItemWeights.emplace(std::piecewise_construct, std::forward_as_tuple("never"),
                                     std::forward_as_tuple(0));
    settings.listItemWeights.emplace(std::piecewise_construct,
                                     std::forward_as_tuple("never too"), std::forward_as_tuple(0));

    const GenerationResult result = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    CHECK(result.success);
    const std::vector<std::string> picks = LinesOf(TextBeforeNull(result.output));
    CHECK_EQ(picks.size(), size_t(10));
    for (const std::string& pick : picks)
        CHECK_EQ(pick, std::string("always"));
}

TEST(ListPickErrorsUseNoEntropy) {
    FeedFullMicrophoneStartup();
    OutputSettings tooMany = ListSettings(FORMAT_LIST_PICK, "a\nb\nc");
    tooMany.outputQuantity = 4;
    const GenerationResult result = CSPRNG::GenerateOutput(MICROPHONE_ONLY, tooMany);
    CHECK(!result.success);
    CHECK(Contains(result.errorMessage, "Can't pick 4 different items"));
    CHECK(!Pool().HasUsedData());

    CHECK(Contains(CSPRNG::ListInputError(ListSettings(FORMAT_LIST_PICK, " \n\n")),
                   "at least one item"));
    CHECK(Contains(CSPRNG::ListInputError(ListSettings(FORMAT_LIST_SHUFFLE, "only")),
                   "at least two items"));

    OutputSettings allZero = ListSettings(FORMAT_LIST_PICK, "a\nb");
    allZero.listUseWeights = true;
    allZero.listItemWeights.emplace(std::piecewise_construct, std::forward_as_tuple("a"),
                                    std::forward_as_tuple(0));
    allZero.listItemWeights.emplace(std::piecewise_construct, std::forward_as_tuple("b"),
                                    std::forward_as_tuple(0));
    CHECK(Contains(CSPRNG::ListInputError(allZero), "weight of 0"));
}

TEST(ListShuffleOutputsEveryItemOnce) {
    FeedFullMicrophoneStartup();
    OutputSettings settings = ListSettings(FORMAT_LIST_SHUFFLE, "one\ntwo\nthree\nfour");

    const GenerationResult perLine = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    CHECK(perLine.success);
    std::vector<std::string> lines = LinesOf(TextBeforeNull(perLine.output));
    std::sort(lines.begin(), lines.end());
    const std::vector<std::string> sortedItems = {"four", "one", "three", "two"};
    CHECK(lines == sortedItems);
    CHECK_EQ(perLine.binaryOutput.size(), size_t(16));
    CHECK(perLine.joinedItemLengths.empty());

    settings.shuffleListInPlace = true;
    const GenerationResult inPlace = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    CHECK(inPlace.success);
    CHECK(inPlace.updatesListText);
    std::vector<std::string> rewritten = LinesOf(TextOf(inPlace.updatedListText));
    std::sort(rewritten.begin(), rewritten.end());
    CHECK(rewritten == sortedItems);
    CHECK_EQ(TextBeforeNull(inPlace.output), std::string(""));
}

TEST(ListShuffleWithSeparatorJoinsItemsAndExportsEachOne) {
    FeedFullMicrophoneStartup();
    OutputSettings settings = ListSettings(FORMAT_LIST_SHUFFLE, "ab,c , d");
    settings.listOneItemPerLine = false;
    SetBuffer(settings.listSeparator, ", ");

    const GenerationResult result = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    CHECK(result.success);
    const std::string joined = TextBeforeNull(result.output);
    CHECK_EQ(joined.size(), size_t(8));
    CHECK_EQ(result.joinedItemLengths.size(), size_t(3));
    CHECK_EQ(result.joinedSeparatorLength, size_t(2));

    GeneratedOutputDetails details;
    details.format = FORMAT_LIST_SHUFFLE;
    details.type = "Shuffle List";
    details.joinedItemLengths = result.joinedItemLengths;
    details.joinedSeparatorLength = result.joinedSeparatorLength;
    std::vector<std::string> exported =
        ItemsOf(TextOf(BuildOutputCsv(details, joined.data(), joined.size())), LinesFormat());
    CHECK_EQ(exported.size(), size_t(4));
    std::vector<std::string> rows(exported.begin() + 1, exported.end());
    std::sort(rows.begin(), rows.end());
    const std::vector<std::string> expected = {"\"ab\"", "\"c\"", "\"d\""};
    CHECK(rows == expected);
}

TEST(ListCsvKeepsSpreadsheetsFromRunningItemsAsFormulas) {
    GeneratedOutputDetails details;
    details.format = FORMAT_LIST_PICK;
    details.type = "Pick From List";
    const std::string text = "=1+1\nplain\n-5\n@SUM(A1)\n+x";
    CHECK_EQ(TextOf(BuildOutputCsv(details, text.data(), text.size())),
             std::string("\"Pick From List\"\r\n\"'=1+1\"\r\n\"plain\"\r\n\"'-5\"\r\n"
                         "\"'@SUM(A1)\"\r\n\"'+x\"\r\n"));

    details.format = FORMAT_RANDOM_CHARACTERS;
    details.type = "T";
    CHECK_EQ(TextOf(BuildOutputCsv(details, "=1", 2)), std::string("\"T\"\r\n\"=1\"\r\n"));
}
