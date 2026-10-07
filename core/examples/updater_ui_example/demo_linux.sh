#!/bin/sh
# Runs the UpdaterUI example on Linux end to end, the way a release reaches a user: a
# local HTTP server with a signed release index, an application directory that offers the
# update, and the built-in GTK 4 windows for the whole round.
#
# Usage: demo_linux.sh [<path to updater_ui_example> [<path to fixture_signer>]]
#
# A Linux application is a directory — the binary plus whatever sits beside it — so the
# script copies one into a temporary place, writes the version it reports into a `version`
# file and its feed and key into `dazzle-demo.conf` beside the binary, and serves a signed
# 2.0.0 package that is the same directory at a higher version. Clicking through "Install Update" then replaces the directory in place
# and starts it again, and the copy that comes back still knows where its feed is.
#
# The server, the copied directory and the answers the user gave the demo are cleaned up
# when it exits, so a second run asks the automatic-checks question again. The answers
# live in $XDG_CONFIG_HOME/dazzle/<application>.conf, which is this demo's own file.
set -eu

here="$(cd "$(dirname "$0")" && pwd)"
build="${here}/../../build"

app="${1:-${build}/examples/updater_ui_example/updater_ui_example}"
signer="${2:-${build}/tests/fixture_signer}"

[ -x "$app" ] || {
  echo "no application binary at $app" >&2
  echo "build it first:  cmake -S core -B core/build && cmake --build core/build" >&2
  exit 1
}
[ -x "$signer" ] || {
  echo "no fixture signer at $signer (it is built with the tests)" >&2
  exit 1
}

work="$(mktemp -d "${TMPDIR:-/tmp}/dazzle-demo.XXXXXX")"
server_pid=""
cleanup() {
  if [ -n "$server_pid" ]; then
    kill "$server_pid" 2>/dev/null || true
  fi
  rm -rf "$work"
  rm -f "${XDG_CONFIG_HOME:-$HOME/.config}/dazzle/DazzleDemo.conf"
}
trap cleanup EXIT INT TERM

# A port nothing is listening on, and the key pair the demo release is signed with.
port="$(python3 -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1]); s.close()')"
key_output="$("$signer" gen-key)"
public_key="$(printf '%s\n' "$key_output" | sed -n 's/^public //p')"
secret_key="$(printf '%s\n' "$key_output" | sed -n 's/^secret //p')"
feed="http://127.0.0.1:${port}/releases.json"

# make_app <directory> <version> [<marker text>]. The binary sits directly in the
# application directory, which is the directory dazzle_updater replaces: the one holding the
# running executable.
make_app() {
  mkdir -p "$1"
  cp "$app" "$1/DazzleDemo"
  printf '%s\n' "$2" > "$1/version"
  cat > "$1/dazzle-demo.conf" <<CONF
feed=$feed
publicKey=$public_key
resetVersion=1.0.0
CONF
  if [ -n "${3:-}" ]; then
    printf '%s\n' "$3" > "$1/version-marker"
  fi
}

# 1. The application the user runs: 1.0.0, carrying its feed and key beside its binary.
make_app "$work/DazzleDemo" "1.0.0"

# 2. The release: the same directory at 2.0.0, zipped the way a release ships it (one
#    top-level directory, the application's own name) and signed with that key.
make_app "$work/staging/DazzleDemo" "2.0.0" "installed by the demo"
(cd "$work/staging" && zip -q -r "$work/DazzleDemo-2.0.0.zip" "DazzleDemo")
signature="$("$signer" sign "$secret_key" "$work/DazzleDemo-2.0.0.zip")"
size="$(wc -c < "$work/DazzleDemo-2.0.0.zip" | tr -d ' ')"
arch="$(uname -m | sed -e 's/x86_64/x64/' -e 's/aarch64/arm64/')"

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
          "platform": "linux",
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

echo "demo application: $work/DazzleDemo   (version 1.0.0)"
echo "release index:    $feed   (offers 2.0.0, signed)"
echo
echo "The application checks for updates as it starts, and UpdaterUI shows what it finds."
echo "Click Install Update, then Install and Relaunch: the application directory is replaced"
echo "in place and restarted, and the copy that comes back reports 2.0.0. Reset Demo forgets"
echo "the answers and goes back to 1.0.0, so the next check finds 2.0.0 again. Close the window"
echo "when you are done; this script waits for it and then cleans up."

# 4. Run it, and wait for it to be quit — including the copy the installer restarts, so
#    the wait covers the whole round. Nothing is passed in: the application reads its own
#    configuration file, which is how the restarted copy finds the feed again.
"$work/DazzleDemo/DazzleDemo" > "$work/app.log" 2>&1 &
app_pid=$!
wait "$app_pid" || true

echo
echo "The application quit."

# The install helper waits for this process to exit and only then swaps the directory, so
# the replacement lands a moment after the application is gone.
replaced=""
for _ in $(seq 1 100); do
  if [ -f "$work/DazzleDemo/version-marker" ]; then
    replaced=yes
    break
  fi
  sleep 0.2
done

# The copy the installer started runs on, and it reads the feed and its version out of this
# directory as it starts: the cleanup below has to wait for the user to quit it, or it
# would delete the files the restarted application is in the middle of reading.
if [ -n "$replaced" ]; then
  while pgrep -f "$work/DazzleDemo/DazzleDemo" > /dev/null 2>&1; do
    sleep 0.5
  done
fi

if [ -n "$replaced" ]; then
  echo "The update went through: the application directory was replaced in place and now reports version $(cat "$work/DazzleDemo/version")."
else
  echo "The update was not installed: the application directory is unchanged."
fi
if [ -s "$work/app.log" ]; then
  echo
  echo "The application printed:"
  sed 's/^/  /' "$work/app.log"
fi
