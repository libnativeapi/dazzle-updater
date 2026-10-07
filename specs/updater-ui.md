# UpdaterUI：内置更新界面

[Updater](updater.md) 不带 UI，界面由应用自己画。`UpdaterUI` 是给不想自己画界面的应用准备的
可选模块：用原生界面带用户走完"检查 → 查看更新说明 → 下载 → 安装并重启"的全过程。
它只是 Updater 的一个使用者，所有数据和动作都来自 Updater，不增加新的更新逻辑。

两种用法可以并存，但同一时刻只走一条流程：`UpdaterUI` 进行中时，应用直接调用 Updater 的
动作会得到 `"another operation is in progress"`，反之亦然。

## 目标与边界

- 原生外观：macOS 用 AppKit 的系统控件，三个窗口 1:1 复刻 Sparkle 2.x；Windows 用 Windows App SDK 的 WinUI 3 控件；
  Linux 用 GTK 4 自己的控件（见"外观"）。不用 WebView。
- 一行代码接入：配好 Updater 的数据源和公钥，菜单项里调用 `CheckForUpdates()` 即可。
- 窗口实现和调度由 core 负责。macOS 可用于 Flutter、Qt、Electron 和原生应用；
  Windows 目前要求 WinUI 3 宿主，Linux 要求 GTK 4 宿主。

目前界面由两个更新窗口和一个只出现一次的询问窗口组成（见"窗口与流程"）。模块名不叫 window，是为以后留出其它形式的界面，
例如后台检查发现更新时改用系统通知（macOS 通知中心、Windows toast），而不必改名。

**暂不包含**（后续再设计）：系统通知、定时自动检查的调度、"退出时安装"、匿名系统信息上报、
自定义文案和配色、自定义窗口内容、移动端界面。

**不做**：独立的"正在解压"阶段。Sparkle 在下载后先解压再进入 Ready；dazzle updater 的签名校验在
`DownloadUpdate()` 里完成，macOS 的解压发生在 `InstallUpdate()` 里（见 [updater.md](updater.md)），
解压的时间已经包含在 Installing 里，没有可以单独显示的步骤。

## 公共接口

新模块，`src/updater_ui.h`：

```cpp
class UpdaterUI {
 public:
  static UpdaterUI& GetInstance();

  // 用户发起的检查（"检查更新…"菜单项）：立即显示窗口并进入"正在检查"，走完整流程；
  // 已是最新、出错都会在窗口里告诉用户。已有更新窗口在显示时，只把它带到前台。
  // 返回 false 表示本平台没有内置界面。
  bool CheckForUpdates();

  // 静默检查（应用启动时、应用自己的定时器）：只在发现更新时显示窗口；
  // 已是最新、出错、或发现的是用户跳过的版本时什么都不显示。已有更新窗口在显示时什么都不做。
  // 用户关闭了自动检查时直接返回；用户还没回答过时先显示询问窗口（见"首次询问"）。
  // 返回 false 表示本平台没有内置界面。
  bool CheckForUpdatesInBackground();

  // 是否有更新窗口正在显示。应用可据此禁用"检查更新…"菜单项。
  bool IsVisible() const;

  // 窗口文案里使用的应用名。默认取自平台：macOS 为主 bundle 的 CFBundleName，
  // Windows 为主程序 VERSIONINFO 的 ProductName。SetAppName("") 恢复默认。
  void SetAppName(const std::string& name);
  std::string GetAppName() const;

  // 用户是否同意自动检查更新：为 false 时 CheckForUpdatesInBackground() 什么都不做。
  // 用户在询问窗口里的回答、应用在自己的设置界面里的修改都落到这里；调用过 Set 就不再询问。
  void SetAutomaticChecksEnabled(bool enabled);
  bool IsAutomaticChecksEnabled() const;

  // 静默检查发现更新时是否先在后台下载，下载完成再显示窗口（见"自动下载"）。默认 false。
  void SetAutomaticDownloadsEnabled(bool enabled);
  bool IsAutomaticDownloadsEnabled() const;

  // 应用是否允许自动下载。为 false 时窗口里不出现自动下载的复选框，
  // IsAutomaticDownloadsEnabled() 恒为 false。默认 true。
  void SetAutomaticDownloadsAllowed(bool allowed);
  bool IsAutomaticDownloadsAllowed() const;

  // 忘掉用户的回答——自动检查的回答、自动下载的选择、跳过的版本——回到首次运行的状态：
  // 下一次 CheckForUpdatesInBackground() 会重新询问，跳过的版本会再次被提示。
  // 供应用自己的"重置"路径以及演示、测试重复走流程时使用；三个桌面平台都写回各自的存储。
  void ResetPreferences();
};
```

- 生成的 C ABI 为 `dazzle_updater_ui_check_for_updates()` 等；binding 中为
  `UpdaterUI.instance.checkForUpdates()`（Dart）、`UpdaterUI::instance().check_for_updates()`
  （Rust）、`UpdaterUI.Instance.CheckForUpdates()`（C#）。
- 数据源、当前版本、公钥全部沿用 Updater 的配置，`UpdaterUI` 不重复提供。
- 整个模块可以用 `-DDAZZLE_WITH_UPDATER_UI=OFF` 裁掉（默认 `ON`）：两个入口和 C ABI 照常存在、
  照常返回 false，但两个窗口的实现、平台偏好存储、Markdown 渲染和 md4c 都不参与编译，AppKit /
  Direct2D 这些系统 UI 框架也不链接。源码里没有一处 `#ifdef`：这个选项只决定 CMake 编哪个文件
  （见"实现分层"），所以两种构建的 ABI 相同，应用和 binding 换一种构建方式照样链接和运行。

