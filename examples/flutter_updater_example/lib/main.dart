import 'package:dazzle/dazzle.dart';
import 'package:flutter/material.dart';

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
  final _updater = Updater.instance;
  final _controller = TextEditingController();
  late String? _feedUrl = _updater.feedUrl;

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  void _apply() {
    _updater.feedUrl = _controller.text;
    setState(() => _feedUrl = _updater.feedUrl);
  }

  void _clear() {
    _updater.feedUrl = null;
    _controller.clear();
    setState(() => _feedUrl = _updater.feedUrl);
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('dazzle')),
      body: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'Appcast feed URL',
              style: Theme.of(context).textTheme.titleMedium,
            ),
            const SizedBox(height: 8),
            SelectableText(
              _feedUrl ?? '(not set)',
              key: const Key('feedUrl'),
              style: Theme.of(context).textTheme.bodyLarge,
            ),
            const SizedBox(height: 24),
            TextField(
              controller: _controller,
              onSubmitted: (_) => _apply(),
              decoration: const InputDecoration(
                labelText: 'Feed URL',
                hintText: 'https://example.com/appcast.xml',
                border: OutlineInputBorder(),
              ),
            ),
            const SizedBox(height: 12),
            Row(
              children: [
                FilledButton(onPressed: _apply, child: const Text('Apply')),
                const SizedBox(width: 12),
                OutlinedButton(onPressed: _clear, child: const Text('Clear')),
              ],
            ),
          ],
        ),
      ),
    );
  }
}
