import 'package:dazzle/dazzle.dart';
import 'package:test/test.dart';

void main() {
  test('feedUrl overrides and resets', () {
    final updater = Updater.instance;
    updater.feedUrl = 'https://example.com/appcast.xml';
    expect(updater.feedUrl, 'https://example.com/appcast.xml');

    updater.feedUrl = null;
    // The Dart VM has no SUFeedURL in its Info.plist, so nothing is configured.
    expect(updater.feedUrl, isNull);
  });

  test('feedUrl round-trips non-ASCII text', () {
    final updater = Updater.instance;
    updater.feedUrl = 'https://例子.example/更新.xml';
    expect(updater.feedUrl, 'https://例子.example/更新.xml');
    updater.feedUrl = null;
  });
}
