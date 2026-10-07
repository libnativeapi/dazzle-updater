use dazzle_updater::{ReleaseIndexSource, Updater};

// Updater and ReleaseIndexSource are process-wide and tests run in parallel, so every
// check that mutates them lives in this one test.
#[test]
fn configuration_round_trips_and_the_actions_keep_their_order() {
    let updater = Updater::instance();
    let source = ReleaseIndexSource::instance();

    // The source defaults to the release index.
    updater.set_source("");
    assert_eq!(updater.source().as_deref(), Some("release-index"));
    updater.set_source("appcast");
    assert_eq!(updater.source().as_deref(), Some("appcast"));
    updater.set_source("");
    assert_eq!(updater.source().as_deref(), Some("release-index"));

    // The address lives on the source, not on Updater.
    source.set_url("https://example.com/myapp/releases.json");
    assert_eq!(
        source.url().as_deref(),
        Some("https://example.com/myapp/releases.json")
    );
    source.set_url("https://例子.example/更新.json");
    assert_eq!(source.url().as_deref(), Some("https://例子.example/更新.json"));
    source.set_url("");
    assert_eq!(source.url(), None);

    updater.set_current_version("1.4.0");
    assert_eq!(updater.current_version().as_deref(), Some("1.4.0"));

    updater.set_public_key("11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo=");
    assert_eq!(
        updater.public_key().as_deref(),
        Some("11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo=")
    );

    // Nothing was checked yet, so the actions that need a result refuse to run.
    assert!(!updater.is_update_available());
    assert!(!updater.download_update());
    assert_eq!(updater.error_code().as_deref(), Some("no-update"));
    assert!(!updater.install_update());
    assert_eq!(updater.error_code().as_deref(), Some("no-update"));

    // A source that is not registered fails the check.
    updater.set_source("nope");
    assert!(!updater.check_for_updates());
    assert_eq!(updater.error_code().as_deref(), Some("unknown-source"));
    assert!(!updater.error_message().unwrap().is_empty());

    // With no address set, the registered source fails without touching the network.
    updater.set_source("");
    assert!(!updater.check_for_updates());
    assert_eq!(updater.error_code().as_deref(), Some("invalid-index"));
}
