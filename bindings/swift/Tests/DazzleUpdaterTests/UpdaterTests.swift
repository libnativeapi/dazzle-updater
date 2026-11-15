import DazzleUpdater
import Foundation
import XCTest

// Native state is process-wide. XCTest runs these methods serially within a process.
final class UpdaterTests: XCTestCase {
    override func setUp() {
        super.setUp()
        Updater.shared.source = nil
        Updater.shared.currentVersion = "1.0.0"
        Updater.shared.publicKey = nil
        ReleaseIndexSource.shared.url = nil
        // A new check clears release and download state left by a previous test.
        _ = Updater.shared.checkForUpdates()
    }

    override func tearDown() {
        ReleaseIndexSource.shared.url = nil
        Updater.shared.source = nil
        _ = Updater.shared.checkForUpdates()
        Updater.shared.currentVersion = nil
        Updater.shared.publicKey = nil
        super.tearDown()
    }

    func testConfigurationAndStrings() {
        let updater = Updater.shared
        let source = ReleaseIndexSource.shared
        XCTAssertTrue(updater === Updater.shared)
        XCTAssertTrue(source === ReleaseIndexSource.shared)
        XCTAssertEqual(updater.source, "release-index")
        updater.source = "missing-source"
        XCTAssertEqual(updater.source, "missing-source")
        updater.source = nil
        XCTAssertEqual(updater.source, "release-index")

        for value in ["https://example.com/releases.json", "https://例子.example/更新.json", ""] {
            source.url = value
            XCTAssertEqual(source.url, value.isEmpty ? nil : value)
        }
        source.url = "https://example.com/index.json\0ignored"
        XCTAssertEqual(source.url, "https://example.com/index.json")
        source.url = nil
        XCTAssertNil(source.url)

        updater.currentVersion = "1.4.0"
        XCTAssertEqual(updater.currentVersion, "1.4.0")
        updater.publicKey = "PUAXw+hDiVqStwqnTRt+vJyYLM8uxJaMwM1V8Sr0Zgw="
        XCTAssertEqual(updater.publicKey, "PUAXw+hDiVqStwqnTRt+vJyYLM8uxJaMwM1V8Sr0Zgw=")
        updater.publicKey = nil
        XCTAssertNil(updater.publicKey)
    }

    func testActionErrorsAndInitialState() throws {
        let updater = Updater.shared
        XCTAssertFalse(updater.isUpdateAvailable)
        XCTAssertFalse(updater.isUpdateDownloaded)
        XCTAssertNil(updater.updateVersion)
        XCTAssertEqual(updater.updateSize, 0)
        updater.cancelDownload()
        XCTAssertEqual(updater.downloadProgress, 0)
        XCTAssertFalse(updater.downloadUpdate())
        XCTAssertEqual(updater.errorCode, "no-update")
        XCTAssertFalse(updater.installUpdate())
        XCTAssertEqual(updater.errorCode, "no-update")

        updater.source = "missing-source"
        XCTAssertFalse(updater.checkForUpdates())
        if updater.errorCode == "unsupported-platform" {
            throw XCTSkip("The native build has no HTTP implementation")
        }
        XCTAssertEqual(updater.errorCode, "unknown-source")
        XCTAssertNotNil(updater.errorMessage)
        updater.source = nil
        XCTAssertFalse(updater.checkForUpdates())
        XCTAssertEqual(updater.errorCode, "invalid-index")
    }

