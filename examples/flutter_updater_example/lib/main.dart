import 'dart:async';
import 'dart:isolate';

import 'package:dazzle/dazzle.dart';
import 'package:flutter/material.dart';

// The whole update flow in one screen: point dazzle at a release index, embed the public
// key your installers are signed with, then check, download and install.
//
// The three actions block, so they run in a worker isolate (Isolate.run) and the UI
// isolate only reads the result afterwards. Progress is read from the UI isolate while
// the download runs, which the API allows.
void main() {
  runApp(const UpdaterExampleApp());
}

class UpdaterExampleApp extends StatelessWidget {
  const UpdaterExampleApp({super.key});

  @override
  Widget build(BuildContext context) => MaterialApp(
    title: 'dazzle',
    theme: ThemeData(colorSchemeSeed: Colors.indigo),
    home: const UpdaterPage(),
  );
}

class UpdaterPage extends StatefulWidget {
  const UpdaterPage({super.key});

  @override
  State<UpdaterPage> createState() => _UpdaterPageState();
}

class _UpdaterPageState extends State<UpdaterPage> {
  final _urlController = TextEditingController(
    text: 'https://example.com/myapp/releases.json',
  );
  final _keyController = TextEditingController();
  String _status = 'Idle';
  bool _busy = false;
  bool _available = false;
  bool _downloaded = false;
  double? _progress;
  Timer? _progressTimer;

  @override
  void dispose() {
    _progressTimer?.cancel();
    _urlController.dispose();
    _keyController.dispose();
    super.dispose();
  }

  void _finish(String message) {
    if (!mounted) {
      return;
    }
    setState(() {
      _status = message;
      _busy = false;
      _progress = null;
      _available = Updater.instance.isUpdateAvailable;
      _downloaded = Updater.instance.isUpdateDownloaded;
    });
  }

  Future<void> _check() async {
    final url = _urlController.text;
    final key = _keyController.text;
    setState(() {
      _busy = true;
      _status = 'Checking…';
    });
    try {
      final message = await Isolate.run(() {
        ReleaseIndexSource.instance.url = url;
        final updater = Updater.instance;
        updater.publicKey = key;
        if (!updater.checkForUpdates()) {
          return 'Check failed: ${updater.errorCode}: ${updater.errorMessage}';
        }
        if (!updater.isUpdateAvailable) {
          return 'Up to date';
        }
        return 'Update ${updater.updateDisplayVersion} is available';
      });
      _finish(message);
    } catch (error) {
      _finish('Check failed: $error');
    }
  }

  Future<void> _download() async {
    final url = _urlController.text;
    final key = _keyController.text;
    setState(() {
      _busy = true;
      _status = 'Downloading…';
    });
    _progressTimer = Timer.periodic(const Duration(milliseconds: 200), (_) {
      final value = Updater.instance.downloadProgress;
      if (mounted) {
        setState(() => _progress = value >= 0 ? value : null);
      }
    });
    try {
      final message = await Isolate.run(() {
        ReleaseIndexSource.instance.url = url;
        final updater = Updater.instance;
        updater.publicKey = key;
        if (!updater.downloadUpdate()) {
          return 'Download failed: ${updater.errorCode}: ${updater.errorMessage}';
        }
        return 'Downloaded and signature verified';
      });
      _finish(message);
    } catch (error) {
      _finish('Download failed: $error');
    } finally {
      _progressTimer?.cancel();
      _progressTimer = null;
    }
  }

  Future<void> _install() async {
    setState(() {
      _busy = true;
      _status = 'Installing…';
    });
    try {
      final message = await Isolate.run(() {
        final updater = Updater.instance;
        if (!updater.installUpdate()) {
          return 'Install failed: ${updater.errorCode}: ${updater.errorMessage}';
        }
        return 'Installer started. Quit the app so it can replace and restart it.';
      });
      _finish(message);
    } catch (error) {
      _finish('Install failed: $error');
    }
  }

  @override
  Widget build(BuildContext context) {
    final textTheme = Theme.of(context).textTheme;
    return Scaffold(
      appBar: AppBar(title: const Text('dazzle')),
      body: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Release index', style: textTheme.titleMedium),
            const SizedBox(height: 8),
            TextField(
              controller: _urlController,
              decoration: const InputDecoration(
                labelText: 'Release index URL',
                border: OutlineInputBorder(),
              ),
            ),
            const SizedBox(height: 16),
            Text('Signing key', style: textTheme.titleMedium),
            const SizedBox(height: 8),
            TextField(
              controller: _keyController,
              decoration: const InputDecoration(
                labelText: 'Ed25519 public key (Base64)',
                border: OutlineInputBorder(),
              ),
            ),
            const SizedBox(height: 24),
            Row(
              children: [
                FilledButton(
                  onPressed: _busy ? null : _check,
                  child: const Text('Check for updates'),
                ),
                const SizedBox(width: 12),
                FilledButton.tonal(
                  onPressed: _busy || !_available || _downloaded
                      ? null
                      : _download,
                  child: const Text('Download'),
                ),
                const SizedBox(width: 12),
                FilledButton.tonal(
                  onPressed: _busy || !_downloaded ? null : _install,
                  child: const Text('Install and restart'),
                ),
              ],
            ),
            const SizedBox(height: 12),
            if (_progress != null)
              LinearProgressIndicator(value: _progress)
            else if (_busy)
              const LinearProgressIndicator(),
            const SizedBox(height: 16),
            SelectableText(
              _status,
              key: const Key('status'),
              style: textTheme.bodyLarge,
            ),
          ],
        ),
      ),
    );
  }
}
