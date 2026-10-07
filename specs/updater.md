# Updater API

`Updater` 从数据源找到新版本，下载并校验，交给安装器替换应用。本文档规定它的
范围、结构、接口签名和行为；实现落地后，各方法的用法以 `core/src/*.h` 的 Doxygen 注释为准。

## 目标与边界

Updater 让一个 macOS 或 Windows 桌面应用完成完整的一轮更新：

```
查询数据源 → 发现新版本 → 展示版本与更新说明 → 下载（带进度、可取消）→ 校验签名 → 安装并重启
```

Updater 不带 UI。界面（提示框、进度条、"重启以更新"按钮）由应用用自己的 UI 框架画，
Updater 只提供状态和动作。不想自己画界面的应用可以用内置的 [UpdaterUI](updater-ui.md)。

**暂不包含**（后续再设计）：Sparkle appcast 兼容（独立模块，见下）、应用自定义
source、定时自动检查、跳过某个版本、channel、增量更新、分阶段发布、强制更新、事件回调、
移动端的下载与安装。

## 结构：Updater 与 source 分离

```
                ┌────────────────────┐
  应用 ───────▶ │ Updater            │  版本信息的展示、下载、签名校验、安装、状态与互斥
                └─────────┬──────────┘
                          │ FindUpdate(query) → Release
                ┌─────────┴──────────┐
                │ UpdateSource       │  内部接口：数据从哪来、什么格式、怎么选版本
                └─────────┬──────────┘
             ┌────────────┴─────────────┐
    ReleaseIndexSource           AppcastSource (后续模块)
    读取 releases.json           读取 Sparkle appcast
```

- **Updater** 不认识任何数据格式，也不持有数据源地址（原来的 `SetFeedUrl()` 删除）。
  它只处理一个与格式无关的 `Release`：版本、说明、下载地址、签名、安装包格式。
- **Source** 负责"数据从哪来、如何解析、哪个条目适用于本机、版本怎么比"。每种格式的
  语义（例如 Sparkle 的 `sparkle:os`、版本比较算法）完全封闭在自己的 source 里，不渗入 Updater。
- 每个 source 是一个独立模块：自己的头文件、自己的单例、自己的配置项
  （`ReleaseIndexSource::SetUrl()`、日后的 `AppcastSource::SetUrl()`）。
- 目前只有 `ReleaseIndexSource`，读取 dazzle updater 自有的 release index（JSON），格式见 [release-index.md](release-index.md)。

Sparkle 兼容层以后作为 `AppcastSource` 模块加入，并可用 CMake 选项
（如 `DAZZLE_WITH_APPCAST`）裁掉。它不改动 Updater 的任何接口。

命名：数据源抽象叫 source，与 Velopack 的 `IUpdateSource`、go-selfupdate 的 `Source` 一致；
具体 source 以所读的格式命名（`ReleaseIndexSource`、`AppcastSource`）。自有格式叫 release index，
不叫 manifest（在 Windows、Android、OHOS、Web 上 manifest 都指应用自身的描述文件），
也不叫 feed（避免沿用 appcast 的概念）。

## 核心决定：同步阻塞的动作

三个动作 `CheckForUpdates()`、`DownloadUpdate()`、`InstallUpdate()` 都是**阻塞调用**，
在调用方所在线程上跑完才返回；结果通过返回值和随后的 getter 读取。

理由：

1. **生成器现在就能生成。** 只用到 `bool`、`int64_t`、`double`、`std::string`，
   不需要给 codegen 增加回调、枚举或结构体支持，四种语言的 binding 同时可用。
2. **core 里没有自己的线程。** 不需要解决"回调在哪个线程触发"这个在每个 binding 里都
   不同的问题（Dart isolate、C# SynchronizationContext、Rust Send）。
3. **异步交给宿主语言。** 调用方自己选线程：Dart `Isolate.run`、C# `Task.Run`、
   Rust `std::thread::spawn`。以后 binding 可在其上手写 `Future`/`Task` 包装，core API 不变。

`GetDownloadProgress()` 和 `CancelDownload()` 可在**任意其它线程**调用，UI 线程用定时器轮询进度。

事件回调等 codegen 支持函数指针后再加，届时是对现有 API 的补充，不是替换。

## 公共接口

### ReleaseIndexSource（新模块，`src/release_index_source.h`）

```cpp
class ReleaseIndexSource {
 public:
  static ReleaseIndexSource& GetInstance();

  // release index 文件的地址，例如 "https://example.com/myapp/releases.json"；"" 清除。
  void SetUrl(const std::string& url);
  std::string GetUrl() const;
};
```

