import 'package:dazzle/dazzle.dart';
import 'package:test/test.dart';

void main() {
  test('feedUrl sets and clears', () {
    final updater = Updater.instance;
    updater.feedUrl = 'https://example.com/appcast.xml';
    expect(updater.feedUrl, 'https://example.com/appcast.xml');

    updater.feedUrl = null;
    expect(updater.feedUrl, isNull);
  });

  test('feedUrl round-trips non-ASCII text', () {
    final updater = Updater.instance;
    updater.feedUrl = 'https://例子.example/更新.xml';
    expect(updater.feedUrl, 'https://例子.example/更新.xml');
    updater.feedUrl = null;
  });
}
