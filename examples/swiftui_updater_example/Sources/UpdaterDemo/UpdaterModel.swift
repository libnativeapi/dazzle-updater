import Combine
import DazzleUpdater
import Foundation

public struct Configuration: Equatable, Sendable {
    public var feed: String
    public var version: String
    public var publicKey: String

    public init(feed: String, version: String, publicKey: String) {
        self.feed = feed
        self.version = version
        self.publicKey = publicKey
    }

    public static func launch(bundle: Bundle = .main, environment: [String: String] = ProcessInfo.processInfo.environment) -> Self {
        func value(_ environmentKey: String, _ bundleKey: String, _ fallback: String = "") -> String {
            environment[environmentKey] ?? bundle.object(forInfoDictionaryKey: bundleKey) as? String ?? fallback
        }
        return Self(
            feed: value("DAZZLE_DEMO_FEED", "DazzleUpdaterDemoFeedURL", "http://127.0.0.1:8765/releases.json"),
            version: value("DAZZLE_DEMO_VERSION", "CFBundleShortVersionString", "1.0.0"),
            publicKey: value("DAZZLE_DEMO_PUBLIC_KEY", "DazzleUpdaterDemoPublicKey")
        )
    }
}

@MainActor
public protocol UpdaterUIClient {
    var isVisible: Bool { get }
    func configure(_ configuration: Configuration)
    func check(background: Bool) -> Bool
    func resetPreferences()
}

@MainActor
public struct NativeUpdaterUIClient: UpdaterUIClient {
    public init() {}
    public var isVisible: Bool { UpdaterUI.shared.isVisible }

    public func configure(_ configuration: Configuration) {
        ReleaseIndexSource.shared.url = configuration.feed.trimmingCharacters(in: .whitespacesAndNewlines)
        Updater.shared.source = "release-index"
        Updater.shared.currentVersion = configuration.version.trimmingCharacters(in: .whitespacesAndNewlines)
        Updater.shared.publicKey = configuration.publicKey.trimmingCharacters(in: .whitespacesAndNewlines)
        UpdaterUI.shared.appName = "DazzleUpdater SwiftUI"
    }

    public func check(background: Bool) -> Bool {
        background ? UpdaterUI.shared.checkForUpdatesInBackground() : UpdaterUI.shared.checkForUpdates()
    }

    public func resetPreferences() { UpdaterUI.shared.resetPreferences() }
}

/// The SwiftUI host only configures and opens the core's native UpdaterUI.
/// It never runs Updater's check/download/install actions alongside that module.
@MainActor
public final class UpdaterModel: ObservableObject {
    @Published public private(set) var configuration: Configuration
    @Published public private(set) var isVisible = false
    @Published public private(set) var status = "Ready to open the native update window."
    private let client: any UpdaterUIClient

    public init(configuration: Configuration = .launch(), client: (any UpdaterUIClient)? = nil) {
        self.configuration = configuration
        self.client = client ?? NativeUpdaterUIClient()
    }

    public func refresh() { isVisible = client.isVisible }

    public func edit(_ field: WritableKeyPath<Configuration, String>, _ value: String) {
        guard !client.isVisible else { return }
        configuration[keyPath: field] = value
        status = "Configuration updated. Check for updates to use it."
    }

    public func check(background: Bool = false) {
        // A foreground call while visible only brings the existing native window forward.
        if !client.isVisible { client.configure(configuration) }
        let started = client.check(background: background)
        refresh()
        status = started
            ? (background ? "Background check requested. UpdaterUI manages permission and update windows."
                          : "The native update window manages checking, download and installation.")
            : "Native update windows are unavailable. Rebuild with DAZZLE_WITH_UPDATER_UI=ON."
    }

    public func resetPreferences() {
        guard !client.isVisible else { return }
        client.resetPreferences()
        status = "Update preferences reset. A background check will ask for permission again."
    }
}
