# CodeEditor API 参考

CodeEditor 是 HuxerUI 的声明式 View，挂载后的节点持有编辑器 Core、平台文本输入、Provider 和交互状态。公开接口见 [Controller](../include/huxerui/codeeditor/controller.h)、[数据模型](../include/huxerui/codeeditor/model.h)、[Provider](../include/huxerui/codeeditor/providers.h) 和[事件](../include/huxerui/codeeditor/events.h)。

Controller 和文档句柄在 UI 线程使用。行列从 0 开始，列和平台文本偏移按 UTF-16 代码单元计数，字符串使用 UTF-8。文本范围是左闭右开区间，Provider 的行范围包含起止行。

## 挂载和文档

在 composition 中调用 `UseCodeEditor()` 创建稳定句柄，并传给 `CodeEditor(controller, options)`。Controller 和它的副本共享一次绑定。从首次 `DocumentLoaded` 事件起可以发送命令。首次事件前和卸载后，写入返回 `false`，查询返回空值。再次挂载时创建新的 Controller。

`EditorOptions::document` 接收 UTF-8 字符串或共享的 `Document` 句柄，默认为空字符串。首次挂载或字符串值改变时，编辑器将文本包装成内存文档。相同的字符串或同一个文档句柄会沿用当前文档及编辑历史。传入不同的句柄或调用 `LoadDocument` 会切换文档并发布 `DocumentLoaded`，设置、主题、键位、元数据和 Provider 随会话保留。

`Document(text)` 构造内存文档，`Document::FromFile(file)` 打开本地映射文件。通过 `GetDocument()` 获取当前文档句柄，用 `GetText()`、`GetLineText()` 和 `GetLineCount()` 读取内容。

## 设置、主题与快捷键

`EditorOptions::settings` 提供声明式设置。命令式更新时，取得 `GetSettings()` 返回的句柄，读取当前设置，修改所需字段后用 `Apply(settings)` 生效。字体、缩放、换行、行距、边栏、只读、Tab、滚动条、选择手柄和装饰刷新策略都归入 `EditorSettings`。构造期的手势阈值、惯性、撤销容量和组合键超时由 `EditorOptions::core_options` 提供。

桌面端滚动条默认宽 6 个逻辑单位，内容溢出时持续可见。Android 和 iOS 默认宽 5 个逻辑单位，并扩大滑块的触摸命中范围。移动端滚动时用 300 ms 淡入，停止 700 ms 后用 300 ms 淡出。可通过 `EditorSettings::scrollbar` 覆盖这些默认值。

Android 和 iOS 默认让边栏跟随正文横向滚动，全选后将选区末端滚入视口。桌面和 Web 默认固定边栏，全选时保持当前视口。行号区两侧的边距各为 10 个逻辑单位，正文从边栏后 3 个逻辑单位开始，默认行高为字体高度的 1.2 倍。

默认使用 `EditorTheme::Dark()`。`Dark()` 和 `Light()` 的配色参照 SweetEditor 的 Android 和 Swing 实现。可用 `Everforest()` 选用 Everforest 配色，或用 `EditorTheme::FromThemeSpec()` 根据 HuxerUI 主题生成配色。用 `DefineTextStyle` 定义主题内的语法样式，也可在会话中用 `RegisterTextStyle` 和 `RegisterBatchTextStyles` 注册。标准样式 ID 为 1 到 12，自定义样式从 100 起。`SetEditorIconProvider` 提供边栏、行内提示和选择菜单的图标。

`EditorKeyMap::DefaultKeyMap()` 使用 Core 的 VS Code 风格默认键位。`KeyBinding` 支持单键和双段组合键。`RegisterCommand` 注册自定义命令，处理函数接收键位及 Controller。`SetKeyMap` 应用到会话。

## Controller 操作

| 能力 | 入口 |
| --- | --- |
| 编辑和撤销 | `InsertText`、`InsertTextAt`、`ReplaceText`、`DeleteText`、`ApplyTextEdits`、行移动／复制／插入／删除、`Undo`、`Redo` |
| 光标和选区 | `SetCursorPosition`、`SetSelection`、`SelectAll`、`GetCursorPosition`、`GetSelection`、`GetSelectedText`、`GetWordRangeAtCursor`、`GetWordAtCursor` |
| 导航和滚动 | `GotoPosition`、`ScrollToLine`、`SetScroll`、`GetScrollMetrics`、`GetPositionRect`、`GetCursorRect` |
| 折叠 | `SetFoldRegions`、`FoldAt`、`UnfoldAt`、`ToggleFold`、`FoldAll`、`UnfoldAll`、`IsLineVisible` |
| 搜索和替换 | `Search`、`GetSearchState`、`FindNextSearchMatch`、`FindPreviousSearchMatch`、`ReplaceCurrentSearchMatch`、`ReplaceAllSearchMatches`、`ClearSearch` |
| 剪贴板 | `CopyToClipboard`、`CutToClipboard`、`PasteFromClipboard` |
| 片段和 Diff | `InsertSnippet`、联动编辑 Tab 停靠点、`SetDiffOriginal` |

