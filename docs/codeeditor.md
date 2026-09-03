# CodeEditor API Reference

Header: `<huxerui/codeeditor.h>` · Namespace: `huxerui::codeeditor` · CMake: `huxerui_use_library(app TARGET CodeEditor::CodeEditor PATH ...)`

## 1. Component entry

```cpp
huxerui::View CodeEditor(CodeEditorOptions options = {}, CodeEditorController controller = {});
```

Returns a declarative `View` carrying a retained editor extension. Place it anywhere a `View` fits:

```cpp
[[huxerui::composable]]
View EditorPage() {
  const auto controller = huxerui::codeeditor::UseCodeEditorController();
  huxerui::codeeditor::CodeEditorOptions options;
  options.initial_text = "int main() { return 0; }";
  return huxerui::codeeditor::CodeEditor(std::move(options), controller).With(Grow{});
}
```

---

## 2. The HuxerUI composition model

CodeEditor is a declarative HuxerUI component — it participates fully in the `UseState` → recomposition → reconcile cycle. Understanding this model is key to using the editor effectively.

### How it works

```text
UseState(value)          — declare reactive state in a composable
       ↓ write            CodeEditor(options)     — declarative UI description
       ↓                   (returns a transient View)
  recomposition           → Runtime reconciles the new options against the retained
       ↓                    editor node (mounted state: undo, cursor, folds, scroll)
  Extension::Update()     → applies only what changed (theme, font, editing options,
                            providers, hooks, diff, document key)
```

**You never imperatively call setters on the editor.** You mutate `UseState`, HuxerUI recomposes your composable, and the editor's retained extension reconciles the new `CodeEditorOptions` against its previous value — applying only the delta without resetting retained state.

### UseState-driven editor

```cpp
[[huxerui::composable]]
View EditorPage() {
  // Declarative state — every write triggers recomposition
  auto dark_mode = UseState(false);
  auto font_size = UseState(14.0F);
  auto current_doc = UseState(std::size_t(0));
  auto dirty_count = UseState(0);
  auto cursor_label = UseState(std::string("Ln 1, Col 1"));

  const auto controller = UseCodeEditorController();
  const Document& doc = documents[current_doc.Get()];

  // Build options from state — this is the single source of truth
  CodeEditorOptions options;
  options.document_key = doc.path;
  options.initial_text = doc.content;
  options.font_size = font_size.Get();

  if (dark_mode.Get()) {
    auto theme = CodeEditorTheme::Default();
    theme.background = Color::Rgb(30, 30, 46);
    theme.text_foreground = Color::Rgb(205, 214, 244);
    options.theme = theme;
  }

  return Column {
    // UI controls that mutate state → recomposition → editor reconciles
    Row {
      Switch("Dark", dark_mode).OnChanged([dark_mode](bool on) { dark_mode = on; }),
      Button("A-").OnClick([font_size] { font_size = font_size.Get() - 1.0F; }),
      Button("A+").OnClick([font_size] { font_size = font_size.Get() + 1.0F; }),
      Text(cursor_label),
    },
    ForEach(documents, [current_doc](std::size_t i) {
      return Button(documents[i].name).OnClick([current_doc, i] { current_doc = i; });
    }),

    // The editor — declarative, reconciles on every recomposition
    CodeEditor(std::move(options), controller)
        .On<CodeEditorEvents::CursorChanged>(
            [cursor_label](uint32_t line, uint32_t col) {
              // Event handlers capture UseState by value — writes invalidate
              // this scope, updating the status bar
              cursor_label = "Ln " + std::to_string(line + 1) + ", Col " + std::to_string(col + 1);
            })
        .On<CodeEditorEvents::TextChanged>([dirty_count] {
          dirty_count = dirty_count.Get() + 1;
        })
        .With(Grow{}),
  };
}
```

### What reconciles live vs. what rebuilds

