// A host application for the built-in update windows: a small window of its own, a
// "Check for Updates…" menu item, and UpdaterUI's windows on top of it. It is the
// example for ../../../specs/updater-ui.md — the whole update flow in one line per entry
// point, with no UI of its own to write.
//
// Usage:
//   updater_ui_example [--dark | --light] [<release-index-url> [<public-key>
//                       [<current-version>]]]
//
// `--dark` and `--light` pin the appearance, so both can be looked at without changing
// the system's own; without either, the windows follow the system.
//
// Without arguments the three come from this bundle's Info.plist (DazzleDemoFeedURL,
// DazzleDemoPublicKey and CFBundleShortVersionString), which is what demo.sh writes: the
// feed survives the update, because the package the installer puts in place is a copy of
// this bundle with a higher version.
//
// Running it inside a bundle matters: macOS gives a window to a bundled application, and
// UpdaterUI asks the platform for the application's own icon and name.

#import <AppKit/AppKit.h>

#include <dazzle_updater.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {

std::string ToUtf8(NSString* value) {
  if (value == nil) {
    return "";
  }
  const char* utf8 = [value UTF8String];
  return utf8 != nullptr ? std::string(utf8) : std::string();
}

/// A string from this bundle's Info.plist, or "" when it is not there.
std::string InfoString(NSString* key) {
  return ToUtf8([[NSBundle mainBundle] objectForInfoDictionaryKey:key]);
}

void PrintConfiguration(const std::string& feed, const std::string& key,
                        const std::string& version) {
  std::printf("feed:            %s\n", feed.empty() ? "(none)" : feed.c_str());
  std::printf("public key:      %s\n", key.empty() ? "(none)" : key.c_str());
  std::printf("current version: %s\n", version.c_str());
  std::fflush(stdout);
}

}  // namespace

@interface DazzleDemoHost : NSObject <NSApplicationDelegate>
@end

@implementation DazzleDemoHost {
  NSWindow* _window;
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
  [self buildMenu];
  [self buildWindow];
  [NSApp activateIgnoringOtherApps:YES];

  // The user started the application, so this is the check they asked for: the window
  // opens at once and walks them through whatever the feed has.
  dazzle_updater::UpdaterUI::GetInstance().CheckForUpdates();
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
  return YES;
}

- (void)checkForUpdates:(id)sender {
  dazzle_updater::UpdaterUI::GetInstance().CheckForUpdates();
}

- (void)checkInBackground:(id)sender {
  // The silent entry point: nothing is shown unless there is something to tell the user
  // about, and the question about automatic checks is asked the first time.
  dazzle_updater::UpdaterUI::GetInstance().CheckForUpdatesInBackground();
}

- (void)buildMenu {
  NSMenu* menu_bar = [[NSMenu alloc] init];
  NSMenuItem* application_item = [[NSMenuItem alloc] init];
  [menu_bar addItem:application_item];
  NSMenu* application_menu = [[NSMenu alloc] init];
  [application_menu addItemWithTitle:@"Check for Updates…"
                              action:@selector(checkForUpdates:)
                       keyEquivalent:@"u"];
  [application_menu addItemWithTitle:@"Check in Background"
                              action:@selector(checkInBackground:)
                       keyEquivalent:@""];
  [application_menu addItem:[NSMenuItem separatorItem]];
  NSString* quit = [NSString stringWithFormat:@"Quit %@",
                                              [[NSProcessInfo processInfo] processName]];
  [application_menu addItemWithTitle:quit action:@selector(terminate:) keyEquivalent:@"q"];
  application_item.submenu = application_menu;
  [NSApp setMainMenu:menu_bar];
}

