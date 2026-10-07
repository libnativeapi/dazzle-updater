// A development tool: drives the real UpdaterUI state machine with a scripted Updater
// port and captures each macOS window to a PNG, so the native windows can be compared
// with prototype/ state by state.
//
// It is not built by CMake and not part of the library or the tests; see README.md next
// to this file for the one-line build and how to read the pictures.
//
// The port is the tests' idea: nothing here touches the network, a package or an
// installer. Where the tests assert, this one only draws, so the windows can be looked at.

#import <AppKit/AppKit.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "../../src/foundation/updater_ui_backend.h"
#include "../../src/foundation/updater_ui_preferences.h"
#include "../../src/updater_ui_runner.h"
#include "../../src/updater_ui_state.h"
#include "../../src/updater_ui_strings.h"

using dazzle_updater::UiPreferences;
using dazzle_updater::UiPreferencesStore;
using dazzle_updater::internal::UiRunner;
using dazzle_updater::internal::UpdaterPort;


namespace {

/// DAZZLE_PREVIEW_OUT names the directory the pictures land in; the default is
/// build/preview under the current directory (the README runs the tool from core/).
NSString* OutputDirectory() {
  const char* directory = getenv("DAZZLE_PREVIEW_OUT");
  return directory != nullptr ? [NSString stringWithUTF8String:directory] : @"build/preview";
}

/// DAZZLE_PREVIEW_DARK=1 renders the dark appearance, DAZZLE_PREVIEW_LANG=zh the
/// Simplified Chinese wording, and DAZZLE_PREVIEW_PREFIX names the files.
bool PreviewDark() { return getenv("DAZZLE_PREVIEW_DARK") != nullptr; }
bool PreviewChinese() {
  const char* language = getenv("DAZZLE_PREVIEW_LANG");
  return language != nullptr && std::string(language) == "zh";
}
std::string PreviewPrefix() {
  const char* prefix = getenv("DAZZLE_PREVIEW_PREFIX");
  return prefix != nullptr ? std::string(prefix) : std::string();
}

NSString* const kNotes = @"## What's new\n"
                         @"\n"
                         @"- **Faster** startup: the window opens in half the time.\n"
                         @"- Fixed a crash when the last document was closed twice.\n"
                         @"- `dazzle_updater` now reports the version it is updating *from*.\n"
                         @"\n"
                         @"### Notes\n"
                         @"\n"
                         @"See the [full announcement](https://example.com/releases/2.6.1) for "
                         @"the details.\n"
                         @"\n"
                         @"```\n"
                         @"dazzle updater --check\n"
                         @"```\n";

class MemoryStore final : public UiPreferencesStore {
 public:
  UiPreferences Read() override { return preferences; }
  void Write(const UiPreferences& value) override { preferences = value; }
  UiPreferences preferences;
};

class ScriptedPort final : public UpdaterPort {
 public:
  bool check_ok = true;
  bool has_update = true;
  bool download_ok = true;
  bool install_ok = true;
  std::string version = "2.6.1";
  std::string display_version = "2.6.1";
  std::string notes;
  std::string notes_url;
  int64_t size = 0;
  double progress = 0.0;
  std::string error_code = "network";
  std::string error_message = "update server unreachable (timeout after 30s)";
  std::string current_version = "2.5.0";

  bool CheckForUpdates() override {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (gate_checks_) {
        cv_.wait(lock, [this] { return released_; });
      }
    }
    downloaded_ = false;  // a check clears what the last one downloaded
    return check_ok;
  }
  bool IsUpdateAvailable() const override { return check_ok && has_update; }
  std::string GetUpdateVersion() const override { return has_update ? version : std::string(); }
  std::string GetUpdateDisplayVersion() const override { return display_version; }
  std::string GetUpdateReleaseNotes() const override { return notes; }
  std::string GetUpdateReleaseNotesUrl() const override { return notes_url; }
  int64_t GetUpdateSize() const override { return size; }
  bool DownloadUpdate() override {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (gate_downloads_) {
        download_cv_.wait(lock, [this] { return download_released_; });
      }
    }
    downloaded_ = download_ok;
    return download_ok;
  }
  double GetDownloadProgress() const override { return progress; }
  void CancelDownload() override { ReleaseDownload(); }
  bool IsUpdateDownloaded() const override { return downloaded_; }
  bool InstallUpdate() override {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (gate_installs_) {
        install_cv_.wait(lock, [this] { return install_released_; });
      }
    }
    return install_ok;
  }
  std::string GetErrorCode() const override { return error_code; }
  std::string GetErrorMessage() const override { return error_message; }
  std::string GetCurrentVersion() const override { return current_version; }

  void GateChecks() {
    std::lock_guard<std::mutex> lock(mutex_);
    gate_checks_ = true;
    released_ = false;
  }
  void ReleaseChecks() {
    std::lock_guard<std::mutex> lock(mutex_);
    released_ = true;
    cv_.notify_all();
  }
  void GateDownloads() {
    std::lock_guard<std::mutex> lock(mutex_);
    gate_downloads_ = true;
    download_released_ = false;
  }
  void ReleaseDownload() {
    std::lock_guard<std::mutex> lock(mutex_);
    download_released_ = true;
    download_cv_.notify_all();
  }
  void ForgetDownload() { downloaded_ = false; }

  void GateInstalls() {
    std::lock_guard<std::mutex> lock(mutex_);
    gate_installs_ = true;
    install_released_ = false;
  }
  void ReleaseInstall() {
    std::lock_guard<std::mutex> lock(mutex_);
    install_released_ = true;
    install_cv_.notify_all();
  }

 private:
  bool downloaded_ = false;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::condition_variable download_cv_;
  std::condition_variable install_cv_;
  bool gate_checks_ = false;
  bool released_ = false;
  bool gate_downloads_ = false;
  bool download_released_ = false;
  bool gate_installs_ = false;
  bool install_released_ = false;
};

