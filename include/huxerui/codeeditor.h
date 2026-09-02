#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <source_location>
#include <string>
#include <utility>
#include <vector>

#include <huxerui/event.h>
#include <huxerui/state.h>
#include <huxerui/view.h>

namespace huxerui::codeeditor {

// Typed editor events, aggregated like ViewEvents. Bind them on the View
// returned by CodeEditor():
//
//   CodeEditor(options).On<CodeEditorEvents::TextChanged>([] { ... })
//
struct CodeEditorEvents {
  struct TextChanged : huxerui::Event<void()> {};
  struct CursorChanged : huxerui::Event<void(std::uint32_t, std::uint32_t)> {};
  struct SelectionChanged
      : huxerui::Event<void(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t)> {};
  struct ScrollChanged : huxerui::Event<void(float, float)> {};
  struct FoldToggled : huxerui::Event<void(std::size_t)> {};
  struct LongPressed : huxerui::Event<void(std::uint32_t, std::uint32_t)> {};
  struct DoubleTapped : huxerui::Event<void(std::uint32_t, std::uint32_t)> {};
  struct LinkClicked : huxerui::Event<void(const std::string&)> {};
  struct CodeLensClicked : huxerui::Event<void(std::int32_t)> {};
  struct GutterIconClicked : huxerui::Event<void(std::uint32_t, std::int32_t)> {};
  struct InlayClicked : huxerui::Event<void(std::uint32_t, std::uint32_t)> {};
};

// Syntax style ids resolved by the default palette the component registers on
// the editor core. Custom ids may be registered through the controller.
enum class CodeEditorStyle : std::int32_t {
  Keyword = 1,
  Type = 2,
  Class = 3,
  Function = 4,
  Variable = 5,
  String = 6,
  Number = 7,
  Comment = 8,
  Preprocessor = 9,
  Builtin = 10,
  Punctuation = 11,
  Annotation = 12,
  Url = 13,
  // Rainbow bracket depth palette (100..107).
  RainbowFirst = 100,
  RainbowLast = 107,
};

// ---- Decoration interface (mirrors the SweetEditor platform wrappers) ----

// One style span inside a line: [column, column + length).
struct CodeEditorStyleSpan {
  std::uint32_t column = 0;
  std::uint32_t length = 0;
  CodeEditorStyle style = CodeEditorStyle::Keyword;
};

// Per-line item lists keyed by 0-based line number.
template <typename T>
using CodeEditorLineEntries = std::vector<std::pair<std::uint32_t, std::vector<T>>>;

struct CodeEditorInlayHint {
  std::uint32_t column = 0;
  std::string text;
};

struct CodeEditorDiagnostic {
  std::uint32_t column = 0;
  std::uint32_t length = 0;
  // 0 = error, 1 = warning, 2 = info, 3 = hint.
  std::int32_t severity = 1;
  std::string message;
};

struct CodeEditorCodeLens {
  std::uint32_t column = 0;
  std::int32_t command_id = 0;
  std::string title;
};

struct CodeEditorLink {
  std::uint32_t column = 0;
  std::uint32_t length = 0;
  std::string url;
};

struct CodeEditorGutterIcon {
  std::int32_t icon_id = 0;
};

struct CodeEditorIndentGuide {
  std::uint32_t start_line = 0;
  std::uint32_t end_line = 0;
  std::uint32_t column = 0;
};

struct CodeEditorFoldRegion {
  std::uint32_t start_line = 0;
  std::uint32_t end_line = 0;
};

struct CodeEditorBracketMatch {
  std::uint32_t line = 0;
  std::uint32_t column = 0;
  std::uint32_t partner_line = 0;
  std::uint32_t partner_column = 0;
};

// Ghost suggestion rendered at the end of a line; committed by Tab when
// `accept_phantom_on_tab` is enabled.
struct CodeEditorPhantomText {
  std::uint32_t column = 0;
  std::string text;
};

// One incremental document edit since the previous refresh.
struct CodeEditorTextChange {
  std::uint32_t start_line = 0;
  std::uint32_t start_column = 0;
  std::uint32_t end_line = 0;
  std::uint32_t end_column = 0;
  std::string new_text;
};

// Everything a provider may contribute for the requested viewport. Empty
// fields are skipped, so providers fill only what they compute.
struct CodeEditorDecorationResult {
  CodeEditorLineEntries<CodeEditorStyleSpan> syntax_spans;
  CodeEditorLineEntries<CodeEditorStyleSpan> overlay_spans;
  CodeEditorLineEntries<CodeEditorStyleSpan> document_highlights;
  CodeEditorLineEntries<CodeEditorInlayHint> inlay_hints;
  CodeEditorLineEntries<CodeEditorDiagnostic> diagnostics;
  CodeEditorLineEntries<CodeEditorCodeLens> code_lens;
  CodeEditorLineEntries<CodeEditorLink> links;
  CodeEditorLineEntries<CodeEditorGutterIcon> gutter_icons;
  CodeEditorLineEntries<CodeEditorPhantomText> phantom_texts;
  std::vector<CodeEditorIndentGuide> indent_guides;
  std::vector<CodeEditorFoldRegion> fold_regions;
  std::optional<CodeEditorBracketMatch> matched_bracket;
};

// Context handed to providers on every refresh.
struct CodeEditorDecorationContext {
  // Visible 0-based line range, inclusive.
  std::uint32_t visible_start_line = 0;
  std::uint32_t visible_end_line = 0;
  std::uint32_t total_line_count = 0;
  std::uint32_t cursor_line = 0;
  std::uint32_t cursor_column = 0;
  // False during fast scrolling; heavy recomputation can wait for settled
  // refreshes.
  bool viewport_settled = true;
  // Full UTF-8 document text, valid for the duration of the call. Providers
  // that keep their own document model can lazily resynchronize from it.
  const std::string* document_text = nullptr;
  // Incremental edits applied since the previous refresh (empty on the first
  // refresh and after a document reload).
  std::vector<CodeEditorTextChange> text_changes;
};

// Unified decoration source: syntax highlighting, diagnostics, inlay hints,
// code lens, links, gutter icons, indent guides, fold regions, document
// highlights, and bracket matching all flow through this interface, so the
// editor never depends on a concrete highlighting engine. Wire SweetLine, a
// language server, or any custom backend by implementing this class.
class CodeEditorDecorationProvider {
 public:
  virtual ~CodeEditorDecorationProvider() = default;

