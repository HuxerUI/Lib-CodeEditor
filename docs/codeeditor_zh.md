# CodeEditor API 参考

头文件：`<huxerui/codeeditor.h>` · 命名空间：`huxerui::codeeditor` · CMake：`huxerui_use_library(app TARGET CodeEditor::CodeEditor PATH ...)`

## 1. 组件入口

```cpp
huxerui::View CodeEditor(CodeEditorOptions options = {}, CodeEditorController controller = {});
```

返回一个携带 retained 编辑器扩展的声明式 `View`。可以放在任何 `View` 适用的位置：

```cpp
[[huxerui::composable]]
View EditorPage() {
  const auto controller = huxerui::codeeditor::UseCodeEditorController();
  huxerui::codeeditor::CodeEditorOptions options;
  options.initial_text = "int main() { return 0; }";
  return huxerui::codeeditor::CodeEditor(std::move(options), controller).With(Grow{});
}
```

## 2. CodeEditorOptions（选项）

### 文档

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `initial_text` | `std::string` | `""` | 初始 UTF-8 文档内容。编辑器挂载或 `document_key` 变化时加载。**不是**受控值——挂载后文本由 retained 内核持有；用 `CodeEditorEvents::TextChanged` 监听变更，用 `Controller::Text()` / `LoadDocument()` 读取或替换。 |
| `document_key` | `std::string` | `""` | 文档稳定标识。重组间改变它会重新加载 `initial_text` 并重建编辑器状态（撤销历史、光标、折叠重置）。留空保持单一默认文档。不同 key 实现多文档切换。 |

### 排版

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `font_family` | `std::string` | `""` | 正文字体族。空 = 平台等宽字体。命名族优先解析平台打包字体（Android 为 `assets/fonts/<族名>.ttf`），找不到回退系统字体表。变更实时生效，不丢失编辑状态。 |
| `font_size` | `float` | `14.0` | 正文字号（逻辑像素）。 |
| `line_spacing_add` | `float` | `0.0` | 每行额外增加的行距。 |
| `line_spacing_mult` | `float` | `1.2` | 行高倍率。 |
| `theme` | `std::optional<CodeEditorTheme>` | `{}` | 显式视觉覆盖。为空时编辑器从环境 `UseTheme()` 派生，自动跟随 `MaterialTheme` 亮/暗切换。详见 §3。 |

### 编辑行为

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `read_only` | `bool` | `false` | 拒绝所有文本修改操作。选区和导航仍可用。 |
| `tab_size` | `uint32_t` | `4` | 一个缩进级的空格宽度。 |
| `backspace_unindent` | `bool` | `true` | 行首退格回退到上一个缩进位。 |
| `insert_spaces` | `bool` | `true` | Tab 插入空格而非制表符。 |
| `auto_closing_pairs` | `std::vector<std::pair<char32_t,char32_t>>` | `{}` | 自动闭合括号对。为空使用内核默认。 |

### 补全

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `completion_provider` | `CompletionProvider` | `{}` | 接收 `CompletionContext`，返回 `std::vector<CompletionItem>`。见 §7。 |
| `completion_trigger_characters` | `std::function<bool(const std::string&)>` | `{}` | 输入给定字符时是否触发补全请求。 |

### 装饰

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `decoration_providers` | `std::vector<std::shared_ptr<CodeEditorDecorationProvider>>` | `{}` | 装饰来源；编辑器合并所有结果。见 §5。 |
| `accept_phantom_on_tab` | `bool` | `true` | Tab 是否提交光标行幽灵文本。 |

### 钩子与 Diff

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `newline_action` | `std::function<std::string(uint32_t,uint32_t)>` | `{}` | 插入换行前调用。返回替换文本或空串用默认。 |
| `original_text` | `std::string` | `""` | Diff 基准文本。非空时计算行级 diff。空则关闭。 |

### 显示

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `render_whitespace` | `bool` | `false` | 空白符标记。 |
| `render_line_breaks` | `bool` | `false` | 换行符显示。 |
| `wrap_mode` | `int` | `0` | `0` 不换行 / `1` 字符换行 / `2` 单词换行。 |
| `sticky_gutter` | `bool` | `false` | 水平滚动时行号区固定。 |
| `scrollbar_thickness` | `float` | `0.0` | 滚动条厚度。 |
| `scrollbar_mode` | `int` | `0` | `0` 常显 / `1` 滚动时 / `2` 不显示。 |
| `content_start_padding` | `float` | `0.0` | 行号区与文本间距。 |

## 3. CodeEditorTheme（主题）

所有颜色为 `huxerui::Color`。工厂：`Default()`（亮色参考）、`FromThemeSpec(const ThemeSpec&)`（环境派生）。含 `operator==`。

**表面**：`background`、`gutter_background`、`current_line_background`（聚焦行高亮）、`separator_color`。

**文本与光标**：`text_foreground`、`line_number_color`、`caret_color`、`caret_width`（默认 2.0）。

