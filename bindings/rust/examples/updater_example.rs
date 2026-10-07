use dazzle_updater::{ReleaseIndexSource, Updater};

fn main() {
    let updater = Updater::instance();
    let source = ReleaseIndexSource::instance();

    println!("source:          {}", updater.source().unwrap_or_default());
    println!(
        "current version: {}",
        updater.current_version().unwrap_or_default()
    );
    println!(
        "release index:   {}",
        source.url().unwrap_or_else(|| "(none)".to_string())
    );
    println!("public key set:  {}", updater.public_key().is_some());
}
