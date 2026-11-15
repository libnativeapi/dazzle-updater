#!/usr/bin/env python3
"""Serve a signed replacement bundle; keep the server alive across app relaunches."""

import argparse
import importlib.util
import json
import os
import platform
import plistlib
import shutil
import signal
import subprocess
import tempfile
import threading
import time
import uuid
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
spec = importlib.util.spec_from_file_location("dazzle_test_server", ROOT / "tools/test_server/server.py")
server_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server_module)


def configure(app, **values):
    path = app / "Contents/Info.plist"
    with path.open("rb") as source:
        document = plistlib.load(source)
    document.update(values)
    with path.open("wb") as output:
        plistlib.dump(document, output)
    subprocess.run(["codesign", "--force", "--deep", "--sign", "-", str(app)], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--smoke", action="store_true", help="open native windows and verify installation after you click through them")
    parser.add_argument("--scenario", choices=server_module.SCENARIOS, default="update")
    args = parser.parse_args()
    app = HERE / "build/DazzleUpdaterSwiftUI.app"
    signer = ROOT / "core/build/tests/fixture_signer"
    if not app.is_dir() or not signer.is_file():
        parser.error("run make swiftui and build core's fixture_signer first (or use make swiftui-demo)")

    with tempfile.TemporaryDirectory(prefix="dazzle-swiftui-demo-") as temporary:
        work = Path(temporary)
        identifier = "com.example.dazzle-swiftui-demo." + uuid.uuid4().hex
        server = server_module.TestServer(("127.0.0.1", 0), server_module.Fixture({}, {}), args.scenario)
        running_app = work / "DazzleUpdaterSwiftUI.app"
        report = work / "smoke.json"
        try:
            keys = dict(line.split(" ", 1) for line in subprocess.check_output(
                [str(signer), "gen-key"], text=True).splitlines())
            shutil.copytree(app, running_app)
            settings = {
                "CFBundleIdentifier": identifier,
                "DazzleUpdaterDemoFeedURL": server.base_url + "/releases.json",
                "DazzleUpdaterDemoPublicKey": keys["public"],
                "DazzleUpdaterDemoAutoCheck": True,
            }
            if args.smoke:
                settings["DazzleUpdaterDemoSmokeReport"] = str(report)
            configure(running_app, **settings)
            replacement = work / "staging/DazzleUpdaterSwiftUI.app"
            shutil.copytree(running_app, replacement)
            configure(replacement, CFBundleShortVersionString="2.0.0", CFBundleVersion="2.0.0")
            package = work / "DazzleUpdaterSwiftUI-2.0.0.zip"
            subprocess.run(["ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", str(replacement), str(package)], check=True)
            signature = subprocess.check_output([str(signer), "sign", keys["secret"], str(package)], text=True).strip()
            feed = work / "releases.json"
            feed.write_text(json.dumps({"schema": 1, "releases": [{
                "version": "2.0.0", "displayVersion": "2.0.0",
                "releaseNotes": "## What's new in 2.0\n\n- **Native UpdaterUI windows** hosted by a SwiftUI application.\n"
                                "- Download progress, cancellation and signature verification.\n"
                                "- Install and relaunch into version **2.0.0**.\n\n"
                                "### 更新说明\n\nThis signed package contains the complete application.",
                "assets": [{"platform": "macos", "arch": {"x86_64": "x64"}.get(platform.machine(), platform.machine()), "format": "zip",
                            "url": server.base_url + "/" + package.name,
                            "size": package.stat().st_size, "signature": signature}],
            }]}), encoding="utf-8")
            server.fixture = server_module.Fixture.from_feed(feed, keys["public"])
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            print(f"Application: {running_app}\nTest console: {server.base_url}/\n"
                  f"Release index: {settings['DazzleUpdaterDemoFeedURL']}\nPublic key: {keys['public']}", flush=True)
            # Launch the executable directly so shell environment overrides cannot
            # replace the signed demo's feed, version or key. Relaunch uses LaunchServices.
            environment = {key: value for key, value in os.environ.items() if not key.startswith("DAZZLE_DEMO_")}
            with (work / "app.log").open("w") as log:
                process = subprocess.Popen([str(running_app / "Contents/MacOS/SwiftUIUpdaterExample")],
                                           env=environment, stdout=log, stderr=log)
                if args.smoke:
                    print("Click Install Update, then Install and Relaunch in the native window.", flush=True)
                    deadline = time.monotonic() + 180
                    while time.monotonic() < deadline:
                        process.poll()  # Reap 1.0.0 so the installer can observe its exit.
                        if report.exists():
                            state = json.loads(report.read_text())
                            if state["phase"] == "failed":
                                raise RuntimeError(state["detail"])
                            if state["phase"] == "passed":
                                with (running_app / "Contents/Info.plist").open("rb") as source:
                                    installed_version = plistlib.load(source)["CFBundleShortVersionString"]
                                if installed_version != "2.0.0" or state["pid"] == process.pid:
                                    raise RuntimeError("the replacement did not relaunch as a new 2.0.0 process")
                                print("PASS: native UpdaterUI installed the signed replacement and relaunched 2.0.0", flush=True)
                                return
                        time.sleep(0.1)
                    raise RuntimeError("smoke test timed out\n" + (work / "app.log").read_text())
                print("Click Install Update, then Install and Relaunch in the native window. The new host reports v2.0.0.\n"
                      "Keep this terminal open across relaunches. Press Ctrl-C to stop and clean up.", flush=True)
                while True:
                    process.poll()
                    time.sleep(0.5)
        finally:
            # Match only this throwaway bundle, including a replacement launched by the installer.
            executable = str(running_app / "Contents/MacOS/SwiftUIUpdaterExample")
            rows = subprocess.check_output(["ps", "-axo", "pid=,command="], text=True).splitlines()
            for row in rows:
                parts = row.strip().split(None, 1)
                if len(parts) == 2 and Path(parts[1]).resolve() == Path(executable).resolve():
                    try:
                        os.kill(int(parts[0]), signal.SIGTERM)
                    except ProcessLookupError:
                        pass
            if "thread" in locals():
                server.shutdown()
                thread.join()
            server.server_close()
            if "process" in locals():
                process.wait(timeout=10)
            shutil.rmtree(Path.home() / "Library/Caches" / identifier, ignore_errors=True)
            subprocess.run(["defaults", "delete", identifier], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
