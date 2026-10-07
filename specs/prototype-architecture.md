# Prototype 架构规则

本文件适用于 `prototype/` 下的开发与维护。

## 定位与边界

- Prototype（`prototype/`，基于 Storybook）承载 dazzle updater 更新界面的交互原型、功能视图和组件展示，使用本地示例数据与模拟行为。
- 原型以 spec 为准：功能、状态、按钮和文案与对应的 spec 一致（如 `updater-ui` 对应
  [updater-ui.md](updater-ui.md)），命名沿用 spec 的模块名和状态名，不使用 Sparkle 的类名。
  窗口尺寸、边距、间距等视觉细节参照 Sparkle（`SUUpdateAlert.xib`、`SUStatusController.m`），
  即平台自身的度量。
- 不接入真实的 release index、下载、签名校验或安装，不引入凭据与生产集成适配器。
- 不依赖 `core/` 或任何 binding；原型描述的是 Updater 的状态（见 [updater.md](updater.md)），
  而不是调用它。
- 以系统原生外观为主：`src/components/ui/` 是按桌面绘制的原生控件（macOS 取 AppKit，分 macOS 27 的 Liquid Glass 与 macOS 15 的扁平外观两代，Windows 取
  WinUI 3 / Fluent 2，Linux 取各桌面的主题：GNOME Adwaita、Ubuntu Yaru、KDE Breeze、Omarchy），
  所有取值来自 `src/styles/native.css` 中按 `data-platform`、`data-desktop` 与 `data-appearance`
  定义的 token，不写字面量。桌面与明暗是两个独立的工具栏选项；Omarchy 在桌面选项里按主题
  展开为二级（`omarchy-<主题>`），取值来自 basecamp/omarchy 各主题的 `colors.toml`。明暗对所有桌面
  生效：Omarchy 主题在自身的明暗下原样显示，另一种明暗由主题颜色推导，并在文档中注明这是原型的推导。
- `src/components/ui/` 只保留用到的控件，不预先建立用不到的组件。

## 目录职责

```text
prototype/
├── .storybook/                 # Story 扫描、全局装饰器、主题工具栏与排序
├── stories/                    # Introduction 等 MDX 文档
└── src/
    ├── styles/
    │   ├── native.css          # 各平台、明暗外观的原生 token
    │   ├── globals.css         # 全局样式
    │   └── themes.ts           # 桌面与明暗外观的列表与切换逻辑
    ├── components/             # 跨功能共享组件及其 Story、局部样式
    │   └── ui/                 # 原生控件：窗口、按钮、进度条、应用图标
    └── features/
        └── <feature>/
            ├── <feature>-view.tsx
            ├── <feature>-view.stories.tsx
            ├── <feature>-view.css
            ├── components/     # 功能内部组件及其 Story、局部样式
            ├── data.ts         # 静态示例数据
            ├── types.ts        # 功能共享类型
            ├── index.ts        # 功能对外导出
            └── download.ts     # 模拟行为，按具体职责命名
```

树中功能文件按需创建，不为尚不存在的职责建立空目录或空文件。

- dazzle updater 是更新框架而不是应用，原型不设宿主应用（没有 `App`、`AppLayout`）：一个功能的完整流程
  由它的 `<Feature>View` 承载，放在 `Examples` 下，以宿主应用的形式展示（如 `Examples/Host App`）。
- `src/components/` 中的组件不依赖具体功能。
- 功能内部组件放在所属功能的 `components/`，跨功能复用后再提升到 `src/components/`。
- Story 与对应组件就近存放；`stories/` 不集中存放组件 Story。
- 全局样式和主题放入 `styles/`；组件与功能视图的局部样式留在组件旁。

## 命名

