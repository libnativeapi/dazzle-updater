#include "release_index_parse.h"

#include "version.h"

#include "../third_party/nlohmann/json.hpp"

#include <cstdint>
#include <string>

namespace dazzle_updater {
namespace {

constexpr int kSchema = 1;
constexpr const char* kUniversalArch = "universal";

/// Reads a required, non-empty string field.
bool RequiredString(const nlohmann::json& object, const char* key, std::string* value) {
  const auto found = object.find(key);
  if (found == object.end() || !found->is_string()) {
    return false;
  }
  *value = found->get<std::string>();
  return !value->empty();
}

/// Reads an optional string field; a missing field or another type leaves `value` alone.
void OptionalString(const nlohmann::json& object, const char* key, std::string* value) {
  const auto found = object.find(key);
  if (found != object.end() && found->is_string()) {
    *value = found->get<std::string>();
  }
}

/// Reads an optional positive integer field; anything else leaves `value` alone.
void OptionalSize(const nlohmann::json& object, const char* key, int64_t* value) {
  const auto found = object.find(key);
  if (found == object.end()) {
    return;
  }
  if (found->is_number_unsigned()) {
    const uint64_t size = found->get<uint64_t>();
    if (size > 0 && size <= static_cast<uint64_t>(INT64_MAX)) {
      *value = static_cast<int64_t>(size);
    }
  } else if (found->is_number_integer()) {
    const int64_t size = found->get<int64_t>();
    if (size > 0) {
      *value = size;
    }
  }
}

/// Reads an integer field; anything else leaves `value` alone.
void OptionalInteger(const nlohmann::json& object, const char* key, int64_t* value) {
  const auto found = object.find(key);
  if (found == object.end() || !found->is_number_integer()) {
    return;
  }
  *value = found->is_number_unsigned() ? static_cast<int64_t>(found->get<uint64_t>())
                                       : found->get<int64_t>();
}

/// One `assets` entry, as far as dazzle_updater reads it.
struct Asset {
  std::string platform;
  std::string arch;
  std::string minimum_system_version;
  std::string url;
  std::string format;
  std::string signature;
  std::string installer_arguments;
  int64_t size = 0;
};

/// Reads an asset. False when a required field is missing or of the wrong type, in which
/// case the asset is skipped.
bool ParseAsset(const nlohmann::json& object, Asset* asset) {
  if (!object.is_object()) {
    return false;
  }
  if (!RequiredString(object, "platform", &asset->platform) ||
      !RequiredString(object, "url", &asset->url) ||
      !RequiredString(object, "format", &asset->format) ||
      !RequiredString(object, "signature", &asset->signature)) {
    return false;
  }
  asset->arch = kUniversalArch;
  OptionalString(object, "arch", &asset->arch);
  OptionalString(object, "minimumSystemVersion", &asset->minimum_system_version);
  OptionalString(object, "installerArguments", &asset->installer_arguments);
  OptionalSize(object, "size", &asset->size);
  return true;
}

/// Whether an asset runs on this machine. `exact_arch` distinguishes an exact
/// architecture match from a universal build, which loses a tie.
bool AssetApplies(const Asset& asset, const UpdateQuery& query, bool* exact_arch) {
  if (asset.platform != query.platform) {
    return false;
  }
  if (asset.arch == query.arch) {
    *exact_arch = true;
  } else if (asset.arch == kUniversalArch) {
    *exact_arch = false;
  } else {
    return false;
  }
  // A machine whose system version is unknown cannot be held back by a minimum.
  if (!asset.minimum_system_version.empty() && !query.system_version.empty()) {
    Version minimum;
    Version system;
    if (Version::Parse(asset.minimum_system_version, &minimum) &&
        Version::Parse(query.system_version, &system) && system.CompareTo(minimum) < 0) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool ParseReleaseIndex(const std::string& document, const UpdateQuery& query, Release* release,
                       bool* found, std::string* error, std::string* error_code) {
  *found = false;
  const nlohmann::json root = nlohmann::json::parse(document, nullptr, false);
  if (root.is_discarded() || !root.is_object()) {
    *error = "the release index is not a JSON object";
    *error_code = "invalid-index";
    return false;
  }
  int64_t schema = -1;
  OptionalInteger(root, "schema", &schema);
  if (schema != kSchema) {
    *error = "unsupported release index schema";
    *error_code = "invalid-index";
    return false;
  }

  Version current;
  if (!Version::Parse(query.current_version, &current)) {
    *error = "the current version \"" + query.current_version + "\" is not a valid version";
    *error_code = "invalid-version";
    return false;
  }
  const auto releases = root.find("releases");
  if (releases == root.end() || !releases->is_array()) {
    *error = "the release index has no releases array";
    *error_code = "invalid-index";
    return false;
  }

  bool have_best = false;
  Version best_version;
  Release best;
  for (const nlohmann::json& entry : *releases) {
    if (!entry.is_object()) {
      continue;
    }
    Release candidate;
    if (!RequiredString(entry, "version", &candidate.version)) {
      continue;
    }
    Version candidate_version;
    if (!Version::Parse(candidate.version, &candidate_version)) {
      continue;  // An invalid version cannot be compared, so the release is skipped.
    }
    if (candidate_version.CompareTo(current) <= 0) {
      continue;
    }
    const auto assets = entry.find("assets");
    if (assets == entry.end() || !assets->is_array()) {
      continue;
    }

    bool have_asset = false;
    bool asset_exact = false;
    Asset best_asset;
    for (const nlohmann::json& asset_json : *assets) {
      Asset asset;
      if (!ParseAsset(asset_json, &asset)) {
        continue;
      }
      bool exact = false;
      if (!AssetApplies(asset, query, &exact)) {
        continue;
      }
      if (!have_asset || (exact && !asset_exact)) {
        have_asset = true;
        asset_exact = exact;
        best_asset = asset;
      }
    }
    if (!have_asset) {
      continue;
    }
    if (have_best && candidate_version.CompareTo(best_version) <= 0) {
      continue;
    }

    candidate.display_version = candidate.version;
    OptionalString(entry, "displayVersion", &candidate.display_version);
    OptionalString(entry, "releaseNotes", &candidate.release_notes);
    OptionalString(entry, "releaseNotesUrl", &candidate.release_notes_url);
    candidate.download_url = best_asset.url;
    candidate.size = best_asset.size;
    candidate.signature = best_asset.signature;
    candidate.format = best_asset.format;
    candidate.installer_arguments = best_asset.installer_arguments;

    best = candidate;
    best_version = candidate_version;
    have_best = true;
  }

  if (!have_best) {
    return true;  // Up to date.
  }
  *release = best;
  *found = true;
  return true;
}

}  // namespace dazzle_updater
