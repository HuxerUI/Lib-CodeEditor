# SweetEditor HuxerUI Component

## 1. Introduction

`SweetEditor` is a cross-platform code editor component built on HuxerUI. It combines HuxerUI's declarative View, layout, focus, text input, painting, and lifecycle with the editor core in `3dparty/SweetEditor` and the syntax highlighting engine in `3dparty/SweetLine`.

```text
HuxerUI
  -> sweetedit_huxer component layer
      -> SweetEditor EditorCore
      -> SweetLine HighlightEngine
```

HuxerUI owns View composition, the Runtime, layout, focus, platform IME, clipboard, painting, and frame scheduling; SweetEditor owns the document, caret, selection, editing, undo/redo, search and replace, completion, snippets, folding, diff, and the render model; SweetLine owns the JSON grammar rules, incremental highlighting, fold regions, indentation guides, brackets, and rainbow-bracket analysis.

The component does not depend on Android `EditText`, UIKit text views, Win32 edit controls, or Web DOM editors. Every platform shares the same C++ editor logic and reaches platform services through the HuxerUI PlatformAdapter.

## 2. Source Structure

```text
project/sweetedit_huxer/
├── CMakeLists.txt
├── include/sweetedit_core/
│   └── sweet_editor.h
├── src/sweet_editor/
│   ├── sweet_editor.cpp
│   ├── sweetline_highlighter.h
│   └── sweetline_highlighter.cpp
├── resources/raw/
├── examples/preview/
├── docs/
│   └── sweet_editor.md
└── 3dparty/
    ├── SweetEditor/
    └── SweetLine/
```

The public API is declared in `include/sweetedit_core/sweet_editor.h`; the implementation lives in `src/sweet_editor/sweet_editor.cpp`. The component is packaged as the `sweetedit_core` dependency library (see the root `CMakeLists.txt`), consumed by the demo app under `examples/preview/`.

## 3. Minimal Usage

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

When `syntax_json` is empty, the built-in C++ grammar resource `raw/syntaxes/cpp.json` is used. For real use, always specify a stable `document_key` and provide the syntax JSON matching the file type.

## 4. HuxerUI Component Architecture

### 4.1 Declarative Entry Point

```cpp
[[huxerui::composable]]
View SweetEditor(SweetEditorOptions options = {}, SweetEditorController controller = {});
```

The component scope manages local state such as the search bar visibility and search text. The returned `View` is a transient declaration; the editor core and interaction state are held by a retained extension on the mounted node.

Semantic output (caret, selection, clicks, and so on) is bound to the returned `View` through **typed events** (see section 7); document loading, full-text reads, caret positioning, and search and replace are controlled programmatically through **`SweetEditorController`** (see section 9).

### 4.2 Retained NodeExtension

The editor behavior is attached to a `NodeExtension` on the Canvas node. The extension creates an `EditorHolder` and wires together `EditorCore`, the Document, the SweetLineHighlighter, the HuxerUI TextMeasurer, the clipboard, the TextInputClient, and the TextSelectionClient.

It implements the following HuxerUI lifecycle entry points:

- `OnPointer()`: mouse, touch, drag, scrollbar, and editor gestures;
- `OnKey()`: keyboard commands and editing shortcuts;
- `OnFocusChanged()`: focus and caret state;
- `OnFrame()`: caret blinking and SweetEditor animation;
- `Paint()`: paints the `EditorRenderModel`;
- `GetTextInputClient()`: IME integration;
- `GetTextSelectionClient()`: selection and selection handles.

```text
SweetEditor()
  -> Canvas View
  -> SweetEditorBehavior retained modifier
  -> SweetEditorBehavior::Extension
  -> EditorHolder
  -> EditorCore + Document + SweetLineHighlighter
```

State changes notify the HuxerUI Runtime through `InvalidatePaint()` instead of forcing recomposition through Canvas event closures. `OnFrame()` returns a `FrameResult` and the Runtime schedules the next frame.

## 5. SweetEditorOptions

The full declaration lives in `include/sweetedit_core/sweet_editor.h`.

### 5.1 Document

```cpp
std::string initial_text;
std::string syntax_json;
std::string document_key;
```

`initial_text` is loaded only when the editor is created or `document_key` changes; it is not a controlled value that overwrites the current text on every recomposition.

`syntax_json` is the SweetLine JSON grammar; when empty, the built-in C++ rules are used.

`document_key` is the stable identity of the current document. Changing it recreates the EditorCore, Document, and highlighter:

```cpp
options.document_key = "src/main.cpp";
```

Multiple files must use different keys, otherwise the Runtime treats them as the same retained editor instance.

### 5.2 Font and Line Spacing

```cpp
float font_size = 14.0F;
float line_spacing_add = 0.0F;
float line_spacing_mult = 1.2F;
```

The editor uses a monospace font; the concrete fallback font is decided by the HuxerUI TextMeasurer for the current platform.

### 5.3 Editing and Indentation

```cpp
bool read_only = false;
uint32_t tab_size = 4;
bool backspace_unindent = true;
bool insert_spaces = true;
std::vector<std::pair<char32_t, char32_t>> auto_closing_pairs;
```

