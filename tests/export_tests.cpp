#include "test_framework.h"
#include "test_data.h"
#include "../src/logic/output_export.h"
#include <cstring>

using Tests::TextOf;

namespace {

std::string Csv(const GeneratedOutputDetails& details, const std::string& text) {
    return TextOf(BuildOutputCsv(details, text.data(), text.size()));
}

std::string Json(const GeneratedOutputDetails& details, const std::string& text) {
    return TextOf(BuildOutputJson(details, "2026-11-07 12:00:00", text.data(), text.size()));
}

bool Contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

bool EndsWith(const std::string& text, const std::string& ending) {
    return text.size() >= ending.size() &&
           text.compare(text.size() - ending.size(), ending.size(), ending) == 0;
}

}

TEST(CsvQuotesEveryFieldAndDoublesQuotes) {
    GeneratedOutputDetails details;
    details.type = "Ty\"pe";
    CHECK_EQ(Csv(details, "x\"y\nz"), std::string("\"Ty\"\"pe\"\r\n\"x\"\"y\"\r\n\"z\"\r\n"));
}

TEST(CsvNumberedItemsSkipBlankLines) {
    GeneratedOutputDetails details;
    details.type = "T";
    details.numberedItems = true;
    CHECK_EQ(Csv(details, "1. foo\n\n2. bar"),
             std::string("\"#\",\"T\"\r\n\"1\",\"foo\"\r\n\"2\",\"bar\"\r\n"));
}

TEST(CsvKeepsSpreadsheetsFromRunningGeneratedTextAsFormulas) {
    GeneratedOutputDetails details;
    details.type = "T";

    details.format = FORMAT_RANDOM_CHARACTERS;
    CHECK_EQ(Csv(details, "=1\nab"), std::string("\"T\"\r\n\"'=1\"\r\n\"ab\"\r\n"));

    details.format = FORMAT_RAW_DATA;
    CHECK_EQ(Csv(details, "+ab/\n-cd_"), std::string("\"T\"\r\n\"'+ab/\"\r\n\"'-cd_\"\r\n"));

    details.format = FORMAT_PASSPHRASE;
    details.numberedItems = true;
    CHECK_EQ(Csv(details, "1. @-word"), std::string("\"#\",\"T\"\r\n\"1\",\"'@-word\"\r\n"));
}

TEST(CsvKeepsIntegerItemsAsPlainNumbers) {
    GeneratedOutputDetails details;
    details.type = "T";
    details.format = FORMAT_INTEGER;
    CHECK_EQ(Csv(details, "-5\n7"), std::string("\"T\"\r\n\"-5\"\r\n\"7\"\r\n"));
}

TEST(OneTimePadCsvWritesFileResultsAsHex) {
    const char key[] = {'\x00', '\xAB'};
    const char ciphertext[] = {'\xFF', '\x10'};
    CHECK_EQ(TextOf(BuildOneTimePadCsv(key, ciphertext, sizeof(key), true)),
             std::string("\"key\",\"ciphertext\"\r\n\"00AB\",\"FF10\"\r\n"));
    CHECK_EQ(TextOf(BuildOneTimePadCsv("AB C", "x\"yz", 4, false)),
             std::string("\"key\",\"ciphertext\"\r\n\"AB C\",\"x\"\"yz\"\r\n"));
}

TEST(JsonEscapesQuotesBackslashesAndControlCharacters) {
    GeneratedOutputDetails details;
    details.type = "T";
    details.securityMode = "TRUE RANDOM";
    details.settings.push_back({"quantity", "2", false});
    details.settings.push_back({"label", "a\"b", true});
    const std::string item = std::string("q\"\\") + '\x01';

    const std::string json = Json(details, item);
    CHECK(Contains(json, "  \"securityMode\": \"TRUE RANDOM\",\n"));
    CHECK(Contains(json, "  \"settings\": {\n    \"quantity\": 2,\n    \"label\": \"a\\\"b\"\n  },\n"));
    CHECK(EndsWith(json, "  \"items\": [\n    \"q\\\"\\\\\\u0001\"\n  ]\n}\n"));
}

TEST(OneTimePadJsonWritesFileResultsAsHex) {
    GeneratedOutputDetails details;
    details.format = FORMAT_ONE_TIME_PAD;
    details.securityMode = "TRUE RANDOM";
    const char key[] = {'\x00', '\xAB'};
    const char ciphertext[] = {'\xFF', '\x10'};
    const std::string json = TextOf(
        BuildOneTimePadJson(details, "2026-11-07 12:00:00", key, ciphertext, sizeof(key), true));
    CHECK(Contains(json, "  \"encoding\": \"hex\",\n"));
    CHECK(Contains(json, "  \"key\": \"00AB\",\n"));
    CHECK(EndsWith(json, "  \"ciphertext\": \"FF10\"\n}\n"));
}
