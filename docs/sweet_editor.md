# SweetEditor HuxerUI 组件

## 1. 简介

`SweetEditor` 是基于 HuxerUI 的跨平台代码编辑器组件。它把 HuxerUI 的声明式 View、布局、焦点、文本输入、绘制和生命周期，与 `3dparty/SweetEditor` 编辑器内核和 `3dparty/SweetLine` 语法高亮引擎组合起来。

```text
HuxerUI
  -> sweetedit_huxer 组件层
      -> SweetEditor EditorCore
      -> SweetLine HighlightEngine
```

HuxerUI 负责 View 组合、Runtime、布局、焦点、平台输入法、剪贴板、绘制和帧调度；SweetEditor 负责文档、光标、选区、编辑、撤销重做、搜索替换、补全、Snippet、折叠、Diff 和渲染模型；SweetLine 负责 JSON 语法规则、增量高亮、折叠区域、缩进线、括号和彩虹括号分析。

组件不依赖 Android `EditText`、UIKit 原生文本框、Win32 编辑框或 Web DOM 编辑器。不同平台共享 C++ 编辑器逻辑，并通过 HuxerUI PlatformAdapter 接入平台服务。

## 2. 源码结构

```text
project/sweetedit_huxer/
├── CMakeLists.txt
├── src/sweet_editor/
│   ├── sweet_editor.h
│   ├── sweet_editor.cpp
│   ├── sweetline_highlighter.h
│   └── sweetline_highlighter.cpp
├── resources/raw/
├── platform/
└── 3dparty/
    ├── SweetEditor/
    └── SweetLine/
```

组件声明当前位于 `src/sweet_editor/sweet_editor.h`，实现位于 `src/sweet_editor/sweet_editor.cpp`。当前它是项目内组件，还不是独立安装的 HuxerUI SDK library。

## 3. 最小使用

```cpp
#include <huxerui/huxerui.h>
#include "sweet_editor/sweet_editor.h"

using namespace huxerui;
using namespace sweetedit_huxer;

View App() {
  SweetEditorOptions options;
  options.document_key = "hello.cpp";
  options.initial_text =
      "#include <iostream>\n"
      "int main() {\n"
      "  std::cout << \"Hello\";\n"
      "  return 0;\n"
      "}\n";

  return SweetEditor(std::move(options)).With(Grow{});
}

const Application application{
    App,
    {.window = {.title = "SweetEditor", .initial_size = {900.0F, 640.0F}}},
};
```

`syntax_json` 为空时使用内置 C++ 语法资源 `raw/syntaxes/cpp.json`。正式使用建议始终指定稳定的 `document_key`，并提供与文件类型匹配的语法 JSON。

## 4. HuxerUI 组件架构

### 4.1 声明式入口

```cpp
[[huxerui::scope]]
View SweetEditor(SweetEditorOptions options = {}, SweetEditorController controller = {});
```

组件作用域管理搜索栏显示状态、搜索文本等局部状态。返回的 `View` 是临时声明；编辑器内核和交互状态由挂载节点上的 retained extension 持有。

语义输出（光标、选区、点击等）通过 **typed events** 绑定在返回的 `View` 上（见第 7 节）；文档加载、全文读取、光标定位与搜索替换通过 **`SweetEditorController`** 程序化控制（见第 9 节）。

### 4.2 retained NodeExtension

编辑器行为挂载在 Canvas 节点的 `NodeExtension` 上。扩展负责创建 `EditorHolder`，并连接 `EditorCore`、Document、SweetLineHighlighter、HuxerUI TextMeasurer、剪贴板、TextInputClient 和 TextSelectionClient。

它实现以下 HuxerUI 生命周期入口：

- `OnPointer()`：鼠标、触摸、拖动、滚动条和编辑器手势；
- `OnKey()`：键盘命令和编辑快捷键；
- `OnFocusChanged()`：焦点和光标状态；
- `OnFrame()`：光标闪烁和 SweetEditor 动画；
- `Paint()`：绘制 `EditorRenderModel`；
- `GetTextInputClient()`：接入 IME；
- `GetTextSelectionClient()`：接入选择和选择手柄。

```text
SweetEditor()
  -> Canvas View
  -> SweetEditorBehavior retained modifier
  -> SweetEditorBehavior::Extension
  -> EditorHolder
  -> EditorCore + Document + SweetLineHighlighter
```

状态变化使用 `InvalidatePaint()` 通知 HuxerUI Runtime，而不是依靠 Canvas 事件闭包强制重组。`OnFrame()` 返回 `FrameResult`，由 Runtime 统一安排下一帧。

## 5. SweetEditorOptions

完整声明见 `src/sweet_editor/sweet_editor.h`。

### 5.1 文档

```cpp
std::string initial_text;
std::string syntax_json;
std::string document_key;
```

