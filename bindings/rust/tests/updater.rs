use dazzle::Updater;

// Updater is process-wide and tests run in parallel, so every check that
// mutates it lives in this one test.
#[test]
fn feed_url_sets_round_trips_and_clears() {
    let updater = Updater::instance();

    updater.set_feed_url("https://example.com/appcast.xml");
    assert_eq!(
        updater.feed_url().as_deref(),
        Some("https://example.com/appcast.xml")
    );

    updater.set_feed_url("https://例子.example/更新.xml");
    assert_eq!(
        updater.feed_url().as_deref(),
        Some("https://例子.example/更新.xml")
    );

    updater.set_feed_url("");
    assert_eq!(updater.feed_url(), None);
}
