// The Linux host application for the built-in update windows: a small GTK 4 window of its
// own, a "Check for Updates…" button, and UpdaterUI's windows on top of it. It is the
// example for ../../../specs/updater-ui.md on Linux — the whole update flow in one line
// per entry point, with no UI of its own to write.
//
// Usage:
//   updater_ui_example [<release-index-url> [<public-key> [<current-version>]]]
//
// Everything it is not told on the command line comes from the application directory it
// runs from: `dazzle-demo.conf` carries the feed and the public key, and `version` the
// version it reports. demo_linux.sh writes both, which is how the copy the installer
// restarts still knows where its feed is — a Linux application has no bundle to carry
// that, so it carries a file beside its binary.
//
// The application is deliberately ordinary: it initializes GTK, runs its own main loop,
// and hands nothing to dazzle but the feed, the key and its version. UpdaterUI draws on
// this same loop (a GTK application's main thread), which is what the Linux backend is
// written for.

#include <gtk/gtk.h>

#include <unistd.h>

#include <dazzle.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr const char* kApplicationId = "com.example.dazzle-demo";

std::string ToUtf8(const char* value) {
  return value != nullptr ? std::string(value) : std::string();
}

std::string Trim(std::string value) {
  while (!value.empty() && (value.back() == '\n' || value.back() == '\r' || value.back() == ' ' ||
                            value.back() == '\t')) {
    value.pop_back();
  }
  return value;
}

/// The directory this binary runs from: a Linux application is a directory, and the
/// update this demo installs replaces exactly this one.
std::string ApplicationDirectory() {
  char executable[4096] = {0};
  const ssize_t length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
  if (length <= 0) {
    return "";
  }
  const std::string path(executable);
  const size_t separator = path.find_last_of('/');
  return separator == std::string::npos ? std::string() : path.substr(0, separator);
}

std::string ReadFile(const std::string& path) {
  FILE* file = std::fopen(path.c_str(), "r");
  if (file == nullptr) {
    return "";
  }
  std::string contents;
  char buffer[256];
  while (std::fgets(buffer, sizeof(buffer), file) != nullptr) {
    contents += buffer;
  }
  std::fclose(file);
  return contents;
}

/// A `key=value` line of the application's own configuration file; "" when it is not
/// there. This is the stand-in for the Info.plist keys the macOS example reads.
std::string ConfigValue(const std::string& config, const std::string& key) {
  const std::string prefix = key + "=";
  size_t at = 0;
  while (at < config.size()) {
    const size_t end = config.find('\n', at);
    const std::string line = config.substr(at, end == std::string::npos ? std::string::npos
                                                                       : end - at);
    if (line.compare(0, prefix.size(), prefix) == 0) {
      return Trim(line.substr(prefix.size()));
    }
    if (end == std::string::npos) {
      break;
    }
    at = end + 1;
  }
  return "";
}

void PrintConfiguration(const std::string& feed, const std::string& key,
                        const std::string& version) {
  std::printf("feed:            %s\n", feed.empty() ? "(none)" : feed.c_str());
  std::printf("public key:      %s\n", key.empty() ? "(none)" : key.c_str());
  std::printf("current version: %s\n", version.empty() ? "(none)" : version.c_str());
  std::fflush(stdout);
}

void OnCheckForUpdates(GtkButton* button, gpointer user_data) {
  (void)button;
  (void)user_data;
  std::printf("check for updates\n");
  std::fflush(stdout);
  dazzle::UpdaterUI::GetInstance().CheckForUpdates();
}

void OnCheckInBackground(GtkButton* button, gpointer user_data) {
  (void)button;
  (void)user_data;
  std::printf("check in background\n");
  std::fflush(stdout);
  // The silent entry point: nothing is shown unless there is something to tell the user
  // about, and the question about automatic checks is asked the first time.
  dazzle::UpdaterUI::GetInstance().CheckForUpdatesInBackground();
}