`initial_text` 只在创建编辑器或 `document_key` 改变时加载，不是每次重组都会覆盖当前文本的受控值。

`syntax_json` 是 SweetLine 的 JSON 语法规则；为空时使用内置 C++ 规则。

`document_key` 是当前文档的稳定身份。改变它会重新创建 EditorCore、Document 和高亮器：

```cpp
options.document_key = "src/main.cpp";
```

多个文件必须使用不同 key，否则 Runtime 会将它们视为同一 retained 编辑器实例。

### 5.2 字体和行距

```cpp
float font_size = 14.0F;
float line_spacing_add = 0.0F;
float line_spacing_mult = 1.2F;
```

编辑器使用等宽字体，实际字体回退由 HuxerUI 当前平台的 TextMeasurer 决定。

### 5.3 编辑和缩进

```cpp
bool read_only = false;
uint32_t tab_size = 4;
bool backspace_unindent = true;
bool insert_spaces = true;
std::vector<std::pair<char32_t, char32_t>> auto_closing_pairs;
```

- `read_only`：禁止文本改变操作；
- `tab_size`：一个缩进级别的宽度；
- `backspace_unindent`：行首空白处退格时回退到上一个缩进位置；
- `insert_spaces`：Tab 插入空格而非制表符；
- `auto_closing_pairs`：配置自动闭合符号。

```cpp
options.tab_size = 2;
options.auto_closing_pairs = {
    {'(', ')'}, {'[', ']'}, {'{', '}'}, {'"', '"'},
};
```

为空时使用 SweetEditor 内核默认括号对。

## 6. 自动补全

### 6.1 Provider

```cpp
options.completion_provider = [](const CompletionContext& context) {
  if (context.trigger_kind == CompletionContext::TriggerKind::Character &&
      context.trigger_character == ".") {
    CompletionItem item;
    item.label = "size";
    item.detail = "size_t";
    item.insert_text = "size()";
    item.kind = CompletionItemKind::Function;
    return std::vector<CompletionItem>{std::move(item)};
  }
  return std::vector<CompletionItem>{};
};

options.completion_trigger_characters = [](const std::string& character) {
  return character == "." || character == ":";
};
```

触发类型为 `Invoked`（手动，例如 Ctrl+Space）、`Character`（触发字符）和 `Retrigger`（面板已打开时继续输入）。

### 6.2 替换范围

```cpp
CompletionItem item;
item.label = "std::vector";
item.insert_text = "std::vector<>";
item.has_text_edit = true;
item.text_edit_start = context.word_start;
item.text_edit_end = context.word_end;
item.text_edit_text = item.insert_text;
```

### 6.3 Snippet

```cpp
item.insert_text_is_snippet = true;
item.insert_text = "if (${1:condition}) {\n\t$0\n}";
```

SweetEditor 内核负责占位符展开和 Tab 停靠点切换。

## 7. 装饰和事件

编辑器语义输出通过 HuxerUI typed events 暴露。事件绑定在 `SweetEditor()` 返回的 View 上，与普通组件一致：

```cpp
SweetEditor(options)
    .On<SweetEditorLinkClicked>([](const std::string& url) {
      // 打开链接或显示提示
    })
    .On<SweetEditorCodeLensClicked>([](int32_t command_id) {
      // 执行命令
    })
    .On<SweetEditorGutterIconClicked>([](uint32_t line, int32_t icon_id) {
      // line 是 0-based 行号
    })
    .On<SweetEditorInlayClicked>([](uint32_t line, uint32_t column) {
      // 处理 Inlay hint
    })
    .On<SweetEditorTextChanged>([] {
      // 文档文本发生变化
    })
    .On<SweetEditorCursorChanged>([](uint32_t line, uint32_t column) {
      // 光标位置变化，0-based
    })
    .On<SweetEditorSelectionChanged>(
        [](uint32_t start_line, uint32_t start_column, uint32_t end_line, uint32_t end_column) {
          // 选区变化
        }
    )
    .On<SweetEditorScrollChanged>([](float scroll_x, float scroll_y) {
      // 滚动位置变化
    })
    .On<SweetEditorFoldToggled>([](std::size_t line) {
      // 折叠状态变化
    })
    .On<SweetEditorLongPressed>([](uint32_t line, uint32_t column) {
      // 长按
    })
    .On<SweetEditorDoubleTapped>([](uint32_t line, uint32_t column) {
      // 双击
    });
```

### 7.1 点击事件

```cpp
SweetEditor(options)
    .On<SweetEditorLinkClicked>([](const std::string& url) {
      // 打开链接或显示提示
    })
    .On<SweetEditorCodeLensClicked>([](int32_t command_id) {
      // 执行命令
    })
    .On<SweetEditorGutterIconClicked>([](uint32_t line, int32_t icon_id) {
      // line 是 0-based 行号
    })
    .On<SweetEditorInlayClicked>([](uint32_t line, uint32_t column) {
      // 处理 Inlay hint
    });
```

