// swift-tools-version: 6.0

import Foundation
import PackageDescription

// Build this shared library with ./build-native.sh before invoking SwiftPM.
// An absolute path also works when this package is a local path dependency.
let nativeLibraryDirectory = URL(fileURLWithPath: #filePath)
    .deletingLastPathComponent().appendingPathComponent("build/lib").path

let package = Package(
    name: "DazzleUpdater",
    platforms: [.macOS(.v13)],
    products: [
        .library(name: "DazzleUpdater", targets: ["DazzleUpdater"]),
        .executable(name: "updater-example", targets: ["UpdaterExample"]),
    ],
    targets: [
        .systemLibrary(name: "CDazzleUpdater"),
        .target(
            name: "DazzleUpdater",
            dependencies: ["CDazzleUpdater"],
            linkerSettings: [
                .linkedLibrary("dazzle_updater"),
                .unsafeFlags(["-L", nativeLibraryDirectory]),
                .unsafeFlags(
                    ["-Xlinker", "-rpath", "-Xlinker", nativeLibraryDirectory],
                    .when(platforms: [.macOS, .linux])
                ),
            ]
        ),
        .executableTarget(
            name: "UpdaterExample", dependencies: ["DazzleUpdater"],
            path: "Examples/UpdaterExample"
        ),
        .testTarget(
            name: "DazzleUpdaterTests", dependencies: ["DazzleUpdater"],
            resources: [.copy("Fixtures")]
        ),
    ]
)
