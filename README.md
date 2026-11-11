# CodeEditor

CodeEditor is a HuxerUI View backed by the [SweetEditor](3dparty/SweetEditor) core. The component exposes a mounted editor session, document handles, typed settings and events, and providers for decorations, completion, and newline actions.

- Library ID is `org.huxerui.lib.codeeditor`.
- Include `<huxerui/codeeditor.h>` and use the `huxerui::codeeditor` namespace.
- Link the `HuxerUI::CodeEditor` CMake target.

## Quick start

```cpp
#include <huxerui/codeeditor.h>
#include <huxerui/huxerui.h>

using namespace huxerui;
namespace ce = huxerui::codeeditor;

[[huxerui::composable]]
View EditorPane() {
  const auto editor = ce::UseCodeEditor();
  return ce::CodeEditor(editor, {
      .document = "int main() {\n  return 0;\n}\n",
  })
      .On<ce::EditorEvents::CursorChanged>([](const ce::CursorChangedEvent& event) {
        // event.cursor_position contains the zero-based line and UTF-16 column.
      })
      .With(Grow{});
}
```

Use `EditorEvents::DocumentLoaded` to send commands when the first document is available or when it is replaced. Supply UTF-8 text or a shared `Document` as `EditorOptions::document`. A new text value or document handle replaces the active document. Use `LoadDocument` to switch documents from an application command.

## Build

Configure your app with HuxerUI SDK 0.4 and initialize the repository's dependencies.

```sh
git submodule update --init --recursive
```

```cmake
huxerui_use_library(your_app
        TARGET HuxerUI::CodeEditor
        PATH "/path/to/Lib-CodeEditor"
)
```

Run `huxerui build <platform>` or `huxerui package <platform>` from `examples/preview`.

Standalone desktop builds enable the Catch2 suite by default. Use an SDK with `HuxerUI::testing` and run `ctest --test-dir <build-dir> --output-on-failure` after building. The suite drives the mounted controller, editor events, IME batches, keymaps, and providers through a windowless HuxerUI session.

## Documentation

- [API reference](docs/codeeditor.md)
- [中文 API 参考](docs/codeeditor_zh.md)
- [Custom syntax highlighting](docs/highlighting-without-sweetline.md)

## Dependencies

The library links SweetEditor. The preview links [SweetLine](3dparty/SweetLine) and registers its [decoration provider](examples/preview/src/sweetline_provider.cpp) alongside the demo completion provider.

## License

Component code license is pending. SweetEditor and SweetLine follow their upstream licenses.
