#include "ed25519.h"

#include <stddef.h>
#include <stdint.h>

#include <cctype>
#include <fstream>
#include <iterator>

extern "C" {
#include "../third_party/monocypher/monocypher-ed25519.h"
}

namespace dazzle {
namespace {

int DecodeBase64Char(char character) {
  if (character >= 'A' && character <= 'Z') {
    return character - 'A';
  }
  if (character >= 'a' && character <= 'z') {
    return character - 'a' + 26;
  }
  if (character >= '0' && character <= '9') {
    return character - '0' + 52;
  }
  if (character == '+') {
    return 62;
  }
  if (character == '/') {
    return 63;
  }
  return -1;
}

}  // namespace

bool DecodeBase64(const std::string& text, std::string* bytes) {
  std::string clean;
  clean.reserve(text.size());
  for (char character : text) {
    if (!std::isspace(static_cast<unsigned char>(character))) {
      clean.push_back(character);
    }
  }
  size_t padding = 0;
  while (!clean.empty() && clean.back() == '=') {
    clean.pop_back();
    ++padding;
  }
  if (padding > 2 || clean.size() % 4 == 1) {
    return false;
  }
  if (padding > 0 && (clean.size() + padding) % 4 != 0) {
    return false;
  }

  std::string decoded;
  decoded.reserve(clean.size() / 4 * 3 + 3);
  uint32_t buffer = 0;
  int bits = 0;
  for (char character : clean) {
    const int value = DecodeBase64Char(character);
    if (value < 0) {
      return false;
    }
    buffer = (buffer << 6) | static_cast<uint32_t>(value);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      decoded.push_back(static_cast<char>((buffer >> bits) & 0xFF));
    }
  }
  if (bytes != nullptr) {
    *bytes = decoded;
  }
  return true;
}

std::string EncodeBase64(const std::string& bytes) {
  static const char kAlphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string encoded;
  encoded.reserve((bytes.size() + 2) / 3 * 4);
  size_t index = 0;
  while (index + 3 <= bytes.size()) {
    const uint32_t chunk =
        (static_cast<uint32_t>(static_cast<unsigned char>(bytes[index])) << 16) |
        (static_cast<uint32_t>(static_cast<unsigned char>(bytes[index + 1])) << 8) |
        static_cast<uint32_t>(static_cast<unsigned char>(bytes[index + 2]));
    encoded.push_back(kAlphabet[(chunk >> 18) & 0x3F]);
    encoded.push_back(kAlphabet[(chunk >> 12) & 0x3F]);
    encoded.push_back(kAlphabet[(chunk >> 6) & 0x3F]);
    encoded.push_back(kAlphabet[chunk & 0x3F]);
    index += 3;
  }
  const size_t remaining = bytes.size() - index;
  if (remaining == 1) {
    const uint32_t chunk =
        static_cast<uint32_t>(static_cast<unsigned char>(bytes[index])) << 16;
    encoded.push_back(kAlphabet[(chunk >> 18) & 0x3F]);
    encoded.push_back(kAlphabet[(chunk >> 12) & 0x3F]);
    encoded.append("==");
  } else if (remaining == 2) {
    const uint32_t chunk =
        (static_cast<uint32_t>(static_cast<unsigned char>(bytes[index])) << 16) |
        (static_cast<uint32_t>(static_cast<unsigned char>(bytes[index + 1])) << 8);
    encoded.push_back(kAlphabet[(chunk >> 18) & 0x3F]);
    encoded.push_back(kAlphabet[(chunk >> 12) & 0x3F]);
    encoded.push_back(kAlphabet[(chunk >> 6) & 0x3F]);
    encoded.push_back('=');
  }
  return encoded;
}

bool IsValidPublicKey(const std::string& public_key) {
  std::string key;
  if (!DecodeBase64(public_key, &key) || key.size() != 32) {
    return false;
  }
  bool all_zero = true;
  for (char character : key) {
    if (character != '\0') {
      all_zero = false;
      break;
    }
  }
  if (all_zero) {
    return false;
  }
  // The identity point encodes as 0x01 followed by 31 zero bytes.
  if (static_cast<unsigned char>(key[0]) == 0x01) {
    bool rest_zero = true;
    for (size_t index = 1; index < key.size(); ++index) {
      if (key[index] != '\0') {
        rest_zero = false;
        break;
      }
    }
    if (rest_zero) {
      return false;
    }
  }
  return true;
}

bool VerifySignature(const std::string& public_key, const std::string& signature,
                     const std::string& data) {
  std::string key;
  std::string raw_signature;
  if (!DecodeBase64(public_key, &key) || key.size() != 32 || !IsValidPublicKey(public_key)) {
    return false;
  }
  if (!DecodeBase64(signature, &raw_signature) || raw_signature.size() != 64) {
    return false;
  }
  const int result = crypto_ed25519_check(
      reinterpret_cast<const uint8_t*>(raw_signature.data()),
      reinterpret_cast<const uint8_t*>(key.data()),
      reinterpret_cast<const uint8_t*>(data.data()), data.size());
  return result == 0;
}

bool VerifyFileSignature(const std::string& public_key, const std::string& signature,
                         const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  const std::string data((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
  if (file.bad()) {
    return false;
  }
  return VerifySignature(public_key, signature, data);
}

}  // namespace dazzle