  virtual CodeEditorDecorationResult ProvideDecorations(const CodeEditorDecorationContext& context) = 0;
};

// ---- Code completion ----

enum class CompletionItemKind : int32_t {
  Keyword = 0,
  Function = 1,
  Variable = 2,
  Class = 3,
  Interface = 4,
  Module = 5,
  Property = 6,
  Snippet = 7,
  Text = 8,
};

struct CompletionItem {
  std::string label;
  std::string detail;
  // Applied on confirmation; falls back to `label` when empty.
  std::string insert_text;
  CompletionItemKind kind{CompletionItemKind::Text};
  std::string filter_text;
  std::string sort_key;
  // When true, `insert_text` is a snippet template expanded by the core.
  bool insert_text_is_snippet{false};
  // Optional replacement range (columns within the cursor line). Without it,
  // text is inserted at the caret.
  bool has_text_edit{false};
  uint32_t text_edit_start{0};
  uint32_t text_edit_end{0};
  std::string text_edit_text;
};

struct CompletionContext {
  enum class TriggerKind : int32_t {
    // Manually triggered (Ctrl+Space).
    Invoked = 0,
    // A trigger character was typed (for example '.').
    Character = 1,
    // Content changed while the panel was already active.
    Retrigger = 2,
  };

  TriggerKind trigger_kind{TriggerKind::Invoked};
  std::string trigger_character;
  uint32_t cursor_line{0};
  uint32_t cursor_column{0};
  std::string line_text;
  // Word (identifier) range around the caret, if any.
  uint32_t word_start{0};
  uint32_t word_end{0};
};

using CompletionProvider = std::function<std::vector<CompletionItem>(const CompletionContext&)>;

// ---- Controller ----

namespace detail {
class CodeEditorControllerState;
struct CodeEditorControllerAccess;
}  // namespace detail

// External control surface for a mounted CodeEditor. A controller is scope
// state created with UseCodeEditorController() and passed to CodeEditor().
// Methods operate on the currently mounted editor node and return false when
// no editor is connected (for example before the component mounts or after it
// unmounts).
class CodeEditorController {
 public:
  CodeEditorController();

  [[nodiscard]] bool IsConnected() const noexcept;

  // Loads a different document, recreating the editor core for
  // `document_key` (like changing CodeEditorOptions).
  bool LoadDocument(const std::string& document_key, const std::string& text) const;
  [[nodiscard]] std::string Text() const;
  bool SetCursor(std::uint32_t line, std::uint32_t column) const;

  // Search and replace operations on the current document.
  bool RunSearch(const std::string& pattern) const;
  bool FindNext() const;
  bool FindPrevious() const;
  bool ReplaceCurrent(const std::string& replacement) const;
  bool ReplaceAll(const std::string& replacement) const;
  bool ClearSearch() const;
  // Toggles the component's built-in search bar.
  bool ToggleSearch() const;

  bool operator==(const CodeEditorController&) const = default;

 private:
  std::shared_ptr<detail::CodeEditorControllerState> state_;

  friend struct detail::CodeEditorControllerAccess;
};

inline CodeEditorController UseCodeEditorController(
    const std::source_location& location = std::source_location::current()
) {
  return huxerui::UseState(CodeEditorController{}, location).Get();
}

// Visual theme for the editor chrome, range effects, and decorations. All
// colors are 0xAARRGGBB; defaults form the light reference theme.
struct CodeEditorTheme {
  // Surfaces.
  int32_t background = 0xFFFFFFFF;
  int32_t gutter_background = 0xFFF2F3F5;
  // Focused (caret) line highlight drawn across the text area.
  int32_t current_line_background = 0x0F000000;
  int32_t separator_color = 0xFFB0B7C3;

