#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace CSPRNG {

inline constexpr char ONE_TIME_PAD_ALPHABET[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz ";
inline constexpr int ONE_TIME_PAD_ALPHABET_SIZE = sizeof(ONE_TIME_PAD_ALPHABET) - 1;

inline constexpr char ONE_TIME_PAD_FILE_CHANGED_ERROR[] =
    "File changed since it was selected, browse it again.";

inline bool IsOneTimePadCharacter(unsigned int character) {
  return (character >= '0' && character <= '9') ||
         (character >= 'A' && character <= 'Z') ||
         (character >= 'a' && character <= 'z') || character == ' ';
}

bool GenerateOneTimePadKey(const std::vector<uint8_t> &randomBytes,
                           size_t length, std::vector<char> &key);

bool GenerateOneTimePadText(const std::vector<uint8_t> &randomBytes,
                            const char *message, size_t length,
                            std::vector<char> &key,
                            std::vector<char> &ciphertext,
                            std::string &errorMessage);

bool GenerateOneTimePadFile(const std::vector<uint8_t> &randomBytes,
                            const std::vector<uint8_t> &fileData,
                            std::vector<char> &key,
                            std::vector<char> &ciphertext);

std::vector<char> BuildOneTimePadExportText(const char *key,
                                            const char *ciphertext,
                                            size_t length, bool fileMode,
                                            const std::string &fileName);

std::vector<uint8_t> OneTimePadValues(const char *text, size_t length);

std::vector<uint8_t> BuildOneTimePadExportBinary(const char *key,
                                                 const char *ciphertext,
                                                 size_t length, bool fileMode);

}