## 用户偏好

Updater 不持久化任何状态；`UpdaterUI` 要记住三件属于用户的事，存在平台的用户偏好里，
随应用走、按用户隔离：

| 偏好 | 含义 | macOS（`NSUserDefaults` 标准域） | Windows（`HKCU\Software\{公司名}\{应用名}\dazzle_updater`） | Linux（`$XDG_CONFIG_HOME/dazzle/<应用名>.conf`） |
|---|---|---|---|---|
| 自动检查 | 未回答 / 是 / 否 | `DazzleAutomaticChecks` | `AutomaticChecks`（DWORD，缺省为未回答） | `AutomaticChecks`（`1`/`0`，缺省为未回答） |
| 自动下载 | 是 / 否 | `DazzleAutomaticDownloads` | `AutomaticDownloads`（DWORD） | `AutomaticDownloads`（`1`/`0`） |
| 跳过的版本 | 版本号或无 | `DazzleSkippedVersion` | `SkippedVersion`（字符串） | `SkippedVersion` |

公司名、应用名取自主程序 VERSIONINFO 的 CompanyName、ProductName；Linux 没有这类应用元数据，
所以文件名取可执行文件的名字（应用自己的设置放在别处）。三个平台都按应用隔离：回答过一个程序的
用户，没有回答过下一个。

读写走内部的一条缝（`src/foundation/updater_ui_preferences.h`）：`platform/<os>/updater_ui_preferences_<os>.*`
给这个平台的存储，没有该文件的平台用跨平台默认实现，把回答留在内存里——一次运行内的行为相同，
进程退出后不保留。macOS 的存储是应用自己的 `NSUserDefaults` 标准域，Windows 的存储是注册表，
键名就是上表列出的三个；Linux 的存储是一个 `[updater-ui]` 组、`key=value` 行的文件
（`$XDG_CONFIG_HOME/dazzle/<应用名>.conf`，缺省即 `~/.config/dazzle/<应用名>.conf`），
不依赖 GSettings 或 dconf，所以只链接库、不建 GTK 界面的应用也照样记得住回答。

典型用法：

```cpp
dazzle_updater::ReleaseIndexSource::GetInstance().SetUrl("https://example.com/myapp/releases.json");
dazzle_updater::Updater::GetInstance().SetPublicKey("pfIShU4dEXqPd5ObYNfDBiQWcXozk7estwzTnF9BamQ=");

// 应用启动后：
dazzle_updater::UpdaterUI::GetInstance().CheckForUpdatesInBackground();

// "检查更新…"菜单项：
dazzle_updater::UpdaterUI::GetInstance().CheckForUpdates();
```

## 线程

- 所有方法**立即返回**，不阻塞调用方。macOS 可从任意线程调用；Windows 首次检查要从
  宿主的 XAML UI 线程调用，建立调度后可从任意线程调用；Linux 需要初始化 GTK 并运行主循环。
- 模块内部自己调度：Updater 的阻塞动作在模块自己的工作线程上执行；界面操作切到平台的 UI 线程——
  macOS 投递到主线程（`dispatch_async(dispatch_get_main_queue(), ...)`）；Windows 首次检查在
  宿主的 XAML UI 线程上调用，后端随后通过自己的消息窗口投递工作。Linux 用
  `g_main_context_invoke_full()` 把窗口操作投递给**应用自己的 GTK 主线程**：GTK 不是线程安全的，
  也只能由初始化它的线程使用，所以 Linux 要求应用是 GTK 应用（`gtk_init()` 已经有主循环），
  否则 `CheckForUpdates()` 与 `CheckForUpdatesInBackground()` 返回 false，什么都不显示。
- 这是 dazzle updater 里唯一自带线程的模块；Updater 本身仍然没有线程（见 [updater.md](updater.md)）。

## 窗口与流程

更新流程由两个窗口承载，按常见更新框架的分法划分（Sparkle 的 `SUUpdateAlert` / `SUStatusController`、
NetSparkle 的 `UpdateAvailableWindow` / `DownloadProgressWindow`、AutoUpdater.NET 的
`UpdateForm` / `DownloadUpdateDialog`）：

| 窗口 | 负责的状态 | 布局 |
|---|---|---|
| **UpdateAvailableWindow** | Available、Downloaded | 应用图标与版本信息、可滚动的更新说明、自动下载复选框、按钮；可调整大小 |
| **UpdateStatusWindow** | Checking、Downloading、Ready、Installing、UpToDate、Error | 应用图标、一行状态或进度条、按钮；固定大小的紧凑布局 |

另有 **UpdatePermissionWindow**，只在首次静默检查前出现一次（见"首次询问"），不参与下面的状态机。

- 同一时刻最多显示一个窗口。流程在两个窗口之间转换时（Checking → Available、Available → Downloading），
  先显示新窗口再关闭旧窗口，新窗口以旧窗口的中心为中心，看起来是原地替换。
- 同一窗口内的状态转换（例如 Downloading → Ready）只更新窗口内容，不重建窗口。
- 两个窗口都非模态、不依附于应用的任何窗口，显示时带到前台。不另外弹出对话框：已是最新、
  出错也显示在 UpdateStatusWindow 里。