- 文件和目录使用小写 kebab-case，如 `updater-view.tsx`、`window-frame.stories.tsx`。
- React 组件使用 PascalCase，功能入口统一为 `<Feature>View`，不使用 `Screen` 后缀。
- Hook 使用 `useXxx`，对应文件使用 `use-xxx.ts`。
- 示例数据统一使用功能内的 `data.ts`，不使用 `fixtures.ts`，不集中到顶层数据目录。
- 模拟行为不是静态数据。按行为命名文件，例如 `check.ts`、`download.ts`、`install.ts`，
  函数使用 `simulateCheck`、`simulateDownload` 等明确名称，不建立顶层 `mocks/`。

## 依赖与状态

- 依赖方向为：功能视图组合内部组件与共享组件，二者都使用 `src/components/ui/`。
- `src/components/ui/` 不依赖 prototype 的任何其它目录；共享组件不反向依赖具体功能；功能不导入
  其他功能的内部实现。
- 功能内部的交互状态留在该功能；布局组件主要负责结构和插槽。
- 外部使用功能组件和类型时通过功能的 `index.ts`；Story 可直接引用功能的 `data.ts`
  和模拟行为以装配示例场景。
- `index.ts` 只导出需要对外使用的组件与类型，不导出 Story。
- 在 `types.ts` 中定义共享行为类型（如 `CheckForUpdates`、`DownloadUpdate`），避免用
  `typeof simulateDownload` 将组件接口绑定到模拟实现。
- 示例数据在交互中作为初始值使用，不直接修改共享导出的数据。

## Storybook 侧边栏

Story 的 `title` 明确指定展示层级，不依赖文件路径自动生成。侧边栏按内容类型分三个根分组
（Storybook 把根分组画成不可折叠的小标题；title 至少两段才成为根分组），任何 Story 只需展开一层：

| 内容 | title 约定 | 示例 |
| --- | --- | --- |
| 示例：在宿主应用里跑完整、可交互的流程 | `Examples/<Example>` | `Examples/Host App` |
| 组件：一个窗口或窗口的一部分，逐个状态展示 | `Components/<Component>` | `Components/Update Available Window` |
| 参考实现（spec 尚未采纳） | `Reference/<Source>` | `Reference/Sparkle` |

- 每个需要展示的功能入口必须有独立的 `*.stories.tsx`；只有组件或 `index.ts` 不会生成侧边栏入口。
- 示例下用 Story 表达可交互的流程与结果，如 `Check for Updates`、`Error · Signature`；单个状态放在
  对应组件的 Story 下，名称取 spec 的状态名，如 `Checking`、`Ready`。
- spec 尚未采纳的参考行为（如 Sparkle 的重要更新、Dock 提醒）只放在 `Reference/Sparkle` 下，
  不进入 `Components` 和 `Examples`，以免被当成已定的行为；每个 Story 注明所依据的源文件。
  spec 采纳后移到对应组件的 Story，并删去参考 Story。
- 在 `.storybook/preview.tsx` 中维护排序：`Introduction`、`Examples`、`Components`（按流程中出现的先后）、
  `Reference`。
- Story 使用唯一、稳定的显式 `id`。单纯移动文件或调整分组时保留 ID，避免已有预览链接失效。
- Story 应展示有意义的默认、空、运行或失败状态；仅为实际存在的状态添加场景。
- 交互演示使用本地状态和模拟回调；固定场景优先通过 args 配置。
- 每个窗口自己绘制窗口外壳（`Window`），画布是当前平台的桌面背景；Story 不再额外包裹窗口。

## 修改与验证

- 移动或重命名时同步更新 import、`index.ts`、类型名、CSS 类名、Story title、排序及相关文档。
- 新增功能时在 `Examples` 下添加示例，其中第一个 Story 是可交互的完整流程。
- 代码变更后在 `prototype/` 下运行：

  ```sh
  pnpm typecheck
  pnpm build
  ```

- 修改 Story 注册或分组后，检查构建产物 `prototype/storybook-static/index.json` 中的 title 与 ID；
  涉及布局或交互时在 Storybook 中验证对应场景。
- 不手工修改或提交 `storybook-static/`、调试日志等生成产物。
