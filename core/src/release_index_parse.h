#pragma once

#include "update_source.h"

#include <string>

// Internal parsing and selection for dazzle's own release index format. It is split from
// release_index_source.cpp so the rules can be unit tested with JSON text, without a
// network. It is not listed in API_HEADERS, so codegen ignores it and it reaches no
// binding.

namespace dazzle {

/**
 * @brief Reads a release index and picks the release to offer this machine.
 *
 * Implements ../specs/release-index.md: the highest release whose version is valid and
 * strictly newer than `query.current_version`, and which has an asset for this machine's
 * platform and architecture whose `minimumSystemVersion` is not above the machine's
 * system version. A release or asset that is malformed or misses a required field is
 * skipped; a document that cannot be read at all fails.
 *
 * @param document The `releases.json` text.
 * @param query The machine and the version to compare against.
 * @param release Filled in when an update was found.
 * @param found Set to true when an update was found, false when the application is up to
 *   date. Both are a successful read.
 * @param error Filled in with the reason when false is returned.
 * @param error_code Filled in with "invalid-index" or "invalid-version" when false is
 *   returned.
 * @return true when the document was read; false when it is unusable.
 */
bool ParseReleaseIndex(const std::string& document, const UpdateQuery& query, Release* release,
                       bool* found, std::string* error, std::string* error_code);

}  // namespace dazzle