### 7.2 行号区图标

```cpp
options.gutter_icon_provider = [](uint32_t start_line, uint32_t end_line) {
  std::vector<std::pair<uint32_t, int32_t>> icons;
  icons.emplace_back(start_line, 2);
  return icons;
};
```

提供器应只返回可见范围内的图标。

### 7.3 编辑器事件

```cpp
SweetEditor(options)
    .On<SweetEditorTextChanged>([] {})
    .On<SweetEditorCursorChanged>([](uint32_t line, uint32_t column) {})
    .On<SweetEditorSelectionChanged>(
        [](uint32_t start_line, uint32_t start_column, uint32_t end_line, uint32_t end_column) {}
    )
    .On<SweetEditorScrollChanged>([](float scroll_x, float scroll_y) {})
    .On<SweetEditorFoldToggled>([](std::size_t line) {})
    .On<SweetEditorLongPressed>([](uint32_t line, uint32_t column) {})
    .On<SweetEditorDoubleTapped>([](uint32_t line, uint32_t column) {});
```

行号和列号均为 0-based。显示给用户时使用 `line + 1`。

## 8. 诊断、Inlay hint 和 CodeLens

```cpp
options.decoration_providers.push_back(
    [](uint32_t start_line, uint32_t end_line) {
      HostDecorationResult result;
      result.inlay_hints.push_back({
          .line = start_line, .column = 4, .text = ": int",
      });
      result.diagnostics.push_back({
          .line = start_line, .column = 0, .length = 3,
          .severity = 1, .message = "Example warning",
      });
      result.codelens.push_back({
          .line = start_line, .column = 0,
          .command_id = 1, .title = "Run",
      });
      return result;
    }
);
```

严重级别：`0 = error`、`1 = warning`、`2 = info`、`3 = hint`。提供器应尽量只计算请求的可见行范围，避免滚动时扫描整个文件。

## 9. 搜索、替换和 controller

`SweetEditor` 暴露一个 `SweetEditorController`，由组件 scope 创建并传给 `SweetEditor()`。controller 提供文档加载、全文读取、光标定位和搜索替换控制；未连接时方法返回 `false`。

```cpp
[[huxerui::scope]]
View Page() {
  auto controller = UseSweetEditorController();
  SweetEditorOptions options;
  // ...

  return Column {
    Row {
      Button("Find").OnClick([controller] { controller.ToggleSearch(); }),
      Button("Goto 5:1").OnClick([controller] {
        controller.SetCursor(4, 0);
      }),
    },
    SweetEditor(options, controller).With(Grow{}),
  };
}
```

### 9.1 controller API

```cpp
controller.IsConnected();

// 文档控制
controller.LoadDocument("main.cpp", source, cpp_syntax);
controller.Text();           // 当前全文（UTF-8）
controller.SetCursor(line, column);

// 搜索替换
controller.RunSearch("pattern");
controller.FindNext();
controller.FindPrevious();
controller.ReplaceCurrent("replacement");
controller.ReplaceAll("replacement");
controller.ClearSearch();
controller.ToggleSearch();    // 切换内置搜索栏
```

### 9.2 内置搜索栏

组件内置声明式搜索/替换栏，Ctrl+F 或 `controller.ToggleSearch()` 切换显示。内置栏包括 Find、Prev、Next、Replace、All 和 Close 控件，操作通过 controller 直接作用于当前文档，不再依赖内部回调桥。

## 10. Phantom text、换行和 Diff

```cpp
options.newline_action = [](uint32_t line, uint32_t column) {
  return std::string{};
};

options.phantom_text_provider = [](uint32_t line) {
  return line == 10 ? "  // generated suggestion" : "";
};
options.accept_phantom_on_tab = true;
```

Phantom text 是视觉建议，不会自动成为文档内容，除非用户接受它。

```cpp
options.original_text = original_source;
```

`original_text` 是 Diff 基准，当前文档仍可编辑。清空它即可关闭 Diff。

## 11. 显示和滚动条

```cpp
options.render_whitespace = true;
options.render_line_breaks = false;
options.wrap_mode = 1;
options.sticky_gutter = true;
options.scrollbar_thickness = 6.0F;
options.scrollbar_mode = 1;
options.content_start_padding = 8.0F;
```

`wrap_mode`：`0 = 不换行`、`1 = 按字符换行`、`2 = 按单词换行`。

`scrollbar_mode`：`0 = 始终显示`、`1 = 临时显示`、`2 = 隐藏`。厚度为 0 时使用内核默认值。

## 12. 多文件示例