```mermaid
stateDiagram-v2
    [*] --> Checking: CheckForUpdates()
    [*] --> Downloaded: CheckForUpdates()，已有下载好的更新
    [*] --> Silent: CheckForUpdatesInBackground()
    Silent --> Available: 发现更新
    Silent --> SilentDownloading: 发现更新，且开启了自动下载
    SilentDownloading --> Downloaded: 下载并校验完成
    SilentDownloading --> [*]: 出错（不显示）
    Silent --> [*]: 已是最新 / 出错 / 跳过的版本（不显示）
    Checking --> Available: 发现更新
    Checking --> UpToDate: 已是最新
    Checking --> Error: 出错
    Available --> Downloading: 安装更新
    Available --> [*]: 以后再说 / 跳过此版本
    Downloaded --> Installing: 安装并重新启动
    Downloaded --> [*]: 以后再说 / 跳过此版本
    Downloading --> Ready: 下载并校验完成
    Downloading --> Error: 出错
    Downloading --> [*]: 取消
    Ready --> Installing: 安装并重新启动
    Ready --> [*]: 以后再说
    Installing --> [*]: 应用退出
    Installing --> Error: 安装器启动失败
    UpToDate --> [*]
    Error --> [*]
```

| 状态 | 窗口 | 窗口内容 | 按钮 | 调用的 Updater 方法 |
|---|---|---|---|---|
| Checking | Status | 不确定进度条，"正在检查更新…" | 取消 | `CheckForUpdates()` |
| Available | Available | 应用图标；"{应用名} 有新版本可用"；"{应用名} {新版本} 已发布，你当前使用的是 {当前版本}。"；可滚动的更新说明；复选框"以后自动下载并安装更新" | 跳过此版本 ··· 以后再说 · **安装更新** | `GetUpdateDisplayVersion()`、`GetUpdateReleaseNotes()` 等 |
| Downloaded | Available | 同 Available，标题为"{应用名} 的新版本已准备好安装"，说明为"{应用名} {新版本} 已下载完成。要现在安装并重新启动 {应用名} 吗？" | 跳过此版本 ··· 以后再说 · **安装并重新启动** | `IsUpdateDownloaded()` |
| Downloading | Status | 确定进度条，"已下载 12.3 MB / 46.0 MB"；总大小未知时为不确定进度条 | 取消 | `DownloadUpdate()`；每 100 ms 读 `GetDownloadProgress()`；取消时 `CancelDownload()` |
| Ready | Status | "更新已准备就绪" | 以后再说 · **安装并重新启动** | — |
| Installing | Status | "正在安装…"（很短） | 无 | `InstallUpdate()`，成功后退出应用（见下） |
| UpToDate | Status | "{应用名} {当前版本} 已是最新版本。" | **好** | — |
| Error | Status | 标题按步骤："检查更新时出错" / "下载更新时出错" / "安装更新时出错"；说明按 `GetErrorCode()` 取文案（见"错误文案"），附 `GetErrorMessage()` 作为详细信息 | **好** | — |

- Checking 状态的"取消"只关闭窗口：Updater 的检查不可中断，工作线程上的检查会跑完，结果被丢弃；
  在它结束前再次调用 `CheckForUpdates()` 会重新显示窗口并沿用这次检查。
- 粗体为默认按钮（回车触发）；Esc 与关闭窗口等同于"以后再说"或"取消"。
- 下载中关闭窗口视为取消。Ready、Downloaded 状态下选"以后再说"，已下载的文件在本次进程内保留：
  之后调用 `CheckForUpdates()` 直接显示 Downloaded，不重新检查（重新检查会让 Updater 清空结果）；
  `CheckForUpdatesInBackground()` 什么都不做。进程退出后不保留，这是 Updater"状态不持久化"的直接结果。
- 复选框"以后自动下载并安装更新"对应 `SetAutomaticDownloadsEnabled()`，勾选即写入用户偏好，
  不需要等用户点按钮；`IsAutomaticDownloadsAllowed()` 为 false 时不显示。
- 更新说明为空但有 `GetUpdateReleaseNotesUrl()` 时，UpdateAvailableWindow 显示"查看更新说明"链接，
  用系统浏览器打开，此时没有更新说明区域，窗口为固定大小。
- Installing 状态没有按钮，窗口也不能关闭。

### 跳过此版本

- "跳过此版本"把这次更新的版本号（`GetUpdateVersion()`）写入用户偏好，然后关闭窗口；
  Downloaded 状态下还会丢弃已下载的文件。
- Updater 没有"丢弃下载"的接口，所以文件留在它的缓存里，由下一次 `CheckForUpdates()` 清除
  （那正是 Updater 清空上一次结果的时机）；在那之前 UpdaterUI 不再展示它，`CheckForUpdates()`
  重新检查而不是显示 Downloaded。
- `CheckForUpdatesInBackground()` 发现的版本等于跳过的版本时，什么都不显示；比它新的版本照常提示，
  并清除跳过记录。
- `CheckForUpdates()` 是用户主动的，不受跳过影响：跳过的版本照常显示为 Available。

### 自动下载

`IsAutomaticDownloadsEnabled()` 为 true 时，`CheckForUpdatesInBackground()` 发现更新（且不是跳过的
版本）后不显示窗口，在工作线程上直接 `DownloadUpdate()`；下载并校验完成才显示 UpdateAvailableWindow
的 Downloaded 状态，用户看到的第一个窗口就是"安装并重新启动"。下载出错时什么都不显示。

期间用户主动调用 `CheckForUpdates()`：窗口显示这次下载的进度，下载继续（不重新检查）；完成后的结果
照常显示。期间再来的静默检查什么都不做。

### 首次询问

用户还没回答过"是否自动检查"时，`CheckForUpdatesInBackground()` 不检查，先显示 UpdatePermissionWindow：

| 内容 | 按钮 |
|---|---|
| 应用图标；"要自动检查更新吗？"；"{应用名} 可以自动检查新版本。你随时可以在 {应用名} 菜单中手动检查更新。"；复选框"自动下载并安装更新"（`IsAutomaticDownloadsAllowed()` 为 false 时不显示） | 不检查 · **自动检查** |

