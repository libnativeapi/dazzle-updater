use crate::ffi;
use crate::ffi_string::{take_c_string, to_c_string};

/// The application's software updater.
///
/// Checks an appcast feed in the Sparkle format and installs the updates it
/// announces. One instance serves the whole process; get it with
/// [`Updater::instance`].
#[derive(Debug)]
pub struct Updater {
    _private: (),
}

static INSTANCE: Updater = Updater { _private: () };

impl Updater {
    /// The process-wide updater.
    pub fn instance() -> &'static Updater {
        &INSTANCE
    }

    /// Sets the URL of the appcast feed, overriding the app's configuration.
    ///
    /// Pass `""` to go back to the URL the app's configuration provides.
    pub fn set_feed_url(&self, feed_url: &str) {
        let feed_url = to_c_string(feed_url);
        unsafe { ffi::dazzle_updater_set_feed_url(feed_url.as_ptr()) }
    }

    /// The URL of the appcast feed, or `None` if none is configured.
    ///
    /// Returns the URL set with [`Updater::set_feed_url`], falling back to the
    /// app's configuration: Info.plist `SUFeedURL` on macOS, the `FeedURL`
    /// resource of type `APPCAST` on Windows.
    pub fn feed_url(&self) -> Option<String> {
        unsafe { take_c_string(ffi::dazzle_updater_get_feed_url()) }
    }
}

#[cfg(test)]
mod tests {
    use super::Updater;

    #[test]
    fn set_feed_url_overrides_and_resets() {
        let updater = Updater::instance();
        updater.set_feed_url("https://example.com/appcast.xml");
        assert_eq!(
            updater.feed_url().as_deref(),
            Some("https://example.com/appcast.xml")
        );
        updater.set_feed_url("");
        // The test binary has no Info.plist, so nothing is configured.
        assert_eq!(updater.feed_url(), None);
    }
}
