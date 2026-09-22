#include "updater_c.h"

#include <cstdio>
#include <string>

#include "../updater.h"
#include "string_utils_c.h"

void dazzle_updater_set_feed_url(const char* feed_url) {
  try {
    dazzle::Updater::GetInstance().SetFeedUrl(feed_url ? feed_url : "");
  } catch (...) {
    fprintf(stderr, "[dazzle] %s: unexpected exception\n", "dazzle_updater_set_feed_url");
  }
}

char* dazzle_updater_get_feed_url(void) {
  try {
    return dazzle::to_c_str(dazzle::Updater::GetInstance().GetFeedUrl());
  } catch (...) {
    fprintf(stderr, "[dazzle] %s: unexpected exception\n", "dazzle_updater_get_feed_url");
    return nullptr;
  }
}