- "自动检查"：写入 `SetAutomaticChecksEnabled(true)` 和复选框的值，然后继续这次静默检查。
- "不检查"或关闭窗口：写入 `SetAutomaticChecksEnabled(false)`，这次和以后的静默检查都什么都不做。
- `CheckForUpdates()` 不询问，也不改动这个回答：用户主动检查走完整流程，是否自动检查仍留到第一次
  静默检查时再问。
- 应用在询问前调用过 `SetAutomaticChecksEnabled()` 的，不再询问。

## 错误文案

Error 状态的说明按 Updater 的 `GetErrorCode()`（见 [updater.md](updater.md)）取本地化文案，
`GetErrorMessage()` 的英文原文附在其后，供用户反馈问题时复制：

| 错误码 | 文案 |
|---|---|
| `network` | 无法连接到更新服务器。请检查网络连接后再试。 |
| `invalid-index` | 更新信息的格式不正确。请稍后再试，或联系开发者。 |
| `signature` | 更新包的签名无效，已停止安装。请稍后再试，或联系开发者。 |
| `permission` | 没有权限替换 {应用名}。请在"系统设置 → 隐私与安全性 → App 管理"中允许 {应用名} 修改应用（macOS），或以有权限的账户运行。 |
| `app-location` | {应用名} 正在从只读或临时位置运行，无法原地更新。请将它移到"应用程序"文件夹，从那里重新打开后再试。 |
| `installer` | 无法启动安装程序。请稍后再试。 |
| 其它 | 发生了未知错误。请稍后再试。 |

`permission` 与 `app-location` 的文案目前是按 macOS 写的（"系统设置 → 隐私与安全性 → App 管理"、
移到"应用程序"文件夹），Linux 与 Windows 的同类失败也会显示它——这是已知的粗糙处，两份文案要等
平台各自的失败方式定下来再分。Linux 今天只会用到 `permission`（应用目录的父目录不可写）。

## 退出应用

`InstallUpdate()` 返回 true 后必须尽快退出进程。Updater 把这件事留给应用；`UpdaterUI` 为了
"一行接入"，自己发起平台标准的**正常退出**，让应用有机会保存状态：

| 平台 | 做法 |
|---|---|
| macOS | 主线程调用 `[NSApp terminate:nil]`，走应用的 `applicationShouldTerminate:` 流程 |
| Windows | 向本进程所有可见顶层窗口（本模块的除外）发送 `WM_CLOSE` |
| Linux | 有 `GApplication` 时调用 `g_application_quit()`，应用的 `shutdown` 处理器照常运行；没有时关闭应用自己的顶层窗口，`gtk_main()` 随最后一个窗口退出 |

如果应用拒绝退出（例如用户在"是否保存"对话框里点了取消），安装器会一直等到进程退出后再替换应用。

## 更新说明的渲染

`GetUpdateReleaseNotes()` 是 Markdown。渲染只支持常见子集：标题、段落、有序/无序列表、粗体、
斜体、行内代码、代码块、链接。链接点击后用系统浏览器打开；不加载任何远程资源（图片不显示）。
只有 `http`、`https` 和 `mailto` 三种链接会渲染成链接，其它 scheme 只显示文字，免得更新说明里的
一行字让窗口去打开别的东西。

| 平台 | 做法 |
|---|---|
| macOS | Markdown → HTML → `NSAttributedString`（`initWithHTML:`），显示在只读 `NSTextView` 中；导入后统一字体（系统字体 11、标题 12/13.5、代码等宽 10）、颜色和列表缩进，因为 HTML 导入器自带字体、颜色和列表标记 |
| Windows | Markdown → RTF，显示在只读 RichEdit 控件（`msftedit.dll`，系统自带）中；深色时用 `EM_SETBKGNDCOLOR` 与字符格式设置配色 |
| Linux | Markdown → `GtkTextBuffer` 的 tag，显示在只读 `GtkTextView`（包在 `GtkScrolledWindow` 里）中：标题、列表缩进、粗体、斜体、行内代码、代码块各是一个 tag，每个链接是名为 `link-<n>` 的 tag，点击时按 tag 找到地址交给桌面打开 |

Markdown 解析用 md4c（`core/third_party/`，单文件、MIT），HTML/RTF 输出由 dazzle updater 自己实现
（`src/markdown/`）；md4c 自带的渲染器不使用，所以窗口只能渲染上面这个子集。两个平台都不需要 WebView。

## 本地化

窗口文案内置英文和简体中文，跟随系统首选语言，其它语言回退到英文。文案表放在跨平台层
（`src/updater_ui_strings.*`），两个平台共用同一份。自定义文案以后再设计。

## 外观