    func testSignedDownloadAndReleaseMetadata() throws {
        #if os(macOS)
        let platform = "macos"
        #elseif os(Linux)
        let platform = "linux"
        #else
        throw XCTSkip("The loopback fixture runs on macOS and Linux")
        #endif
        #if os(macOS) || os(Linux)
        let fixture = try XCTUnwrap(Bundle.module.url(
            forResource: "server", withExtension: "py", subdirectory: "Fixtures"
        ))
        let server = Process()
        let output = Pipe()
        server.executableURL = URL(fileURLWithPath: "/usr/bin/env")
        server.arguments = ["python3", "-u", fixture.path, platform]
        server.standardOutput = output
        try server.run()
        defer {
            if server.isRunning { server.terminate() }
            server.waitUntilExit()
        }
        var portData = Data()
        while let byte = try output.fileHandleForReading.read(upToCount: 1), !byte.isEmpty {
            if byte[0] == 10 { break }
            portData.append(byte)
        }
        let port = try XCTUnwrap(Int(String(decoding: portData, as: UTF8.self)))
        let base = "http://127.0.0.1:\(port)"
        let updater = Updater.shared
        ReleaseIndexSource.shared.url = base + "/releases.json"
        updater.publicKey = "PUAXw+hDiVqStwqnTRt+vJyYLM8uxJaMwM1V8Sr0Zgw="
        let checked = updater.checkForUpdates()
        if updater.errorCode == "unsupported-platform" {
            throw XCTSkip("The native build has no HTTP implementation")
        }
        XCTAssertTrue(checked, updater.errorMessage ?? "check failed")
        XCTAssertTrue(updater.isUpdateAvailable)
        XCTAssertEqual(updater.updateVersion, "2.0.0")
        XCTAssertEqual(updater.updateDisplayVersion, "第二版")
        XCTAssertEqual(updater.updateReleaseNotes, "## 更新\n\nSwift 支持")
        XCTAssertEqual(updater.updateReleaseNotesURL, base + "/notes")
        XCTAssertEqual(updater.updateSize, (Int64(1) << 33) + 123)
        XCTAssertNil(updater.errorCode)
        XCTAssertNil(updater.errorMessage)
        XCTAssertTrue(updater.downloadUpdate(), updater.errorMessage ?? "download failed")
        XCTAssertTrue(updater.isUpdateDownloaded)
        XCTAssertEqual(updater.downloadProgress, 1)

        XCTAssertTrue(updater.checkForUpdates())
        XCTAssertFalse(updater.isUpdateDownloaded)
        updater.publicKey = "11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo="
        XCTAssertFalse(updater.downloadUpdate())
        XCTAssertEqual(updater.errorCode, "signature")
        XCTAssertFalse(updater.isUpdateDownloaded)

        updater.currentVersion = "2.0.0"
        XCTAssertTrue(updater.checkForUpdates())
        XCTAssertFalse(updater.isUpdateAvailable)
        XCTAssertNil(updater.updateVersion)
        XCTAssertEqual(updater.updateSize, 0)
        #endif
    }

    func testUIConfiguration() {
        let ui = UpdaterUI.shared
        XCTAssertTrue(ui === UpdaterUI.shared)
        let name = ui.appName
        let allowed = ui.isAutomaticDownloadsAllowed
        defer {
            ui.appName = name
            ui.isAutomaticDownloadsAllowed = allowed
        }
        // Only in-memory settings: do not write preferences or open native windows.
        ui.appName = "示例 Swift App"
        XCTAssertEqual(ui.appName, "示例 Swift App")
        XCTAssertFalse(ui.isVisible)
        ui.isAutomaticDownloadsAllowed = false
        XCTAssertFalse(ui.isAutomaticDownloadsAllowed)
        XCTAssertFalse(ui.isAutomaticDownloadsEnabled)
        ui.isAutomaticDownloadsAllowed = true
        XCTAssertTrue(ui.isAutomaticDownloadsAllowed)
    }

    func testConcurrentConfiguration() {
        // The stateless Swift wrappers are Sendable; the core synchronizes access.
        let source = ReleaseIndexSource.shared
        let updater = Updater.shared
        DispatchQueue.concurrentPerform(iterations: 8) { _ in
            for _ in 0..<100 {
                source.url = "https://例子.example/更新.json"
                XCTAssertEqual(source.url, "https://例子.example/更新.json")
                _ = updater.downloadProgress
            }
        }
    }
}
