import 'dazzle_bindings.dart';
import 'ffi_string.dart';

/// The application's software updater.
///
/// Checks an appcast feed in the Sparkle format and installs the updates it
/// announces. One instance serves the whole process; get it with
/// [Updater.instance].
final class Updater {
  Updater._();

  /// The process-wide updater.
  static final Updater instance = Updater._();

  /// The URL of the appcast feed, or `null` if none is configured.
  ///
  /// Setting it overrides the app's configuration; set `null` or `''` to go
  /// back to the URL the configuration provides (Info.plist `SUFeedURL` on
  /// macOS, the `FeedURL` resource of type `APPCAST` on Windows).
  String? get feedUrl => takeCString(dazzle_updater_get_feed_url());

  set feedUrl(String? value) =>
      withCString(value ?? '', dazzle_updater_set_feed_url);
}
