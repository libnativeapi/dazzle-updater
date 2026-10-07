#include "../../foundation/system.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "../../foundation/http.h"

// Linux's system seam (../../specs/updater.md, "平台可用性"): check and download through
// the libcurl HTTP stack, install by replacing the application's own directory with the
// one a zip carries and starting it again.
//
// A Linux desktop application is a directory — the executable plus whatever sits beside
// it — and the update is that directory at a higher version. The seam therefore needs no
// package manager: it unpacks the zip next to the download, checks that it holds the same
// executable this process is running, and hands the swap to a detached shell script that
// waits for this process to exit (a running executable may be replaced on Linux, but its
// directory is not ours to rewrite while it is being read).
//
// What the platform cannot tell, it does not invent: AppVersionFromPlatform() is empty
// because Linux has no metadata every application carries, so the application sets its
// own version with Updater::SetCurrentVersion().

namespace dazzle {
namespace {

std::string ReadLink(const std::string& path) {
  char buffer[PATH_MAX];
  const ssize_t length = readlink(path.c_str(), buffer, sizeof(buffer) - 1);
  if (length <= 0) {
    return "";
  }
  buffer[length] = '\0';
  return std::string(buffer);
}

std::string ExecutablePath() {
  return ReadLink("/proc/self/exe");
}

/// Everything before the last separator; "" when there is none (a bare file name).
std::string DirectoryOf(const std::string& path) {
  const size_t separator = path.find_last_of('/');
  if (separator == std::string::npos) {
    return "";
  }
  return path.substr(0, separator);
}

std::string BaseNameOf(const std::string& path) {
  const size_t separator = path.find_last_of('/');
  return separator == std::string::npos ? path : path.substr(separator + 1);
}

std::string EnvironmentVariable(const char* name) {
  const char* value = std::getenv(name);
  return value != nullptr ? std::string(value) : std::string();
}

/// An XDG base directory: the environment variable when it is set to an absolute path,
/// `fallback` (already absolute) otherwise.
std::string XdgDirectory(const char* variable, const std::string& fallback) {
  const std::string value = EnvironmentVariable(variable);
  if (!value.empty() && value[0] == '/') {
    return value;
  }
  return fallback;
}

/// Creates `path` and every missing parent; true when it exists afterwards.
bool EnsureDirectory(const std::string& path) {
  if (path.empty()) {
    return false;
  }
  struct stat info;
  if (stat(path.c_str(), &info) == 0) {
    return S_ISDIR(info.st_mode);
  }
  if (!EnsureDirectory(DirectoryOf(path))) {
    return false;
  }
  return mkdir(path.c_str(), 0755) == 0;
}

bool FileExists(const std::string& path) {
  struct stat info;
  return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

bool IsExecutable(const std::string& path) {
  return access(path.c_str(), X_OK) == 0;
}

bool IsWritable(const std::string& path) {
  return access(path.c_str(), W_OK) == 0;
}

/// A value from /etc/os-release, e.g. VERSION_ID="24.04" for the release index's
/// minimumSystemVersion. Quoted values lose their quotes; "" when the key is absent.
std::string OsReleaseValue(const std::string& key) {
  std::ifstream file("/etc/os-release");
  std::string line;
  const std::string prefix = key + "=";
  while (std::getline(file, line)) {
    if (line.compare(0, prefix.size(), prefix) != 0) {
      continue;
    }
    std::string value = line.substr(prefix.size());
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
      value = value.substr(1, value.size() - 2);
    }
    return value;
  }
  return "";
}

/// The only tool this seam runs: the system's own unzip, which every desktop Ubuntu
/// ships. The cross-platform layer verifies the package's signature before we get here,
/// so the archive is trustworthy; it is unpacked next to the download, never over the
/// running application.
bool ExtractZip(const std::string& archive, const std::string& destination,
                const std::string& log_path) {
  const pid_t child = fork();
  if (child < 0) {
    return false;
  }
  if (child == 0) {
    const int log = open(log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (log >= 0) {
      dup2(log, STDOUT_FILENO);
      dup2(log, STDERR_FILENO);
      close(log);
    }
    execlp("unzip", "unzip", "-q", "-o", archive.c_str(), "-d", destination.c_str(),
           static_cast<char*>(nullptr));
    _exit(127);
  }
  int status = 0;
  if (waitpid(child, &status, 0) != child) {
    return false;
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/// Starts the script that swaps the directory once this process has exited, detached
/// from it so the swap survives the exit. A script rather than in-process code, because
/// that work only begins after the application is gone.
bool StartInstallHelper(const std::string& app_directory, const std::string& new_directory,
                        const std::string& work_directory, const std::string& executable,
                        const std::string& log_path, std::string* error) {
  const char* script_text =
      "#!/bin/sh\n"
      "# dazzle: replace the application once the old process has exited.\n"
      "pid=\"$1\"\n"
      "app=\"$2\"\n"
      "new=\"$3\"\n"
      "work=\"$4\"\n"
      "exe=\"$5\"\n"
      "while kill -0 \"$pid\" 2>/dev/null; do sleep 0.2; done\n"
      "backup=\"$app.dazzle-old.$$\"\n"
      "if mv \"$app\" \"$backup\" 2>/dev/null; then\n"
      "  if mv \"$new\" \"$app\" 2>/dev/null; then\n"
      "    rm -rf \"$backup\"\n"
      "  else\n"
      "    mv \"$backup\" \"$app\"\n"
      "  fi\n"
      "fi\n"
      "rm -rf \"$work\"\n"
      "if [ -x \"$exe\" ]; then\n"
      "  setsid \"$exe\" >/dev/null 2>&1 &\n"
      "fi\n";
  const std::string script_path = work_directory + "/install.sh";
  {
    std::ofstream script(script_path, std::ios::binary | std::ios::trunc);
    if (!script) {
      if (error != nullptr) {
        *error = "cannot write the install helper";
      }
      return false;
    }
    script << script_text;
  }
  chmod(script_path.c_str(), 0755);

  const std::string pid = std::to_string(getpid());
  const pid_t child = fork();
  if (child < 0) {
    if (error != nullptr) {
      *error = "cannot start the install helper";
    }
    return false;
  }
  if (child == 0) {
    setsid();  // its own session: it outlives this process
    const int log = open(log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (log >= 0) {
      dup2(log, STDOUT_FILENO);
      dup2(log, STDERR_FILENO);
      close(log);
    }
    execl("/bin/sh", "sh", script_path.c_str(), pid.c_str(), app_directory.c_str(),
          new_directory.c_str(), work_directory.c_str(), executable.c_str(),
          static_cast<char*>(nullptr));
    _exit(127);
  }
  return true;
}

/// The single directory directly inside `directory`; "" when there is none or more than
/// one, which is what a package of this application looks like.
std::string SingleDirectoryIn(const std::string& directory) {
  std::string found;
  // opendir/readdir rather than a shell listing: no shell, and no output to parse.
  DIR* handle = opendir(directory.c_str());
  if (handle == nullptr) {
    return "";
  }
  while (struct dirent* entry = readdir(handle)) {
    const std::string name = entry->d_name;
    if (name == "." || name == "..") {
      continue;
    }
    struct stat info;
    if (stat((directory + "/" + name).c_str(), &info) != 0 || !S_ISDIR(info.st_mode)) {
      continue;
    }
    if (!found.empty()) {
      closedir(handle);
      return "";
    }
    found = name;
  }
  closedir(handle);
  return found.empty() ? "" : directory + "/" + found;
}

}  // namespace

bool SystemSupportsCheck() {
  return HttpIsAvailable();
}

bool SystemSupportsDownload() {
  return HttpIsAvailable();
}

bool SystemSupportsInstall() {
  // The swap itself needs nothing but a writable application directory, so installing is
  // reported as available even in a build without an HTTP stack: the file to install can
  // come from anywhere.
  return true;
}

std::string SystemName() {
  return "linux";
}

std::string SystemArch() {
  struct utsname info;
  if (uname(&info) != 0) {
    return "";
  }
  const std::string machine = info.machine;
  if (machine == "x86_64" || machine == "amd64") {
    return "x64";
  }
  if (machine == "aarch64" || machine == "arm64") {
    return "arm64";
  }
  if (machine == "i386" || machine == "i486" || machine == "i586" || machine == "i686") {
    return "x86";
  }
  return "";
}

std::string SystemVersion() {
  const std::string version = OsReleaseValue("VERSION_ID");
  if (!version.empty()) {
    return version;
  }
  struct utsname info;
  return uname(&info) == 0 ? std::string(info.release) : "";
}

std::string AppVersionFromPlatform() {
  // Linux has no metadata every application carries; an application that knows its own
  // version passes it to Updater::SetCurrentVersion().
  return "";
}

std::string AppNameFromPlatform() {
  // The executable's own name, which is what a Linux desktop shows for a program that
  // has not named itself.
  const std::string name = BaseNameOf(ExecutablePath());
  return name.empty() ? "" : name;
}

std::string DownloadDirectory() {
  const std::string home = EnvironmentVariable("HOME");
  const std::string directory =
      XdgDirectory("XDG_CACHE_HOME", home.empty() ? "/tmp" : home + "/.cache") + "/dazzle";
  if (!EnsureDirectory(directory)) {
    return "";
  }
  return directory;
}

bool InstallDownloadedUpdate(const std::string& downloaded_path, const std::string& format,
                             const std::string& installer_arguments, std::string* error,
                             std::string* error_code) {
  (void)installer_arguments;  // a zip carries the whole application directory

  auto fail = [error, error_code](const std::string& code, const std::string& message) {
    if (error != nullptr) {
      *error = message;
    }
    if (error_code != nullptr) {
      *error_code = code;
    }
    return false;
  };

  if (format != "zip") {
    return fail("installer", "the installer format \"" + format + "\" is not supported on Linux");
  }
  const std::string executable = ExecutablePath();
  if (executable.empty()) {
    return fail("installer", "the running application cannot be located");
  }
  const std::string app_directory = DirectoryOf(executable);
  const std::string executable_name = executable.substr(app_directory.size() + 1);
  if (app_directory.empty() || executable_name.empty()) {
    return fail("installer", "the running application has no directory to replace");
  }
  if (!IsWritable(DirectoryOf(app_directory))) {
    return fail("permission", "no permission to replace the application at " + app_directory);
  }
  if (!FileExists(downloaded_path)) {
    return fail("installer", "the downloaded installer is missing");
  }
  // Extract next to the download, never over the running application.
  const std::string work_directory =
      DownloadDirectory() + "/update-" + std::to_string(getpid());
  if (!EnsureDirectory(work_directory)) {
    return fail("permission", "cannot create a working directory for the update");
  }
  const std::string log_path = work_directory + "/install.log";
  if (!ExtractZip(downloaded_path, work_directory, log_path)) {
    return fail("installer",
                "cannot extract the downloaded zip (unzip is needed and may be missing)");
  }

  const std::string new_directory = SingleDirectoryIn(work_directory);
  if (new_directory.empty()) {
    return fail("installer", "the downloaded zip does not hold exactly one directory");
  }
  // The package must carry the same program, or the update would replace the application
  // with something else entirely.
  const std::string new_executable = new_directory + "/" + executable_name;
  if (!IsExecutable(new_executable)) {
    // Naming the file that is missing, and where it was looked for, is what turns this
    // failure into something a release's author can act on.
    return fail("installer", "the downloaded package is not this application: " +
                                 new_executable + " is missing or not executable");
  }

  std::string helper_error;
  if (!StartInstallHelper(app_directory, new_directory, work_directory, executable, log_path,
                          &helper_error)) {
    return fail("installer", helper_error);
  }
  return true;
}

}  // namespace dazzle
