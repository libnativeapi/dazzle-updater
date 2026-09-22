use dazzle::Updater;

fn main() {
    let updater = Updater::instance();

    let configured = updater.feed_url();
    println!(
        "Configured feed URL: {}",
        configured.as_deref().unwrap_or("(none)")
    );

    updater.set_feed_url("https://example.com/appcast.xml");
    println!(
        "Feed URL:            {}",
        updater.feed_url().as_deref().unwrap_or("(none)")
    );
}
