# 更新测试服务器

所有示例共用的真实 HTTP release-index 服务器，使用 Python 3.9+ 标准库，无 pip 依赖。
提供浏览器控制台、21 个固定场景地址，以及运行中切换场景的 API。

## 快速开始

在仓库根目录运行（macOS / Linux，需要 CMake 和 C++ 编译器）：

```sh
make test-server
# 或指定端口、初始场景
make test-server PORT=8766 SCENARIO=slow-download
```

打开终端打印的控制台地址，默认 <http://127.0.0.1:8765/>。将控制台中的
Release index 地址、公钥填入示例，当前版本用 `1.0.0`，然后检查、下载。
默认启动时生成临时 Ed25519 密钥，并用 core 的 `fixture_signer` 对一个约 4 MB
的 zip 签名，实际下载和签名验证都经过 core。**这个默认 zip 是下载测试文件，
不包含可安装的应用。** 每次启动公钥不同，需要更新示例中的公钥。

服务器仅发布索引引用的安装包，不提供任意目录访问或私钥下载。Ctrl-C 停止服务器并清理临时文件。

## 完整安装与重启

UpdaterUI 和 SwiftUI demo 已接入此服务器，使用可替换、可重启的临时应用：

```sh
make demo                             # macOS
make demo SCENARIO=bad-signature       # macOS，从错误签名开始
make demo-linux SCENARIO=slow-download # Linux，需 GTK 4 和桌面会话
make swiftui-demo                     # macOS，SwiftUI 宿主 + UpdaterUI 原生窗口
```

demo 会打印自己的控制台地址，端口自动选择。在控制台切换场景后回到 demo 重新检查。
`update` 可走完 **Install Update → Install and Relaunch**，重启后版本为 `2.0.0`。
UpdaterUI 的 `Reset Demo` 清理跳过版本、自动检查等偏好并将当前版本恢复为 `1.0.0`，可重复测试。
SwiftUI demo 同样使用 UpdaterUI 原生窗口；终端按 Ctrl-C 清理，
重新运行 `make swiftui-demo` 即可从 `1.0.0` 再次测试。

也可以签名并发布自己的实际安装包：

```sh
python3 tools/test_server/server.py --package /path/to/MyApp-2.0.0.zip
```

macOS zip 应包含一个 `.app`，Linux zip 应包含一个应用目录，Windows 使用真实 exe/msi
安装程序。`--version` 必须与安装包内的版本一致；`--platform`、`--arch` 默认是服务器所在机器，
给其他平台提供包时需要显式指定。打包应用时携带正确的原生库与运行配置。

复用已有的签名索引时无需 signer（Windows 上也能运行）：

```sh
python3 tools/test_server/server.py \
  --feed /path/to/fixtures/releases.json --public-key '<Base64 public key>'
```

索引必须是有效的 schema 1 且至少包含一个 release 和一个 asset。安装包应位于索引目录中，
原 URL 的路径对应目录内的相对文件，例如 `http://old-host/packages/App.zip` 对应
`/path/to/fixtures/packages/App.zip`。服务器保留签名并重写下载 URL。

## 各示例接入

- **C++ 控制台**：`core/build/examples/updater_example/updater_example '<Release index URL>' '<公钥>' 1.0.0`。
  默认只检查、下载和校验；使用真实安装包及合适的应用目录后才加 `--install`。
- **MyGo**：在窗口填写配置，或 `go run . --feed '<URL>' --public-key '<公钥>' --version 1.0.0`
  （在 `examples/mygo_updater_example` 运行）。`slow-download` 可测试 Cancel 和重新下载。
- **Flutter**：`flutter run -d macos`，在窗口填写 URL 和公钥。macOS 的调试和发布配置都包含
  网络客户端权限。完整安装需要可分发应用及对应的真实更新包。
- **SwiftUI**：`make swiftui` 构建 macOS `.app`，在窗口填写 URL 和公钥；`make swiftui-demo`
  自动准备可安装的签名更新包，`make swiftui-smoke` 在手动点击原生窗口后验证实际替换与重启。