| State change | Effect on retained editor |
|---|---|
| `font_size` / `font_family` | Live font switch — undo, cursor, scroll, folds preserved |
| `theme` | Live restyle — no editor state loss |
| `read_only`, `tab_size`, `insert_spaces`, etc. | Live apply through core setters |
| `decoration_providers` list | Provider list swaps live |
| `completion_provider`, `newline_action` | Hook swap live |
| `original_text` | Diff baseline update live |
| `wrap_mode`, `sticky_gutter`, scrollbars | Display settings apply live |
| `document_key` | **Editor rebuild** — new document, undo/folds reset |
| `initial_text` (same key) | Ignored — use `LoadDocument()` for programmatic replace |

### The controller is also UseState

`UseCodeEditorController()` creates a `State<CodeEditorController>` — the controller is scope state, not a singleton. It persists across recompositions of the same scope and reconnects when the editor mounts/unmounts:

```cpp
const auto controller = UseCodeEditorController();  // State<Controller>

Button("Format").OnClick([controller] {
  // Controller methods return false while the editor is not mounted
  if (controller.IsConnected()) {
    auto text = controller.Text();
    controller.LoadDocument("formatted", FormatCode(text));
  }
});
```

### Events use UseState for observation

Event handlers are lambdas that capture `UseState` by value. A write inside a handler invalidates the capturing scope, triggering recomposition:

```cpp
auto text_length = UseState(std::size_t(0));

CodeEditor(options)
    .On<CodeEditorEvents::TextChanged>([controller, text_length] {
      // Reads the authoritative text and publishes it as state
      text_length = controller.Text().size();
    });
```

---

## 3. CodeEditorOptions

### Document

| Field | Type | Default | Description |
|---|---|---|---|
| `initial_text` | `std::string` | `""` | Initial UTF-8 document content. Loaded when the editor mounts or when `document_key` changes. **Not** a controlled value — after mount the retained core owns the text; use `CodeEditorEvents::TextChanged` to observe and `Controller::Text()` / `LoadDocument()` to read or replace it. |
| `document_key` | `std::string` | `""` | Stable document identity. Changing it between recompositions reloads `initial_text` and recreates the editor state (undo history, cursor, folds reset). Empty keeps one default document. Distinct keys enable multi-document switching. |

### Typography

| Field | Type | Default | Description |
|---|---|---|---|
| `font_family` | `std::string` | `""` | Content font family. Empty = platform monospace. A named family resolves against platform-bundled fonts (`assets/fonts/<family>.ttf` on Android) then the system table. Changes apply live without state loss. |
| `font_size` | `float` | `14.0` | Content font size in logical pixels. |
| `line_spacing_add` | `float` | `0.0` | Extra spacing added to each line height. |
| `line_spacing_mult` | `float` | `1.2` | Line height multiplier. |
| `theme` | `std::optional<CodeEditorTheme>` | `{}` | Explicit visual override. When empty the editor derives from the ambient `UseTheme()` and follows `MaterialTheme` light/dark live. See §3. |

### Editing behavior

| Field | Type | Default | Description |
|---|---|---|---|
| `read_only` | `bool` | `false` | Rejects all text-changing operations. Selection and navigation remain active. |
| `tab_size` | `uint32_t` | `4` | Width of one indentation level in spaces. |
| `backspace_unindent` | `bool` | `true` | Backspace on leading whitespace steps back to the previous indentation stop. |
| `insert_spaces` | `bool` | `true` | Tab inserts spaces instead of a literal `\t`. |
| `auto_closing_pairs` | `std::vector<std::pair<char32_t, char32_t>>` | `{}` | Auto-closing bracket pairs. Empty uses core defaults: `( )`, `{ }`, `[ ]`. |

### Completion

| Field | Type | Default | Description |
|---|---|---|---|
| `completion_provider` | `CompletionProvider` | `{}` | Callback receiving a `CompletionContext`; returns `std::vector<CompletionItem>`. See §7. |
| `completion_trigger_characters` | `std::function<bool(const std::string&)>` | `{}` | Returns `true` when typing the given character should fire a request. |

### Decoration

| Field | Type | Default | Description |
|---|---|---|---|
| `decoration_providers` | `std::vector<std::shared_ptr<CodeEditorDecorationProvider>>` | `{}` | Decoration sources; the editor merges all results. See §5. |
| `accept_phantom_on_tab` | `bool` | `true` | Whether Tab commits the caret-line phantom text. |

