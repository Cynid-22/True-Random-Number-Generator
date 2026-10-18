#include "one_time_pad_format.h"
#include "cancellation.h"
#include <windows.h>

namespace CSPRNG {

static int OneTimePadAlphabetIndex(char character) {
  if (character >= '0' && character <= '9')
    return character - '0';
  if (character >= 'A' && character <= 'Z')
    return 10 + (character - 'A');
  if (character >= 'a' && character <= 'z')
    return 36 + (character - 'a');
  if (character == ' ')
    return 62;
  return -1;
}

static bool DrawUniformAlphabetIndex(const std::vector<uint8_t> &randomBytes,
                                     size_t &randomByteIndex,
                                     int &alphabetIndex) {
  constexpr int acceptBelow = 256 - (256 % ONE_TIME_PAD_ALPHABET_SIZE);
  while (randomByteIndex < randomBytes.size()) {
    uint8_t draw = randomBytes[randomByteIndex++];
    if (draw < acceptBelow) {
      alphabetIndex = draw % ONE_TIME_PAD_ALPHABET_SIZE;
      return true;
    }
  }
  return false;
}

static void WipeVector(std::vector<char> &data) {
  SecureZeroMemory(data.data(), data.size());
  data.clear();
}

bool GenerateOneTimePadKey(const std::vector<uint8_t> &randomBytes,
                           size_t length, std::vector<char> &key) {
  WipeVector(key);
  key.reserve(length + 1);
  size_t randomByteIndex = 0;

  for (size_t i = 0; i < length; i++) {
    if ((i & 0x3FFF) == 0)
      ThrowIfGenerationCancelled();

    int keyValue = 0;
    if (!DrawUniformAlphabetIndex(randomBytes, randomByteIndex, keyValue)) {
      WipeVector(key);
      return false;
    }
    key.push_back(ONE_TIME_PAD_ALPHABET[keyValue]);
  }

  return true;
}

bool GenerateOneTimePadText(const std::vector<uint8_t> &randomBytes,
                            const char *message, size_t length,
                            std::vector<char> &key,
                            std::vector<char> &ciphertext,
                            std::string &errorMessage) {
  for (size_t i = 0; i < length; i++) {
    if (OneTimePadAlphabetIndex(message[i]) < 0) {
      errorMessage = "Message may only contain letters (A-Z, a-z), numbers "
                     "(0-9) and spaces. No punctuation.";
      return false;
    }
  }

  if (!GenerateOneTimePadKey(randomBytes, length, key)) {
    errorMessage = "Insufficient entropy (rejection sampling exhausted). "
                   "Please retry.";
    return false;
  }

  WipeVector(ciphertext);
  ciphertext.reserve(length + 1);
  for (size_t i = 0; i < length; i++) {
    int cipherValue = (OneTimePadAlphabetIndex(message[i]) +
                       OneTimePadAlphabetIndex(key[i])) %
                      ONE_TIME_PAD_ALPHABET_SIZE;
    ciphertext.push_back(ONE_TIME_PAD_ALPHABET[cipherValue]);
  }

  key.push_back('\0');
  ciphertext.push_back('\0');
  return true;
}

bool GenerateOneTimePadFile(const std::vector<uint8_t> &randomBytes,
                            const std::vector<uint8_t> &fileData,
                            std::vector<char> &key,
                            std::vector<char> &ciphertext) {
  WipeVector(key);
  WipeVector(ciphertext);
  if (randomBytes.size() < fileData.size())
    return false;

  key.resize(fileData.size());
  ciphertext.resize(fileData.size());
  for (size_t i = 0; i < fileData.size(); i++) {
    if ((i & 0xFFFFF) == 0)
      ThrowIfGenerationCancelled();
    key[i] = static_cast<char>(randomBytes[i]);
    ciphertext[i] = static_cast<char>(fileData[i] ^ randomBytes[i]);
  }

  return true;
}

static void AppendText(std::vector<char> &output, const std::string &text) {
  output.insert(output.end(), text.begin(), text.end());
}

static void AppendAsTextOrHex(std::vector<char> &output, const char *data,
                              size_t length, bool encodeAsHex) {
  if (!encodeAsHex) {
    output.insert(output.end(), data, data + length);
    return;
  }
  static const char HEX_DIGITS[] = "0123456789ABCDEF";
  for (size_t i = 0; i < length; i++) {
    uint8_t byte = static_cast<uint8_t>(data[i]);
    output.push_back(HEX_DIGITS[byte >> 4]);
    output.push_back(HEX_DIGITS[byte & 0x0F]);
  }
}

std::vector<char> BuildOneTimePadExportText(const char *key,
                                            const char *ciphertext,
                                            size_t length, bool fileMode,
                                            const std::string &fileName) {
  const std::string lengthText = std::to_string(length);
  std::string header = "TRNG One-Time Pad: key and ciphertext\n\n";
  std::string keyTitle;
  std::string ciphertextTitle;

  if (fileMode) {
    header += "File:     " + fileName + " (" + lengthText + " bytes)\n"
              "Method:   One-Time Pad (Vernam cipher)\n"
              "Encrypt:  each byte: ciphertext = file XOR key\n"
              "Decrypt:  file = ciphertext XOR key (the same XOR)\n"
              "Encoding: KEY and CIPHERTEXT below are hexadecimal, 2 "
              "characters per byte\n";
    keyTitle = "KEY (" + lengthText + " bytes, hex)\n";
    ciphertextTitle = "CIPHERTEXT (" + lengthText + " bytes, hex)\n";
  } else {
    header += "Method:   One-Time Pad (Vernam cipher)\n"
              "Encrypt:  ciphertext = (message + key) mod 63\n"
              "Alphabet: 0-9 A-Z a-z space (0-9 = 0-9, A-Z = 10-35, "
              "a-z = 36-61, space = 62)\n"
              "Decrypt:  message = (ciphertext - key) mod 63\n";
    keyTitle =
        "KEY (" + lengthText + " characters, spaces are part of the key)\n";
    ciphertextTitle = "CIPHERTEXT (" + lengthText + " characters)\n";
  }

  header += "\nWARNING: This file contains the KEY. Anyone who has the key and "
            "the ciphertext can\nread the message. Keep it secret and never "
            "reuse the key. Without the key the\nciphertext is mathematically "
            "impossible to decrypt, and this program can't\nrecover it "
            "either.\n\n";

  const std::string keyBegin = "-----BEGIN KEY-----\n";
  const std::string keyEnd = "\n-----END KEY-----\n\n";
  const std::string ciphertextBegin = "-----BEGIN CIPHERTEXT-----\n";
  const std::string ciphertextEnd = "\n-----END CIPHERTEXT-----\n";

  const size_t bodyLength = fileMode ? length * 2 : length;
  std::vector<char> output;
  output.reserve(header.size() + keyTitle.size() + keyBegin.size() +
                 keyEnd.size() + ciphertextTitle.size() +
                 ciphertextBegin.size() + ciphertextEnd.size() +
                 bodyLength * 2);

  AppendText(output, header);
  AppendText(output, keyTitle);
  AppendText(output, keyBegin);
  AppendAsTextOrHex(output, key, length, fileMode);
  AppendText(output, keyEnd);
  AppendText(output, ciphertextTitle);
  AppendText(output, ciphertextBegin);
  AppendAsTextOrHex(output, ciphertext, length, fileMode);
  AppendText(output, ciphertextEnd);
  return output;
}

}
