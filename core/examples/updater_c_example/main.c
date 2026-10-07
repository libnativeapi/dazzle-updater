// Configures the updater through the C ABI and runs one check.
//
// Usage: updater_c_example <release-index-url> [public-key] [current-version]
#include <dazzle_updater.h>
#include <stdio.h>

static void PrintError(const char* action) {
  char* code = dazzle_updater_get_error_code();
  char* message = dazzle_updater_get_error_message();
  fprintf(stderr, "%s failed: %s: %s\n", action, code ? code : "", message ? message : "");
  dazzle_free_c_str(code);
  dazzle_free_c_str(message);
}

int main(int argc, char** argv) {
  if (argc < 2) {
    /* `make examples` runs this program without arguments, which prints the usage and
       succeeds; a real run needs a release index to check. */
    fprintf(stderr, "usage: updater_c_example <release-index-url> [public-key] [current-version]\n");
    return 0;
  }
  dazzle_release_index_source_set_url(argv[1]);

  if (argc > 2) {
    dazzle_updater_set_public_key(argv[2]);
  }
  if (argc > 3) {
    dazzle_updater_set_current_version(argv[3]);
  }

  char* source = dazzle_updater_get_source();
  char* version = dazzle_updater_get_current_version();
  printf("source:          %s\n", source ? source : "");
  printf("current version: %s\n", version ? version : "");
  dazzle_free_c_str(source);
  dazzle_free_c_str(version);

  if (!dazzle_updater_check_for_updates()) {
    PrintError("check");
    return 1;
  }
  if (!dazzle_updater_is_update_available()) {
    printf("up to date\n");
    return 0;
  }

  char* update = dazzle_updater_get_update_display_version();
  printf("update:          %s\n", update ? update : "");
  dazzle_free_c_str(update);
  return 0;
}