- `read_only`: forbids text-changing operations;
- `tab_size`: the width of one indentation level;
- `backspace_unindent`: Backspace on leading whitespace steps back to the previous indentation stop;
- `insert_spaces`: Tab inserts spaces instead of a literal tab character;
- `auto_closing_pairs`: configures auto-closing symbol pairs.

```cpp
options.tab_size = 2;
options.auto_closing_pairs = {
    {'(', ')'}, {'[', ']'}, {'{', '}'}, {'"', '"'},
};
```

When empty, the SweetEditor core default bracket pairs are used.

## 6. Code Completion

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

Trigger kinds are `Invoked` (manual, for example Ctrl+Space), `Character` (a trigger character was typed), and `Retrigger` (continued typing while the panel is already open).

### 6.2 Replacement Range

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

The SweetEditor core handles placeholder expansion and Tab stop cycling.

## 7. Decorations and Events

Editor semantic output is exposed through HuxerUI typed events. The events bind to the View returned by `SweetEditor()`, just like ordinary components:

```cpp
SweetEditor(options)
    .On<SweetEditorLinkClicked>([](const std::string& url) {
      // open the link or show a toast
    })
    .On<SweetEditorCodeLensClicked>([](int32_t command_id) {
      // run the command
    })
    .On<SweetEditorGutterIconClicked>([](uint32_t line, int32_t icon_id) {
      // line is a 0-based line number
    })
    .On<SweetEditorInlayClicked>([](uint32_t line, uint32_t column) {
      // handle the inlay hint
    })
    .On<SweetEditorTextChanged>([] {
      // the document text changed
    })
    .On<SweetEditorCursorChanged>([](uint32_t line, uint32_t column) {
      // caret position changed, 0-based
    })
    .On<SweetEditorSelectionChanged>(
        [](uint32_t start_line, uint32_t start_column, uint32_t end_line, uint32_t end_column) {
          // selection changed
        }
    )
    .On<SweetEditorScrollChanged>([](float scroll_x, float scroll_y) {
      // scroll position changed
    })
    .On<SweetEditorFoldToggled>([](std::size_t line) {
      // fold state changed
    })
    .On<SweetEditorLongPressed>([](uint32_t line, uint32_t column) {
      // long press
    })
    .On<SweetEditorDoubleTapped>([](uint32_t line, uint32_t column) {
      // double tap
    });
```

### 7.1 Click Events

```cpp
SweetEditor(options)
    .On<SweetEditorLinkClicked>([](const std::string& url) {
      // open the link or show a toast
    })
    .On<SweetEditorCodeLensClicked>([](int32_t command_id) {
      // run the command
    })
    .On<SweetEditorGutterIconClicked>([](uint32_t line, int32_t icon_id) {
      // line is a 0-based line number
    })
    .On<SweetEditorInlayClicked>([](uint32_t line, uint32_t column) {
      // handle the inlay hint
    });
```

### 7.2 Gutter Icons

```cpp
options.gutter_icon_provider = [](uint32_t start_line, uint32_t end_line) {
  std::vector<std::pair<uint32_t, int32_t>> icons;
  icons.emplace_back(start_line, 2);
  return icons;
};
```

The provider should return icons for the visible range only.

### 7.3 Editor Events

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

Line and column numbers are 0-based. Use `line + 1` when displaying them to users.

## 8. Diagnostics, Inlay Hints, and CodeLens

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

Severity: `0 = error`, `1 = warning`, `2 = info`, `3 = hint`. Providers should try to compute only the requested visible line range to avoid scanning the whole file while scrolling.

## 9. Search, Replace, and the Controller

`SweetEditor` exposes a `SweetEditorController` that the caller creates with `UseSweetEditorController()` and passes to `SweetEditor()`. The controller provides document loading, full-text reads, caret positioning, and search/replace control; methods return `false` while no editor is connected (the component is not mounted or has unmounted).

```cpp
[[huxerui::composable]]
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

### 9.1 Controller API

```cpp
controller.IsConnected();

// Document control
controller.LoadDocument("main.cpp", source, cpp_syntax);
controller.Text();           // current full text (UTF-8)
controller.SetCursor(line, column);

// Search and replace
controller.RunSearch("pattern");
controller.FindNext();
controller.FindPrevious();
controller.ReplaceCurrent("replacement");
controller.ReplaceAll("replacement");
controller.ClearSearch();
controller.ToggleSearch();    // toggles the built-in search bar
```

### 9.2 Built-in Search Bar

The component ships a declarative search/replace bar, toggled with Ctrl+F or `controller.ToggleSearch()`. The built-in bar includes Find, Prev, Next, Replace, All, and Close controls; operations act on the current document directly through the controller, no longer relying on an internal callback bridge.

## 10. Phantom Text, Newline, and Diff

```cpp
options.newline_action = [](uint32_t line, uint32_t column) {
  return std::string{};
};

