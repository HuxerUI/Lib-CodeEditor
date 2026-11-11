# CodeEditor

A cross-platform code editor for [HuxerUI](https://github.com/HuxerUI/HuxerUI), built on the
[SweetEditor](3dparty/SweetEditor) core. The component is a declarative `View`; documents, editing,
completion, search, decorations, and events are exposed the HuxerUI-native way.

- Header: `<huxerui/codeeditor.h>` — namespace `huxerui::codeeditor`
- CMake target: `codeeditor` (alias `HuxerUI::CodeEditor`)
- Platforms: Windows, macOS, Linux, Web, Android, iOS (following HuxerUI)

## Features

- Declarative component: `CodeEditor(options, controller)` returns an ordinary HuxerUI `View`
- Retained `NodeExtension`: the editor core, document, and input state live on the mounted node
- Editing: caret/selection, undo/redo, auto-indent, auto-closing brackets, Tab/space policy, read-only, pinch zoom
- Custom fonts: `font_family` accepts HuxerUI font families loaded from resources, files, or installed fonts
- Completion: provider-driven panel, snippet expansion, replacement ranges
- Search and replace: built-in declarative search bar or programmatic `EditorController`
- Full theming: `EditorTheme` covers every visual surface including the syntax token palette
  and rainbow brackets; defaults derive from the ambient HuxerUI theme (light/dark follows
  `MaterialTheme` automatically), and changes apply live without editor state loss
- Decorations: syntax spans, diagnostics, inlay hints, code lens, links, gutter icons, indent guides,
  fold regions, document highlights, bracket matching, phantom text, diff — all through **one
  unified provider interface**
- No built-in highlighting engine: wire SweetLine, a language server, or your own analyzer through
  `EditorDecorationProvider` (the demo shows the optional SweetLine integration)
- Accessibility: the editor exposes a text-field semantic whose value is the document, honoring the
  read-only option; options are validated at composition entry with `HuxerUI`-prefixed diagnostics
- Typed events aggregated in `EditorEvents`

## Quick start

```cpp
#include <huxerui/huxerui.h>
#include <huxerui/codeeditor.h>

using namespace huxerui;

View App() {
  huxerui::codeeditor::EditorOptions options;
  options.initial_text = "int main() {\n  return 0;\n}\n";

  return huxerui::codeeditor::CodeEditor(std::move(options)).With(Grow{});
}

const Application application{
    App,
    {.window = {.title = "CodeEditor", .initial_size = {900.0F, 640.0F}}},
};
```

`document_key` is optional: leave it empty to keep one default document, or set it when switching
between documents so the editor recreates its document state.

## Using the library

Consume the library from any HuxerUI application with `huxerui_use_library`:

```cmake
# In your app's CMakeLists.txt, after huxerui_add_app(...):
huxerui_use_library(your_app
        TARGET HuxerUI::CodeEditor
        PATH "/path/to/Lib-CodeEditor"
)
```

Building requires HuxerUI SDK 0.4.0 (`HUXERUI_HOME`); see the
[HuxerUI installation guide](https://github.com/HuxerUI/HuxerUI/blob/main/docs/guide/installation.md).
After cloning, initialize the pinned SweetEditor and SweetLine submodules with Git.

```sh
git submodule update --init --recursive
```

## Highlighting: implement a decoration provider

The editor never depends on a highlighting engine. Implement
`EditorDecorationProvider` and return whatever you can compute for the visible range —
syntax spans, diagnostics, inlay hints, code lens, links, gutter icons, indent guides, fold
regions, document highlights, bracket matches, and phantom text:

```cpp
class MyHighlighting final : public huxerui::codeeditor::EditorDecorationProvider {
 public:
  huxerui::codeeditor::EditorDecorationResult ProvideDecorations(
      const huxerui::codeeditor::EditorDecorationContext& context) override {
    huxerui::codeeditor::EditorDecorationResult result;
    result.syntax_spans.emplace_back(
        0, std::vector{{huxerui::codeeditor::StyleSpan{0, 3, huxerui::codeeditor::EditorStyle::Keyword}}}
    );
    return result;
  }
};

options.decoration_providers.push_back(std::make_shared<MyHighlighting>());
```

Style ids come from the `EditorStyle` palette the editor registers by default. For a full
reference — incremental analysis, overscan, folds, rainbow brackets — see the optional SweetLine
integration in [`examples/preview/src/sweetline_provider.cpp`](examples/preview/src/sweetline_provider.cpp).

## Events and controller

```cpp
huxerui::codeeditor::CodeEditor(options, controller)
    .On<huxerui::codeeditor::EditorEvents::TextChanged>([] { /* document changed */ })
    .On<huxerui::codeeditor::EditorEvents::CursorChanged>([](uint32_t line, uint32_t column) {})
    .On<huxerui::codeeditor::EditorEvents::LinkClicked>([](const std::string& url) {});
```

`UseEditorController()` creates scope state for programmatic control — `LoadDocument`,
`Text`, `SetCursor`, search/replace, and `ToggleSearch`. All methods return `false` while no
editor is mounted.

## Repository layout

```text
Lib-CodeEditor/
├── CMakeLists.txt                 # codeeditor library (links SweetEditor only)
├── HuxerUIProject.cmake           # CLI project metadata and SDK discovery
├── include/huxerui/codeeditor.h   # public API
├── src/codeeditor/                # component implementation
├── tests/                         # Catch2 suite for pure public-API logic
├── examples/preview/              # demo app; wires optional SweetLine highlighting
│   ├── src/sweetline_provider.*   # reference EditorDecorationProvider
│   ├── resources/raw/fonts/       # Maple Mono (+ no-ligature build)
│   └── platform/                  # per-platform shells
└── 3dparty/SweetEditor/           # editor core (vendored gitlink)
```

## Building and CI

Run `huxerui build <platform>` or `huxerui package <platform>` from `examples/preview`.
Use the same CLI commands on Termux. The [release workflow](.github/workflows/release.yml)
packages Windows, Linux, macOS, and Android using the pinned public SDK release. Successful
builds on `main` update the continuous release, and tag pushes (`v*`) publish stable releases.

The Catch2 suite under `tests/` covers options validation, disconnected controller behavior, and
theme derivation. It uses the Catch2 amalgamation bundled with SweetEditor and builds by default
when configuring the library root on a desktop host. Run it through `ctest`.

## Documentation

- [API Reference](docs/codeeditor.md) — full parameter tables, events, decoration interface, controller, and completion types
- [中文 API 参考](docs/codeeditor_zh.md) — 完整参数表、事件、装饰接口、控制器与补全类型
- [Highlighting without SweetLine](docs/highlighting-without-sweetline.md) — split the engine out and plug in your own highlighter

## Third-party dependencies

- [SweetEditor](3dparty/SweetEditor) — cross-platform editor core
- [SweetLine](3dparty/SweetLine) — highlighting engine used by the demo

## License

Component code license is pending; SweetEditor and SweetLine follow their upstream licenses.