- macOS：**1:1 复刻 Sparkle 2.x 的三个窗口**。窗口是标准 `NSWindow`，窗口内全部是 AppKit 自己的控件
  （`NSButton` 圆角按钮、small 尺寸的勾选框、bar 风格的 `NSProgressIndicator`、`NSBox` 包着只读
  `NSTextView`），字体、颜色、尺寸都取系统值，dazzle updater 不画任何一个控件，所以每一代 macOS 上它们
  都和 Sparkle 的窗口长得一样——系统怎么画 Sparkle 的，就怎么画这里的。Sparkle 用 Auto Layout 排版，
  这里按同样的常量手工排在**对齐矩形**（Auto Layout 约束的就是对齐矩形）上，于是 Sparkle 某条约束里
  的数字在这里含义不变（`platform/macos/updater_ui_backend_macos.mm`）。原型 `prototype/` 里的 macOS
  自绘 token 不再约束 macOS 实现，只作设计参考。
  - 字体与颜色：窗口标题行 `systemBold`（13 pt 粗体）、其下说明 `smallSystem`（11 pt）、状态文字
    `monospacedDigitSystemFont` 13 pt，均为 `controlTextColor`；错误详情（dazzle updater 自己的）用
    11 pt `secondaryLabelColor`、可选中。窗口与说明框背景是系统的 `windowBackgroundColor` /
    `textBackgroundColor`。默认按钮带 `\r` 键等价符，由 AppKit 画成强调色；状态的"退路"（以后再说/
    跳过/取消）带 `\033`，Esc 等同于按它。应用图标取 `[NSApp applicationIconImage]`。
  - **UpdateAvailableWindow** = `Sparkle/SUUpdateAlert.xib`：`Titled | Closable | Miniaturizable |
    Resizable`，标题栏透明、标题隐藏（留给窗口列表与无障碍）、可按背景拖动；内容 532 × 370，最小
    462 × 150。一个竖向 stack（默认间距 8，标题行之后与说明框之后各 12）：标题行 64 高——图标 64 在
    左边 20 处，标题与说明组成的块以图标为中心垂直居中、从 92 起、右边距 20、两行相距 8；说明框左右
    各 20，吃掉其余高度且不低于 200；勾选框一行紧贴左边距 20；按钮行：跳过此版本靠左、另两个靠右、
    底边距 20、相距 12，稍后提醒与安装按钮等宽、安装按钮不窄于 120、跳过按钮不窄于安装按钮；
    macOS 26 起三个按钮都是 `NSControlSizeLarge`（`SUUpdateAlert.m`），更早的系统是常规尺寸。没有
    说明时不画说明框，窗口按各行的高度收紧。
  - **UpdateStatusWindow** = `Sparkle/SUStatusController.m`：`Titled | Closable`，内容 400 宽、
    107 高（最小 213 × 107）；图标在 (20, 4)；标题（粗体）与其下 8 pt 的进度条组成的块以图标为中心
    垂直居中、左边线 92、右边距 20；按钮在该块下 8 pt、距底与距右各 20、宽度不小于 100，macOS 26
    起为 `NSControlSizeLarge`；状态文字与按钮**同一条基线**、从 92 起、与按钮至少相距 25。
  - **UpdatePermissionWindow** = `Sparkle/SUUpdatePermissionPrompt.xib`：仅 `Titled`，没有关闭按钮，
    无标题；内容 437 宽，一个间距为 0 的 stack：提问行——图标在 (23, 18)，问题距顶 20、13 pt 粗体，
    说明在其下 8 pt、11 pt、不低于 34 高，两者从 106 起到右边距 20；勾选框行——勾选框上方 6 pt、
    从 106 起；回答行——按钮上方 15 pt、下方 20 pt，"自动检查"靠右边距 20、不窄于 150，"不检查"在
    其左 14 pt、不窄于 101。dazzle updater 不收集匿名系统信息，Sparkle 那两行不画。
  - **更新说明** = `Sparkle/SUTextViewReleaseNotesView.m`（Sparkle 用 `NSTextView` 显示 Markdown
    说明的那条路径；不用 WebView）：macOS 12 起由 Foundation 解析 Markdown
    （`-initWithMarkdownString:`），再按它留下的 presentation intent 排版——正文系统字体 13 pt，
    标题为粗体的 1.5/1.3/1.2/1.1 倍、上下各 0.8 em，段落上下各 0.5 em（列表项内的段落只在下方
    0.3 em），列表项每层缩进 1.5 em、Menlo 的"•"或序号后接一个制表位（制表距 1.38 em），代码块
    等宽系统字体、下方 0.25 em；粗体、斜体、行内代码与链接由属性字符串携带、TextKit 绘制（链接
    是 AppKit 默认的链接样式）。macOS 12 以下走 Markdown → HTML → `NSAttributedString`
    （`initWithHTML:`），再改成同样的字号与间距。文本视图 inset 4 × 8、只有竖向滚动条、
    `usesAdaptiveColorMappingForDarkAppearance`；外面的 `NSBox` 为 custom 类型、6 pt 圆角、
    1 pt `separatorColor` 边框、`textBackgroundColor` 填充，内容视图按 5 pt 圆角裁切。
  - 文案也取自 Sparkle：`Sparkle/Base.lproj/Sparkle.strings` 与 `Sparkle/zh_CN.lproj/Sparkle.strings`
    （"Remind Me Later/稍后提醒我"、"Installing update…/正在安装更新…"、"Update Error!/更新错误！"、
    各错误码的整句文案等）。窗口标题：检查、已是最新、出错为"软件更新"，下载/就绪/安装为"正在更新{应用名}"。
  - 由系统绘制、模块不重做的部分：控件本身、窗口圆角与标题栏、红绿灯、窗口阴影、缩放；缩放时说明框
    跟着窗口伸缩，其余各行贴着自己的边（autoresizing）。