MemoryStore g_store;
ScriptedPort g_port;

/// A runner that reads `g_store` when it first needs it, which is how a round is started
/// from a given set of user answers. The module's own singleton is never destroyed either.
UiRunner* NewRunner() {
  UiRunner* runner = new UiRunner(dazzle_updater::internal::GetUiBackend(), g_port, g_store,
                                  dazzle_updater::internal::UiStringsFor({PreviewChinese() ? "zh-Hans"
                                                                                  : "en"}));
  runner->SetAppName("Dazzle Demo");
  return runner;
}

NSWindow* CurrentWindow() {
  NSWindow* main_window = [NSApp mainWindow];
  if (main_window != nil && main_window.isVisible) {
    return main_window;
  }
  for (NSWindow* window in [NSApp windows]) {
    if (window.isVisible && window.contentView != nil) {
      return window;
    }
  }
  return nil;
}

/// Everything that can be pressed: the windows are AppKit's own NSButtons, the
/// "View Release Notes" link included.
void CollectClickable(NSView* view, NSMutableArray* found) {
  if ([view isKindOfClass:[NSButton class]]) {
    [found addObject:view];
  }
  for (NSView* child in view.subviews) {
    CollectClickable(child, found);
  }
}

/// Presses the window's button with this title, the way a click would.
void Click(NSString* title) {
  NSWindow* window = CurrentWindow();
  if (window == nil) {
    std::printf("no window for click %s\n", title.UTF8String);
    return;
  }
  NSMutableArray* buttons = [NSMutableArray array];
  CollectClickable(window.contentView, buttons);
  for (id button in buttons) {
    NSString* button_title = [button respondsToSelector:@selector(title)] ? (NSString*)[button title] : nil;
    if ([button_title isEqualToString:title]) {
      [(NSButton*)button performClick:nil];
      return;
    }
  }
  std::printf("no button %s in %s\n", title.UTF8String, window.title.UTF8String);
}

/// Sends a key to the window's content view, the way the real key would arrive: this is
/// how the return and escape keys are checked without a keyboard.
void SendKey(NSString* characters, unsigned short key_code) {
  NSWindow* window = CurrentWindow();
  if (window == nil) {
    std::printf("no window for key %s\n", characters.UTF8String);
    return;
  }
  NSEvent* event = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                    location:NSZeroPoint
                               modifierFlags:0
                                   timestamp:0
                                windowNumber:window.windowNumber
                                     context:nil
                                  characters:characters
                 charactersIgnoringModifiers:characters
                                   isARepeat:NO
                                     keyCode:key_code];
  // What NSApplication does with a key press: the key equivalents first (the default
  // button's return, the way out's escape), then the first responder.
  if (![window performKeyEquivalent:event]) {
    [window sendEvent:event];
  }
}

void Capture(NSString* name) {
  NSWindow* window = CurrentWindow();
  if (window == nil) {
    std::printf("no window to capture for %s\n", name.UTF8String);
    return;
  }
  // The window's frame view, not just the content: it draws the window's own background
  // and its title bar, which the content view leaves transparent.
  NSView* view = window.contentView.superview != nil ? window.contentView.superview : window.contentView;
  const NSRect bounds = view.bounds;
  NSBitmapImageRep* rep = [view bitmapImageRepForCachingDisplayInRect:bounds];
  [view cacheDisplayInRect:bounds toBitmapImageRep:rep];
  NSData* png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
  const std::string prefix = PreviewPrefix();
  NSString* file = [NSString stringWithFormat:@"%s%s.png", prefix.c_str(), name.UTF8String];
  [[NSFileManager defaultManager] createDirectoryAtPath:OutputDirectory()
                            withIntermediateDirectories:YES
                                             attributes:nil
                                                  error:nil];
  NSString* path = [OutputDirectory() stringByAppendingPathComponent:file];
  [png writeToFile:path atomically:YES];
  std::printf("%-24s %s  %gx%g  app_active=%d window_key=%d\n", name.UTF8String,
              window.title.UTF8String, bounds.size.width, bounds.size.height,
              (int)NSApp.isActive, (int)window.isKeyWindow);
}

