#include "test_support.h"

#include "../src/release_index_parse.h"

#include <string>

using dazzle::ParseReleaseIndex;
using dazzle::Release;
using dazzle::UpdateQuery;

namespace {

struct Outcome {
  bool ok = false;
  bool found = false;
  std::string error;
  std::string error_code;
  Release release;
};

Outcome Run(const std::string& document, const std::string& current_version = "1.0.0",
            const std::string& platform = "macos", const std::string& arch = "arm64",
            const std::string& system_version = "14.5") {
  UpdateQuery query;
  query.current_version = current_version;
  query.platform = platform;
  query.arch = arch;
  query.system_version = system_version;
  Outcome outcome;
  outcome.ok = ParseReleaseIndex(document, query, &outcome.release, &outcome.found, &outcome.error,
                                 &outcome.error_code);
  return outcome;
}

/// Two releases: 1.4.0 has a Windows, a universal and an arm64 mac asset; 1.5.0 needs
/// macOS 15 and is there to be filtered out by a lower system version.
const char* const kTwoReleases = R"json({
  "schema": 1,
  "releases": [
    {
      "version": "1.4.0",
      "displayVersion": "1.4",
      "releaseNotes": "## New",
      "releaseNotesUrl": "https://example.com/changelog#1.4.0",
      "unknownField": {"ignored": true},
      "assets": [
        {"platform": "windows", "arch": "x64", "url": "https://example.com/a.exe",
         "format": "exe", "signature": "c2ln"},
        {"platform": "macos", "arch": "universal", "url": "https://example.com/a-universal.zip",
         "format": "zip", "signature": "dW5p", "size": 10},
        {"platform": "macos", "arch": "arm64", "url": "https://example.com/a-arm64.zip",
         "format": "zip", "signature": "YXJt", "size": 20}
      ]
    },
    {
      "version": "1.5.0",
      "assets": [
        {"platform": "macos", "arch": "arm64", "url": "https://example.com/b-arm64.zip",
         "format": "zip", "signature": "Yi1hcm0=", "size": 30, "minimumSystemVersion": "15.0"}
      ]
    }
  ]
})json";

}  // namespace

DAZZLE_TEST(release_index_picks_the_newest_applicable_release) {
  const Outcome outcome = Run(kTwoReleases, "1.0.0", "macos", "arm64", "14.5");
  EXPECT_TRUE(outcome.ok);
  EXPECT_TRUE(outcome.found);
  EXPECT_EQ(outcome.release.version, std::string("1.4.0"));
  EXPECT_EQ(outcome.release.display_version, std::string("1.4"));
  EXPECT_EQ(outcome.release.release_notes, std::string("## New"));
  EXPECT_EQ(outcome.release.release_notes_url, std::string("https://example.com/changelog#1.4.0"));
  // The exact architecture wins over the universal build of the same release.
  EXPECT_EQ(outcome.release.download_url, std::string("https://example.com/a-arm64.zip"));
  EXPECT_EQ(outcome.release.size, static_cast<int64_t>(20));
  EXPECT_EQ(outcome.release.signature, std::string("YXJt"));
  EXPECT_EQ(outcome.release.format, std::string("zip"));
}

DAZZLE_TEST(release_index_falls_back_to_a_universal_asset) {
  const Outcome outcome = Run(kTwoReleases, "1.0.0", "macos", "x64", "14.5");
  EXPECT_TRUE(outcome.ok);
  EXPECT_TRUE(outcome.found);
  EXPECT_EQ(outcome.release.version, std::string("1.4.0"));
  EXPECT_EQ(outcome.release.download_url, std::string("https://example.com/a-universal.zip"));
}

DAZZLE_TEST(release_index_honours_minimum_system_version) {
  // With macOS 15 the newer release applies, and it is picked.
  const Outcome newer = Run(kTwoReleases, "1.0.0", "macos", "arm64", "15.0");
  EXPECT_TRUE(newer.ok);
  EXPECT_TRUE(newer.found);
  EXPECT_EQ(newer.release.version, std::string("1.5.0"));

  // An unknown system version cannot be held back by a minimum.
  const Outcome unknown = Run(kTwoReleases, "1.0.0", "macos", "arm64", "");
  EXPECT_TRUE(unknown.ok);
  EXPECT_TRUE(unknown.found);
  EXPECT_EQ(unknown.release.version, std::string("1.5.0"));
}

DAZZLE_TEST(release_index_reports_being_up_to_date) {
  const Outcome outcome = Run(kTwoReleases, "1.6.0");
  EXPECT_TRUE(outcome.ok);
  EXPECT_FALSE(outcome.found);
  EXPECT_EQ(outcome.release.version, std::string(""));
}

DAZZLE_TEST(release_index_ignores_other_platforms) {
  const Outcome outcome = Run(kTwoReleases, "1.0.0", "linux", "x64", "14.5");
  EXPECT_TRUE(outcome.ok);
  EXPECT_FALSE(outcome.found);
}

DAZZLE_TEST(release_index_skips_broken_releases_and_assets) {
  const char* const document = R"json({
    "schema": 1,
    "releases": [
      {"version": "1.x", "assets": [
        {"platform": "macos", "url": "https://example.com/bad.zip", "format": "zip", "signature": "x"}]},
      {"version": "1.1.0", "assets": [
        {"platform": "macos", "url": "https://example.com/no-signature.zip", "format": "zip"},
        {"platform": "macos", "arch": "arm64", "url": "https://example.com/ok.zip", "format": "zip", "signature": "b2s="}]},
      {"version": "1.2.0", "assets": "not an array"},
      {"displayVersion": "1.3", "assets": []}
    ]
  })json";
  const Outcome outcome = Run(document, "1.0.0");
  EXPECT_TRUE(outcome.ok);
  EXPECT_TRUE(outcome.found);
  EXPECT_EQ(outcome.release.version, std::string("1.1.0"));
  // displayVersion defaults to version when the release does not set one.
  EXPECT_EQ(outcome.release.display_version, std::string("1.1.0"));
  EXPECT_EQ(outcome.release.download_url, std::string("https://example.com/ok.zip"));
}

DAZZLE_TEST(release_index_reports_an_unusable_document) {
  const Outcome not_json = Run("this is not json");
  EXPECT_FALSE(not_json.ok);
  EXPECT_EQ(not_json.error_code, std::string("invalid-index"));

  const Outcome wrong_schema = Run(R"json({"schema": 2, "releases": []})json");
  EXPECT_FALSE(wrong_schema.ok);
  EXPECT_EQ(wrong_schema.error_code, std::string("invalid-index"));

  const Outcome no_schema = Run(R"json({"releases": []})json");
  EXPECT_FALSE(no_schema.ok);
  EXPECT_EQ(no_schema.error_code, std::string("invalid-index"));

  const Outcome no_releases = Run(R"json({"schema": 1})json");
  EXPECT_FALSE(no_releases.ok);
  EXPECT_EQ(no_releases.error_code, std::string("invalid-index"));

  const Outcome empty_document = Run("");
  EXPECT_FALSE(empty_document.ok);
  EXPECT_EQ(empty_document.error_code, std::string("invalid-index"));
}

DAZZLE_TEST(release_index_rejects_an_invalid_current_version) {
  const Outcome outcome = Run(kTwoReleases, "not a version");
  EXPECT_FALSE(outcome.ok);
  EXPECT_EQ(outcome.error_code, std::string("invalid-version"));
}