- Windows 的已实现后端使用 WinUI 3，位于 core；以下自绘路线为设计参考：
  - **自绘（模块内）**：Win32 顶层窗口，窗口内的控件（按钮、复选框、进度条、链接）不用 Common Controls，
  而是由 dazzle updater 用 Direct2D + DirectWrite 自绘，外观取 WinUI 3 的 Fluent 规格。Common Controls v6
  在 Windows 11 上是旧式外观，且没有公开的深色模式支持；自绘才能与系统其它部分一致并跟随深浅色。
  - 规格以 microsoft-ui-xaml 的主题资源为准（`Common_themeresources_any.xaml`、
    `Button_themeresources.xaml`、`CheckBox_themeresources.xaml`、`ProgressBar_themeresources.xaml`）：
    按钮高 32、内边距 11,5,11,6、圆角 4，填充 `ControlFillColorDefault`，边框为 `ControlElevationBorder`
    （底边 `ControlStrokeColorSecondary`）；默认按钮用 `AccentFillColorDefault`；复选框 20 × 20、圆角 4，
    未勾选描边 `ControlStrongStrokeColorDefault`；进度条轨道 1、指示条 3；正文 Segoe UI Variable 14px。
  - 强调色取系统强调色；深浅色读 `HKCU\…\Themes\Personalize\AppsUseLightTheme`，收到
    `WM_SETTINGCHANGE`（`ImmersiveColorSet`）时重绘。
  - **WinUI 3 框架版（模块内已实现）**：`core/src/platform/windows/updater_ui_backend_winui3.cpp`
    实现同一个 `UiBackend` seam（与 macOS 的 AppKit、Linux 的 GTK 4 并列），由跨平台状态机、
    `updater_ui_content.cpp` 与 `updater_ui_strings.cpp` 驱动，控件、强调色、深浅色、DPI 与无障碍
    交给框架。`examples/winui3_updater_ui_example/` 退化为薄宿主（只调 C ABI）。
    - **窗口属于宿主的 XAML 线程**：XAML 属于启动 `Application` 的那个线程。首次检查要从
      该线程调用；后端通过探测窗口确认线程，并创建自己的 Win32 消息窗口（`HWND_MESSAGE`）。
      后续调用可来自其它线程。状态机产生的检查结果、下载进度、关闭窗口和退出请求由 core 用
      `PostMessageW` 投递到消息窗口，在宿主已有的消息循环上执行。示例无需定时刷新，也无需
      传入 DispatcherQueue 或跨 C ABI 的 COM 对象句柄。连续等待中的绘制只保留最新一份，
      关闭、退出等动作按队列顺序保留。
    - **尺寸按内容区给**（`AppWindow.ResizeClient` + 每显示器 DPI 缩放），高度按内容实测
      （Update 窗口有更新说明时为 370，可调整大小）。`ResizeClient` 会在请求值上**再加一个系统标题栏**，
      即使内容已经延伸进标题栏，所以按第一次得到的客户区校正一次。
      Permission 用**无标题栏的独立窗口**（无关闭按钮、Esc = 不检查、不画标题）。
    - 关闭按钮经 `AppWindow.Closing` 取消后报给状态机（Installing 时不报，窗口关不掉）；Esc 同关闭，
      回车按默认按钮；默认按钮用 WinUI 的 `AccentButtonStyle`，颜色取宿主合并的主题资源，
      深色时标题栏按钮的颜色一并设置。更新说明与其它平台一样经 md4c 渲染进 `RichTextBlock`，
      只有 http/https/mailto 成为链接。应用图标取主程序的第一个图标资源，没有时用系统的通用图标。
    - **宿主要声明 DPI 感知**（`app.manifest` 的 `PerMonitorV2`，WinUI 3 模板自带）：否则整个进程按
      100% 渲染后被系统拉伸，窗口是糊的。这是宿主的清单，模块无法替它声明。

