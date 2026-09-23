import 'package:dazzle/dazzle.dart';

void main() {
  final updater = Updater.instance;

  print('Initial feed URL: ${updater.feedUrl ?? '(none)'}');

  updater.feedUrl = 'https://example.com/appcast.xml';
  print('Feed URL:         ${updater.feedUrl ?? '(none)'}');
}
