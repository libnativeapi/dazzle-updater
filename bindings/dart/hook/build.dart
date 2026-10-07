import 'dart:io';

import 'package:code_assets/code_assets.dart';
import 'package:hooks/hooks.dart';

/// Builds the dazzle_updater core library as a shared library with CMake and bundles
/// it as the code asset that `lib/src/dazzle_updater_bindings.dart` binds to.
void main(List<String> args) async {
  await build(args, (input, output) async {
    if (!input.config.buildCodeAssets) {
      return;
    }
    final code = input.config.code;
    final targetOS = code.targetOS;
    if (targetOS != OS.current) {
      throw UnsupportedError(
        'dazzle_updater: building for $targetOS from ${OS.current} is not supported yet.',
      );
    }

    // bindings/dart -> core/
    final coreDir = input.packageRoot.resolve('../../core/');
    final buildDir = input.outputDirectory.resolve('cmake/');

    await _run('cmake', [
      '-S',
      coreDir.toFilePath(),
      '-B',
      buildDir.toFilePath(),
      '-DDAZZLE_BUILD_SHARED=ON',
      '-DBUILD_TESTING=OFF',
      '-DCMAKE_BUILD_TYPE=Release',
      if (targetOS == OS.macOS)
        '-DCMAKE_OSX_ARCHITECTURES=${_appleArch(code.targetArchitecture)}',
    ]);
    await _run('cmake', [
      '--build',
      buildDir.toFilePath(),
      '--target',
      'dazzle_updater',
      '--config',
      'Release',
    ]);

    final fileName = targetOS.dylibFileName('dazzle_updater');
    final libDir = buildDir.resolve('src/');
    // Multi-config generators (Visual Studio) add a per-configuration directory.
    final library =
        [
          libDir.resolve(fileName),
          libDir.resolve('Release/$fileName'),
        ].firstWhere(
          (uri) => File.fromUri(uri).existsSync(),
          orElse: () => throw StateError('dazzle_updater: $fileName was not built.'),
        );

    output.assets.code.add(
      CodeAsset(
        package: input.packageName,
        name: 'src/dazzle_updater_bindings.dart',
        linkMode: DynamicLoadingBundled(),
        file: library,
      ),
    );
    // Every source file, not the directory: a directory's timestamp only changes when
    // entries are added or removed, so editing a header would reuse a stale library.
    output.dependencies.addAll(_coreSources(coreDir));
  });
}

Iterable<Uri> _coreSources(Uri coreDir) sync* {
  yield coreDir.resolve('CMakeLists.txt');
  for (final dir in ['src/', 'include/']) {
    for (final entity in Directory.fromUri(
      coreDir.resolve(dir),
    ).listSync(recursive: true)) {
      if (entity is File) {
        yield entity.uri;
      }
    }
  }
}

String _appleArch(Architecture architecture) => switch (architecture) {
  Architecture.arm64 => 'arm64',
  Architecture.x64 => 'x86_64',
  _ => throw UnsupportedError('dazzle_updater: unsupported architecture $architecture'),
};

Future<void> _run(String executable, List<String> arguments) async {
  final result = await Process.run(executable, arguments);
  if (result.exitCode != 0) {
    throw ProcessException(
      executable,
      arguments,
      '${result.stdout}\n${result.stderr}',
      result.exitCode,
    );
  }
}