- (void)buildWindow {
  const std::string name = InfoString(@"CFBundleName");
  const std::string version = dazzle_updater::Updater::GetInstance().GetCurrentVersion();

  NSView* content = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 420, 150)];

  NSImage* icon = [NSApp applicationIconImage];
  NSImageView* icon_view = [[NSImageView alloc] initWithFrame:NSMakeRect(20, 20, 64, 64)];
  icon_view.image = icon != nil ? icon : [NSImage imageNamed:NSImageNameApplicationIcon];
  [content addSubview:icon_view];

  NSTextField* title = [NSTextField labelWithString:
                                        [NSString stringWithFormat:@"%s %s",
                                                                   name.empty() ? "DazzleUpdater" : name.c_str(),
                                                                   version.c_str()]];
  title.font = [NSFont boldSystemFontOfSize:15];
  title.frame = NSMakeRect(100, 66, 300, 20);
  [content addSubview:title];

  NSTextField* detail = [NSTextField
      labelWithString:@"A host application: the update windows on top are UpdaterUI's."];
  detail.textColor = [NSColor secondaryLabelColor];
  detail.font = [NSFont systemFontOfSize:11];
  detail.frame = NSMakeRect(100, 44, 300, 18);
  [content addSubview:detail];

  NSButton* background = [NSButton buttonWithTitle:@"Check in Background"
                                            target:self
                                            action:@selector(checkInBackground:)];
  background.frame = NSMakeRect(100, 16, 180, 24);
  [content addSubview:background];

  NSButton* check = [NSButton buttonWithTitle:@"Check for Updates…"
                                       target:self
                                       action:@selector(checkForUpdates:)];
  check.frame = NSMakeRect(288, 16, 112, 24);
  check.keyEquivalent = @"\r";
  [content addSubview:check];

  _window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 420, 150)
                                        styleMask:NSWindowStyleMaskTitled |
                                                  NSWindowStyleMaskClosable |
                                                  NSWindowStyleMaskMiniaturizable
                                          backing:NSBackingStoreBuffered
                                            defer:NO];
  _window.title = [NSString stringWithFormat:@"%s", name.empty() ? "DazzleUpdater" : name.c_str()];
  _window.contentView = content;
  _window.releasedWhenClosed = NO;
  [_window center];
  [_window makeKeyAndOrderFront:nil];
}

@end

int main(int argc, char** argv) {
  @autoreleasepool {
    dazzle_updater::Updater& updater = dazzle_updater::Updater::GetInstance();
    dazzle_updater::UpdaterUI& ui = dazzle_updater::UpdaterUI::GetInstance();

    // The arguments win over the bundle's Info.plist, so the same binary can be pointed
    // at another feed from a terminal.
    std::vector<std::string> arguments(argv + 1, argv + argc);
    NSString* appearance = nil;
    for (auto it = arguments.begin(); it != arguments.end();) {
      if (*it == "--dark") {
        appearance = NSAppearanceNameDarkAqua;
        it = arguments.erase(it);
      } else if (*it == "--light") {
        appearance = NSAppearanceNameAqua;
        it = arguments.erase(it);
      } else {
        ++it;
      }
    }
    const std::string feed = !arguments.empty() ? arguments[0] : InfoString(@"DazzleDemoFeedURL");
    const std::string key =
        arguments.size() > 1 ? arguments[1] : InfoString(@"DazzleDemoPublicKey");
    const std::string version =
        arguments.size() > 2 ? arguments[2] : InfoString(@"CFBundleShortVersionString");
    PrintConfiguration(feed, key, version);

    if (feed.empty() || key.empty()) {
      std::fprintf(stderr,
                   "no release index or public key: pass them as arguments, or run "
                   "demo.sh, which writes them into this bundle's Info.plist\n");
    } else {
      dazzle_updater::ReleaseIndexSource::GetInstance().SetUrl(feed);
      updater.SetPublicKey(key);
      // Needed only when the platform cannot tell the version by itself — that is, when
      // the binary runs outside its bundle.
      if (!version.empty()) {
        updater.SetCurrentVersion(version);
      }
      // Automatic downloads off, so the demo shows the Available window before anything
      // is fetched; the check box in the window can turn them on.
      ui.SetAutomaticDownloadsEnabled(false);
    }

    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    if (appearance != nil) {
      [NSApp setAppearance:[NSAppearance appearanceNamed:appearance]];
    }
    DazzleDemoHost* host = [[DazzleDemoHost alloc] init];
    [NSApp setDelegate:host];
    [NSApp run];
  }
  return 0;
}
