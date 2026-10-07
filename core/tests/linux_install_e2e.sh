#!/bin/sh
# End-to-end install test on Linux.
#
# It builds two throwaway application directories around the updater example, serves a
# release index over HTTP, checks that a wrong public key is rejected, and then lets the
# version 1.0.0 directory install the signed version 2.0.0 update through dazzle: the
# Ed25519 check, the libcurl download, the detached install helper, the replacement of
# the directory, and the restart.
#
# Usage: linux_install_e2e.sh <path to updater_example> <path to fixture_signer>
set -eu

example="${1:?usage: linux_install_e2e.sh <updater_example> <fixture_signer>}"
signer="${2:?usage: linux_install_e2e.sh <updater_example> <fixture_signer>}"

work="$(mktemp -d "${TMPDIR:-/tmp}/dazzle-e2e.XXXXXX")"
cache="${XDG_CACHE_HOME:-$HOME/.cache}/dazzle"
server_pid=""
cleanup() {
  if [ -n "$server_pid" ]; then
    kill "$server_pid" 2>/dev/null || true
  fi
  rm -rf "$work"
  rm -rf "$cache"
}
trap cleanup EXIT INT TERM

fail() {
  echo "FAIL: $1" >&2
  exit 1
}

# make_app <directory> <version> [<marker text>]. The executable sits directly in the
# application directory, which is the directory dazzle replaces: it is the one holding
# the running executable.
make_app() {
  mkdir -p "$1"
  cp "$example" "$1/dazzle-e2e"
  printf '%s\n' "$2" > "$1/version"
  if [ -n "${3:-}" ]; then
    printf '%s\n' "$3" > "$1/version-marker"
  fi
}

make_app "$work/DazzleE2E" "1.0.0"
make_app "$work/staging/DazzleE2E" "2.0.0" "shipped in 2.0.0"

# Package the new version the way a release ships it: one top-level directory, the
# application's own name.
(cd "$work/staging" && zip -q -r "$work/DazzleE2E-2.0.0.zip" "DazzleE2E")

key_output="$("$signer" gen-key)"
public_key="$(printf '%s\n' "$key_output" | sed -n 's/^public //p')"
secret_key="$(printf '%s\n' "$key_output" | sed -n 's/^secret //p')"
other_public_key="$("$signer" gen-key | sed -n 's/^public //p')"
[ -n "$public_key" ] || fail "the fixture signer produced no public key"
[ -n "$secret_key" ] || fail "the fixture signer produced no secret key"

signature="$("$signer" sign "$secret_key" "$work/DazzleE2E-2.0.0.zip")"
[ -n "$signature" ] || fail "the fixture signer produced no signature"
size="$(wc -c < "$work/DazzleE2E-2.0.0.zip" | tr -d ' ')"
arch="$(uname -m | sed -e 's/x86_64/x64/' -e 's/aarch64/arm64/')"

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
          "platform": "linux",
          "arch": "$arch",
          "url": "http://127.0.0.1:$port/DazzleE2E-2.0.0.zip",
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
  # $1 = public key; prints the app's output, returns its exit status. The version is the
  # directory's own: an application that has not declared one is up to date with anything.
  "$work/DazzleE2E/dazzle-e2e" "http://127.0.0.1:$port/releases.json" "$1" "1.0.0" --install 2>&1
}

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

# The helper waits for the process to exit, replaces the directory, and starts it again.
installed=""
for _ in $(seq 1 150); do
  if [ -f "$work/DazzleE2E/version-marker" ]; then
    installed=yes
    break
  fi
  sleep 0.2
done
[ -n "$installed" ] || {
  printf '%s\n' "$output"
  fail "the application directory was not replaced"
}
[ -x "$work/DazzleE2E/dazzle-e2e" ] || fail "the replaced directory lost its executable"
echo "PASS: the update was downloaded, verified, installed and restarted"
