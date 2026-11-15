#!/bin/sh
# End-to-end install test on macOS.
#
# It builds two throwaway application bundles around the updater example, serves a
# release index over HTTP, checks that a wrong public key is rejected, and then lets the
# version 1.0.0 bundle install the signed version 2.0.0 update through dazzle_updater: the
# Ed25519 check, the detached install helper, the replacement of the bundle, and the
# relaunch.
#
# Usage: macos_install_e2e.sh <path to updater_example> <path to fixture_signer>
set -eu

example="${1:?usage: macos_install_e2e.sh <updater_example> <fixture_signer>}"
signer="${2:?usage: macos_install_e2e.sh <updater_example> <fixture_signer>}"

bundle_id="com.example.dazzle_updater-e2e"
work="$(mktemp -d "${TMPDIR:-/tmp}/dazzle_updater-e2e.XXXXXX")"
server_pid=""
cleanup() {
  if [ -n "$server_pid" ]; then
    kill "$server_pid" 2>/dev/null || true
  fi
  rm -rf "$work"
  rm -rf "$HOME/Library/Caches/$bundle_id"
}
trap cleanup EXIT INT TERM

fail() {
  echo "FAIL: $1" >&2
  exit 1
}

# make_bundle <path> <version>
make_bundle() {
  mkdir -p "$1/Contents/MacOS"
  cp "$example" "$1/Contents/MacOS/dazzle_updater-e2e"
  cat > "$1/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleIdentifier</key><string>$bundle_id</string>
  <key>CFBundleName</key><string>DazzleUpdaterE2E</string>
  <key>CFBundleExecutable</key><string>dazzle_updater-e2e</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>$2</string>
  <key>CFBundleVersion</key><string>$2</string>
</dict>
</plist>
PLIST
}

make_bundle "$work/DazzleUpdaterE2E.app" "1.0.0"
make_bundle "$work/staging/DazzleUpdaterE2E.app" "2.0.0"
echo "shipped in 2.0.0" > "$work/staging/DazzleUpdaterE2E.app/Contents/version-marker"

# Package the new version the way a release ships it.
ditto -c -k --sequesterRsrc --keepParent "$work/staging/DazzleUpdaterE2E.app" "$work/DazzleUpdaterE2E-2.0.0.zip"

key_output="$("$signer" gen-key)"
public_key="$(printf '%s\n' "$key_output" | sed -n 's/^public //p')"
secret_key="$(printf '%s\n' "$key_output" | sed -n 's/^secret //p')"
other_public_key="$("$signer" gen-key | sed -n 's/^public //p')"
[ -n "$public_key" ] || fail "the fixture signer produced no public key"
[ -n "$secret_key" ] || fail "the fixture signer produced no secret key"

signature="$("$signer" sign "$secret_key" "$work/DazzleUpdaterE2E-2.0.0.zip")"
[ -n "$signature" ] || fail "the fixture signer produced no signature"
size="$(wc -c < "$work/DazzleUpdaterE2E-2.0.0.zip" | tr -d ' ')"
arch="$(uname -m | sed 's/x86_64/x64/')"

port="$(python3 -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1]); s.close()')"

cat > "$work/releases.json" <<JSON
{
  "schema": 1,
  "releases": [
    {
      "version": "2.0.0",
      "displayVersion": "2.0",
      "releaseNotes": "## End to end",
      "assets": [
        {
          "platform": "macos",
          "arch": "$arch",
          "url": "http://127.0.0.1:$port/DazzleUpdaterE2E-2.0.0.zip",
          "size": $size,
          "format": "zip",
          "signature": "$signature"
        }
      ]
    }
  ]
}
JSON

(cd "$work" && exec python3 -m http.server "$port" --bind 127.0.0.1) > "$work/server.log" 2>&1 &
server_pid=$!
ready=""
for _ in $(seq 1 50); do
  if curl -fsS "http://127.0.0.1:$port/releases.json" > /dev/null 2>&1; then
    ready=yes
    break
  fi
  sleep 0.1
done
[ -n "$ready" ] || fail "the local release index server did not start"

run_app() {
  # $1 = public key; prints the app's output, returns its exit status.
  "$work/DazzleUpdaterE2E.app/Contents/MacOS/dazzle_updater-e2e" \
    "http://127.0.0.1:$port/releases.json" "$1" "" --install 2>&1
}

# The bundle's own CFBundleShortVersionString is the current version: nothing is passed in.
if wrong_output="$(run_app "$other_public_key")"; then
  printf '%s\n' "$wrong_output"
  fail "a package signed by another key was accepted"
fi
printf '%s\n' "$wrong_output" | grep -q "signature" || {
  printf '%s\n' "$wrong_output"
  fail "the wrong key did not fail with the signature error code"
}

if ! output="$(run_app "$public_key")"; then
  printf '%s\n' "$output"
  fail "the update run failed"
fi
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -q "update:          2.0" || fail "the update was not reported"
printf '%s\n' "$output" | grep -q "downloaded and signature verified" ||
  fail "the download was not verified"
printf '%s\n' "$output" | grep -q "installer started" || fail "the installer was not started"

# The helper waits for the process to exit, replaces the bundle, and relaunches it.
installed=""
for _ in $(seq 1 150); do
  if [ -f "$work/DazzleUpdaterE2E.app/Contents/version-marker" ]; then
    installed=yes
    break
  fi
  sleep 0.2
done
[ -n "$installed" ] || fail "the application bundle was never replaced"

version="$(plutil -extract CFBundleShortVersionString raw -o - "$work/DazzleUpdaterE2E.app/Contents/Info.plist")"
[ "$version" = "2.0.0" ] || fail "the installed bundle reports version $version"

echo "PASS: 1.0.0 installed the signed 2.0.0 update and was replaced in place"
