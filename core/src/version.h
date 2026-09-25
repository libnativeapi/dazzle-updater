#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Internal version comparison, used by ReleaseIndexSource to pick the newest applicable
// release and to compare a machine's system version against a minimum. The syntax and
// the comparison rules are in ../specs/release-index.md ("版本号"). It is not listed in
// API_HEADERS, so codegen ignores it and it reaches no binding.

namespace dazzle {

/**
 * @brief A version parsed into the parts that take part in comparison.
 *
 * Dot-separated numbers, an optional SemVer-style pre-release suffix, and optional build
 * metadata that is dropped:
 *
 *     1.4   1.4.0   1.4.0.2   1.5.0-beta.1   1.5.0+20260920
 */
class Version {
 public:
  Version() = default;

  /**
   * @brief Parses `text`.
   *
   * @param text The version, e.g. "1.5.0-beta.1". Surrounding whitespace is ignored and
   *   everything from '+' on is dropped.
   * @param version Filled in when the text is valid.
   * @return true when `text` follows the syntax; false leaves `version` untouched.
   */
  static bool Parse(const std::string& text, Version* version);

  /**
   * @brief Compares this version with another.
   *
   * Numbers are compared segment by segment, padding the shorter side with zeros, so
   * "1.4" equals "1.4.0". A version without a pre-release suffix outranks one with a
   * suffix, and suffixes compare as SemVer 2.0 clause 11 says: numeric identifiers
   * numerically, the rest by ASCII, and a numeric identifier below an alphanumeric one.
   *
   * @param other The version to compare against.
   * @return -1 when this version precedes `other`, 0 when they are equal, 1 when it
   *   follows it.
   */
  int CompareTo(const Version& other) const;

 private:
  std::vector<uint64_t> numbers_;
  std::vector<std::string> prerelease_;  // Empty for a release version.
};

}  // namespace dazzle
