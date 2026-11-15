import Testing
@testable import UpdaterDemo

@MainActor
private final class StubUI: UpdaterUIClient {
    var isVisible = false
    var available = true
    var configurations: [Configuration] = []
    var checks: [Bool] = []
    var resets = 0
    func configure(_ configuration: Configuration) { configurations.append(configuration) }
    func check(background: Bool) -> Bool {
        checks.append(background)
        isVisible = available && !background
        return available
    }
    func resetPreferences() { resets += 1 }
}

@MainActor
struct UpdaterModelTests {
    private let configuration = Configuration(feed: "https://example.com/releases.json", version: "1.0.0", publicKey: "key")

    @Test func foregroundUsesNativeUI() {
        let client = StubUI()
        let model = UpdaterModel(configuration: configuration, client: client)
        model.check()
        #expect(client.configurations == [configuration])
        #expect(client.checks == [false] && model.isVisible)
        model.edit(\.publicKey, "changed")
        model.check()
        #expect(model.configuration == configuration)
        #expect(client.configurations.count == 1)
        #expect(client.checks == [false, false])
    }

    @Test func backgroundUsesNativePermissionFlow() {
        let client = StubUI()
        let model = UpdaterModel(configuration: configuration, client: client)
        model.check(background: true)
        #expect(client.checks == [true])
        #expect(client.configurations == [configuration])
    }

    @Test func closedWindowAllowsConfigurationAndReset() {
        let client = StubUI()
        let model = UpdaterModel(configuration: configuration, client: client)
        model.check()
        model.resetPreferences()
        #expect(client.resets == 0)
        client.isVisible = false
        model.refresh()
        model.edit(\.version, "2.0.0")
        model.resetPreferences()
        #expect(!model.isVisible && model.configuration.version == "2.0.0")
        #expect(client.resets == 1)
    }

    @Test func unavailableUIIsReported() {
        let client = StubUI()
        client.available = false
        let model = UpdaterModel(configuration: configuration, client: client)
        model.check()
        #expect(!model.isVisible)
        #expect(model.status.contains("unavailable"))
    }
}
