# CodeEditor Component

## 1. Overview

`huxerui::codeeditor::CodeEditor` is a HuxerUI code editor built on the SweetEditor core
(`3dparty/SweetEditor`). HuxerUI owns composition, layout, focus, IME, clipboard, painting, and
frame scheduling; SweetEditor owns the document, editing, undo/redo, search, completion, folding,
and the render model.

The public header is `<huxerui/codeeditor.h>`; everything lives in `huxerui::codeeditor`. The
implementation sits in `src/codeeditor/codeeditor.cpp` and is packaged as the `codeeditor`
dependency library.

## 2. Minimal usage

```cpp
#include <huxerui/huxerui.h>
#include <huxerui/codeeditor.h>

View App() {
  huxerui::codeeditor::CodeEditorOptions options;
  options.initial_text = "int main() {\n  return 0;\n}\n";
  return huxerui::codeeditor::CodeEditor(std::move(options)).With(Grow{});
}
```

`initial_text` loads when the editor is created or when `document_key` changes; it is not a
controlled value. `document_key` is optional — empty keeps one default document; distinct keys
recreate the document state (multi-file switching).

## 3. Architecture

```text
CodeEditor()                          composable entry (search bar + Canvas)
  -> Canvas View
  -> CodeEditorBehavior retained modifier
  -> CodeEditorBehavior::Extension    NodeExtension: pointer/key/focus/frame/paint, IME clients
  -> EditorHolder                     core + document + completion + decoration pipeline
  -> SweetEditor EditorCore
```

State changes notify the runtime through `InvalidatePaint()`; `OnFrame()` returns a `FrameResult`
and stays bounded (the initial highlight bootstrap is capped). The extension clips its painting to
the component bounds.

## 4. Decoration providers

Highlighting and every decoration flow through one interface (mirroring the SweetEditor platform
wrappers): the editor never depends on an engine.

```cpp
class CodeEditorDecorationProvider {
 public:
  virtual ~CodeEditorDecorationProvider() = default;
  virtual CodeEditorDecorationResult ProvideDecorations(const CodeEditorDecorationContext& context) = 0;
};
```

`CodeEditorDecorationContext` carries the visible line range (inclusive), total line count, caret
position, a `viewport_settled` flag (false during fast scrolling), the full UTF-8 document text
(valid for the call), and the incremental `text_changes` since the previous refresh.

`CodeEditorDecorationResult` accepts per-line syntax spans, overlay spans (rainbow brackets),
document highlights, inlay hints, diagnostics, code lens, links, gutter icons, phantom text,
indent guides, fold regions, and one bracket match. Empty fields are skipped; results from
multiple providers are merged.

Refresh cadence: immediately on viewport changes (`settled=false`), after edits and caret moves,
once when the viewport settles (`settled=true`), and once after a document loads (fold regions are
published there). Style ids resolve through the default `CodeEditorStyle` palette registered on
the core.

The reference implementation — SweetLine incremental analysis, overscan, folds, rainbow brackets,
bracket matching, word highlights, TODO/FIXME diagnostics, URL links, code lens, gutter icons,
phantom text — lives in `examples/preview/src/sweetline_provider.cpp`.

## 5. Options

Documents: `initial_text`, `document_key`. Typography: `font_size`, `font_family`,
`line_spacing_add`, `line_spacing_mult`. `font_family` selects the content font: empty uses the
platform monospace default; a named family resolves against fonts bundled by the host platform
(`assets/fonts/<family>.ttf` on Android) and falls back to the system family table. Font changes
apply live through the core's font-metrics hook (caret, scroll anchor, undo, and fold state are
preserved). `theme` (`CodeEditorTheme`) covers every visual surface: component and gutter
backgrounds, focused-line highlight, caret color/width, line numbers, selection, search and
bracket-match backgrounds, document highlights, IME composition and diagnostic underlines, diff
rows and gutters, links and code lens, indent guides, inlay hints, fold placeholders, gutter icons,
the completion panel, and the syntax token palette (13 token colors plus 8 rainbow bracket
levels). Theme defaults derive from the ambient HuxerUI `ThemeSpec` (`FromThemeSpec`), so the
editor follows `MaterialTheme` light/dark automatically; override `options.theme` with a fully
populated struct for manual control, or place one through the environment with
`Theme{ThemeDefinition{}.Set(CodeEditorTheme{...}), ...}`. Editing: `read_only`, `tab_size`, `backspace_unindent`, `insert_spaces`,
`auto_closing_pairs`. All editing and presentation options reconcile live on recomposition;
only a document-key switch rebuilds the editor. Completion: `completion_provider`, `completion_trigger_characters`.
Decorations: `decoration_providers`, `accept_phantom_on_tab`. Hooks: `newline_action`.
Diff: `original_text` (empty disables). Display: `render_whitespace`, `render_line_breaks`,
`wrap_mode` (0/1/2), `sticky_gutter`, `scrollbar_thickness`, `scrollbar_mode` (0/1/2),
`content_start_padding`.