这个模块是纯跨平台的：解析、条目选择和版本比较都在 `src/` 里，HTTP 走
`src/foundation/http.h`（平台实现在 `platform/<os>/http_<os>.*`），所以它不声明 `Impl`，
也不需要 `platform/<os>/release_index_source_<os>` 文件。

生成的 C ABI 为 `dazzle_release_index_source_set_url()` 等；binding 中为
`ReleaseIndexSource.instance.url`（Dart）、`ReleaseIndexSource::instance().set_url()`（Rust）、
`ReleaseIndexSource.Instance.Url`（C#）。

### Updater（`src/updater.h`）

#### 配置

```cpp
// 使用哪个 source，按名字选择。默认 "release-index"；日后 Sparkle 兼容模块注册为 "appcast"。
// 名字未注册时，CheckForUpdates() 返回 false。
void SetSource(const std::string& name);
std::string GetSource() const;

// 当前应用的版本，交给 source 与数据源中的版本比较。
// 默认值取自平台：macOS/iOS 为主 bundle 的 CFBundleShortVersionString，Windows 为主程序
// VERSIONINFO 的 ProductVersion，其它平台为 ""。SetCurrentVersion("") 恢复默认。
void SetCurrentVersion(const std::string& version);
std::string GetCurrentVersion() const;

// 校验安装包签名的 Ed25519 公钥，Base64 编码（32 字节）。
void SetPublicKey(const std::string& public_key);
std::string GetPublicKey() const;
```

source 用字符串名选择，是因为 codegen 目前不支持枚举和对象参数；它是公共 API 里唯一的
字符串式选择，等 codegen 支持枚举后可换成枚举。

Sparkle 的 EdDSA 签名同样是对安装包字节的 Ed25519 签名，所以同一把密钥以后在两种
source 下都能用。

#### 检查

```cpp
// 通过当前 source 查询数据源，找出适用于本机且比 GetCurrentVersion() 新的最高版本。
// 返回 true 表示查询完成（无论是否有更新）；false 表示失败，原因见 GetErrorMessage()。
bool CheckForUpdates();

// 最近一次成功查询是否发现了更新。
bool IsUpdateAvailable() const;

// 发现的更新的信息。IsUpdateAvailable() 为 false 时均返回 "" / 0。
std::string GetUpdateVersion() const;
std::string GetUpdateDisplayVersion() const;      // 缺省时同 GetUpdateVersion()
std::string GetUpdateReleaseNotes() const;        // Markdown
std::string GetUpdateReleaseNotesUrl() const;
int64_t GetUpdateSize() const;                    // 安装包字节数；未知为 0
```

更新说明统一为 Markdown：release index 直接给 Markdown；日后 AppcastSource 负责把
appcast 里的 HTML 转换过来（或只提供 `GetUpdateReleaseNotesUrl()`），Updater 对外只有一种格式。

每次调用 `CheckForUpdates()` 都先清空上一次的结果，包括已下载的文件。

#### 下载

```cpp
// 下载发现的更新到 dazzle_updater 的缓存目录，并用 GetPublicKey() 校验签名。
// 返回 true 表示文件已下载且签名有效；false 见 GetErrorMessage()（含被取消）。
bool DownloadUpdate();

// 下载进度 0.0–1.0；总大小未知时为 -1.0；未在下载时为 0.0。任意线程可调用。
double GetDownloadProgress() const;

// 中止进行中的 DownloadUpdate()，使其返回 false。任意线程可调用；没有下载时什么都不做。
void CancelDownload();

// 已下载并通过校验的更新是否就绪、可以安装。
bool IsUpdateDownloaded() const;
```

**签名是强制的。** 未设置公钥、`Release` 没有签名、或签名不匹配，`DownloadUpdate()`
都返回 false 并删除文件。更新器是远程代码执行的通道，不提供关闭校验的开关。

#### 安装

```cpp
// 启动安装已下载的更新。返回 true 后，调用方应尽快退出进程：
// 安装器会等待本进程退出，然后替换应用并重新启动它（Windows 上由安装程序自己决定是否启动）。
// 返回 false 表示没有启动安装，应用照常运行，原因见 GetErrorMessage()。
bool InstallUpdate();
```

dazzle updater 不替应用退出：各 UI 框架有自己的退出流程（保存文档、关闭窗口），由应用在
`InstallUpdate()` 返回 true 后自行调用。

| 平台 | 安装包格式（`Release::format`） | 做法 |
|---|---|---|
| macOS | `zip`，内含一个 `.app` | 解压到临时目录，确认新包的 bundle identifier 与当前一致；启动一个脱离的辅助进程，等当前 pid 退出后替换 `.app` 并 `open` 新版本 |
| Windows | `exe`、`msi` | 以 `installerArguments` 为参数启动安装程序（`msi` 通过 `msiexec /i`） |

#### 错误