**链接与 CodeLens**：`link_color`、`active_link_color`、`codelens_color`、`active_codelens_color`。

**选区与高亮**：`selection_background`、`search_match_background`、`search_current_background`、`bracket_match_background`、`document_highlight_text/read/write`、`ime_composition_underline`。

**诊断**：`diagnostic_error/warning/info/hint_underline`。

**Diff**：`diff_added/removed_background`、`diff_added/removed_gutter`。

**装饰**：`indent_guide_color`、`inlay_hint_background/text`、`fold_placeholder_background/text`、`gutter_icon_color`。

**语法调色板**：`syntax_keyword/type/class/function/variable/string/number/comment/preprocessor/builtin/punctuation/annotation/url`、`syntax_rainbow`（8 色彩虹括号）。

**补全面板**：`completion_background/border/selected_background/label/detail`。

```cpp
auto theme = huxerui::codeeditor::CodeEditorTheme::Default();
theme.background = Color::Rgb(30, 30, 46);
theme.text_foreground = Color::Rgb(205, 214, 244);
theme.syntax_keyword = Color::Rgb(203, 166, 247);
options.theme = theme;
```

## 4. CodeEditorEvents（事件）

所有事件通过 `.On<事件>(处理器)` 绑定在 `CodeEditor()` 返回的 `View` 上。行/列参数均为 **0 基**；显示给用户时 `line + 1`。

| 事件 | 处理器签名 | 参数说明 |
|---|---|---|
| `TextChanged` | `void()` | 任何文本编辑后触发。 |
| `CursorChanged` | `void(uint32_t line, uint32_t column)` | `line` 光标行（0 基）；`column` 光标列（0 基）。 |
| `SelectionChanged` | `void(uint32_t sl, uint32_t sc, uint32_t el, uint32_t ec)` | 选区起点到终点（0 基，已归一化）。 |
| `ScrollChanged` | `void(float x, float y)` | 水平/垂直滚动偏移（逻辑像素）。 |
| `FoldToggled` | `void(std::size_t line)` | 被展开/折叠的区域起始行（0 基）。 |
| `LongPressed` | `void(uint32_t line, uint32_t column)` | 长按位置（0 基）。 |
| `DoubleTapped` | `void(uint32_t line, uint32_t column)` | 双击位置（0 基）。 |
| `LinkClicked` | `void(const std::string& url)` | 被点击链接的 URL 字符串。 |
| `CodeLensClicked` | `void(int32_t command_id)` | provider 分配的命令 id。 |
| `GutterIconClicked` | `void(uint32_t line, int32_t icon_id)` | `line` 行（0 基）；`icon_id` provider 分配的图标 id。`1` 菱形 / `2` 圆形 / 其他 圆点。 |
| `InlayClicked` | `void(uint32_t line, uint32_t column)` | 被点击内联提示的位置（0 基）。 |

```cpp
CodeEditor(options, controller)
    .On<CodeEditorEvents::CursorChanged>([status](uint32_t line, uint32_t col) {
      status = "行 " + std::to_string(line + 1) + ", 列 " + std::to_string(col + 1);
    })
    .On<CodeEditorEvents::GutterIconClicked>([this](uint32_t line, int32_t id) {
      if (id == 2) ToggleBreakpoint(line);
    });
```

## 5. CodeEditorDecorationProvider（装饰提供者）

```cpp
class CodeEditorDecorationProvider {
 public:
  virtual ~CodeEditorDecorationProvider() = default;
  virtual CodeEditorDecorationResult ProvideDecorations(
      const CodeEditorDecorationContext& context) = 0;
};
```

### 上下文（Context）

| 字段 | 类型 | 说明 |
|---|---|---|
| `visible_start_line` | `uint32_t` | 可见首行（0 基，含）。 |
| `visible_end_line` | `uint32_t` | 可见末行（0 基，含）。 |
| `total_line_count` | `uint32_t` | 文档总行数。 |
| `cursor_line` | `uint32_t` | 光标行（0 基）。 |
| `cursor_column` | `uint32_t` | 光标列（0 基）。 |
| `viewport_settled` | `bool` | 快速滚动中 `false`（跳过重计算）；稳定后 `true`。 |
| `document_text` | `const std::string*` | 完整 UTF-8 文档文本。可能 `nullptr`。仅调用期间有效。 |
| `text_changes` | `std::vector<CodeEditorTextChange>` | 上次刷新以来的增量编辑。 |

### 增量变更（TextChange）

`start_line/start_column/end_line/end_column`（编辑前坐标）、`new_text`（替换文本，删除为空）。

### 结果（Result）

