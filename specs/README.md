# specs

`core/` 的设计规范：新代码必须遵守什么。怎么用某个 API 看头文件的 Doxygen 注释。

## 分层

```
include/dazzle.h    单一公共入口
src/*.h / *.cpp     接口层：公共 API 的唯一定义处
src/platform/<os>/  平台实现层：接口层的六份实现
src/capi/           C ABI 层
```

依赖自上而下，不得反向：接口层不引用 `platform/`，`capi/` 只引用接口层。

- 公共头文件里不出现任何平台类型、平台头文件或 `#ifdef` 平台分支。
- 平台差异藏在 PIMPL 后面：公共类声明 `class Impl`，持有 `std::unique_ptr<Impl> pimpl_`，
  每个平台文件实现 `Impl` 子类、构造函数和析构函数。
- 六个平台目录（macos、windows、linux、android、ios、ohos）每个模块都要有一份实现，
  不支持的平台也要留桩，否则该平台链接失败。

## 公共 API 风格

- 属性是相邻的一对 `SetX(v)` + `GetX() const`；布尔属性 `SetX(bool)` + `IsX() const`。
  getter 一律 `const`。
- 动作用祈使动词，并有固定的反义词（`Start`/`Stop`、`Enable`/`Disable`）。
- 不加重载：含义不同就起不同的名字。
- 字符串传入用 `const std::string&`，返回按值；浮点用 `double`。
- 失败不抛异常：不会失败用 `void`；平台可能不支持用 `bool`（注释写明 `false` 的含义）；
  查找和工厂返回 `nullptr`。
- 平台差异写在注释里，不体现在签名上。行为因平台而异的 API 带六行的
  `@note Platform availability:` 块（✅ / ⚠️ / ❌）。

## C ABI

| 对象 | 约定 | 例 |
|---|---|---|
| 函数 | `dazzle_<模块>_<动作>` | `dazzle_updater_get_feed_url` |
| 文件 | `<模块>_c.h` / `<模块>_c.cpp` | `updater_c.h` |
| 导出宏 | `DAZZLE_EXPORT` | |

- 所有导出符号都带 `dazzle_` 前缀，保证与其它原生库一起链接时不冲突。
- 每个入口都捕获异常，不让异常穿过 C 边界。
- 返回的字符串由调用方持有，用 `dazzle_free_c_str()` 释放；没有值时返回 `NULL`。
- 由 `tools/codegen/` 从 C++ 头文件生成（`./codegen`），不要手改；只有 `common_c.h` 和
  `string_utils_c.*` 是手写的。生成器支持的类型和命名规则见
  [tools/codegen/README.md](../tools/codegen/README.md)。
- 公共 API 只用生成器支持的类型。用了不支持的类型，生成时会打印 `skipped` 警告，
  该方法不会出现在任何 binding 里。

## 与 Sparkle 的关系

dazzle 是重新设计的更新框架，只在 feed 这一层兼容 Sparkle：读取 Sparkle 格式的 appcast
（RSS 2.0 + `sparkle:` 扩展），已有的 appcast 无需修改。

- 不复刻 Sparkle / WinSparkle 的 API，API 按本规范自行设计。
- 不读取它们的配置约定，例如 macOS Info.plist 的 `SUFeedURL`、WinSparkle 的 `FeedURL`
  资源。配置一律通过 dazzle 自己的 API 完成（如 `Updater::SetFeedUrl()`）。