```cpp
// 最近一次失败的动作的错误说明（英文，供日志和展示）；最近一次动作成功时为 ""。
std::string GetErrorMessage() const;

// 最近一次失败的动作的错误码，供界面按原因取本地化文案；最近一次动作成功时为 ""。
std::string GetErrorCode() const;
```

错误码是稳定的字符串，新增错误码不改变已有错误码的含义：

| 错误码 | 含义 | 可能出现在 |
|---|---|---|
| `busy` | 另一个动作进行中（`"another operation is in progress"`） | 全部 |
| `unsupported-platform` | 本平台不支持该动作 | 全部 |
| `no-update` | 顺序不对：没有可下载的更新，或没有已下载的更新 | 下载、安装 |
| `unknown-source` | `SetSource()` 指定的名字没有注册 | 检查 |
| `invalid-version` | 当前版本不符合版本号语法 | 检查 |
| `network` | 无法连接、超时、HTTP 错误 | 检查、下载 |
| `invalid-index` | 数据源格式不正确或 `schema` 不受支持 | 检查 |
| `signature` | 未设置公钥、没有签名或签名不匹配 | 下载 |
| `cancelled` | 被 `CancelDownload()` 中止 | 下载 |
| `permission` | 没有权限写入或替换应用 | 安装 |
| `app-location` | 应用在只读、临时或被系统转移（App Translocation）的位置运行 | 安装 |
| `installer` | 解压或启动安装器失败 | 安装 |

字符串而不是枚举，原因与 `SetSource()` 相同：codegen 目前不支持枚举。

不叫 `GetLastError`，避免与 Win32 的同名函数混淆。

## 线程与状态

- 所有方法线程安全。
- 三个动作互斥：一个动作进行中时调用另一个（或同一个），立即返回 false，
  `GetErrorMessage()` 为 `"another operation is in progress"`。
- 顺序约束：`DownloadUpdate()` 要求 `IsUpdateAvailable()`，`InstallUpdate()` 要求
  `IsUpdateDownloaded()`，否则返回 false。
- 查询进行中修改 source 或其配置，不影响本次查询。
- 状态只在内存里，不持久化；进程重启后需要重新检查。

典型用法：

```cpp
dazzle_updater::ReleaseIndexSource::GetInstance().SetUrl("https://example.com/myapp/releases.json");

auto& updater = dazzle_updater::Updater::GetInstance();
updater.SetPublicKey("pfIShU4dEXqPd5ObYNfDBiQWcXozk7estwzTnF9BamQ=");

// 在后台线程：
if (updater.CheckForUpdates() && updater.IsUpdateAvailable()) {
  // 回到 UI 线程展示 GetUpdateDisplayVersion() / GetUpdateReleaseNotes()，
  // 用户确认后再到后台线程：
  if (updater.DownloadUpdate() && updater.InstallUpdate()) {
    // 通知 UI 线程退出应用
  }
}
```

## 内部接口：UpdateSource

不在 `API_HEADERS` 里，codegen 不读取，不进入任何 binding。暂不向应用开放自定义 source。

```cpp
// src/update_source.h

// 与格式无关的一个版本。source 把自己格式里的条目转换成它。
struct Release {
  std::string version;
  std::string display_version;
  std::string release_notes;        // Markdown
  std::string release_notes_url;
  std::string download_url;
  int64_t size = 0;
  std::string signature;            // Ed25519，Base64
  std::string format;               // "zip" | "exe" | "msi"
  std::string installer_arguments;
};

// 本机信息，由 Updater 填好交给 source。
struct UpdateQuery {
  std::string current_version;
  std::string platform;             // "macos" | "windows" | "linux" | "ios" | "android" | "ohos"
  std::string arch;                 // "x64" | "arm64" | "x86"
  std::string system_version;       // 如 "14.5"、"10.0.22631"
};

class UpdateSource {
 public:
  virtual ~UpdateSource() = default;
  // 查询数据源。返回 true 表示查询完成：找到时 *found = true 并填好 *release。
  // 返回 false 时把原因写入 *error，并把上表的稳定错误码写入 *error_code；
  // Updater 原样上报 *error_code（只有 source 知道自己的格式为什么失败）。
  // 在调用 CheckForUpdates() 的线程上执行。
  virtual bool FindUpdate(const UpdateQuery& query, Release* release, bool* found,
                          std::string* error, std::string* error_code) = 0;
};

// 按名字注册，供 Updater::SetSource() 选择。各 source 模块在静态初始化时注册自己。
void RegisterUpdateSource(const std::string& name, UpdateSource* source);

// 按名字取回已注册的 source，没有时为 nullptr。
UpdateSource* FindUpdateSource(const std::string& name);
```