| 字段 | 类型 | 说明 |
|---|---|---|
| `syntax_spans` | `LineEntries<StyleSpan>` | 逐行语法 token。 |
| `overlay_spans` | `LineEntries<StyleSpan>` | 彩虹括号。 |
| `document_highlights` | `LineEntries<StyleSpan>` | 同词高亮。 |
| `inlay_hints` | `LineEntries<InlayHint>` | 内联提示。 |
| `diagnostics` | `LineEntries<Diagnostic>` | 诊断波浪线。 |
| `code_lens` | `LineEntries<CodeLens>` | 可点击命令。 |
| `links` | `LineEntries<Link>` | 可点击 URL。 |
| `gutter_icons` | `LineEntries<GutterIcon>` | 行号区图标。 |
| `phantom_texts` | `LineEntries<PhantomText>` | 幽灵文本（Tab 提交）。 |
| `indent_guides` | `std::vector<IndentGuide>` | 缩进线。 |
| `fold_regions` | `std::vector<FoldRegion>` | 可折叠区域（每文档一次）。 |
| `matched_bracket` | `std::optional<BracketMatch>` | 光标下的括号对。 |

### 条目类型

- **StyleSpan** — `{column, length, style}`。`style` 通过主题语法调色板解析颜色。
- **InlayHint** — `{column, text}`。
- **Diagnostic** — `{column, length, severity, message}`。severity：`0` 错误 / `1` 警告 / `2` 信息 / `3` 提示。
- **CodeLens** — `{column, command_id, title}`。
- **Link** — `{column, length, url}`。
- **GutterIcon** — `{icon_id}`。`1` 菱形 / `2` 圆形 / 其他 圆点。
- **PhantomText** — `{column, text}`。
- **IndentGuide** — `{start_line, end_line, column}`。
- **FoldRegion** — `{start_line, end_line}`。
- **BracketMatch** — `{line, column, partner_line, partner_column}`。

### 刷新模型

| 事件 | 策略 |
|---|---|
| 视口建立（首帧/切文档/键盘） | 全量 settled，同帧渲染 |
| 快速滚动 | 轻量（语法+缩进线），停稳后全量 |
| 编辑或光标移动 | 全量 settled |
| 主题或字体切换 | 热应用（不丢状态） |
| 文档切换 | 编辑器重建 |

参考实现：`examples/preview/src/sweetline_provider.cpp`

## 6. CodeEditorController（控制器）

```cpp
inline CodeEditorController UseCodeEditorController();
```

| 方法 | 返回值 | 说明 |
|---|---|---|
| `IsConnected()` | `bool` | 编辑器已挂载并绑定。 |
| `LoadDocument(key, text)` | `bool` | 替换文档（撤销/光标/折叠重置）。 |
| `Text()` | `std::string` | 完整文档文本（UTF-8）。 |
| `SetCursor(line, column)` | `bool` | 移动光标（0 基），自动滚入可见。 |
| `RunSearch(pattern)` | `bool` | 执行搜索，高亮全部匹配。 |
| `FindNext()` | `bool` | 下一个匹配。 |
| `FindPrevious()` | `bool` | 上一个匹配。 |
| `ReplaceCurrent(replacement)` | `bool` | 替换当前匹配并前进。 |
| `ReplaceAll(replacement)` | `bool` | 替换所有匹配。 |
| `ClearSearch()` | `bool` | 清除搜索高亮。 |
| `ToggleSearch()` | `bool` | 显示/隐藏搜索栏（也可 Ctrl+F）。 |

## 7. 补全类型

### CompletionContext

`trigger_kind`（`Invoked` 0 / `Character` 1 / `Retrigger` 2）、`trigger_character`、`cursor_line/column`、`line_text`（光标行文本）、`word_start/end`（单词范围）。

### CompletionItem

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `label` | `std::string` | — | 显示标签。 |
| `detail` | `std::string` | — | 详情（右侧）。 |
| `insert_text` | `std::string` | — | 确认时插入的文本。为空用 `label`。 |
| `kind` | `CompletionItemKind` | `Text` | 徽章种类。 |
| `insert_text_is_snippet` | `bool` | `false` | Snippet 模板。 |
| `has_text_edit` | `bool` | `false` | 替换范围而非在光标处插入。 |
| `text_edit_start/end` | `uint32_t` | `0` | 替换范围（行内列号）。 |
| `text_edit_text` | `std::string` | — | 替换文本。 |

Kind：`Keyword`(0) `Function`(1) `Variable`(2) `Class`(3) `Interface`(4) `Module`(5) `Property`(6) `Snippet`(7) `Text`(8)。

## 8. 架构

```text
CodeEditor() -> Canvas -> CodeEditorBehavior -> Extension(NodeExtension) -> EditorHolder -> SweetEditor EditorCore
```

## 9. 限制

- `initial_text` 是初始化器，不是完全受控值。
- 色块 inlay 和分隔线尚未暴露。
- 折叠区域归 provider 所有；编辑器保留交互式折叠状态。

## 10. 验证清单

挂载、重组、文档切换、卸载、编辑、剪贴板、撤销重做、中文 IME、Emoji、光标闪烁、选区、滚动、折叠、括号、高亮、补全、Snippet、搜索替换、Diff、诊断、Inlay、CodeLens、行号区点击、双指缩放、自定义字体、主题切换、Android arm64-v8a 构建。