### Hooks and diff

| Field | Type | Default | Description |
|---|---|---|---|
| `newline_action` | `std::function<std::string(uint32_t line, uint32_t column)>` | `{}` | Invoked before inserting a newline. Return replacement text or empty for default. |
| `original_text` | `std::string` | `""` | Diff baseline. Non-empty computes a line-level diff (added/removed backgrounds). Empty disables. |

### Display

| Field | Type | Default | Description |
|---|---|---|---|
| `render_whitespace` | `bool` | `false` | Whitespace markers (space dots, tab arrows). |
| `render_line_breaks` | `bool` | `false` | Line-break symbols. |
| `wrap_mode` | `int` | `0` | `0` = no wrap, `1` = character wrap, `2` = word wrap. |
| `sticky_gutter` | `bool` | `false` | Keep the gutter fixed during horizontal scrolling. |
| `scrollbar_thickness` | `float` | `0.0` | Scrollbar thickness. `0.0` = core default. |
| `scrollbar_mode` | `int` | `0` | `0` = always, `1` = transient, `2` = never. |
| `content_start_padding` | `float` | `0.0` | Extra padding between gutter and text. |

---

## 4. CodeEditorTheme

All colors are `huxerui::Color`. Factories: `Default()` (light reference), `FromThemeSpec(const ThemeSpec&)` (ambient derivation). Has `operator==`.

### Surfaces
`background`, `gutter_background`, `current_line_background` (focused-line highlight), `separator_color`.

### Text and caret
`text_foreground`, `line_number_color`, `caret_color`, `caret_width` (float, default 2.0).

### Links and code lens
`link_color`, `active_link_color`, `codelens_color`, `active_codelens_color`.

### Selection and highlights
`selection_background`, `search_match_background`, `search_current_background`, `bracket_match_background`, `document_highlight_text/read/write`, `ime_composition_underline`.

### Diagnostics (wavy underlines)
`diagnostic_error/warning/info/hint_underline`.

### Diff
`diff_added/removed_background`, `diff_added/removed_gutter`.

### Decorations
`indent_guide_color`, `inlay_hint_background/text`, `fold_placeholder_background/text`, `gutter_icon_color`.

### Syntax palette
`syntax_keyword/type/class/function/variable/string/number/comment/preprocessor/builtin/punctuation/annotation/url`, `syntax_rainbow` (`std::array<Color, 8>`).

### Completion panel
`completion_background/border/selected_background/label/detail`.

### Usage

```cpp
auto theme = huxerui::codeeditor::CodeEditorTheme::Default();
theme.background = Color::Rgb(30, 30, 46);
theme.text_foreground = Color::Rgb(205, 214, 244);
theme.syntax_keyword = Color::Rgb(203, 166, 247);
options.theme = theme;
```

---

## 5. CodeEditorEvents

All events bind with `.On<Event>(handler)` on the `View` returned by `CodeEditor()`. Line and column parameters are **0-based**; display `line + 1` to users.

| Event | Handler signature | Parameters |
|---|---|---|
| `TextChanged` | `void()` | Fired after any text edit. |
| `CursorChanged` | `void(uint32_t line, uint32_t column)` | `line` — 0-based caret line. `column` — 0-based caret column. |
| `SelectionChanged` | `void(uint32_t sl, uint32_t sc, uint32_t el, uint32_t ec)` | Selection start (sl, sc) to end (el, ec), 0-based, normalized. |
| `ScrollChanged` | `void(float x, float y)` | Horizontal and vertical scroll offsets in logical pixels. |
| `FoldToggled` | `void(std::size_t line)` | 0-based line of the fold that was expanded or collapsed. |
| `LongPressed` | `void(uint32_t line, uint32_t column)` | 0-based position of the long-press. |
| `DoubleTapped` | `void(uint32_t line, uint32_t column)` | 0-based position of the double-tap. |
| `LinkClicked` | `void(const std::string& url)` | URL string of the clicked link token. |
| `CodeLensClicked` | `void(int32_t command_id)` | Command id assigned by the provider's `CodeEditorCodeLens.command_id`. |
| `GutterIconClicked` | `void(uint32_t line, int32_t icon_id)` | `line` — 0-based line. `icon_id` — provider-assigned id. `1` = diamond, `2` = circle, others = dot. |
| `InlayClicked` | `void(uint32_t line, uint32_t column)` | 0-based position of the clicked inlay hint. |

