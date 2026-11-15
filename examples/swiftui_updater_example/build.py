#!/usr/bin/env python3
"""Build a self-contained, ad-hoc signed macOS application bundle."""

import plistlib
import shutil
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
BINDING = ROOT / "bindings/swift"


def run(*arguments):
    subprocess.run(arguments, cwd=HERE, check=True)


def main():
    run(str(BINDING / "build-native.sh"), "-DDAZZLE_WITH_UPDATER_UI=ON")
    run("swift", "build", "-c", "release")
    binary_dir = Path(subprocess.check_output(
        ["swift", "build", "-c", "release", "--show-bin-path"], cwd=HERE, text=True).strip())
    app = HERE / "build/DazzleSwiftUI.app"
    if app.exists():
        shutil.rmtree(app)
    executable = app / "Contents/MacOS/SwiftUIUpdaterExample"
    frameworks = app / "Contents/Frameworks"
    executable.parent.mkdir(parents=True)
    frameworks.mkdir(parents=True)
    shutil.copy2(binary_dir / "SwiftUIUpdaterExample", executable)
    shutil.copy2(BINDING / "build/lib/libdazzle_updater.dylib", frameworks)
    # The distributed app must load its embedded library, even on this workstation.
    run("install_name_tool", "-delete_rpath", str(BINDING / "build/lib"), str(executable))
    with (app / "Contents/Info.plist").open("wb") as output:
        plistlib.dump({
            "CFBundleIdentifier": "com.example.dazzle-swiftui",
            "CFBundleName": "Dazzle SwiftUI",
            "CFBundleDisplayName": "Dazzle SwiftUI",
            "CFBundleExecutable": executable.name,
            "CFBundlePackageType": "APPL",
            "CFBundleShortVersionString": "1.0.0",
            "CFBundleVersion": "1.0.0",
            "LSMinimumSystemVersion": "13.0",
            "NSHighResolutionCapable": True,
            "NSAppTransportSecurity": {"NSAllowsLocalNetworking": True},
        }, output)
    run("codesign", "--force", "--deep", "--sign", "-", str(app))
    run("codesign", "--verify", "--deep", "--strict", str(app))
    print(f"Application: {app}", flush=True)


if __name__ == "__main__":
    main()
