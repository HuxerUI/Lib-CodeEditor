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

## 2. CodeEditorOptions

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

## 3. CodeEditorTheme

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

## 4. CodeEditorEvents

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

## 5. CodeEditorDecorationProvider

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

## 6. CodeEditorController

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

## 7. Completion types

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

## 8. Architecture

```text
CodeEditor() -> Canvas -> CodeEditorBehavior -> Extension(NodeExtension) -> EditorHolder -> SweetEditor EditorCore
```

## 9. Limitations

- `initial_text` is an initializer, not a fully controlled value.
- Color-swatch inlays and separator guides not yet exposed.
- Fold regions are provider-owned; the editor preserves interactive fold state.

## 10. Validation checklist

Mount, recomposition, document switching, unmount, editing, clipboard, undo/redo, CJK IME, emoji, caret blink, selection, scrolling, folding, brackets, highlighting, completion, snippets, search/replace, diff, diagnostics, inlay hints, code lens, gutter clicks, pinch zoom, custom fonts, theme switching, Android arm64-v8a build.