void BuildHostWindow(GtkApplication* application) {
  // Linux has no application metadata for UpdaterUI to read a name from — it would use
  // the executable's own name — so this application names itself, the way any desktop
  // application does.
  const std::string name = "Dazzle Demo";
  const std::string version = dazzle::Updater::GetInstance().GetCurrentVersion();

  GtkWidget* window = gtk_application_window_new(application);
  gtk_window_set_title(GTK_WINDOW(window), (name + " " + version).c_str());
  gtk_window_set_default_size(GTK_WINDOW(window), 420, 150);

  GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_margin_start(box, 20);
  gtk_widget_set_margin_end(box, 20);
  gtk_widget_set_margin_top(box, 20);
  gtk_widget_set_margin_bottom(box, 20);

  GtkWidget* heading = gtk_label_new((name + " " + version).c_str());
  gtk_label_set_xalign(GTK_LABEL(heading), 0.0f);
  gtk_widget_add_css_class(heading, "title-3");
  gtk_box_append(GTK_BOX(box), heading);

  GtkWidget* detail =
      gtk_label_new("A host application: the update windows on top are UpdaterUI's.");
  gtk_label_set_xalign(GTK_LABEL(detail), 0.0f);
  gtk_label_set_wrap(GTK_LABEL(detail), TRUE);
  gtk_widget_add_css_class(detail, "dim-label");
  gtk_box_append(GTK_BOX(box), detail);

  GtkWidget* buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_halign(buttons, GTK_ALIGN_END);
  GtkWidget* background = gtk_button_new_with_label("Check in Background");
  g_signal_connect(background, "clicked", G_CALLBACK(OnCheckInBackground), nullptr);
  gtk_box_append(GTK_BOX(buttons), background);
  GtkWidget* check = gtk_button_new_with_label("Check for Updates…");
  gtk_widget_add_css_class(check, "suggested-action");
  g_signal_connect(check, "clicked", G_CALLBACK(OnCheckForUpdates), nullptr);
  gtk_box_append(GTK_BOX(buttons), check);
  gtk_box_append(GTK_BOX(box), buttons);

  gtk_window_set_child(GTK_WINDOW(window), box);
  gtk_window_present(GTK_WINDOW(window));
}

void OnActivate(GtkApplication* application, gpointer user_data) {
  (void)user_data;
  BuildHostWindow(application);

  // The user started the application, so this is the check they asked for: the window
  // opens at once and walks them through whatever the feed has.
  dazzle::UpdaterUI::GetInstance().CheckForUpdates();
}

}  // namespace

int main(int argc, char** argv) {
  dazzle::Updater& updater = dazzle::Updater::GetInstance();
  dazzle::UpdaterUI& ui = dazzle::UpdaterUI::GetInstance();

  const std::string directory = ApplicationDirectory();
  const std::string config =
      directory.empty() ? std::string()
                        : ReadFile(directory + "/dazzle-demo.conf");
  const std::vector<std::string> arguments(argv + 1, argv + argc);
  const std::string feed = arguments.size() > 0 ? arguments[0] : ConfigValue(config, "feed");
  const std::string key =
      arguments.size() > 1 ? arguments[1] : ConfigValue(config, "publicKey");
  const std::string version =
      arguments.size() > 2 ? arguments[2]
                           : Trim(directory.empty() ? std::string()
                                                    : ReadFile(directory + "/version"));
  PrintConfiguration(feed, key, version);

  if (feed.empty() || key.empty()) {
    std::fprintf(stderr,
                 "no release index or public key: pass them as arguments, or run "
                 "demo_linux.sh, which starts a local feed and runs this application\n");
  } else {
    dazzle::ReleaseIndexSource::GetInstance().SetUrl(feed);
    updater.SetPublicKey(key);
    // Linux has no metadata every application carries, so the version is the
    // application's own to declare.
    if (!version.empty()) {
      updater.SetCurrentVersion(version);
    }
    // Automatic downloads off, so the demo shows the Available window before anything is
    // fetched; the check box in the window can turn them on.
    ui.SetAutomaticDownloadsEnabled(false);
  }
  ui.SetAppName("Dazzle Demo");

  g_set_application_name("Dazzle Demo");
  GtkApplication* application = gtk_application_new(kApplicationId, G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(application, "activate", G_CALLBACK(OnActivate), nullptr);
  const int status = g_application_run(G_APPLICATION(application), argc, argv);
  g_object_unref(application);
  return status;
}
