#include "../../foundation/system.h"

#import <Foundation/Foundation.h>

#include <unistd.h>

#include <string>

namespace dazzle {
namespace {

std::string ToUtf8(NSString* value) {
  if (!value) {
    return "";
  }
  const char* utf8 = [value UTF8String];
  return utf8 ? std::string(utf8) : std::string();
}

NSString* ToNSString(const std::string& value) {
  return [NSString stringWithUTF8String:value.c_str()];
}

std::string BundleIdentifier() {
  return ToUtf8([[NSBundle mainBundle] bundleIdentifier]);
}

/// The CFBundleIdentifier inside an `.app`, or "" when there is none.
std::string BundleIdentifierOf(NSString* app_path) {
  NSBundle* bundle = [NSBundle bundleWithPath:app_path];
  return bundle ? ToUtf8([bundle objectForInfoDictionaryKey:@"CFBundleIdentifier"]) : "";
}

/// Runs a tool to completion; true when it exits with status 0. Its output goes to
/// `log_path`, which is created or truncated.
bool RunTool(NSString* launch_path, NSArray* arguments, NSString* log_path) {
  NSTask* task = [[NSTask alloc] init];
  [task setLaunchPath:launch_path];
  [task setArguments:arguments];
  if (log_path) {
    [[NSFileManager defaultManager] createFileAtPath:log_path contents:[NSData data] attributes:nil];
    NSFileHandle* log = [NSFileHandle fileHandleForWritingAtPath:log_path];
    if (log) {
      [task setStandardOutput:log];
      [task setStandardError:log];
    }
  }
  @try {
    [task launch];
  } @catch (NSException* exception) {
    return false;
  }
  [task waitUntilExit];
  return [task terminationStatus] == 0;
}

/// The single `.app` directly inside `directory`; nil when there is none or more than one.
NSString* SingleAppIn(NSString* directory) {
  NSArray* entries = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:directory error:nil];
  NSString* found = nil;
  for (NSString* entry in entries) {
    if (![entry hasSuffix:@".app"]) {
      continue;
    }
    if (found) {
      return nil;
    }
    found = [directory stringByAppendingPathComponent:entry];
  }
  return found;
}

/// Writes the script that replaces the application once this process has exited, and
/// starts it detached. A script rather than in-process code, because that work only
/// begins after the application is gone.
bool StartInstallHelper(NSString* working_directory, NSString* app_path, NSString* new_app_path,
                        NSString* log_path, std::string* error) {
  const char* script_text =
      "#!/bin/sh\n"
      "# dazzle: replace the application once the old process has exited.\n"
      "pid=\"$1\"\n"
      "app=\"$2\"\n"
      "new=\"$3\"\n"
      "work=\"$4\"\n"
      "while kill -0 \"$pid\" 2>/dev/null; do sleep 0.2; done\n"
      "backup=\"$app.dazzle-old.$$\"\n"
      "if mv \"$app\" \"$backup\" 2>/dev/null; then\n"
      "  if ditto \"$new\" \"$app\" 2>/dev/null; then\n"
      "    rm -rf \"$backup\"\n"
      "  else\n"
      "    rm -rf \"$app\"\n"
      "    mv \"$backup\" \"$app\"\n"
      "  fi\n"
      "fi\n"
      "rm -rf \"$work\"\n"
      "open \"$app\"\n";
  NSString* script_path = [working_directory stringByAppendingPathComponent:@"install.sh"];
  NSString* script = [NSString stringWithUTF8String:script_text];
  if (![script writeToFile:script_path atomically:YES encoding:NSUTF8StringEncoding error:nil]) {
    if (error) {
      *error = "cannot write the install helper";
    }
    return false;
  }
  [[NSFileManager defaultManager] setAttributes:@{NSFilePosixPermissions : @(0755)}
                                   ofItemAtPath:script_path
                                          error:nil];
  NSTask* task = [[NSTask alloc] init];
  [task setLaunchPath:@"/bin/sh"];
  [task setArguments:@[
    script_path, [NSString stringWithFormat:@"%d", getpid()], app_path, new_app_path,
    working_directory
  ]];
  if (log_path) {
    [[NSFileManager defaultManager] createFileAtPath:log_path contents:[NSData data] attributes:nil];
    NSFileHandle* log = [NSFileHandle fileHandleForWritingAtPath:log_path];
    if (log) {
      [task setStandardOutput:log];
      [task setStandardError:log];
    }
  }
  @try {
    [task launch];
  } @catch (NSException* exception) {
    if (error) {
      *error = "cannot start the install helper";
    }
    return false;
  }
  return true;
}

}  // namespace

bool SystemSupportsCheck() {
  return true;
}

bool SystemSupportsDownload() {
  return true;
}

