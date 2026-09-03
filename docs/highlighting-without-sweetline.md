# Syntax Highlighting Without SweetLine

The CodeEditor component ships **no highlighting engine**. Syntax highlighting, and every other
decoration, flows through one optional plug-in interface — `huxerui::codeeditor::EditorDecorationProvider`.
SweetLine is used only by the demo (`examples/preview`), never by the library. This document
explains how to plug in your own engine and why the architecture is split that way.

## 1. Why SweetLine is not a dependency

The component library (`CMakeLists.txt`) links only the SweetEditor core. The demo's
`examples/preview/CMakeLists.txt` adds SweetLine itself and feeds it through the provider
interface. The library never includes a SweetLine header, so consumers are free to use:

- SweetLine (demo path),
- a language server (LSP) with an incremental document mirror,
- Tree-sitter, Highlight.js, or any regex/lexer-based tokenizer,
- an embeddable engine of your own.

Nothing in the editor's behavior assumes which engine produces the spans.

## 2. The provider contract

```cpp
class EditorDecorationProvider {
 public:
  virtual ~EditorDecorationProvider() = default;
  virtual void ProvideDecorations(
      const EditorDecorationContext& context,
      EditorDecorationReceiver& receiver) = 0;
};
```

- `ProvideDecorations` is called by the editor when the viewport changes, the text changes, or the
  caret moves.
- **Synchronous engines** build a result and call `receiver.Accept(std::move(result))` before
  returning.
- **Asynchronous engines** store the receiver reference, do the work on another thread, and deliver
  via `receiver.Accept(...)` later. The editor's `IsCancelled()` contract lets you drop stale
  results (for example a slow analysis for a viewport the user already scrolled past).
- Zero, one, or multiple `Accept` calls per request are valid; the editor merges results from all
  registered providers (`decoration_providers` on `EditorOptions`).

### The context

| Field | Meaning |
|---|---|
| `visible_start_line` / `visible_end_line` | Viewport lines to decorate (0-based, inclusive). |
| `total_line_count` | Document line count. |
| `cursor_line` / `cursor_column` | Caret position (0-based). |
| `viewport_settled` | `false` during fast scrolling — defer heavy analysis. |
| `document_text` | Full UTF-8 text, or `nullptr` when no change validation is needed. |
| `text_changes` | `std::vector<sweeteditor::TextChange>` incremental edits since the last refresh; empty on first call. Each change carries `range` (pre-edit coordinates) and `new_text`. |

### The result

```cpp
struct EditorDecorationResult {
  // Per line: (line, spans)
  std::vector<std::pair<size_t, std::vector<sweeteditor::StyleSpan>>> syntax_spans;
  std::vector<std::pair<size_t, std::vector<sweeteditor::StyleSpan>>> overlay_spans;       // rainbow brackets
  std::vector<std::pair<size_t, std::vector<sweeteditor::DocumentHighlight>>> document_highlights;
  std::vector<std::pair<size_t, std::vector<sweeteditor::InlayHint>>> inlay_hints;
  std::vector<std::pair<size_t, std::vector<sweeteditor::Diagnostic>>> diagnostics;
  std::vector<std::pair<size_t, std::vector<sweeteditor::CodeLensItem>>> code_lens;
  std::vector<std::pair<size_t, std::vector<sweeteditor::LinkSpan>>> links;
  std::vector<std::pair<size_t, std::vector<sweeteditor::GutterIcon>>> gutter_icons;
  std::vector<std::pair<size_t, std::vector<sweeteditor::PhantomText>>> phantom_texts;
  std::vector<sweeteditor::IndentGuide> indent_guides;
  std::vector<sweeteditor::FoldRegion> fold_regions;
  std::optional<sweeteditor::TextPosition> matched_bracket_open;
  std::optional<sweeteditor::TextPosition> matched_bracket_close;
};
```

Decoration data types are the SweetEditor core types re-exported as aliases from
`huxerui::codeeditor`, so no conversion layer exists. All line numbers are 0-based; `StyleSpan`
uses `column`, `length`, and `style_id` — the style ids are integers the editor's theme maps to
colors through `EditorStyle` / the syntax palette.

