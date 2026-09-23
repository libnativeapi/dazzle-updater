#include <dazzle.h>
#include <stdio.h>

int main(void) {
  char* initial = dazzle_updater_get_feed_url();
  printf("Initial feed URL: %s\n", initial ? initial : "(none)");
  dazzle_free_c_str(initial);

  dazzle_updater_set_feed_url("https://example.com/appcast.xml");

  char* feed_url = dazzle_updater_get_feed_url();
  int status = feed_url ? 0 : 1;
  printf("Feed URL:         %s\n", feed_url ? feed_url : "(none)");
  dazzle_free_c_str(feed_url);

  return status;
}
