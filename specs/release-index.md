# Release index 格式（schema 1）

Release index 是 dazzle 自有的数据源格式，由 `ReleaseIndexSource` 读取：一个放在任意静态
HTTP(S) 地址上的 JSON 文件，约定命名为 `releases.json`，列出应用的
各个版本和每个版本在各平台的安装包。与 Sparkle appcast 无关。

## 示例

```json
{
  "schema": 1,
  "releases": [
    {
      "version": "1.4.0",
      "displayVersion": "1.4",
      "date": "2026-09-20T08:00:00Z",
      "releaseNotes": "## 新功能\n\n- 支持暗色模式\n- 修复启动崩溃",
      "releaseNotesUrl": "https://example.com/myapp/changelog#1.4.0",
      "assets": [
        {
          "platform": "macos",
          "arch": "universal",
          "minimumSystemVersion": "12.0",
          "url": "https://example.com/myapp/MyApp-1.4.0.zip",
          "size": 48213504,
          "format": "zip",
          "signature": "E6mJ3dHn...Base64...=="
        },
        {
          "platform": "windows",
          "arch": "x64",
          "minimumSystemVersion": "10.0.17763",
          "url": "https://example.com/myapp/MyApp-1.4.0-x64.exe",
          "size": 39845888,
          "format": "exe",
          "installerArguments": "/SILENT",
          "signature": "q7PLx0bY...Base64...=="
        }
      ]
    }
  ]
}
```

## 字段

### 顶层

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `schema` | 整数 | ✅ | 格式版本，当前为 `1`。大于客户端支持的版本时查询失败（`"unsupported release index schema"`） |
| `releases` | 数组 | ✅ | 版本列表，顺序不限 |

### release

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `version` | 字符串 | ✅ | 用于比较的版本号，规则见下 |
| `displayVersion` | 字符串 | | 展示给用户的版本号，缺省用 `version` |
| `date` | 字符串 | | 发布时间，RFC 3339；仅供展示，不参与选择 |
| `releaseNotes` | 字符串 | | 更新说明，Markdown |
| `releaseNotesUrl` | 字符串 | | 完整更新说明的网页地址 |
| `assets` | 数组 | ✅ | 各平台的安装包 |

### asset

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `platform` | 字符串 | ✅ | `macos`、`windows`、`linux`、`ios`、`android`、`ohos` |
| `arch` | 字符串 | | `x64`、`arm64`、`x86`、`universal`；缺省等同 `universal` |
| `minimumSystemVersion` | 字符串 | | 本机系统版本低于它时不适用 |
| `url` | 字符串 | ✅ | 安装包下载地址 |
| `size` | 整数 | | 字节数，用于展示；下载进度以 HTTP `Content-Length` 为准 |
| `format` | 字符串 | ✅ | `zip`（macOS，内含一个 `.app`）、`exe`、`msi`（Windows） |
| `installerArguments` | 字符串 | | `exe`/`msi` 安装程序的命令行参数 |
| `signature` | 字符串 | ✅ | 对安装包全部字节的 Ed25519 签名，Base64 |

未知字段一律忽略，以便在不升级 `schema` 的前提下增加可选字段。必填字段缺失或类型不对的
release/asset 被跳过，不导致整个查询失败。

## 选择规则

1. 对每个 release，在 `assets` 里找适用于本机的 asset：`platform` 等于本机平台；`arch` 等于
   本机架构，或为 `universal`；`minimumSystemVersion` 不高于本机系统版本。架构精确匹配的优先于
   `universal`。找不到则该 release 不适用。
2. 在适用的 release 中，取 `version` 严格大于当前版本的最高者。
3. 没有这样的 release，即"已是最新"。

## 版本号

`version` 为点分数字，可带 SemVer 风格的预发布后缀和构建元数据：

```
1.4  1.4.0  1.4.0.2  1.5.0-beta.1  1.5.0+20260920
```

比较规则：

1. 去掉 `+` 之后的构建元数据，它不参与比较。
2. 点分数字逐段按数值比较，段数不同时短的一方补 0（`1.4` == `1.4.0`）。
3. 数字部分相同时，无预发布后缀的大于有后缀的（`1.5.0` > `1.5.0-beta.1`）。
4. 预发布后缀按 SemVer 2.0 第 11 条比较（逐段：数字段按数值，其余按 ASCII，数字段小于非数字段）。

不符合此语法的 `version`（包括当前版本）视为无效：无效的 release 被跳过；当前版本无效时
查询失败。

## 签名

- 算法 Ed25519，对安装包文件的全部字节签名，签名与公钥均为 Base64。
- 公钥由应用通过 `Updater::SetPublicKey()` 内置，不出现在 release index 里。
- release index 本身不签名：篡改 release index 最多指向另一个同样需要有效签名的安装包，且版本必须高于当前版本，
  不能回退。release index 的签名留待以后（届时通过新增可选字段加入）。
- 签名工具：dazzle 以后提供 `dazzle sign` 命令；在此之前，任何能输出 Ed25519 原始签名的工具都可用
  （Sparkle 的 `generate_keys` / `sign_update` 产出的密钥与签名同样有效）。
