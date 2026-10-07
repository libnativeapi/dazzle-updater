#!/bin/sh
# Runs the UpdaterUI example end to end, the way a release reaches a user: a local HTTP
# server with a signed release index, an application bundle that offers the update, and
# the built-in windows for the whole round.
#
# Usage: demo.sh [--dark | --light] [<path to updater_ui_example.app>
#                                    [<path to fixture_signer>]]
#
# --dark and --light are handed to the application, so either appearance can be looked at
# without changing the system's own.
#
# The application is built as a bundle (see CMakeLists.txt). The script copies it into a
# temporary directory, writes the feed URL and the public key into the copy's Info.plist,
# and serves a signed 2.0.0 package that is the same bundle with a higher version — so
# clicking through "Install and Update" replaces the demo in place and relaunches it, and
# the relaunched copy still knows where its feed is.
#
# The server, the copied bundle and the answers the user gives the demo are cleaned up
# when it exits, so a second run asks the automatic-checks question again.
set -eu

here="$(cd "$(dirname "$0")" && pwd)"
build="${here}/../../build"

# The application's own switches come first; the two optional paths after them.
appearance=""
if [ "${1:-}" = "--dark" ] || [ "${1:-}" = "--light" ]; then
  appearance="$1"
  shift
fi
app="${1:-${build}/examples/updater_ui_example/updater_ui_example.app}"
signer="${2:-${build}/tests/fixture_signer}"

[ -d "$app" ] || {
  echo "no application bundle at $app" >&2
  echo "build it first:  cmake -S core -B core/build && cmake --build core/build" >&2
  exit 1
}
[ -x "$signer" ] || {
  echo "no fixture signer at $signer (it is built with the tests)" >&2
  exit 1
}

bundle_id="com.example.dazzle-demo"
work="$(mktemp -d "${TMPDIR%/}/dazzle-demo.XXXXXX")"
server_pid=""
cleanup() {
  if [ -n "$server_pid" ]; then
    kill "$server_pid" 2>/dev/null || true
  fi
  rm -rf "$work"
  rm -rf "$HOME/Library/Caches/$bundle_id"
  defaults delete "$bundle_id" >/dev/null 2>&1 || true
}
trap cleanup EXIT INT TERM

# A port nothing is listening on, and the key pair the demo release is signed with.
port="$(python3 -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1]); s.close()')"
key_output="$("$signer" gen-key)"
public_key="$(printf '%s\n' "$key_output" | sed -n 's/^public //p')"
secret_key="$(printf '%s\n' "$key_output" | sed -n 's/^secret //p')"
feed="http://127.0.0.1:${port}/releases.json"

# set_version <app> <version>: what the bundle reports as its own version.
set_version() {
  plutil -replace CFBundleShortVersionString -string "$2" "$1/Contents/Info.plist"
  plutil -replace CFBundleVersion -string "$2" "$1/Contents/Info.plist"
}

# 1. The application the user runs: 1.0.0, with its feed and key in the bundle.
cp -R "$app" "$work/DazzleDemo.app"
plutil -replace CFBundleName -string "Dazzle Demo" "$work/DazzleDemo.app/Contents/Info.plist"
plutil -replace CFBundleIdentifier -string "$bundle_id" \
  "$work/DazzleDemo.app/Contents/Info.plist"
plutil -replace DazzleDemoFeedURL -string "$feed" "$work/DazzleDemo.app/Contents/Info.plist"
plutil -replace DazzleDemoPublicKey -string "$public_key" \
  "$work/DazzleDemo.app/Contents/Info.plist"
# What "Reset Demo" goes back to; the 2.0.0 copy below inherits it, so the demo can be
# walked again after the update was installed.
plutil -replace DazzleDemoResetVersion -string "1.0.0" "$work/DazzleDemo.app/Contents/Info.plist"
set_version "$work/DazzleDemo.app" "1.0.0"

