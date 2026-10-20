#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace WordLists {

struct WordList {
  const char *name = "";
  const char *usage = "";
  const char *warning = nullptr;
  std::vector<std::string> words;
  double bitsPerWord = 0.0;
  size_t shortestWordLength = 0;
  size_t longestWordLength = 0;
  bool noWordStartsAnother = false;
  std::array<bool, 256> usesCharacter{};
  std::string characters;
  std::string loadError;

  bool IsUsable() const { return loadError.empty(); }
};

void LoadAll();

const WordList &Get(int wordListId);

void WipeAll();

}