- Linux 用 **GTK 4 自己的控件**（`GtkWindow` + `GtkHeaderBar`、`GtkLabel`、`GtkButton`、
  `GtkCheckButton`、`GtkProgressBar`、只读 `GtkTextView`），外观、深浅色、强调色、字体与无障碍
  都由 GTK 和当前主题给出，dazzle updater 只给尺寸和间距：
  - UpdateAvailableWindow：默认 532 × 370，最小 462 × 150，可调整大小；更新说明区域最小高 200 pt、
    6 px 圆角与 1 px 描边（一条 GTK CSS）。
  - UpdateStatusWindow：宽 400，高按内容（图标 64 + 文字/进度条 + 按钮行 ≈ 150），固定大小。
  - UpdatePermissionWindow：宽 437，高按内容；标题栏没有关闭按钮（`set_show_title_buttons(FALSE)`
    与 `set_deletable(FALSE)`），用户必须回答，Esc 等同"不检查"。
  - 标题栏是 GTK 的客户端装饰：状态窗口显示标题，Available 与 Permission 只把标题留给窗口列表
    （与原型一致，标题区留空），拖动、缩放与按钮都由系统给。
  - 强调色与按钮风格取主题：默认按钮加 `suggested-action`、同时**把键盘焦点给它**
    （`gtk_widget_grab_focus()`）——GTK 默认把焦点给第一个可聚焦的控件，不这样做的话回车会按到
    "以后再说"，而不是默认按钮；回车由 `gtk_window_set_default_widget()` 承接，Esc 由窗口的按键
    控制器转成"以后再说/取消"。
  - 应用图标取图标主题里**应用 id** 命名的图标，取不到时用主题的更新图标（Adwaita 的
    `software-update-available`），再取不到就是 `dialog-information`：Linux 没有 bundle 里的图标文件。
  - 窗口位置交给合成器：Wayland 不允许客户端设置窗口位置，所以流程里换窗口时新窗口由合成器放置
    （通常居中），而不是像 macOS 那样以旧窗口中心为中心。
  - 应用是 `GtkApplication` 且有活动窗口时，更新窗口设为该窗口的 transient（对话框）：停留在应用窗口
    之上，Hyprland 这类平铺合成器会把它浮动显示而不是平铺成整屏；被替换的旧窗口不作父窗口，新窗口
    继承旧窗口的父窗口。
  - **Omarchy**（Hyprland 桌面，不是 GNOME）只通过 gsettings 告诉 GTK 深色或浅色（Adwaita /
    Adwaita-dark），主题本身的配色 GTK 并不知道，所以一个普通 GTK 4 窗口在 Omarchy 上是 Adwaita
    的灰与蓝，而周围的状态栏、启动器、终端都是主题色。模块直接读 Omarchy 的调色板文件
    `~/.local/state/omarchy/current/theme/colors.toml`（3.x 之前在
    `~/.config/omarchy/current/theme/`），键名按 omarchy-theme-color 的别名顺序解析
    （`background`→`bg`→`color0` 等），缺的派生色按同样的规则补，然后生成一份只作用于本模块窗口的
    CSS（选择器都以 `window.dazzle_updater` 开头，宿主自己的窗口不受影响），**按 Omarchy shell
    自己的控件库来画**（`/usr/share/omarchy/shell`：`Commons/Style.qml`、`Ui/Button.qml`、
    `Ui/Toggle.qml`、`Ui/ToggleSwitch.qml`，以及 `shell.toml` 的 `[controls]`）：
    - 字体：全部用 fontconfig 的 `monospace` 别名（Omarchy 的字体设置写的就是它）、12 px 基准；
      标题行 16 px 粗体、`bright_foreground`，说明 12 px，状态文字 11 px、`dark_foreground`。
    - 控件状态：静止是前景色 4% 的填充加 1 px、40% 的描边；悬停与键盘焦点 8% / 25%；按下 22%；
      "选中"（默认按钮）是 18% 填充、文字为 `accent` 粗体。没有焦点环，焦点就是控件自己的
      填充与描边。按钮高 28 px（内容 14 + 上下各 6 的内边距 + 1 px 边）、左右内边距 10 px。
    - 圆角跟随 Hyprland 的 `decoration:rounding`（`hyprctl getoption`，每个进程问一次，拿不到按 0），
      标准 Omarchy 是 0，即直角。
    - 布尔选项不是勾选框：Omarchy 的控件库里没有勾选框，用的是 **Toggle 行**——整行是一个控件
      （同样的填充与描边，上下内边距 16 px），标签 13 px 粗体靠左，右端一个 42 × 22 的开关
      （`GtkSwitch`）：未选中是 4% 填充、40% 描边、16 px 的旋钮取前景色变暗 1.25 倍；选中是
      18% 填充、无描边、旋钮取前景色；圆角大于 0 时是胶囊，否则是方的；点行内任意处都能切换。
    - 更新说明框用 `dark_background`、同一种描边；链接、进度条用 `accent`，文本选区用 `selection`。
    - 窗口**不装 GtkHeaderBar**：Omarchy 的窗口没有标题栏（Hyprland 只画边框），GTK 在合成器要求
      服务端装饰时自己也不画头栏，除非被强塞一个标题栏控件；三个窗口因此和桌面上其他窗口一样是
      一块光板，用合成器的快捷键关闭，标题仍留给窗口列表。
    - 文件在每次画窗口时重读、变了才换 CSS，所以运行中切换主题会落到下一个窗口。
      `XDG_CURRENT_DESKTOP` 含 GNOME 时不读该文件，GNOME 下的行为不变；
      `DAZZLE_OMARCHY_COLORS=<colors.toml>` 可在任何机器上指定一份调色板，供预览工具使用。
  - 应用是 `GtkApplication` 且有活动窗口时，更新窗口设为该窗口的 transient（对话框）：停留在应用窗口
    之上，Hyprland 这类平铺合成器会把它浮动显示而不是平铺成整屏；被替换的旧窗口不作父窗口，新窗口
    继承旧窗口的父窗口。
- 窗口标题：两个更新窗口在 Checking/UpToDate/Error 时为"软件更新"，在 Downloading/Ready/Installing 时为
  "正在更新{应用名}"（Sparkle 的 `Updating %@`）；UpdatePermissionWindow 没有标题。

## 平台可用性

`CheckForUpdates()` 与 `CheckForUpdatesInBackground()` 的注释带 `@note Platform availability:` 块：

| 平台 | 可用 | 说明 |
|---|---|---|
| macOS | ✅ | AppKit |
| Windows | ✅ | WinUI 3 窗口（要求宿主是 WinUI 3 应用，窗口画在它的 XAML 线程上） |
| Linux | ✅ | GTK 4 窗口；要求应用本身是 GTK 应用（模块画在应用的 GTK 主循环上），构建时找不到 GTK 4 的构建返回 false |
| iOS | ❌ | 应用应引导用户去 App Store / TestFlight |
| Android | ❌ | |
| OHOS | ❌ | |

❌ 的平台返回 false，不显示任何东西。裁掉模块的构建（`DAZZLE_WITH_UPDATER_UI=OFF`）返回同样的
false，所以应用不需要为"这个平台没有界面"和"这个构建没有界面"分别写代码。

## 实现分层

```
src/updater_ui.h / .cpp          公共类：只是 facade，转发给下面这一份状态机
src/updater_ui_runner.h / .cpp   状态机与工作线程：决定下一个状态、调 Updater、让 backend 画
src/updater_ui_content.h / .cpp  状态 → 窗口内容（窗口、文案、按钮、控件）的纯函数
src/updater_ui_strings.h / .cpp  文案表（en、zh-Hans）+ {0} 占位符替换
src/updater_ui_state.h           UiState、UiEvent、UiContent、UiBackend 接口（内部）
src/updater_ui_port.h / .cpp     Updater 的动作与 getter，藏在接口后面（测试替身）
src/markdown/                    md4c 之上的 HTML 输出（Windows 的 RTF 同目录，随其实现）
src/foundation/updater_ui_backend.h + _default.cpp        窗口缝的声明与空实现
src/foundation/updater_ui_preferences.h + _default.cpp    偏好缝的声明与内存实现
src/platform/<os>/updater_ui_backend_<os>.*               两个窗口、窗口切换、退出应用
src/platform/<os>/updater_ui_preferences_<os>.*           用户偏好的读写与系统首选语言
src/platform/linux/updater_ui_notes_linux.*              Markdown → GtkTextBuffer 的 tag（GTK 自己的文本控件没有标记语言）
```

