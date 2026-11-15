#!/usr/bin/env python3
"""Build a self-contained, ad-hoc signed macOS application bundle."""

import plistlib
import re
import shutil
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
BINDING = ROOT / "bindings/swift"
MINIMUM_MACOS = "13.0"  # Keep in sync with Package.swift's .macOS(.v13).


def run(*arguments):
    subprocess.run(arguments, cwd=HERE, check=True)


def verify_build_version(binary, sdk_version):
    """AppKit's appearance compatibility depends on the executable's linked SDK."""
    output = subprocess.check_output(["xcrun", "vtool", "-show-build", str(binary)], text=True)
    for field, expected in [("minos", MINIMUM_MACOS), ("sdk", sdk_version)]:
        versions = re.findall(rf"^\s*{field}\s+(\S+)\s*$", output, re.MULTILINE)
        # vtool can omit trailing zero components of the SDK version.
        normalized = lambda value: tuple((value.split(".") + ["0", "0"])[:3])
        if not versions or any(normalized(value) != normalized(expected) for value in versions):
            raise RuntimeError(f"Unexpected {field} in {binary}; expected {expected}:\n{output}")
    print(f"Verified {binary.name}: minimum macOS {MINIMUM_MACOS}, SDK {sdk_version}", flush=True)


def main():
    sdk_path = subprocess.check_output(
        ["xcrun", "--sdk", "macosx", "--show-sdk-path"], text=True).strip()
    sdk_version = subprocess.check_output(
        ["xcrun", "--sdk", "macosx", "--show-sdk-version"], text=True).strip()
    run(str(BINDING / "build-native.sh"), "-DDAZZLE_WITH_UPDATER_UI=ON",
        f"-DCMAKE_OSX_SYSROOT={sdk_path}", f"-DCMAKE_OSX_DEPLOYMENT_TARGET={MINIMUM_MACOS}")
    # Select the active Xcode SDK for compilation and explicitly preserve its version
    # when linking. SwiftPM's Swift Build backend can otherwise stamp the deployment
    # target as the SDK version, making AppKit use the older appearance even on macOS 27.
    build_command = ["swift", "build", "-c", "release", "--sdk", sdk_path,
                     "-Xlinker", "-platform_version", "-Xlinker", "macos",
                     "-Xlinker", MINIMUM_MACOS, "-Xlinker", sdk_version]
    run(*build_command)
    binary_dir = Path(subprocess.check_output(
        [*build_command, "--show-bin-path"], cwd=HERE, text=True).strip())
    verify_build_version(binary_dir / "SwiftUIUpdaterExample", sdk_version)
    verify_build_version(BINDING / "build/lib/libdazzle_updater.dylib", sdk_version)
    app = HERE / "build/DazzleUpdaterSwiftUI.app"
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
            "CFBundleName": "DazzleUpdater SwiftUI",
            "CFBundleDisplayName": "DazzleUpdater SwiftUI",
            "CFBundleExecutable": executable.name,
            "CFBundlePackageType": "APPL",
            "CFBundleShortVersionString": "1.0.0",
            "CFBundleVersion": "1.0.0",
            "LSMinimumSystemVersion": MINIMUM_MACOS,
            "NSHighResolutionCapable": True,
            "NSAppTransportSecurity": {"NSAllowsLocalNetworking": True},
        }, output)
    run("codesign", "--force", "--deep", "--sign", "-", str(app))
    run("codesign", "--verify", "--deep", "--strict", str(app))
    print(f"Application: {app}", flush=True)


if __name__ == "__main__":
    main()