# 2. The release: the same bundle at 2.0.0, zipped the way a release ships it and signed
#    with the key the application carries.
mkdir -p "$work/staging"
cp -R "$work/DazzleDemo.app" "$work/staging/DazzleDemo.app"
set_version "$work/staging/DazzleDemo.app" "2.0.0"
echo "installed by the demo" > "$work/staging/DazzleDemo.app/Contents/version-marker"
ditto -c -k --sequesterRsrc --keepParent "$work/staging/DazzleDemo.app" "$work/DazzleDemo-2.0.0.zip"
signature="$("$signer" sign "$secret_key" "$work/DazzleDemo-2.0.0.zip")"
size="$(wc -c < "$work/DazzleDemo-2.0.0.zip" | tr -d ' ')"
arch="$(uname -m | sed 's/x86_64/x64/')"

cat > "$work/releases.json" <<JSON
{
  "schema": 1,
  "releases": [
    {
      "version": "2.0.0",
      "displayVersion": "2.0",
      "releaseNotes": "## What's new in 2.0\n\n- **Faster startup.** The main window now opens in about half the time.\n- **Fixed** a crash when the last document was closed twice.\n- The update window now shows the version you are updating *from*.\n\n### Under the hood\n\nEvery release is signed, and the signature is checked before anything is installed. The [full changelog](https://example.com/releases/2.0) lists the smaller fixes.\n\n\`\`\`\n$ dazzle-demo --version\nDazzle Demo 2.0.0\n\`\`\`\n",
      "assets": [
        {
          "platform": "macos",
          "arch": "$arch",
          "url": "http://127.0.0.1:$port/DazzleDemo-2.0.0.zip",
          "size": $size,
          "format": "zip",
          "signature": "$signature"
        }
      ]
    }
  ]
}
JSON

# 3. Serve it, and wait until it answers.
(cd "$work" && exec python3 -m http.server "$port" --bind 127.0.0.1) > "$work/server.log" 2>&1 &
server_pid=$!
ready=""
for _ in $(seq 1 50); do
  if curl -fsS "$feed" > /dev/null 2>&1; then
    ready=yes
    break
  fi
  sleep 0.1
done
[ -n "$ready" ] || {
  echo "the local release index server did not start; see $work/server.log" >&2
  exit 1
}

echo "demo application: $work/DazzleDemo.app   (version 1.0.0)"
echo "release index:    $feed   (offers 2.0.0, signed)"
echo
echo "The application checks for updates as it starts, and UpdaterUI shows what it finds."
echo "Click Install Update, then Install and Relaunch: the bundle is replaced in place and"
echo "relaunched, and the copy that comes back reports 2.0.0. Reset Demo forgets the answers"
echo "and goes back to 1.0.0, so the next check finds 2.0.0 again. Quit the application when"
echo "you are done; this script waits for it and then cleans up."

# 4. Launch it as a real application (frontmost, with its own Dock icon), then wait for
#    the launch to happen and for the application to be quit — including the copy the
#    installer relaunches, so the wait covers the whole round.
pattern="DazzleDemo.app/Contents/MacOS/updater_ui_example"
if [ -n "$appearance" ]; then
  open --stdout "$work/app.log" --stderr "$work/app.log" "$work/DazzleDemo.app" \
    --args "$appearance"
else
  open --stdout "$work/app.log" --stderr "$work/app.log" "$work/DazzleDemo.app"
fi
running=""
for _ in $(seq 1 100); do
  if pgrep -f "$pattern" > /dev/null 2>&1; then
    running=yes
    break
  fi
  sleep 0.1
done
if [ -z "$running" ]; then
  echo "the demo application did not start; see $work/app.log" >&2
  exit 1
fi
while pgrep -f "$pattern" > /dev/null 2>&1; do
  sleep 1
done

echo
echo "The application quit."
if [ -f "$work/DazzleDemo.app/Contents/version-marker" ]; then
  installed="$(plutil -extract CFBundleShortVersionString raw -o - \
    "$work/DazzleDemo.app/Contents/Info.plist")"
  echo "The update went through: the bundle was replaced in place and now reports version $installed."
else
  echo "The update was not installed: the bundle is unchanged."
fi
if [ -s "$work/app.log" ]; then
  echo
  echo "The application printed:"
  sed 's/^/  /' "$work/app.log"
fi