```cpp
CodeEditor(options, controller)
    .On<CodeEditorEvents::CursorChanged>([status](uint32_t line, uint32_t col) {
      status = "Ln " + std::to_string(line + 1) + ", Col " + std::to_string(col + 1);
    })
    .On<CodeEditorEvents::GutterIconClicked>([this](uint32_t line, int32_t id) {
      if (id == 2) ToggleBreakpoint(line);
    });
```

---

## 6. CodeEditorDecorationProvider

```cpp
class CodeEditorDecorationProvider {
 public:
  virtual ~CodeEditorDecorationProvider() = default;
  virtual CodeEditorDecorationResult ProvideDecorations(
      const CodeEditorDecorationContext& context) = 0;
};
```

### CodeEditorDecorationContext

| Field | Type | Description |
|---|---|---|
| `visible_start_line` | `uint32_t` | First visible line (0-based, inclusive). |
| `visible_end_line` | `uint32_t` | Last visible line (0-based, inclusive). |
| `total_line_count` | `uint32_t` | Total lines in the document. |
| `cursor_line` | `uint32_t` | Caret line (0-based). |
| `cursor_column` | `uint32_t` | Caret column (0-based). |
| `viewport_settled` | `bool` | `false` during fast scrolling — skip heavy computation. |
| `document_text` | `const std::string*` | Full UTF-8 text. May be `nullptr`. Valid only during the call. |
| `text_changes` | `std::vector<CodeEditorTextChange>` | Incremental edits since last refresh. |

### CodeEditorTextChange

| Field | Description |
|---|---|
| `start_line`, `start_column` | Change range start (pre-edit coordinates). |
| `end_line`, `end_column` | Change range end. |
| `new_text` | Replacement text (UTF-8). Empty for deletes. |

### CodeEditorDecorationResult

| Field | Type | Description |
|---|---|---|
| `syntax_spans` | `LineEntries<StyleSpan>` | Per-line syntax tokens. |
| `overlay_spans` | `LineEntries<StyleSpan>` | Rainbow brackets. |
| `document_highlights` | `LineEntries<StyleSpan>` | Same-word highlights. |
| `inlay_hints` | `LineEntries<InlayHint>` | Inline pills. |
| `diagnostics` | `LineEntries<Diagnostic>` | Squigglies. |
| `code_lens` | `LineEntries<CodeLens>` | Clickable commands. |
| `links` | `LineEntries<Link>` | Clickable URLs. |
| `gutter_icons` | `LineEntries<GutterIcon>` | Line-number area icons. |
| `phantom_texts` | `LineEntries<PhantomText>` | Ghost text (Tab commits). |
| `indent_guides` | `std::vector<IndentGuide>` | Indent guide lines. |
| `fold_regions` | `std::vector<FoldRegion>` | Foldable regions (once per document). |
| `matched_bracket` | `std::optional<BracketMatch>` | Bracket pair under cursor. |

### Item types

**StyleSpan** — `{column, length, style}`. `style` is a `CodeEditorStyle` enum value resolving through the theme's syntax palette.

**InlayHint** — `{column, text}`.

**Diagnostic** — `{column, length, severity, message}`. Severity: `0` = error, `1` = warning, `2` = info, `3` = hint.

**CodeLens** — `{column, command_id, title}`. `command_id` is delivered back via `CodeLensClicked`.

**Link** — `{column, length, url}`. `url` delivered via `LinkClicked`.

**GutterIcon** — `{icon_id}`. Delivered via `GutterIconClicked`. `1` = diamond, `2` = circle, others = dot.

**PhantomText** — `{column, text}`.

**IndentGuide** — `{start_line, end_line, column}`.

