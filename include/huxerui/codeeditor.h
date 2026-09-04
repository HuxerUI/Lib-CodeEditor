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

#include <sweeteditor/decoration.h>
#include <sweeteditor/editor_core.h>

#include <huxerui/color.h>
#include <huxerui/event.h>
#include <huxerui/state.h>
#include <huxerui/theme.h>
#include <huxerui/view.h>

namespace huxerui::codeeditor {

// Typed editor events, aggregated like ViewEvents. Bind them on the View
// returned by CodeEditor():
//
//   CodeEditor(options).On<EditorEvents::TextChanged>([] { ... })
//
// Information the editor hands back when the selection/context gesture fires
// and the built-in menu is disabled (built_in_context_menu = false). The
// application builds its own UI from this data.
struct EditorContextMenuInfo {
  // Caret position (0-based).
  uint32_t cursor_line = 0;
  uint32_t cursor_column = 0;
  // Whether a non-empty selection exists.
  bool has_selection = false;
  // Selection range (0-based), valid when has_selection.
  uint32_t selection_start_line = 0;
  uint32_t selection_start_column = 0;
  uint32_t selection_end_line = 0;
  uint32_t selection_end_column = 0;
  // Selected text (or empty) and the full caret line text.
  std::string selected_text;
  std::string caret_line_text;
  // Local editor-space position where the gesture fired (for placing UI).
  float position_x = 0.0F;
  float position_y = 0.0F;
};

struct EditorEvents {
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
  // Fired when the built-in search UI toggles (Ctrl+F), so an application can
  // mirror the visibility of its own search bar.
  struct SearchVisibilityChanged : huxerui::Event<void(bool)> {};
  // Fired when the context menu executes an application-defined command id
  // (one outside the built-in set below).
  struct ContextCommandInvoked : huxerui::Event<void(std::int32_t)> {};
  // Fired instead of the built-in menu when built_in_context_menu is false:
  // carries the caret/selection/text/position so the application draws its own
  // selection-and-copy UI.
  struct ContextMenuRequested : huxerui::Event<void(const EditorContextMenuInfo&)> {};
};

// Syntax style ids resolved by the default palette the component registers on
// the editor core. Custom ids may be registered through the controller.
enum class EditorStyle : std::int32_t {
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
// SweetEditor core decoration types, re-exported for convenience.
// The editor consumes these directly — no re-wrapping layer.
using StyleSpan = sweeteditor::StyleSpan;
using InlayHint = sweeteditor::InlayHint;
using Diagnostic = sweeteditor::Diagnostic;
using CodeLensItem = sweeteditor::CodeLensItem;
using LinkSpan = sweeteditor::LinkSpan;
using GutterIcon = sweeteditor::GutterIcon;
using IndentGuide = sweeteditor::IndentGuide;
using FoldRegion = sweeteditor::FoldRegion;
using PhantomText = sweeteditor::PhantomText;
using TextChange = sweeteditor::TextChange;
using DocumentHighlight = sweeteditor::DocumentHighlight;

// Everything a provider may contribute for the requested viewport. Empty
// fields are skipped, so providers fill only what they compute.
struct EditorDecorationResult {
  std::vector<std::pair<size_t, std::vector<sweeteditor::StyleSpan>>> syntax_spans;
  std::vector<std::pair<size_t, std::vector<sweeteditor::StyleSpan>>> overlay_spans;
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

// Context handed to providers on every refresh.
struct EditorDecorationContext {
  size_t visible_start_line = 0;
  size_t visible_end_line = 0;
  size_t total_line_count = 0;
  size_t cursor_line = 0;
  size_t cursor_column = 0;
  bool viewport_settled = true;
  const std::string* document_text = nullptr;
  std::vector<sweeteditor::TextChange> text_changes;
};

// Unified decoration source: syntax highlighting, diagnostics, inlay hints,
// code lens, links, gutter icons, indent guides, fold regions, document
// highlights, and bracket matching all flow through this interface, so the
// editor never depends on a concrete highlighting engine. Wire SweetLine, a
// language server, or any custom backend by implementing this class.
class EditorDecorationReceiver {
 public:
  virtual ~EditorDecorationReceiver() = default;
  // Returns false when the request is stale or cancelled.
  virtual bool Accept(EditorDecorationResult result) = 0;
  [[nodiscard]] virtual bool IsCancelled() const = 0;
};

class EditorDecorationProvider {
 public:
  virtual ~EditorDecorationProvider() = default;
  // Synchronous providers call receiver.Accept() before returning.
  // Asynchronous providers hold the receiver and deliver later.
  virtual void ProvideDecorations(
      const EditorDecorationContext& context, EditorDecorationReceiver& receiver
  ) = 0;
};

// A selectable entry for the selection/context menu. The editor executes the
// built-in ids 0..6 (cut, copy, paste, select all, find, fold all, unfold
// all); any other id is reported through EditorEvents::ContextCommandInvoked
// so applications can attach their own actions.
struct EditorContextItem {
  std::string label;
  std::int32_t command = -1;
  bool enabled = true;
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
class EditorControllerState;
struct EditorControllerAccess;
}  // namespace detail

// External control surface for a mounted CodeEditor. A controller is scope
// state created with UseEditorController() and passed to CodeEditor().
// Methods operate on the currently mounted editor node and return false when
// no editor is connected (for example before the component mounts or after it
// unmounts).
class EditorController {
 public:
  EditorController();

