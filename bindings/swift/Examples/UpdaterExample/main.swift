import DazzleUpdater

let source = ReleaseIndexSource.shared
source.url = "https://example.com/myapp/releases.json"

let updater = Updater.shared
updater.currentVersion = "1.4.0"
updater.publicKey = "11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo="

print("Source:", updater.source ?? "")
print("Release index:", source.url ?? "")
print("Current version:", updater.currentVersion ?? "")
print("Public key:", updater.publicKey ?? "")