  // Text, caret, and gutter.
  int32_t text_foreground = 0xFF1F1F1F;
  int32_t line_number_color = 0xFF9AA0A6;
  int32_t caret_color = 0xFF1F1F1F;
  float caret_width = 2.0F;

  // Links and code lens.
  int32_t link_color = 0xFF4C9DFF;
  int32_t active_link_color = 0xFF4C9DFF;
  int32_t codelens_color = 0xB0344A73;
  int32_t active_codelens_color = 0xFF3A5FA0;

  // Selection, search, bracket match, and document highlights.
  int32_t selection_background = 0x554A90E2;
  int32_t search_match_background = 0x33F59E0B;
  int32_t search_current_background = 0x55F59E0B;
  int32_t bracket_match_background = 0x260F766E;
  int32_t document_highlight_text = 0x142563EB;
  int32_t document_highlight_read = 0x1C2563EB;
  int32_t document_highlight_write = 0x282563EB;
  int32_t ime_composition_underline = 0xFF2563EB;

  // Diagnostic underlines (wavy).
  int32_t diagnostic_error_underline = 0xFFDC2626;
  int32_t diagnostic_warning_underline = 0xFFD97706;
  int32_t diagnostic_info_underline = 0xFF0EA5E9;
  int32_t diagnostic_hint_underline = 0xFF64748B;

  // Diff presentation.
  int32_t diff_added_background = 0x1FA6E22E;
  int32_t diff_removed_background = 0x1FF92672;
  int32_t diff_added_gutter = 0x2FA6E22E;
  int32_t diff_removed_gutter = 0x2FF92672;

  // Decorations.
  int32_t indent_guide_color = 0xFFC8C8C8;
  int32_t inlay_hint_background = 0x143B82F6;
  int32_t inlay_hint_text = 0xB0344A73;
  int32_t fold_placeholder_background = 0x2E748DB0;
  int32_t fold_placeholder_text = 0xFF284A70;
  int32_t gutter_icon_color = 0xFF267F99;

  // Completion panel.
  int32_t completion_background = 0xF0FAFBFD;
  int32_t completion_border = 0x30A0A8B8;
  int32_t completion_selected_background = 0x3D3B82F6;
  int32_t completion_label = 0xFF1F2937;
  int32_t completion_detail = 0xFF8A94A6;
};

// ---- Options ----

// Declarative configuration for a CodeEditor instance.
struct CodeEditorOptions {
  // Initial document content; loaded when the editor is created or when
  // `document_key` changes. Not a controlled value.
  std::string initial_text;
  // Optional document identity. Changing it between recompositions reloads
  // `initial_text` and recreates the editor. Empty keeps one default document.
  std::string document_key;

  float font_size = 14.0F;
  float line_spacing_add = 0.0F;
  float line_spacing_mult = 1.2F;
  // Visual theme (colors, caret width); defaults form the light reference.
  CodeEditorTheme theme;

  bool read_only = false;
  uint32_t tab_size = 4;
  bool backspace_unindent = true;
  bool insert_spaces = true;
  std::vector<std::pair<char32_t, char32_t>> auto_closing_pairs;

  // Optional code completion.
  CompletionProvider completion_provider;
  std::function<bool(const std::string&)> completion_trigger_characters;

  // Decoration sources: syntax highlighting, diagnostics, inlay hints, code
  // lens, links, gutter icons, indent guides, fold regions, document
  // highlights, and bracket matching. The editor ships no highlighting
  // engine; implement CodeEditorDecorationProvider (or reuse the optional
  // SweetLine integration from examples/preview) to light the editor up.
  std::vector<std::shared_ptr<CodeEditorDecorationProvider>> decoration_providers;
  // Whether Tab commits the caret-line phantom text supplied by providers.
  bool accept_phantom_on_tab = true;

  // Newline action hook: invoked before inserting a newline; may return
  // replacement text.
  std::function<std::string(uint32_t line, uint32_t column)> newline_action;

  // When set, the core computes a line-level diff against this original text.
  // Empty disables the diff.
  std::string original_text;

  // Display options.
  bool render_whitespace = false;
  bool render_line_breaks = false;
  // 0 = none, 1 = character wrap, 2 = word wrap.
  int wrap_mode = 0;
  bool sticky_gutter = false;
  // 0 = always, 1 = transient, 2 = never. Zero thickness keeps the default.
  float scrollbar_thickness = 0.0F;
  int scrollbar_mode = 0;
  float content_start_padding = 0.0F;
};

// A HuxerUI code editor component backed by the SweetEditor core.
//
// The component owns one retained editor-core extension, bridges text
// measurement to HuxerUI's platform measurer, paints the editor render model,
// forwards input into the core, and applies decorations from the registered
// CodeEditorDecorationProvider instances.
huxerui::View CodeEditor(CodeEditorOptions options = {}, CodeEditorController controller = {});

}  // namespace huxerui::codeeditor
