import 'package:dazzle_updater/dazzle_updater.dart';
import 'package:test/test.dart';

void main() {
  test('the source defaults to the release index and round-trips', () {
    final updater = Updater.instance;
    updater.source = null;
    expect(updater.source, 'release-index');

    updater.source = 'appcast';
    expect(updater.source, 'appcast');

    updater.source = null;
    expect(updater.source, 'release-index');
  });

  test('the release index url sets, clears and round-trips non-ASCII text', () {
    final source = ReleaseIndexSource.instance;
    source.url = 'https://example.com/myapp/releases.json';
    expect(source.url, 'https://example.com/myapp/releases.json');

    source.url = 'https://例子.example/更新.json';
    expect(source.url, 'https://例子.example/更新.json');

    source.url = null;
    expect(source.url, isNull);
  });

  test('the current version and the public key round-trip', () {
    final updater = Updater.instance;
    updater.currentVersion = '1.4.0';
    expect(updater.currentVersion, '1.4.0');

    updater.publicKey = '11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo=';
    expect(updater.publicKey, '11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo=');
  });

  test('the actions refuse to run out of order, with a stable code', () {
    final updater = Updater.instance;
    expect(updater.isUpdateAvailable, isFalse);

    expect(updater.downloadUpdate(), isFalse);
    expect(updater.errorCode, 'no-update');

    expect(updater.installUpdate(), isFalse);
    expect(updater.errorCode, 'no-update');
  });

  test('an unknown source and an empty url fail the check', () {
    final updater = Updater.instance;
    updater.source = 'nope';
    expect(updater.checkForUpdates(), isFalse);
    expect(updater.errorCode, 'unknown-source');

    updater.source = null;
    ReleaseIndexSource.instance.url = null;
    expect(updater.checkForUpdates(), isFalse);
    expect(updater.errorCode, 'invalid-index');
  });
}
