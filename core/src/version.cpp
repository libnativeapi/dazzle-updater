#include "version.h"

#include <algorithm>
#include <cctype>

namespace dazzle_updater {
namespace {

std::string Trim(const std::string& text) {
  size_t begin = 0;
  size_t end = text.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(begin, end - begin);
}

bool IsDigits(const std::string& text) {
  if (text.empty()) {
    return false;
  }
  for (char character : text) {
    if (character < '0' || character > '9') {
      return false;
    }
  }
  return true;
}

bool IsPrereleaseIdentifier(const std::string& text) {
  if (text.empty()) {
    return false;
  }
  for (char character : text) {
    const bool alphanumeric = (character >= '0' && character <= '9') ||
                              (character >= 'a' && character <= 'z') ||
                              (character >= 'A' && character <= 'Z');
    if (!alphanumeric && character != '-') {
      return false;
    }
  }
  return true;
}

/// Splits on '.', rejecting empty parts ("1..4", "1." and "").
bool SplitOnDots(const std::string& text, std::vector<std::string>* parts) {
  parts->clear();
  size_t begin = 0;
  while (true) {
    const size_t dot = text.find('.', begin);
    const std::string part =
        dot == std::string::npos ? text.substr(begin) : text.substr(begin, dot - begin);
    if (part.empty()) {
      return false;
    }
    parts->push_back(part);
    if (dot == std::string::npos) {
      return true;
    }
    begin = dot + 1;
  }
}

std::string WithoutLeadingZeros(const std::string& digits) {
  const size_t first = digits.find_first_not_of('0');
  return first == std::string::npos ? std::string("0") : digits.substr(first);
}

/// Compares two numeric pre-release identifiers of unbounded length.
int CompareNumericIdentifiers(const std::string& a, const std::string& b) {
  const std::string left = WithoutLeadingZeros(a);
  const std::string right = WithoutLeadingZeros(b);
  if (left.size() != right.size()) {
    return left.size() < right.size() ? -1 : 1;
  }
  if (left == right) {
    return 0;
  }
  return left < right ? -1 : 1;
}

}  // namespace

bool Version::Parse(const std::string& text, Version* version) {
  if (version == nullptr) {
    return false;
  }
  std::string core = Trim(text);
  if (core.empty()) {
    return false;
  }

  // Build metadata takes no part in comparison.
  const size_t plus = core.find('+');
  if (plus != std::string::npos) {
    if (plus + 1 == core.size()) {
      return false;
    }
    core = core.substr(0, plus);
  }

  std::vector<std::string> prerelease;
  const size_t dash = core.find('-');
  if (dash != std::string::npos) {
    if (!SplitOnDots(core.substr(dash + 1), &prerelease)) {
      return false;
    }
    for (const std::string& identifier : prerelease) {
      if (!IsPrereleaseIdentifier(identifier)) {
        return false;
      }
    }
    core = core.substr(0, dash);
  }

  std::vector<std::string> parts;
  if (!SplitOnDots(core, &parts)) {
    return false;
  }
  std::vector<uint64_t> numbers;
  numbers.reserve(parts.size());
  for (const std::string& part : parts) {
    if (!IsDigits(part)) {
      return false;
    }
    uint64_t value = 0;
    for (char character : part) {
      const uint64_t digit = static_cast<uint64_t>(character - '0');
      if (value > (UINT64_MAX - digit) / 10) {
        return false;  // A segment no machine can compare meaningfully.
      }
      value = value * 10 + digit;
    }
    numbers.push_back(value);
  }

  version->numbers_ = numbers;
  version->prerelease_ = prerelease;
  return true;
}

int Version::CompareTo(const Version& other) const {
  const size_t count = std::max(numbers_.size(), other.numbers_.size());
  for (size_t index = 0; index < count; ++index) {
    const uint64_t left = index < numbers_.size() ? numbers_[index] : 0;
    const uint64_t right = index < other.numbers_.size() ? other.numbers_[index] : 0;
    if (left != right) {
      return left < right ? -1 : 1;
    }
  }
  if (prerelease_.empty() && other.prerelease_.empty()) {
    return 0;
  }
  if (prerelease_.empty()) {
    return 1;
  }
  if (other.prerelease_.empty()) {
    return -1;
  }
  const size_t shared = std::min(prerelease_.size(), other.prerelease_.size());
  for (size_t index = 0; index < shared; ++index) {
    const std::string& left = prerelease_[index];
    const std::string& right = other.prerelease_[index];
    const bool left_numeric = IsDigits(left);
    const bool right_numeric = IsDigits(right);
    if (left_numeric && right_numeric) {
      const int result = CompareNumericIdentifiers(left, right);
      if (result != 0) {
        return result;
      }
      continue;
    }
    if (left_numeric != right_numeric) {
      return left_numeric ? -1 : 1;  // A numeric identifier is below an alphanumeric one.
    }
    if (left != right) {
      return left < right ? -1 : 1;
    }
  }
  if (prerelease_.size() != other.prerelease_.size()) {
    return prerelease_.size() < other.prerelease_.size() ? -1 : 1;
  }
  return 0;
}

}  // namespace dazzle_updater