- **WinUI 3**：启动前设置 `DAZZLE_DEMO_FEED`、`DAZZLE_DEMO_PUBLIC_KEY`、
  `DAZZLE_DEMO_VERSION=1.0.0`，可再设置 `DAZZLE_DEMO_CHECK=1` 自动检查。

给其他机器（如 Windows 测试机）使用时，服务器绑定所有接口并显式设置客户端可访问的地址：

```sh
# 在已有 fixture_signer 的 macOS / Linux 机器上，为 Windows 发布 exe
python3 tools/test_server/server.py \
  --host 0.0.0.0 --port 8765 --base-url http://192.168.1.2:8765 \
  --platform windows --arch x64 --format exe --package /path/to/Setup.exe \
  --installer-arguments=/SILENT
```

## 场景与预期

动态地址 `/releases.json` 使用控制台当前选择；固定地址
`/scenarios/<场景>/releases.json` 始终使用指定场景，适合自动化。所有响应禁用缓存。

| 场景 | 预期 |
| --- | --- |
| `update` | 有更新，真实下载并校验签名；实际应用包可继续安装 |
| `no-update` | 空 releases，已是最新 |
| `older-version` | 只有 0.0.1，当前为 1.0.0 时已是最新 |
| `no-compatible-asset` | 无匹配平台，已是最新 |
| `minimum-system-version` | 最低系统 9999.0，本机版本已知时不适用 |
| `invalid-json` / `unsupported-schema` | 检查失败，`invalid-index` |
| `index-404` / `index-500` | 检查失败，`network` |
| `slow-index` | 延迟索引，观察 Checking 状态 |
| `download-404` / `download-500` | 发现更新，下载失败，`network` |
| `slow-download` | 默认约 8 秒，观察进度、取消和重试 |
| `unknown-length` | 同样慢速，但无 Content-Length，进度不确定 |
| `interrupted-download` | 声明完整长度，只发送一半，`network` |
| `bad-signature` | 签名替换为零值，完整下载后 `signature` |
| `tampered-package` | 文件改变一个字节，长度不变，下载后 `signature` |
| `redirect` | 索引、安装包都经 302，下载及校验成功 |
| `unsupported-format` | 下载校验成功，点击安装时 `installer` |
| `no-notes` / `long-notes` | 无说明 / 长 Markdown 说明，观察窗口布局 |

场景影响真实网络响应与文件内容，不模拟客户端状态。`slow-index` 默认延迟 3 秒，可用
`--index-delay 10` 调整；慢速下载用 `--download-seconds 20` 调整。检查结果中的下载地址绑定到
本次场景，因此切换后要**重新检查**才会使用新场景；进行中的下载不受影响。

跳过版本、自动检查和权限询问属于客户端行为：配合 `update`、demo 的 `Reset Demo`
及前台／后台检查测试。用服务器停止来测试连接失败；超时可增大 `--index-delay`，
具体超时行为由各平台 HTTP 实现决定。

## API 与验证

```sh
curl http://127.0.0.1:8765/api/state
curl -X POST http://127.0.0.1:8765/api/scenario \
  -H 'Content-Type: application/json' -d '{"scenario":"bad-signature"}'
make test-server-test
```

`/api/state` 包含当前场景、URL、公钥和场景清单。`--port 0 --ready-file /path/to/ready.json`
可供测试脚本自动选择端口，并在监听成功后输出同样的配置 JSON。

验证包含 HTTP 协议测试和全部 21 个场景对真实 C++ `updater_example` 的集成测试：成功路径
实际验签，失败路径校验 core 错误码，格式错误会实际调用安装 API。不会安装默认测试包。
仅运行 HTTP 测试可用 `python3 tools/test_server/test_server.py HttpTests`；
完整替换与重启还由现有 core 的 macOS / Linux install e2e 测试覆盖。