```cpp
View EditorPage() {
  auto current_file = UseState(0);
  const char* keys[] = {"main.cpp", "example.java", "example.lua"};
  const std::string texts[] = {cpp_text, java_text, lua_text};
  const std::string syntaxes[] = {cpp_syntax, java_syntax, lua_syntax};
  const std::size_t index = static_cast<std::size_t>(current_file.Get());

  SweetEditorOptions options;
  options.document_key = keys[index];
  options.initial_text = texts[index];
  options.syntax_json = syntaxes[index];

  return Column {
    Row {
      Button("C++").OnClick([current_file] { current_file = 0; }),
      Button("Java").OnClick([current_file] { current_file = 1; }),
      Button("Lua").OnClick([current_file] { current_file = 2; }),
    }.With(Spacing(8.0F)),
    SweetEditor(std::move(options)).With(Grow{}),
  }.With(CrossAlign(CrossAxisAlignment::Stretch));
}
```

切换文件时必须改变 `document_key`。稳定 key 保留 retained 编辑器；新 key 创建新文档实例。

## 13. 构建集成

项目 CMake 会添加两个第三方库：

```cmake
add_subdirectory(3dparty/SweetEditor ...)
add_subdirectory(3dparty/SweetLine ...)
target_link_libraries(sweetedit_huxer PRIVATE
    SweetEditor::sweeteditor_static
    SweetLine::sweetline_static)
```

使用 `HUXERUI_HOME` 指定 HuxerUI 源码或 SDK。

### Android

```bash
cd project/sweetedit_huxer/platform/android
HUXERUI_HOME=/path/to/huxerui \
HUXERUI_HOST_TOOL_ROOT=/path/to/host-tools \
bash ./gradlew :app:buildCMakeDebug[arm64-v8a] --no-daemon
```

完整 APK：

```bash
bash ./gradlew :app:assembleDebug --no-daemon
```

当前工程按 Android API 23 边界配置，并需要 Android SDK、NDK、CMake、Gradle 和 HuxerUI host codegen 工具。

### 桌面、Web、iOS

Windows、Linux、macOS 使用 `huxerui::RunApplication()`；Web 使用 Emscripten WebAssembly；iOS 工程在 `platform/ios/`，三者都复用同一 C++ 组件。

## 14. Unicode 和输入法

应用文本使用 UTF-8 `std::string`，编辑器内核使用 UTF-16 编辑和组合文本语义。必须区分：

- UTF-8 字节长度不是光标列；
- UTF-16 offset 不是 Unicode grapheme 数量；
- Emoji 可能占多个 UTF-16 code unit；
- IME 组合范围必须来自 HuxerUI 文本输入契约；
- 应用层不要按原始 UTF-8 字节切割文档；
- 不要在每次重组时覆盖 `initial_text`。

组件已通过 HuxerUI TextInputClient 提供输入状态、选区、组合范围、输入几何位置和会话 revision。

## 15. 当前限制

`initial_text` 是初始化值，不是完整受控 `TextEditingValue`。编辑开始后，当前文档由 SweetEditor retained 内核持有；`SweetEditorTextChanged` typed event 可以通知应用内容变化，`SweetEditorController::Text()` 可以读取全文，`LoadDocument()` 可以程序化加载文档，`SetCursor()` 可以控制光标。当前还没有把文档文本作为双向受控值（每次输入都由应用回写）的完整受控模型，也没有多个组件共享同一个文档实例的 API。

编辑器语义输出已经通过 typed events 暴露（见第 7 节），文档和搜索控制通过 `SweetEditorController`（见第 9 节）。`SweetEditorOptions` 中剩余的 `std::function` 字段只属于 provider/service 语义（补全、装饰、行号区图标、Phantom text、换行策略），后续可以进一步迁移为 Environment/service。

## 16. 已知问题

- 首屏语法高亮：某些启动时序下，编辑器内容先以无高亮状态显示，需要滚动一次才出现高亮。正在修复中。

## 17. 修改后的验证清单

建议验证：挂载、重组、文档 key 切换、卸载、文本编辑、剪贴板、撤销重做、中文 IME、Emoji、光标闪烁、鼠标和触摸选择、滚动、折叠、括号、高亮、补全、Snippet、搜索、替换、Diff、诊断、Inlay hint、CodeLens、命令区点击（gutter/CodeLens 不弹键盘）、滑动不弹键盘，以及 Android `arm64-v8a` 原生编译。

```bash
git diff --check
git status --short
```

相关文档：

```text
../../docs/extending-huxerui.md
../../docs/design/architecture.md
../../docs/design/text-input.md
3dparty/SweetEditor/README_zh.md
3dparty/SweetEditor/docs/zh/api-editor-core.md
3dparty/SweetLine/README_zh.md
3dparty/SweetLine/docs/zh/api_core.md
3dparty/SweetLine/docs/zh/syntax_rule.md
```