options.phantom_text_provider = [](uint32_t line) {
  return line == 10 ? "  // generated suggestion" : "";
};
options.accept_phantom_on_tab = true;
```

Phantom text is a visual suggestion; it does not become document content unless the user accepts it.

```cpp
options.original_text = original_source;
```

`original_text` is the diff baseline; the current document remains editable. Clear it to disable the diff.

## 11. Display and Scrollbars

```cpp
options.render_whitespace = true;
options.render_line_breaks = false;
options.wrap_mode = 1;
options.sticky_gutter = true;
options.scrollbar_thickness = 6.0F;
options.scrollbar_mode = 1;
options.content_start_padding = 8.0F;
```

`wrap_mode`: `0 = no wrap`, `1 = wrap by character`, `2 = wrap by word`.

`scrollbar_mode`: `0 = always visible`, `1 = transient`, `2 = hidden`. A thickness of 0 keeps the core default.

## 12. Multi-File Example

```cpp
// Needs [[huxerui::composable]] (omitted in the example).
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

You must change `document_key` when switching files. A stable key keeps the retained editor; a new key creates a new document instance.

## 13. Build Integration

The project CMake adds the two third-party libraries:

```cmake
add_subdirectory(3dparty/SweetEditor ...)
add_subdirectory(3dparty/SweetLine ...)
target_link_libraries(sweetedit_core PRIVATE
    SweetEditor::sweeteditor_static
    SweetLine::sweetline_static)
```

Use `HUXERUI_HOME` to point at the HuxerUI source or SDK. See the README's "HuxerUI Dependency and Installation" section for the exact install commands and environment variables.

### Android

```bash
cd examples/preview/platform/android
HUXERUI_HOME=/path/to/huxerui \
HUXERUI_HOST_TOOL_ROOT=/path/to/host-tools \
sh gradlew :app:assembleRelease \
  -PtermuxAndroidToolchain=/path/to/termux-android-toolchain.cmake \
  -PtermuxCxxStaging=$HOME/sweetedit-android-cxx \
  -PhuxeruiAbis=arm64-v8a \
  -Pandroid.aapt2FromMavenOverride=/data/data/com.termux/files/usr/bin/aapt2 \
  --no-daemon
```

> Under Termux, `android.aapt2FromMavenOverride` points at the Termux-native `aapt2`;
> `termuxCxxStaging` moves CMake intermediates to a non-FUSE partition to avoid mtime issues.

Debug native target:

```bash
sh gradlew :app:buildCMakeDebug[arm64-v8a] --no-daemon
```

The project is configured against the Android API 23 boundary and needs the Android SDK, NDK, CMake, Gradle, and the HuxerUI host codegen tools.

### Desktop, Web, iOS

Windows, Linux, and macOS use `huxerui::RunApplication()`; Web uses Emscripten WebAssembly; the iOS project lives in `platform/ios/`. All three reuse the same C++ component.

## 14. Unicode and IME

Application text uses UTF-8 `std::string`; the editor core uses UTF-16 for editing and composition semantics. Keep these distinctions:

- a UTF-8 byte length is not a caret column;
- a UTF-16 offset is not a count of Unicode graphemes;
- an emoji may span multiple UTF-16 code units;
- IME composition ranges must come from the HuxerUI text-input contract;
- the application must not cut the document at raw UTF-8 byte boundaries;
- do not overwrite `initial_text` on every recomposition.

The component already provides input state, selection, composition ranges, input geometry, and session revision through the HuxerUI TextInputClient.

## 15. Current Limitations

`initial_text` is an initializer, not a fully controlled `TextEditingValue`. Once editing starts, the current document is owned by the SweetEditor retained core; the `SweetEditorTextChanged` typed event can notify the application of content changes, `SweetEditorController::Text()` can read the full text, `LoadDocument()` can load a document programmatically, and `SetCursor()` can control the caret. There is not yet a fully controlled model where the document text is written back by the application on every input, nor an API to share a single document instance across multiple components.

Editor semantic output is already exposed through typed events (see section 7); document and search control go through `SweetEditorController` (see section 9). The remaining `std::function` fields of `SweetEditorOptions` only carry provider/service semantics (completion, decorations, gutter icons, phantom text, newline policy) and may later migrate to Environment/services.

## 16. Known Issues

- First-paint syntax highlighting: under some startup timings the editor content first shows without highlighting until a scroll; this is being fixed.

## 17. Validation Checklist After Changes

Recommended validation: mount, recomposition, document-key switching, unmount, text editing, clipboard, undo/redo, CJK IME, emoji, caret blinking, mouse and touch selection, scrolling, folding, brackets, highlighting, completion, snippets, search, replace, diff, diagnostics, inlay hints, code lens, gutter/CodeLens clicks (without popping the keyboard), swipe without popping the keyboard, and the Android `arm64-v8a` native build.

```bash
git diff --check
git status --short
```

Related documentation:

```text
3dparty/SweetEditor/README.md
3dparty/SweetEditor/docs/en/api-editor-core.md
3dparty/SweetLine/README.md
3dparty/SweetLine/docs/en/api_core.md
3dparty/SweetLine/docs/en/syntax_rule.md
https://github.com/HuxerUI/HuxerUI/blob/main/docs/design/architecture.md
https://github.com/HuxerUI/HuxerUI/blob/main/docs/design/text-input.md
```
