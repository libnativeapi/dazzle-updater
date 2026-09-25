#include "test_support.h"

#include "../src/version.h"

#include <string>

using dazzle::Version;

namespace {

bool Valid(const std::string& text) {
  Version version;
  return Version::Parse(text, &version);
}

/// 99 marks input the tests expect to be valid; a comparison never returns it.
int Compare(const std::string& left, const std::string& right) {
  Version a;
  Version b;
  if (!Version::Parse(left, &a) || !Version::Parse(right, &b)) {
    return 99;
  }
  return a.CompareTo(b);
}

}  // namespace

DAZZLE_TEST(version_accepts_the_documented_syntax) {
  EXPECT_TRUE(Valid("1"));
  EXPECT_TRUE(Valid("1.4"));
  EXPECT_TRUE(Valid("1.4.0"));
  EXPECT_TRUE(Valid("1.4.0.2"));
  EXPECT_TRUE(Valid("1.5.0-beta.1"));
  EXPECT_TRUE(Valid("1.5.0+20260920"));
  EXPECT_TRUE(Valid("1.5.0-beta.1+20260920"));
  EXPECT_TRUE(Valid(" 1.4.0 "));  // Surrounding whitespace is ignored.
}

DAZZLE_TEST(version_rejects_invalid_text) {
  EXPECT_FALSE(Valid(""));
  EXPECT_FALSE(Valid("   "));
  EXPECT_FALSE(Valid("v1.4"));
  EXPECT_FALSE(Valid("1..4"));
  EXPECT_FALSE(Valid("1."));
  EXPECT_FALSE(Valid(".1"));
  EXPECT_FALSE(Valid("1,4"));
  EXPECT_FALSE(Valid("1.4.0-"));
  EXPECT_FALSE(Valid("1.4.0+"));
  EXPECT_FALSE(Valid("1.4.0-beta..1"));
  EXPECT_FALSE(Valid("1.4.0-beta 1"));
  EXPECT_FALSE(Valid("1.4.0 beta"));
  EXPECT_FALSE(Valid("18446744073709551616"));  // The segment does not fit in 64 bits.
}

DAZZLE_TEST(version_compares_numbers_segment_by_segment) {
  EXPECT_EQ(Compare("1.4", "1.4.0"), 0);
  EXPECT_EQ(Compare("1.4.0", "1.4.0"), 0);
  EXPECT_EQ(Compare("1.4.0.2", "1.4.0"), 1);
  EXPECT_EQ(Compare("1.4.1", "1.4.0"), 1);
  EXPECT_EQ(Compare("1.10", "1.9"), 1);
  EXPECT_EQ(Compare("2", "1.9.9"), 1);
  EXPECT_EQ(Compare("1.4.0", "1.4.1"), -1);
}

DAZZLE_TEST(version_ranks_a_release_above_its_prereleases) {
  EXPECT_EQ(Compare("1.5.0", "1.5.0-beta.1"), 1);
  EXPECT_EQ(Compare("1.5.0-beta.1", "1.5.0"), -1);
  EXPECT_EQ(Compare("1.5.0-beta.2", "1.5.0-beta.10"), -1);
  EXPECT_EQ(Compare("1.5.0-beta.10", "1.5.0-beta.2"), 1);
  EXPECT_EQ(Compare("1.5.0-alpha", "1.5.0-beta"), -1);
  EXPECT_EQ(Compare("1.5.0-1", "1.5.0-alpha"), -1);  // A number ranks below a word.
  EXPECT_EQ(Compare("1.5.0-alpha", "1.5.0-alpha.1"), -1);
  EXPECT_EQ(Compare("1.5.0-beta.1", "1.5.0-beta.1"), 0);
}

DAZZLE_TEST(version_ignores_build_metadata) {
  EXPECT_EQ(Compare("1.5.0+20260920", "1.5.0"), 0);
  EXPECT_EQ(Compare("1.5.0+a", "1.5.0+b"), 0);
  EXPECT_EQ(Compare("1.5.0-beta.1+a", "1.5.0-beta.1+b"), 0);
}