**FoldRegion** — `{start_line, end_line}`. Arrow renders at `start_line`.

**BracketMatch** — `{line, column, partner_line, partner_column}`.

### Refresh model

| Event | Strategy |
|---|---|
| Viewport established | Full settled, same frame |
| Fast scrolling | Light (syntax + guides), then full on settle |
| Edit or caret move | Full settled |
| Theme or font change | Hot-applied |
| Document switch | Editor rebuild |

### Reference implementation

`examples/preview/src/sweetline_provider.cpp` — full SweetLine integration.

---

## 7. CodeEditorController

```cpp
inline CodeEditorController UseCodeEditorController();
```

Methods operate on the mounted editor and return `false` when disconnected.

| Method | Return | Description |
|---|---|---|
| `IsConnected()` | `bool` | An editor is mounted and bound. |
| `LoadDocument(key, text)` | `bool` | Replaces the document (undo, cursor, folds reset). |
| `Text()` | `std::string` | Full document text (UTF-8). |
| `SetCursor(line, column)` | `bool` | Moves the caret (0-based), scrolls into view. |
| `RunSearch(pattern)` | `bool` | Runs a search, highlights all matches. |
| `FindNext()` | `bool` | Jumps to the next match. |
| `FindPrevious()` | `bool` | Jumps to the previous match. |
| `ReplaceCurrent(replacement)` | `bool` | Replaces the current match, advances. |
| `ReplaceAll(replacement)` | `bool` | Replaces all matches. |
| `ClearSearch()` | `bool` | Clears search highlights. |
| `ToggleSearch()` | `bool` | Shows/hides the built-in search bar (also Ctrl+F). |

---

## 8. Completion types

### CompletionContext

| Field | Type | Description |
|---|---|---|
| `trigger_kind` | `TriggerKind` | `Invoked` (0, Ctrl+Space), `Character` (1, trigger char), `Retrigger` (2, panel open). |
| `trigger_character` | `std::string` | Character that fired the request. |
| `cursor_line` / `cursor_column` | `uint32_t` | Caret position (0-based). |
| `line_text` | `std::string` | Full caret line text. |
| `word_start` / `word_end` | `uint32_t` | Word range around the caret. |

### CompletionItem

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | `std::string` | — | Display label. |
| `detail` | `std::string` | — | Detail (right side). |
| `insert_text` | `std::string` | — | Text inserted on confirm. Falls back to `label`. |
| `kind` | `CompletionItemKind` | `Text` | Badge kind. |
| `filter_text` | `std::string` | — | Fuzzy filter override. |
| `sort_key` | `std::string` | — | Sort override. |
| `insert_text_is_snippet` | `bool` | `false` | Snippet template (`${1:ph}`, `$0`). |
| `has_text_edit` | `bool` | `false` | Replace range instead of insert at caret. |
| `text_edit_start` / `text_edit_end` | `uint32_t` | `0` | Replacement range (columns in cursor line). |
| `text_edit_text` | `std::string` | — | Replacement text. |

### CompletionItemKind

`Keyword`(0), `Function`(1), `Variable`(2), `Class`(3), `Interface`(4), `Module`(5), `Property`(6), `Snippet`(7), `Text`(8).

---

## 9. Architecture

```text
CodeEditor() -> Canvas -> CodeEditorBehavior -> Extension(NodeExtension) -> EditorHolder -> SweetEditor EditorCore
```

## 10. Limitations

- `initial_text` is an initializer, not a fully controlled value.
- Color-swatch inlays and separator guides not yet exposed.
- Fold regions are provider-owned; the editor preserves interactive fold state.

## 11. Validation checklist

Mount, recomposition, document switching, unmount, editing, clipboard, undo/redo, CJK IME, emoji, caret blink, selection, scrolling, folding, brackets, highlighting, completion, snippets, search/replace, diff, diagnostics, inlay hints, code lens, gutter clicks, pinch zoom, custom fonts, theme switching, Android arm64-v8a build.

---

## 12. Usage scenarios

### Minimal editor

