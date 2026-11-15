// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "SwiftUIUpdaterExample",
    platforms: [.macOS(.v13)],
    products: [.executable(name: "SwiftUIUpdaterExample", targets: ["SwiftUIUpdaterExample"])],
    dependencies: [.package(path: "../../bindings/swift")],
    targets: [
        .target(name: "UpdaterDemo", dependencies: [.product(name: "DazzleUpdater", package: "swift")]),
        .executableTarget(
            name: "SwiftUIUpdaterExample", dependencies: ["UpdaterDemo"],
            linkerSettings: [.unsafeFlags(["-Xlinker", "-rpath", "-Xlinker", "@executable_path/../Frameworks"])]
        ),
        .testTarget(name: "UpdaterDemoTests", dependencies: ["UpdaterDemo"]),
    ]
)
