import 'package:dazzle_updater/dazzle_updater.dart';

void main() {
  final updater = Updater.instance;
  final source = ReleaseIndexSource.instance;

  print('source:          ${updater.source ?? '(none)'}');
  print('current version: ${updater.currentVersion ?? '(none)'}');
  print('release index:   ${source.url ?? '(none)'}');
  print('public key set:  ${updater.publicKey != null}');
}
