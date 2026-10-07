#include "test_support.h"

#include "../src/release_index_source.h"
#include "../src/updater.h"

#include <cstdint>
#include <string>

using dazzle_updater::ReleaseIndexSource;
using dazzle_updater::Updater;

namespace {

// RFC 8032, section 7.1, test 1: a real key, so the configuration checks pass.
const char* const kPublicKey = "11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo=";

/// The state these tests expect the singleton to start each test in.
void ResetUpdater() {
  Updater& updater = Updater::GetInstance();
  updater.SetSource("");
  updater.SetCurrentVersion("1.0.0");
  updater.SetPublicKey(kPublicKey);
  ReleaseIndexSource::GetInstance().SetUrl("");
}

}  // namespace

DAZZLE_TEST(updater_defaults_the_source_to_the_release_index) {
  Updater& updater = Updater::GetInstance();
  updater.SetSource("");
  EXPECT_EQ(updater.GetSource(), std::string("release-index"));
  updater.SetSource("appcast");
  EXPECT_EQ(updater.GetSource(), std::string("appcast"));
  updater.SetSource("");
}

DAZZLE_TEST(updater_round_trips_the_public_key) {
  Updater& updater = Updater::GetInstance();
  updater.SetPublicKey("abc");
  EXPECT_EQ(updater.GetPublicKey(), std::string("abc"));
  updater.SetPublicKey("");
  EXPECT_EQ(updater.GetPublicKey(), std::string(""));
  ResetUpdater();
}

DAZZLE_TEST(updater_fails_on_a_source_that_is_not_registered) {
  ResetUpdater();
  Updater& updater = Updater::GetInstance();
  updater.SetSource("nope");
  EXPECT_FALSE(updater.CheckForUpdates());
  EXPECT_EQ(updater.GetErrorCode(), std::string("unknown-source"));
  EXPECT_FALSE(updater.GetErrorMessage().empty());
  updater.SetSource("");
}

DAZZLE_TEST(updater_fails_when_the_release_index_url_is_empty) {
  ResetUpdater();
  EXPECT_FALSE(Updater::GetInstance().CheckForUpdates());
  EXPECT_EQ(Updater::GetInstance().GetErrorCode(), std::string("invalid-index"));
  EXPECT_TRUE(Updater::GetInstance().GetErrorMessage().size() > 0);
}

DAZZLE_TEST(updater_starts_out_with_nothing_found) {
  ResetUpdater();
  Updater& updater = Updater::GetInstance();
  EXPECT_FALSE(updater.IsUpdateAvailable());
  EXPECT_FALSE(updater.IsUpdateDownloaded());
  EXPECT_EQ(updater.GetUpdateVersion(), std::string(""));
  EXPECT_EQ(updater.GetUpdateDisplayVersion(), std::string(""));
  EXPECT_EQ(updater.GetUpdateReleaseNotes(), std::string(""));
  EXPECT_EQ(updater.GetUpdateSize(), static_cast<int64_t>(0));
}

DAZZLE_TEST(updater_refuses_actions_in_the_wrong_order) {
  ResetUpdater();
  Updater& updater = Updater::GetInstance();
  EXPECT_FALSE(updater.DownloadUpdate());
  EXPECT_EQ(updater.GetErrorCode(), std::string("no-update"));
  EXPECT_FALSE(updater.InstallUpdate());
  EXPECT_EQ(updater.GetErrorCode(), std::string("no-update"));
}

DAZZLE_TEST(updater_cancel_does_nothing_without_a_download) {
  ResetUpdater();
  Updater& updater = Updater::GetInstance();
  updater.CancelDownload();
  EXPECT_EQ(updater.GetDownloadProgress(), 0.0);
  // The next download resets the flag, so a check still behaves normally afterwards.
  EXPECT_FALSE(updater.CheckForUpdates());
  EXPECT_EQ(updater.GetErrorCode(), std::string("invalid-index"));
}