```cpp
huxerui::codeeditor::CodeEditorOptions options;
options.initial_text = "// type here\n";
return huxerui::codeeditor::CodeEditor(std::move(options)).With(Grow{});
```

That's it — no key, no provider, no controller. The editor works out of the box with platform defaults.

### Syntax highlighting

Implement `CodeEditorDecorationProvider` and return `syntax_spans` for the visible range:

```cpp
class MyHighlighter final : public huxerui::codeeditor::CodeEditorDecorationProvider {
 public:
  huxerui::codeeditor::CodeEditorDecorationResult ProvideDecorations(
      const huxerui::codeeditor::CodeEditorDecorationContext& context) override {
    huxerui::codeeditor::CodeEditorDecorationResult result;
    for (uint32_t line = context.visible_start_line; line <= context.visible_end_line; ++line) {
      const std::string text = GetLineText(line);  // your document model
      // tokenize `text` and emit spans...
      result.syntax_spans.emplace_back(line, std::vector{
          {0, 3, huxerui::codeeditor::CodeEditorStyle::Keyword},   // "int" cols 0..3
          {8, 4, huxerui::codeeditor::CodeEditorStyle::Function},  // "main" cols 8..12
      });
    }
    return result;
  }
};

options.decoration_providers.push_back(std::make_shared<MyHighlighter>());
```

The editor calls the provider when the viewport changes, the text changes, or the caret moves. `context.viewport_settled` is `false` during fast scrolling — skip expensive analysis there and do it on the settled pass.

### Multi-document switching

Use distinct `document_key` values. Changing the key between recompositions reloads `initial_text` and recreates editor state:

```cpp
[[huxerui::composable]]
View FileTabs() {
  auto current_file = UseState(0);
  const Document& doc = documents[current_file.Get()];

  huxerui::codeeditor::CodeEditorOptions options;
  options.document_key = doc.path;       // changing this switches the document
  options.initial_text = doc.content;

  return Column {
    TabBar(documents, current_file),
    huxerui::codeeditor::CodeEditor(std::move(options)).With(Grow{}),
  };
}
```

### Read-only viewer with diagnostics

```cpp
options.read_only = true;
options.decoration_providers.push_back(std::make_shared<LinterProvider>(diagnostics));

class LinterProvider final : public huxerui::codeeditor::CodeEditorDecorationProvider {
 public:
  // diagnostics_ is a snapshot from your language server
  explicit LinterProvider(std::vector<Lint> diagnostics) : diagnostics_(std::move(diagnostics)) {}

  CodeEditorDecorationResult ProvideDecorations(const CodeEditorDecorationContext& context) override {
    CodeEditorDecorationResult result;
    for (const Lint& lint : diagnostics_) {
      if (lint.line < context.visible_start_line || lint.line > context.visible_end_line) continue;
      result.diagnostics.emplace_back(lint.line, std::vector<CodeEditorDiagnostic>{
          {lint.column, lint.length, lint.severity, lint.message},
      });
    }
    return result;
  }
 private:
  std::vector<Lint> diagnostics_;
};
```

### Code completion with snippets

```cpp
options.completion_trigger_characters = [](const std::string& ch) { return ch == "."; };

options.completion_provider = [](const CompletionContext& ctx) {
  if (ctx.trigger_kind != CompletionContext::TriggerKind::Character) return std::vector<CompletionItem>{};
  return std::vector<CompletionItem>{
      {.label = "length", .detail = "size_t", .insert_text = "length()", .kind = CompletionItemKind::Property},
      {.label = "push_back", .detail = "void(T)", .insert_text = "push_back($0)", .kind = CompletionItemKind::Function, .insert_text_is_snippet = true},
      {.label = "for", .detail = "snippet", .insert_text = "for (int ${1:i} = 0; ${1:i} < ${2:n}; ++${1:i}) {\n\t$0\n}", .kind = CompletionItemKind::Snippet, .insert_text_is_snippet = true},
  };
};
```

Snippet syntax: `${N:text}` = tab stop with placeholder, `$0` = final cursor position. Tab cycles stops; Esc closes.

### Search bar with programmatic control