bool SystemSupportsInstall() {
  return true;
}

std::string SystemName() {
  return "macos";
}

std::string SystemArch() {
#if defined(__arm64__)
  return "arm64";
#elif defined(__x86_64__)
  return "x64";
#else
  return "x86";
#endif
}

std::string SystemVersion() {
  const NSOperatingSystemVersion version = [[NSProcessInfo processInfo] operatingSystemVersion];
  std::string value =
      std::to_string(version.majorVersion) + "." + std::to_string(version.minorVersion);
  if (version.patchVersion != 0) {
    value += "." + std::to_string(version.patchVersion);
  }
  return value;
}

std::string AppVersionFromPlatform() {
  return ToUtf8([[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleShortVersionString"]);
}

std::string AppNameFromPlatform() {
  std::string name = ToUtf8([[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleName"]);
  if (name.empty()) {
    name = ToUtf8([[NSProcessInfo processInfo] processName]);
  }
  return name;
}

std::string DownloadDirectory() {
  NSFileManager* manager = [NSFileManager defaultManager];
  NSURL* caches = [manager URLForDirectory:NSCachesDirectory
                                  inDomain:NSUserDomainMask
                         appropriateForURL:nil
                                    create:YES
                                     error:nil];
  std::string directory = caches ? ToUtf8([caches path]) : ToUtf8(NSTemporaryDirectory());
  if (directory.empty()) {
    directory = "/tmp";
  }
  const std::string identifier = BundleIdentifier();
  directory += "/" + (identifier.empty() ? std::string("dazzle") : identifier);
  [manager createDirectoryAtPath:ToNSString(directory)
      withIntermediateDirectories:YES
                       attributes:nil
                            error:nil];
  return directory;
}

bool InstallDownloadedUpdate(const std::string& downloaded_path, const std::string& format,
                             const std::string& installer_arguments, std::string* error,
                             std::string* error_code) {
  (void)installer_arguments;  // A zip carries the whole bundle; it takes no arguments.

  auto fail = [error, error_code](const std::string& code, const std::string& message) {
    if (error) {
      *error = message;
    }
    if (error_code) {
      *error_code = code;
    }
    return false;
  };

  if (format != "zip") {
    return fail("installer", "the installer format \"" + format + "\" is not supported on macOS");
  }
  NSString* app_path = [[NSBundle mainBundle] bundlePath];
  if (!app_path || [app_path length] == 0) {
    return fail("installer", "the running application has no bundle to replace");
  }
  const std::string app = ToUtf8(app_path);
  if (app.find("/AppTranslocation/") != std::string::npos) {
    return fail("app-location",
                "the application is running from a temporary App Translocation location");
  }
  NSFileManager* manager = [NSFileManager defaultManager];
  NSString* parent = [app_path stringByDeletingLastPathComponent];
  if (![manager isWritableFileAtPath:parent]) {
    return fail("permission", "no permission to replace the application at " + app);
  }
  if (![manager fileExistsAtPath:ToNSString(downloaded_path)]) {
    return fail("installer", "the downloaded installer is missing");
  }

  // Extract next to the download, never over the running application.
  NSString* work_directory =
      [ToNSString(DownloadDirectory())
          stringByAppendingPathComponent:[NSString stringWithFormat:@"update-%d", getpid()]];
  [manager removeItemAtPath:work_directory error:nil];
  if (![manager createDirectoryAtPath:work_directory
             withIntermediateDirectories:YES
                              attributes:nil
                                   error:nil]) {
    return fail("permission", "cannot create a working directory for the update");
  }
  NSString* log_path = [work_directory stringByAppendingPathComponent:@"install.log"];
  if (!RunTool(@"/usr/bin/ditto", @[ @"-x", @"-k", ToNSString(downloaded_path), work_directory ],
               log_path)) {
    [manager removeItemAtPath:work_directory error:nil];
    return fail("installer", "cannot extract the downloaded zip");
  }
  NSString* new_app = SingleAppIn(work_directory);
  if (!new_app) {
    [manager removeItemAtPath:work_directory error:nil];
    return fail("installer", "the downloaded zip does not hold exactly one .app");
  }

  // The new bundle must be the same application, or the update would replace the
  // application with something else entirely.
  const std::string new_identifier = BundleIdentifierOf(new_app);
  const std::string identifier = BundleIdentifier();
  if (new_identifier.empty() || new_identifier != identifier) {
    [manager removeItemAtPath:work_directory error:nil];
    return fail("installer", "the downloaded application is not this application");
  }

  std::string helper_error;
  if (!StartInstallHelper(work_directory, app_path, new_app, log_path, &helper_error)) {
    [manager removeItemAtPath:work_directory error:nil];
    return fail("installer", helper_error);
  }
  return true;
}

}  // namespace dazzle