struct Step {
  const char* capture;
  std::function<void()> action;
};

}  // namespace

int main() {
  @autoreleasepool {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    // The demo's icon, as a bundled application would have: the tool itself is no bundle.
    NSImage* icon = [[NSImage alloc] initWithContentsOfFile:@"examples/updater_ui_example/DazzleDemo.icns"];
    if (icon != nil) {
      [NSApp setApplicationIconImage:icon];
    }
    if (PreviewDark()) {
      [NSApp setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]];
    }
    [NSApp activateIgnoringOtherApps:YES];

    UiRunner* runner = NewRunner();

    g_port.notes = kNotes.UTF8String;
    g_port.size = 46000000;

    // Every entry drives one round; where a round has to start from preferences the
    // runner has not read yet, it starts from a fresh runner, which reads the store then.
    std::vector<Step> steps;

    // 1. the user asks, and the check hangs on the port so the state can be captured
    g_port.GateChecks();
    steps.push_back({"01-checking", [runner] { runner->CheckForUpdates(); }});
    steps.push_back({"02-available", [&] { g_port.ReleaseChecks(); }});

    // 2. the download: gated first so the bar can be seen, then half full, then finished
    g_port.GateDownloads();
    steps.push_back({"03-downloading", [&] {
                       // The default button answers the return key; the escape key is
                       // checked on the notes-link window below.
                       SendKey(@"\r", 36);
                     }});
    steps.push_back({"04-progress-half", [&] { g_port.progress = 0.5; }});
    steps.push_back({"05-ready", [&] { g_port.ReleaseDownload(); }});

    // 3. the install: the window stays up through Installing, and a failed installer is
    //    what the Error state shows
    g_port.GateInstalls();
    steps.push_back({"06-installing", [&] { Click(@"Install and Relaunch"); }});
    steps.push_back({"07-error-install", [&] {
                       g_port.install_ok = false;
                       g_port.ReleaseInstall();
                     }});

    // 4. up to date, from a runner whose round has not downloaded anything yet
    steps.push_back({"08-up-to-date", [&] {
                       Click(@"OK");
                       g_port.ForgetDownload();
                       g_port.install_ok = true;
                       g_port.has_update = false;
                       NewRunner()->CheckForUpdates();
                     }});

    // 5. an error the window explains by its code
    steps.push_back({"09-error-signature", [&] {
                       Click(@"OK");
                       g_port.ForgetDownload();
                       g_port.has_update = true;
                       g_port.check_ok = false;
                       g_port.error_code = "signature";
                       g_port.error_message = "signature mismatch: expected pfIShU4…, got 7Yb2Qx…";
                       NewRunner()->CheckForUpdates();
                     }});

    // 6. the first silent check, which asks before it does anything
    steps.push_back({"10-permission", [&] {
                       Click(@"OK");
                       g_port.ForgetDownload();
                       g_port.check_ok = true;
                       g_store.preferences = UiPreferences{};
                       NewRunner()->CheckForUpdatesInBackground();
                     }});

    // 7. notes that are a link rather than a box
    steps.push_back({"11-available-notes-link", [&] {
                       SendKey(@"\033", 53);  // escape, the permission prompt's way out
                       g_port.ForgetDownload();
                       g_port.notes.clear();
                       g_port.notes_url = "https://example.com/releases/2.6.1";
                       NewRunner()->CheckForUpdates();
                     }});

    // 8. automatic downloads: the first window the user sees offers the package
    steps.push_back({"12-downloaded", [&] {
                       Click(@"Later");
                       g_port.ForgetDownload();
                       UiPreferences preferences;
                       preferences.automatic_checks_answered = true;
                       preferences.automatic_checks = true;
                       preferences.automatic_downloads = true;
                       g_store.preferences = preferences;
                       g_port.notes = kNotes.UTF8String;
                       g_port.notes_url.clear();
                       g_port.ReleaseDownload();
                       NewRunner()->CheckForUpdatesInBackground();
                     }});
    steps.push_back({nullptr, [&] {}});

    __block size_t index = 0;
    // __block, and the block is not copied into itself: the chain is re-created per step.
    __block void (^run_step)(void) = nil;
    void (^step_holder)(void) = ^{
      if (index >= steps.size()) {
        [NSApp terminate:nil];
        return;
      }
      steps[index].action();
      const char* capture = steps[index].capture;
      index += 1;
      void (^next)(void) = run_step;
      dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.7 * NSEC_PER_SEC)),
                     dispatch_get_main_queue(), ^{
                       if (capture != nullptr) {
                         Capture([NSString stringWithUTF8String:capture]);
                       }
                       next();
                     });
    };
    run_step = step_holder;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.2 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), run_step);
    [NSApp run];
  }
  return 0;
}
