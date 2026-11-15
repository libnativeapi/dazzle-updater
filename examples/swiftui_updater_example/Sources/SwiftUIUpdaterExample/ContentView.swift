import SwiftUI
import UpdaterDemo

struct ContentView: View {
    @ObservedObject var model: UpdaterModel

    private func field(_ key: WritableKeyPath<Configuration, String>) -> Binding<String> {
        Binding(get: { model.configuration[keyPath: key] }, set: { model.edit(key, $0) })
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 20) {
            HStack(spacing: 14) {
                Image(systemName: "arrow.down.app.fill")
                    .font(.system(size: 42)).foregroundStyle(.tint)
                VStack(alignment: .leading, spacing: 4) {
                    Text("Dazzle SwiftUI").font(.largeTitle.bold())
                    Text("Native UpdaterUI example").foregroundStyle(.secondary)
                }
                Spacer()
                Text("v\(Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "1.0.0")")
                    .font(.headline.monospacedDigit()).padding(10)
                    .background(.quaternary, in: Capsule())
            }
            GroupBox("Update configuration") {
                Grid(alignment: .leading, horizontalSpacing: 16, verticalSpacing: 12) {
                    GridRow {
                        Text("Release index URL")
                        TextField("https://example.com/releases.json", text: field(\.feed))
                    }
                    GridRow {
                        Text("Current version")
                        TextField("1.0.0", text: field(\.version))
                    }
                    GridRow {
                        Text("Signing public key")
                        TextField("Base64 Ed25519 public key", text: field(\.publicKey))
                            .font(.system(.body, design: .monospaced))
                    }
                }
                .textFieldStyle(.roundedBorder).disabled(model.isVisible).padding(10)
            }
            Text(model.status).foregroundStyle(.secondary)
                .frame(maxWidth: .infinity, minHeight: 40, alignment: .leading)
            HStack {
                Button("Check in background") { model.check(background: true) }
                    .disabled(model.isVisible)
                Button("Reset preferences") { model.resetPreferences() }
                    .disabled(model.isVisible)
                Spacer()
                Button(model.isVisible ? "Show update window" : "Check for updates…") { model.check() }
                    .buttonStyle(.borderedProminent)
            }
        }
        .padding(24).frame(minWidth: 720, minHeight: 340)
        .task {
            while !Task.isCancelled {
                model.refresh()
                do { try await Task.sleep(for: .milliseconds(200)) } catch { return }
            }
        }
    }
}