## 3. Splitting an engine out

Do it in three steps:

1. **Keep the engine out of the library.** Add it in your own application's `CMakeLists.txt`, and
   only add a provider source file on the app side.
2. **Own your document model.** The provider keeps whatever model the engine needs (a parsed tree,
   an incremental buffer, an LSP mirror). Use `context.text_changes` + `context.document_text` to
   patch it incrementally, and rebuild only when they diverge.
3. **Publish viewport slices.** Analyze at least `visible_start_line..visible_end_line` and return
   per-line spans. Skip the viewport when `viewport_settled == false` if your engine is slow, and
   return the full set on the settled pass.

```cpp
#include <huxerui/codeeditor.h>

class MyEngineProvider final : public huxerui::codeeditor::EditorDecorationProvider {
 public:
  MyEngineProvider(std::string grammar, std::string initial_text)
      : engine_(std::move(grammar)) {
    engine_.SetDocument(std::move(initial_text));
  }

  void ProvideDecorations(const huxerui::codeeditor::EditorDecorationContext& context,
                          huxerui::codeeditor::EditorDecorationReceiver& receiver) override {
    // 1. sync the engine's document
    if (!context.text_changes.empty()) {
      for (const sweeteditor::TextChange& change : context.text_changes) {
        engine_.ApplyEdit(change.range, change.new_text);
      }
    } else if (context.document_text && engine_.Text() != *context.document_text) {
      engine_.SetDocument(*context.document_text);  // divergence -> rebuild
    }

    // 2. tokenize the viewport
    huxerui::codeeditor::EditorDecorationResult result;
    for (size_t line = context.visible_start_line;
         line <= context.visible_end_line && line < context.total_line_count; ++line) {
      std::vector<sweeteditor::StyleSpan> spans = engine_.TokenizeLine(line);
      if (!spans.empty()) result.syntax_spans.emplace_back(line, std::move(spans));
    }

    // 3. heavy families only on the settled viewport
    if (context.viewport_settled) {
      // result.diagnostics / inlay_hints / code_lens / ... from your engine
    }

    // 4. synchronous delivery
    receiver.Accept(std::move(result));
  }

 private:
  MyEngine engine_;
};

// usage
huxerui::codeeditor::EditorOptions options;
options.initial_text = source;
options.decoration_providers.push_back(
    std::make_shared<MyEngineProvider>(grammar_json, source));
```

## 4. Reference provider (SweetLine demo)

`examples/preview/src/sweetline_provider.cpp` is the reference implementation showing:

- incremental sync via `context.text_changes` (SweetLine's `analyzeIncrementalInLineRange`),
- full rebuild only on text divergence,
- viewport overscan analysis before publishing a slice,
- indent guides and fold regions (published once),
- rainbow brackets and bracket matching,
- word-under-cursor document highlights,
- TODO/FIXME diagnostics, URL links, color inlays,
- `Run` code lens and class gutter icons,
- breakpoint gutter icons and phantom text from the host app.

The demo's `SweetLineDecorationProvider` is created per document key and memoized in
`app.cpp` — engine construction compiles a grammar, so it must not repeat on every recomposition.

## 5. Theming engine styles

The demo maps SweetLine style names to the integer ids of `huxerui::codeeditor::EditorStyle`
(`Keyword`, `Type`, `Class`, `Function`, ...). The editor's theme then colors those ids through
`EditorTheme::syntax_*`. For a foreign engine, map your own style tokens onto the same enum so
the palette applies:

| Engine token | EditorStyle |
|---|---|
| keyword / control | `EditorStyle::Keyword` |
| type / builtin type | `EditorStyle::Type` |
| class / struct | `EditorStyle::Class` |
| function / method | `EditorStyle::Function` |
| string | `EditorStyle::String` |
| number | `EditorStyle::Number` |
| comment | `EditorStyle::Comment` |
| operator / punctuation | `EditorStyle::Punctuation` |
| everything else | `EditorStyle::Variable` |
