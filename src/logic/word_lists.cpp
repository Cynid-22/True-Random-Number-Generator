#include "word_lists.h"
#include "../../config/AppConfig.h"
#include "../core/output_format.h"
#include "../resource.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <new>
#include <string_view>
#include <windows.h>

namespace WordLists {

struct WordListSource {
  const char *name;
  int resourceId;
  bool linesStartWithDiceRoll;
  const char *usage;
  const char *warning;
};

static const WordListSource WORD_LIST_SOURCES[] = {
    {"NordPass", IDR_WORDLIST_NORDPASS, false,
     "Large English dictionary. The most bits per word, so the shortest "
     "passphrase for a given strength, but many words are rare and harder "
     "to remember.",
     nullptr},
    {"EFF Large", IDR_WORDLIST_EFF_LARGE, true,
     "The EFF's list for passphrases made with five dice. Common, memorable "
     "words, and no word starts with another, so a passphrase can only be "
     "read one way.",
     nullptr},
    {"Diceware 8k", IDR_WORDLIST_DICEWARE_8K, false,
     "Diceware list sized for computer generation, with a whole number of "
     "bits per word. Very short entries, many with digits or symbols "
     "(a2, a&p, ain't, at&t): compact, but harder to remember.",
     nullptr},
    {"BIP39 English", IDR_WORDLIST_BIP39_ENGLISH, false,
     "Bitcoin's BIP39 list: short common words, each identified by its first "
     "4 letters.",
     "Not a wallet recovery phrase: in BIP39 the last word carries checksum "
     "bits, which this generator doesn't add, so wallets will usually reject "
     "it."},
};
static_assert(std::size(WORD_LIST_SOURCES) == WORD_LIST_COUNT);

struct CharacterRange {
  int first;
  int last;
};

static const CharacterRange NAMED_CHARACTER_RANGES[] = {
    {'a', 'z'}, {'A', 'Z'}, {'0', '9'}};

static std::array<WordList, WORD_LIST_COUNT> s_wordLists;

static bool IsSpaceInLine(char character) {
  return character == ' ' || character == '\t' || character == '\r';
}

static bool IsDigit(char character) {
  return character >= '0' && character <= '9';
}

static std::string LineError(size_t lineNumber, const char *problem) {
  return "line " + std::to_string(lineNumber) + " " + problem;
}

static std::string ParseWords(const char *text, size_t size,
                              bool linesStartWithDiceRoll,
                              std::vector<std::string> &words) {
  words.reserve(static_cast<size_t>(std::count(text, text + size, '\n')) + 1);

  size_t lineStart = 0;
  for (size_t lineNumber = 1; lineStart < size; lineNumber++) {
    const char *newline = static_cast<const char *>(
        memchr(text + lineStart, '\n', size - lineStart));
    size_t end = newline ? static_cast<size_t>(newline - text) : size;
    size_t begin = lineStart;
    lineStart = end + 1;

    while (begin < end && IsSpaceInLine(text[begin]))
      begin++;
    while (end > begin && IsSpaceInLine(text[end - 1]))
      end--;
    if (begin == end)
      continue;

    if (linesStartWithDiceRoll) {
      size_t rollEnd = begin;
      while (rollEnd < end && IsDigit(text[rollEnd]))
        rollEnd++;
      size_t wordBegin = rollEnd;
      while (wordBegin < end && IsSpaceInLine(text[wordBegin]))
        wordBegin++;
      if (rollEnd == begin || wordBegin == rollEnd)
        return LineError(lineNumber, "is not a dice roll followed by a word");
      begin = wordBegin;
    }

    if (std::any_of(text + begin, text + end, IsSpaceInLine))
      return LineError(lineNumber, "holds more than one word");
    words.emplace_back(text + begin, end - begin);
  }
  return std::string();
}

static bool IsInNamedRange(int character) {
  for (const CharacterRange &range : NAMED_CHARACTER_RANGES) {
    if (character >= range.first && character <= range.last)
      return true;
  }
  return false;
}

static std::string DescribeCharacters(const std::array<bool, 256> &usesCharacter) {
  std::string description;
  auto appendGroup = [&description](const std::string &group) {
    if (group.empty())
      return;
    if (!description.empty())
      description += ", ";
    description += group;
  };
  auto spacedCharacters = [&usesCharacter](int first, int last,
                                           bool inNamedRanges) {
    std::string spaced;
    for (int character = first; character <= last; character++) {
      if (!usesCharacter[character] || IsInNamedRange(character) != inNamedRanges)
        continue;
      if (!spaced.empty())
        spaced += ' ';
      spaced += static_cast<char>(character);
    }
    return spaced;
  };
  auto isUsed = [](bool used) { return used; };

  for (const CharacterRange &range : NAMED_CHARACTER_RANGES) {
    const bool usesWholeRange =
        std::all_of(usesCharacter.begin() + range.first,
                    usesCharacter.begin() + range.last + 1, isUsed);
    appendGroup(usesWholeRange
                    ? std::string{static_cast<char>(range.first), '-',
                                  static_cast<char>(range.last)}
                    : spacedCharacters(range.first, range.last, true));
  }
  appendGroup(spacedCharacters('!', '~', false));
  if (std::any_of(usesCharacter.begin() + 128, usesCharacter.end(), isUsed))
    appendGroup("non-ASCII letters");
  return description;
}

static std::string MeasureWords(WordList &list) {
  const size_t wordCount = list.words.size();
  if (wordCount < AppConfig::WORD_LIST_MIN_WORDS)
    return "it has fewer than " + std::to_string(AppConfig::WORD_LIST_MIN_WORDS) +
           " words";
  if (wordCount > AppConfig::WORD_LIST_MAX_WORDS)
    return "it has more than " + std::to_string(AppConfig::WORD_LIST_MAX_WORDS) +
           " words";

  std::vector<std::string_view> sortedWords(list.words.begin(), list.words.end());
  std::sort(sortedWords.begin(), sortedWords.end());
  list.noWordStartsAnother = true;
  for (size_t i = 0; i + 1 < wordCount; i++) {
    if (sortedWords[i] == sortedWords[i + 1])
      return "the word \"" + std::string(sortedWords[i]) +
             "\" appears more than once";
    if (sortedWords[i + 1].substr(0, sortedWords[i].size()) == sortedWords[i])
      list.noWordStartsAnother = false;
  }

  list.shortestWordLength = SIZE_MAX;
  list.longestWordLength = 0;
  for (const std::string &word : list.words) {
    list.shortestWordLength = std::min(list.shortestWordLength, word.size());
    list.longestWordLength = std::max(list.longestWordLength, word.size());
    for (unsigned char character : word)
      list.usesCharacter[character] = true;
  }
  list.characters = DescribeCharacters(list.usesCharacter);
  list.bitsPerWord = std::log2(static_cast<double>(wordCount));
  return std::string();
}

static bool FindEmbeddedText(int resourceId, const char *&text, size_t &size) {
  HRSRC resourceInfo =
      FindResourceA(nullptr, MAKEINTRESOURCEA(resourceId), (LPCSTR)RT_RCDATA);
  if (!resourceInfo)
    return false;
  HGLOBAL resourceHandle = LoadResource(nullptr, resourceInfo);
  if (!resourceHandle)
    return false;
  text = static_cast<const char *>(LockResource(resourceHandle));
  size = SizeofResource(nullptr, resourceInfo);
  return text != nullptr && size > 0;
}

static void Load(WordList &list, const WordListSource &source) {
  list = WordList();

  std::string problem;
  try {
    const char *text = nullptr;
    size_t size = 0;
    if (!FindEmbeddedText(source.resourceId, text, size))
      problem = "it is missing from the program";
    else
      problem = ParseWords(text, size, source.linesStartWithDiceRoll, list.words);
    if (problem.empty())
      problem = MeasureWords(list);
  } catch (const std::bad_alloc &) {
    problem = "there is not enough memory to load it";
  }

  if (!problem.empty()) {
    list = WordList();
    list.loadError =
        std::string("The ") + source.name + " word list can't be used: " + problem + ".";
  }
  list.name = source.name;
  list.usage = source.usage;
  list.warning = source.warning;
}

void LoadAll() {
  for (int wordListId = 0; wordListId < WORD_LIST_COUNT; wordListId++)
    Load(s_wordLists[wordListId], WORD_LIST_SOURCES[wordListId]);
}

const WordList &Get(int wordListId) {
  if (wordListId < 0 || wordListId >= WORD_LIST_COUNT)
    wordListId = WORD_LIST_NORDPASS;
  return s_wordLists[wordListId];
}

void WipeAll() {
  for (WordList &list : s_wordLists) {
    for (std::string &word : list.words) {
      if (!word.empty())
        SecureZeroMemory(word.data(), word.size());
    }
    list.words.clear();
    list.words.shrink_to_fit();
  }
}

}