`ApplyTextEdits` 的所有位置按编辑前的文档填写。Core 校验范围冲突，并以单次撤销记录应用整批编辑。应用可以通过 `SearchState` 展示搜索状态、结果数量及错误。剪贴板能力取决于 HuxerUI 平台服务。用 `SetDiffOriginal` 设置 UTF-8 比较基线。传 `std::nullopt` 清除 Diff，传空字符串则与空文档比较。

## Provider

在 `EditorOptions` 中传入 Provider，或通过 Controller 增删实例。`LanguageConfiguration` 提供语言标识及可选的括号、自动闭合和缩进配置。`EditorMetadata` 是应用持有的 `std::any`，会随每次请求传给 Provider。

### 装饰

`DecorationProvider::GetCapabilities` 声明提供的装饰类别。`ProvideDecorations` 收到可见范围、预取范围、文档快照、增量编辑、语言及元数据。`DecorationReceiver` 支持同一次请求多次提交，也可以从工作线程调用。结果在 UI 线程应用。请求被替换、Provider 被移除、文档切换或组件卸载时，旧接收器会取消。

每一类装饰独立设置应用模式。`Merge` 追加同类数据，`ReplaceRange` 替换可见行范围内的数据，`ReplaceAll` 替换整类数据。合并多个 Provider 时优先级依次是 `ReplaceAll`、`ReplaceRange`、`Merge`。如果已分析的版本等于 `previous_text_revision`，可以重放增量编辑，否则从 `document_text` 重建。

应用数据变化时用 `RequestDecorationRefresh` 请求刷新。也可以直接通过 Controller 设置或清除样式区间、行内提示、诊断、链接、CodeLens、边栏图标、折叠、引导线及其他装饰。[自定义高亮示例](highlighting-without-sweetline.md)展示了一个独立 Provider。预览工程通过相同接口接入 SweetLine。

### 补全和换行

`CompletionProvider::ProvideCompletions` 返回 `Task<CompletionResult>`。当前请求的结果按 `sort_key` 排序，空排序键使用 `label`。`is_incomplete` 标记后续输入需要继续查询的列表。`IsTriggerCharacter` 定义自动触发字符。`TriggerCompletion` 将焦点交给编辑器并请求候选，`ShowCompletionItems` 显示指定候选，`DismissCompletion` 关闭弹窗。

`SetCompletionItemRenderer` 接收一个工厂函数。函数根据候选项和选中状态返回补全行的 HuxerUI `View`。使用不可聚焦的行内容，让键盘输入留在编辑器中。选中、确认和滚动由编辑器处理。传入空工厂可恢复默认行。

补全优先使用 `text_edit`，其次使用 `insert_text` 和 `label`。只有 `text_edit` 决定替换范围，省略时从光标插入。普通补全及 `additional_text_edits` 作为一个编辑批次。`InsertTextFormat::Snippet` 交由 Core 展开并管理联动停靠点。

`NewLineActionProvider` 同步执行，按注册顺序取第一个有值的结果。所有 Provider 都返回空时，Enter 使用 Core 的默认缩进。

## 事件和交互

在返回的 View 上用 `.On<EditorEvents::...>(handler)` 绑定事件。`TextChanged` 携带增量编辑、编辑类型及动作来源。光标、选区、滚动和缩放事件携带当前状态。折叠、图标、行内提示、CodeLens、链接、长按、双击和上下文菜单等事件携带编辑器本地坐标。

右键打开 HuxerUI 上下文菜单。触屏选区使用相对可见选区居中的横向工具条。通过 `SetContextMenuItemProvider` 和 `SetSelectionMenuItemProvider` 提供当次菜单的完整操作列表。自定义操作触发对应的类型化事件。内置 Open Link 通过 `LinkClicked` 将目标交给宿主打开。

`ShowInlineSuggestion` 在指定位置展示行内建议。Tab 接受，Escape 关闭，输入或移动光标会关闭建议。接受和关闭均有对应事件。

平台文本输入命令按 HuxerUI 批次一次提交给 Core。Core 结果统一驱动事件、输入状态、动画和重绘。输入几何使用编辑器本地坐标，由 HuxerUI 完成窗口坐标转换。
