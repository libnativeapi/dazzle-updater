#pragma once

#include "common_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Pass NULL or "" to go back to the URL the app's configuration provides.
DAZZLE_EXPORT
void dazzle_updater_set_feed_url(const char* feed_url);

/// Returns NULL when no feed URL is configured.
/// Caller owns the returned string; free it with dazzle_free_c_str().
DAZZLE_EXPORT
char* dazzle_updater_get_feed_url(void);

#ifdef __cplusplus
}
#endif