  [[nodiscard]] bool IsConnected() const noexcept;

  // Loads a different document, recreating the editor core for
  // `document_key` (like changing EditorOptions).
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

  bool operator==(const EditorController&) const = default;

 private:
  std::shared_ptr<detail::EditorControllerState> state_;

  friend struct detail::EditorControllerAccess;
};

inline EditorController UseEditorController(
    const std::source_location& location = std::source_location::current()
) {
  return huxerui::UseState(EditorController{}, location).Get();
}

// Visual theme for the editor chrome, range effects, and decorations.
// Defaults derive from the ambient HuxerUI ThemeSpec (so the editor follows
// MaterialTheme light/dark automatically); override `options.theme` with a
// fully populated struct to take manual control.
struct EditorTheme {
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

  // Syntax token palette (resolves the EditorStyle ids).
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
  // Rainbow bracket depth palette (EditorStyle::RainbowFirst..Last).
  std::array<Color, 8> syntax_rainbow;

  // Completion panel.
  Color completion_background;
  Color completion_border;
  Color completion_selected_background;
  Color completion_label;
  Color completion_detail;

  // The light reference theme (the historical hardcoded look).
  static EditorTheme Default();
  // Derives an editor theme from an ambient HuxerUI theme specification.
  static EditorTheme FromThemeSpec(const ThemeSpec& spec);
  // Everforest Dark — warm, low-contrast green forest palette.
  static EditorTheme Everforest();

  bool operator==(const EditorTheme&) const = default;
};
// ---- Options ----

// Declarative configuration for a CodeEditor instance.
struct EditorOptions {
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
  std::optional<EditorTheme> theme;

  bool read_only = false;
  uint32_t tab_size = 4;
  bool backspace_unindent = true;
  bool insert_spaces = true;
  std::vector<std::pair<char32_t, char32_t>> auto_closing_pairs;

  // Selection/context menu customization. Empty uses the built-in list
  // (Cut/Copy/Paste/Select All/Find). Supplying a builder replaces the whole
  // list; ids outside the built-in set fire ContextCommandInvoked.
  std::function<std::vector<EditorContextItem>()> context_menu_items;

  // Optional code completion.
  CompletionProvider completion_provider;
  std::function<bool(const std::string&)> completion_trigger_characters;

  // Decoration sources: syntax highlighting, diagnostics, inlay hints, code
  // lens, links, gutter icons, indent guides, fold regions, document
  // highlights, and bracket matching. The editor ships no highlighting
  // engine; implement EditorDecorationProvider (or reuse the optional
  // SweetLine integration from examples/preview) to light the editor up.
  std::vector<std::shared_ptr<EditorDecorationProvider>> decoration_providers;
  // Whether Tab commits the caret-line phantom text supplied by providers.
  bool accept_phantom_on_tab = true;
  // When true the component draws the default selection/context menu on
  // long-press / double-tap; when false it emits ContextMenuRequested with the
  // caret, selection, text, and position instead, and the application builds
  // its own menu UI.
  bool built_in_context_menu = true;
  // When true the component composes its default search bar above the editor;
  // when false, callers compose the exported EditorSearchBar themselves (or
  // none) and mirror visibility through the SearchVisibilityChanged event.
  bool built_in_search_bar = true;

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
// EditorDecorationProvider instances.
huxerui::View CodeEditor(EditorOptions options = {}, EditorController controller = {});

// The exported search bar UI. Compose it wherever you like and feed it the
// three application states; set EditorOptions::built_in_search_bar = false on
// the editor when you take over the search UI.
huxerui::View EditorSearchBar(
    huxerui::State<std::string> search_text,
    huxerui::State<std::string> replace_text,
    huxerui::State<bool> visible,
    EditorController controller
);

}  // namespace huxerui::codeeditor