- 状态机在跨平台层：它决定"下一步调用 Updater 的哪个方法、显示哪个状态"，平台层只负责
  "把这个状态画出来"和"把按钮点击报回来"。状态属于哪个窗口是固定的（见上表），由平台层据此
  选择窗口，状态机不需要知道窗口的存在。这样两个平台的流程行为保证一致。
- 窗口缝是一条返回对象的缝：`GetUiBackend()` 是内部自由函数，返回一个只有一个实例的
  `UiBackend`，因为窗口要活过一次调用。它是内部接口，公共类不携带它（见 [README.md](README.md)
  的分层规则）。接口大致为 `Draw(content)`、`Hide()`、`BringToFront()`、`QuitApplication()`，
  以及把按钮事件回传给状态机的 `SetEventSink`。实现**不得**在 `Draw`/`Hide` 里回调 sink：
  状态机这时持有自己的锁。
- 平台界面放进 `platform/<os>/updater_ui_backend_<os>`；没有该文件的平台编译跨平台默认实现，
  两个 `CheckForUpdates*()` 直接返回 false，所以没有界面实现的平台（今天：iOS、Android、OHOS，
  以及没找到 GTK 4 或裁掉界面的 Linux 构建）不需要任何文件。
  `-DDAZZLE_WITH_UPDATER_UI=OFF` 把平台目录里的 `updater_ui_*` 从构建里剔除，于是同一个默认实现
  接管——"这个平台没有界面"和"这个构建没有界面"是同一段代码。跨平台的状态机、文案、内容组装和
  C ABI 始终编译，所以两种构建导出的符号相同。
- macOS 的窗口这一份按 ARC 编译（`-fobjc-arc`，只对这两个文件设置）；库的其余 ObjC++ 部分仍是
  手动管理。
- 链接依赖：macOS 在 `DAZZLE_WITH_UPDATER_UI=ON` 时增加 `-framework AppKit`；Windows 增加
  `user32`、`gdi32`、`shell32`、`d2d1`、`dwrite`、`dwmapi`、`uiautomationcore`。裁掉 UI 的构建
  完全不链接这些系统库。Linux 在 `DAZZLE_WITH_UPDATER_UI=ON` 且找到 GTK 4 时增加 GTK 4 的
  编译与链接参数，找不到 GTK 4 就退回默认界面实现（返回 false）；libcurl 同理只影响 HTTP 缝。
- 模块自己的线程：一条工作线程按队列跑 Updater 的三个阻塞动作（同时只有一个），下载期间它不阻塞
  等待，而是每 100 ms 把进度画一次；平台层再把每次绘制投递到自己的 UI 线程（macOS 主线程）。

## 落地步骤

前提：[updater.md](updater.md) 的 macOS 实现已完成。

1. ✅ 跨平台层：状态机与文案表，用一个记录调用的假 `Impl` 做单元测试（覆盖每条状态转换）。
   `core/tests/updater_ui_test.cpp` 用脚本化的 Updater 替身与记录型 backend 跑完整流程，
   `updater_ui_content_test.cpp` 直接检查每个状态画出来的内容。
2. ⏳ Markdown → HTML（macOS 用）已完成并有单测；RTF（Windows 用）随 Windows 实现一起做。
3. ✅ `updater_ui.h` 加入 `API_HEADERS`，`./codegen` 重新生成 C ABI 与四个 binding；
   `DAZZLE_WITH_UPDATER_UI` 开关与两条缝的默认实现同时落地，其余平台走默认实现。
4. ✅ macOS 实现：两个 NSWindow 与询问窗口、窗口切换、`NSApp terminate`、`NSUserDefaults` 偏好。
   `core/tools/updater_ui_preview/` 是一个手动编译的开发工具，把每个状态截成 PNG 与 prototype
   对照（用法见该目录的 README）。
5. ⏳ Windows 实现：Win32 窗口 + Direct2D 自绘的 Fluent 控件 + UI Automation + 注册表偏好，
   以及 Markdown → RTF。
6. ✅ Linux 界面：`platform/linux/updater_ui_backend_linux.cpp`（GTK 4 的三个窗口）、
   `updater_ui_notes_linux.*`（Markdown → `GtkTextBuffer`）与 `updater_ui_preferences_linux.cpp`
   （`$XDG_CONFIG_HOME/dazzle/<应用名>.conf`）。预览工具
   `core/tools/updater_ui_preview/updater_ui_preview_linux.cpp` 把每个状态渲染成 PNG；
   `examples/updater_ui_example/demo_linux.sh` 起本地签名 feed 走完整轮次。
7. ✅ C++ 示例：`core/examples/updater_ui_example` 是一个 macOS 应用 bundle，`demo.sh`（`make demo`）
   起一个本地签名 feed 并运行它，可以点完整个流程，包括"安装更新"把示例 bundle 原地替换成新版本
   再重启。⏳ 各 binding 与 Flutter 示例补上内置界面的用法
   （`examples/flutter_updater_example` 的"检查更新…"按钮）。
