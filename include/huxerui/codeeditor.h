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

#include <array>

#include <huxerui/color.h>
#include <huxerui/event.h>
#include <huxerui/state.h>
#include <huxerui/theme.h>
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

// Visual theme for the editor chrome, range effects, and decorations.
// Defaults derive from the ambient HuxerUI ThemeSpec (so the editor follows
// MaterialTheme light/dark automatically); override `options.theme` with a
// fully populated struct to take manual control.
struct CodeEditorTheme {
  // Surfaces.
  Color background;
  Color gutter_background;
  // Focused (caret) line highlight drawn across the text area.
  Color current_line_background;
  Color separator_color;

  // Text, caret, and gutter.
  Color text_foreground;
  Color line_number_color;
  Color caret_color;
  float caret_width = 2.0F;

  // Links and code lens.
  Color link_color;
  Color active_link_color;
  Color codelens_color;
  Color active_codelens_color;

  // Selection, search, bracket match, and document highlights.
  Color selection_background;
  Color search_match_background;
  Color search_current_background;
  Color bracket_match_background;
  Color document_highlight_text;
  Color document_highlight_read;
  Color document_highlight_write;
  Color ime_composition_underline;

  // Diagnostic underlines (wavy).
  Color diagnostic_error_underline;
  Color diagnostic_warning_underline;
  Color diagnostic_info_underline;
  Color diagnostic_hint_underline;

  // Diff presentation.
  Color diff_added_background;
  Color diff_removed_background;
  Color diff_added_gutter;
  Color diff_removed_gutter;

  // Decorations.
  Color indent_guide_color;
  Color inlay_hint_background;
  Color inlay_hint_text;
  Color fold_placeholder_background;
  Color fold_placeholder_text;
  Color gutter_icon_color;

  // Syntax token palette (resolves the CodeEditorStyle ids).
  Color syntax_keyword;
  Color syntax_type;
  Color syntax_class;
  Color syntax_function;
  Color syntax_variable;
  Color syntax_string;
  Color syntax_number;
  Color syntax_comment;
  Color syntax_preprocessor;
  Color syntax_builtin;
  Color syntax_punctuation;
  Color syntax_annotation;
  Color syntax_url;
  // Rainbow bracket depth palette (CodeEditorStyle::RainbowFirst..Last).
  std::array<Color, 8> syntax_rainbow;

  // Completion panel.
  Color completion_background;
  Color completion_border;
  Color completion_selected_background;
  Color completion_label;
  Color completion_detail;

  // The light reference theme (the historical hardcoded look).
  static CodeEditorTheme Default();
  // Derives an editor theme from an ambient HuxerUI theme specification.
  static CodeEditorTheme FromThemeSpec(const ThemeSpec& spec);

  bool operator==(const CodeEditorTheme&) const = default;
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
  // Optional custom font family. Empty uses the platform monospace font;
  // a named family resolves against fonts bundled by the host platform
  // (for example "MapleMono" from assets/fonts/MapleMono.ttf on Android),
  // falling back to the system family table. Android additionally honors
  // an "@noliga" suffix that disables ligatures ("Family@noliga").
  std::string font_family;
  float line_spacing_add = 0.0F;
  float line_spacing_mult = 1.2F;
  // Explicit visual override; when empty the editor derives its theme from
  // the ambient HuxerUI theme (UseTheme) and follows Theme changes live.
  std::optional<CodeEditorTheme> theme;

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