跨平台层还包括两个可单独测试的模块：版本比较（`src/version.h`，见
[release-index.md](release-index.md) 的"版本号"）和 release index 的解析与选择
（`src/release_index_parse.h`，输入 JSON 文本，不碰网络）。

source 需要的 HTTP 能力由内部的 `src/foundation/http.h` 提供（平台实现放在
`platform/<os>/http_<os>.*`），Updater 下载安装包也用它。

## 平台可用性

动作类方法的注释带 `@note Platform availability:` 块：

| 平台 | 检查 | 下载 | 安装 | HTTP 实现 |
|---|---|---|---|---|
| macOS | ✅ | ✅ | ✅ | NSURLSession |
| Windows | ✅ | ✅ | ✅ | WinHTTP |
| Linux | ✅ | ✅ | ✅ | libcurl（构建时找不到 libcurl 时退回跨平台默认实现，于是检查与下载都不可用） |
| iOS | ✅ | ❌ | ❌ | NSURLSession |
| Android | ❌ | ❌ | ❌ | 桩 |
| OHOS | ❌ | ❌ | ❌ | 桩 |

❌ 的平台返回 false，`GetErrorMessage()` 为 `"not supported on this platform"`。
iOS 上检查仍有用：应用发现新版本后引导用户去 App Store / TestFlight。

Linux 的安装是"替换应用自己的目录再重启"：Linux 桌面应用就是一个目录（可执行文件加它旁边
的东西），安装包是那个目录的新版本（`format` 为 `zip`，内含恰好一个顶层目录）。解压发生在下载
目录里，随后由一个脱离进程的辅助脚本等本进程退出，再把新目录换上去并重新启动同一个可执行文件，
失败时把旧目录放回去。因此 `SystemSupportsInstall()` 在 Linux 上为 true，安装不需要包管理器，
也不需要 root——但它要求应用自己所在目录的父目录可写，否则报 `permission`。

## 实现分层

- **跨平台（`src/`）**：`Updater` 的状态机与互斥（`updater.cpp`）、`UpdateSource` 注册表
  （`update_source.cpp`）、版本比较（`version.*`）、release index 的解析与选择
  （`release_index_parse.*`，输入 JSON 文本，不碰网络）、Ed25519 校验与 Base64
  （`ed25519.*`）、HTTP 缝（`foundation/http.*`）。第三方代码放 `core/third_party/`：
  JSON 用 nlohmann/json，Ed25519 用 Monocypher（含 `monocypher-ed25519`）。
- **平台层**：只有两个缝，都是内部自由函数。`foundation/system.h`（`system_<os>`）提供
  能力位（是否能检查/下载/安装）、默认版本号、平台/架构/系统版本、下载目录与安装；
  `foundation/http.h`（`http_<os>`）提供 HTTP GET（到内存或文件，带进度与取消）。
  `Updater` 本身不携带平台缝——它是一份跨平台实现，平台差异只在这两个缝后面。

## 落地步骤

当前状态：步骤 1、步骤 2 与步骤 3 的 macOS 部分（NSURLSession 下载、Ed25519 校验、
zip 解压与脱离式辅助进程安装）已完成，由 `core/tests/` 的单元测试与
`core/tests/macos_install_e2e.sh` 端到端覆盖。目前只有 macOS 有平台文件
（`platform/macos/{http,system}_macos.mm`）；其它平台没有平台文件，编译的是跨平台默认
实现（`foundation/{http,system}_default.cpp`），因此回答 `"not supported on this
platform"`。Flutter 示例已改到新流程，但本机没有 Flutter SDK，未验证。

1. [release-index.md](release-index.md) 定稿；实现版本比较与 release index 解析 + 单元测试。
2. 删除 `Updater::SetFeedUrl()`，加入上述 Updater 方法与 `ReleaseIndexSource` 模块，
   `./codegen` 重新生成；不支持的平台由跨平台默认实现兜底，不需要留桩。同步更新
   CLAUDE.md、`specs/README.md`、各 binding 与 Flutter 示例中对 feed URL 的引用。
3. macOS 实现（NSURLSession、zip 安装）；`flutter_updater_example` 走完整流程。
4. Windows 实现（WinHTTP、安装程序启动）。
5. ✅ Linux 实现：libcurl 的 HTTP 缝、`system_linux.cpp`（uname 架构、`/etc/os-release` 版本、
   XDG 下载目录、zip 安装与脱离式辅助脚本）与 GTK 4 的内置界面（见
   [updater-ui.md](updater-ui.md)）。`core/tests/linux_install_e2e.sh` 端到端覆盖下载、签名校验、
   目录替换与重启。iOS 的检查与下载仍未做。
6. 各 binding 的示例与测试补上检查流程（指向本地起的静态 releases.json）。
7. 另起 `AppcastSource` 模块（单独的设计文档）。