## 6. Events

All events aggregate in `CodeEditorEvents` and bind on the returned `View`:

```cpp
CodeEditor(options)
    .On<CodeEditorEvents::TextChanged>([] {})
    .On<CodeEditorEvents::CursorChanged>([](uint32_t line, uint32_t column) {})      // 0-based
    .On<CodeEditorEvents::SelectionChanged>([](uint32_t, uint32_t, uint32_t, uint32_t) {})
    .On<CodeEditorEvents::ScrollChanged>([](float x, float y) {})
    .On<CodeEditorEvents::FoldToggled>([](std::size_t line) {})
    .On<CodeEditorEvents::LinkClicked>([](const std::string& url) {})
    .On<CodeEditorEvents::CodeLensClicked>([](int32_t command_id) {})
    .On<CodeEditorEvents::GutterIconClicked>([](uint32_t line, int32_t icon_id) {})
    .On<CodeEditorEvents::InlayClicked>([](uint32_t line, uint32_t column) {})
    .On<CodeEditorEvents::LongPressed>([](uint32_t line, uint32_t column) {})
    .On<CodeEditorEvents::DoubleTapped>([](uint32_t line, uint32_t column) {});
```

Line and column numbers are 0-based; display `line + 1`.

## 7. Controller

```cpp
[[huxerui::composable]]
View Page() {
  const auto controller = huxerui::codeeditor::UseCodeEditorController();
  // ...
  return huxerui::codeeditor::CodeEditor(options, controller).With(Grow{});
}
```

`IsConnected()`, `LoadDocument(key, text)`, `Text()`, `SetCursor(line, column)`,
`RunSearch(pattern)`, `FindNext()`, `FindPrevious()`, `ReplaceCurrent(replacement)`,
`ReplaceAll(replacement)`, `ClearSearch()`, `ToggleSearch()` (built-in search bar, also Ctrl+F).
Methods return `false` while no editor is mounted.

## 8. Consumption

```cmake
huxerui_use_library(your_app
        TARGET CodeEditor::CodeEditor
        PATH "/path/to/Lib-CodeEditor"
)
```

The library links the vendored SweetEditor core. SweetLine is intentionally not a dependency; the
demo adds it itself to showcase the provider integration.

## 9. Decoration refresh model

| Event | Refresh | Notes |
|---|---|---|
| Viewport established (first paint, document switch, keyboard insets) | Full settled refresh, painted in the same frame | Two-pass model build: the first publishes the visible range, decorations land, the second rebuilds with them |
| Fast scrolling | Light (syntax spans + indent guides), then full on settle | Heavy categories (rainbows, diagnostics, inlay hints, code lens, links, gutter icons) are untouched during scroll and applied when the viewport settles |
| Text edit or caret move | Full settled refresh | Incremental analysis feeds the decoration provider |
| Theme or font change | Hot-applied | No editor state loss (undo, caret, scroll, folds) |
| Document switch | Holder rebuild | New document, new decoration state |

## 10. Limitations

- `initial_text` is an initializer, not a fully controlled `TextEditingValue`; use
  `CodeEditorEvents::TextChanged`, `CodeEditorController::Text()`, and `LoadDocument()` to observe
  and drive content.
- Color-swatch inlays and separator/bracket connector guides are not yet exposed through the
  decoration result.
- Fold regions are provider-owned; the editor preserves interactive fold state after publication.

## 11. Validation checklist

Mount, recomposition, document-key switching, unmount, editing, clipboard, undo/redo, CJK IME,
emoji, caret blink, mouse/touch selection, scrolling, folding, brackets, highlighting (via a
provider), completion, snippets, search/replace, diff, diagnostics, inlay hints, code lens,
gutter/CodeLens clicks without raising the keyboard, pinch zoom, and the Android arm64-v8a build.