```cpp
[[huxerui::composable]]
View SearchDemo() {
  const auto controller = UseCodeEditorController();
  auto query = UseState(std::string());

  return Column {
    Row {
      Button("Find").OnClick([controller] { controller.ToggleSearch(); }),
      Button("Next").OnClick([controller] { controller.FindNext(); }),
      Button("Replace All").OnClick([controller] { controller.ReplaceAll("replacement"); }),
    },
    huxerui::codeeditor::CodeEditor(options, controller).With(Grow{}),
  };
}
```

Ctrl+F toggles the built-in bar (contains Find / Prev / Next / Replace / All / Close).

### Dark mode

```cpp
auto theme = CodeEditorTheme::Default();
theme.background = Color::Rgb(30, 30, 46);
theme.gutter_background = Color::Rgb(24, 24, 37);
theme.current_line_background = Color::Rgb(255, 255, 255, 0.06F);
theme.text_foreground = Color::Rgb(205, 214, 244);
theme.syntax_keyword = Color::Rgb(203, 166, 247);
theme.syntax_string = Color::Rgb(166, 218, 179);
theme.syntax_comment = Color::Rgb(108, 112, 152);
options.theme = theme;
```

Or don't set `options.theme` at all — the editor follows the ambient `MaterialTheme` automatically.

### Custom font (Android)

Ship a TTF at `assets/fonts/<FamilyName>.ttf` and set:

```cpp
options.font_family = "MyFont";   // loads assets/fonts/MyFont.ttf
options.font_size = 15.0F;
```

Font changes apply live — undo history, cursor, scroll position, and fold state are preserved.

### Event-driven status bar

```cpp
auto cursor_status = UseState(std::string("Ln 1, Col 1"));
auto dirty = UseState(false);

CodeEditor(options)
    .On<CodeEditorEvents::CursorChanged>([cursor_status](uint32_t line, uint32_t col) {
      cursor_status = "Ln " + std::to_string(line + 1) + ", Col " + std::to_string(col + 1);
    })
    .On<CodeEditorEvents::TextChanged>([dirty] { dirty = true; })
    .On<CodeEditorEvents::SelectionChanged>(
        [](uint32_t sl, uint32_t sc, uint32_t el, uint32_t ec) {
          // sl..ec is the selection range (0-based, normalized)
        }
    );
```

### Gutter icons (breakpoints)

```cpp
class BreakpointProvider final : public CodeEditorDecorationProvider {
 public:
  CodeEditorDecorationResult ProvideDecorations(const CodeEditorDecorationContext& context) override {
    CodeEditorDecorationResult result;
    for (uint32_t line : breakpoints_) {
      if (line >= context.visible_start_line && line <= context.visible_end_line) {
        result.gutter_icons.emplace_back(line, std::vector<CodeEditorGutterIcon>{{2}});  // 2 = circle
      }
    }
    return result;
  }
  void Toggle(uint32_t line) { /* add/remove from breakpoints_ */ }
 private:
  std::set<uint32_t> breakpoints_;
};

// handle clicks:
editor.On<CodeEditorEvents::GutterIconClicked>([provider](uint32_t line, int32_t id) {
  if (id == 2) provider->Toggle(line);
});
```

### Diff view

```cpp
options.original_text = pristine_file_content;  // the baseline
options.initial_text = current_file_content;     // what the user is editing
```

Added/removed lines get background colors from `theme.diff_added/removed_background`. The document remains fully editable; the diff recomputes on every edit.

### Inline AI suggestion (phantom text)

```cpp
class CopilotProvider final : public CodeEditorDecorationProvider {
 public:
  CodeEditorDecorationResult ProvideDecorations(const CodeEditorDecorationContext& context) override {
    CodeEditorDecorationResult result;
    if (context.cursor_line < 100) {
      result.phantom_texts.emplace_back(
          context.cursor_line,
          std::vector<CodeEditorPhantomText>{{GetLineColumns(context.cursor_line), " // suggested completion"}}
      );
    }
    return result;
  }
};
// options.accept_phantom_on_tab = true (default) — Tab commits the suggestion
```
