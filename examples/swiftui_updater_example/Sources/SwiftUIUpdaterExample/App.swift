import AppKit
import SwiftUI
import UpdaterDemo

@main
struct SwiftUIUpdaterExample: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var delegate

    var body: some Scene {
        Window("DazzleUpdater · SwiftUI", id: "updater") {
            ContentView(model: delegate.model)
                .task { delegate.startDemoIfNeeded() }
        }
        .defaultSize(width: 760, height: 360)
        .windowResizability(.contentMinSize)
        .commands {
            CommandGroup(after: .appInfo) {
                Button("Check for Updates…") { delegate.model.check() }
            }
        }
    }
}

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate {
    let model = UpdaterModel()
    private var started = false

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApplication.shared.setActivationPolicy(.regular)
        NSApplication.shared.activate(ignoringOtherApps: true)
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }

    func startDemoIfNeeded() {
        guard !started else { return }
        started = true
        // The interactive smoke test observes the new process after UpdaterUI's
        // own installer has quit the app, replaced its bundle and relaunched it.
        if let report = Bundle.main.object(forInfoDictionaryKey: "DazzleUpdaterDemoSmokeReport") as? String {
            let version = Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? ""
            if let data = try? JSONSerialization.data(withJSONObject: [
                "phase": version == "2.0.0" ? "passed" : "running",
                "version": version,
                "pid": ProcessInfo.processInfo.processIdentifier,
            ]) {
                try? data.write(to: URL(fileURLWithPath: report), options: .atomic)
            }
        }
        if Bundle.main.object(forInfoDictionaryKey: "DazzleUpdaterDemoAutoCheck") as? Bool == true {
            model.check()
        }
    }
}
