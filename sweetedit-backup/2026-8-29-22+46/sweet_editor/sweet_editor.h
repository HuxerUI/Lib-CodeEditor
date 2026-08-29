#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <huxerui/huxerui.h>

namespace sweetedit_huxer {

// Code completion data model, mirroring the SweetEditor platform reference
// implementation (Android `completion` package): a provider receives a
// CompletionContext and returns CompletionItems; the editor shows a caret
// following panel with up/down/enter/escape handling and applies the selected
// item through the core (replaceText / insertText / insertSnippet).

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
  // Optional replacement range (columns within the cursor line), matching the
  // textEdit semantics of the reference item. Without it, text is inserted at
  // the caret.
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

// Host decoration provider data (reference DecorationProvider/DecorationResult).
struct HostInlayHint {
  uint32_t line{0};
  uint32_t column{0};
  std::string text;
};

struct HostDiagnostic {
  uint32_t line{0};
  uint32_t column{0};
  uint32_t length{0};
  // 0 = error, 1 = warning, 2 = info, 3 = hint.
  int32_t severity{1};
  std::string message;
};

struct HostCodeLens {
  uint32_t line{0};
  uint32_t column{0};
  int32_t command_id{0};
  std::string title;
};

struct HostDecorationResult {
  std::vector<HostInlayHint> inlay_hints;
  std::vector<HostDiagnostic> diagnostics;
  std::vector<HostCodeLens> codelens;
};

// Returns host decorations for the visible line range [start_line, end_line].
using HostDecorationProvider = std::function<HostDecorationResult(uint32_t start_line, uint32_t end_line)>;

// Returns gutter icon ids to render for each line in [start_line, end_line]
// (reference GutterIconProvider): the host can mark breakpoints/errors etc.
using GutterIconProvider = std::function<std::vector<std::pair<uint32_t, int32_t>>(uint32_t start_line, uint32_t end_line)>;

// Declarative configuration for a SweetEditor instance.
struct SweetEditorOptions {
  std::string initial_text;
  // SweetLine syntax definition (JSON) used to highlight `initial_text`. When
  // empty the built-in C++ grammar (raw/syntaxes/cpp.json) is used.
  std::string syntax_json;
  // Stable identity for the current document. Changing it between
  // recompositions reloads `initial_text` with `syntax_json` (the editor core
  // and highlighter are rebuilt), enabling multi-file demo switching.
  std::string document_key;
  float font_size = 14.0F;
  float line_spacing_add = 0.0F;
  float line_spacing_mult = 1.2F;
  bool read_only = false;
  // Editing feel (reference platform defaults): tab stop width, whether
  // Backspace on leading whitespace unindents to the tab stop, whether Tab
  // inserts spaces up to the next stop instead of a literal tab, and the
  // auto-closing bracket pairs. Empty pairs keep the core's default () {} [].
  uint32_t tab_size = 4;
  bool backspace_unindent = true;
  bool insert_spaces = true;
  std::vector<std::pair<char32_t, char32_t>> auto_closing_pairs;
  // Optional code completion. When `completion_trigger_characters` is set,
  // typing one of those characters fires a CHARACTER request; Ctrl+Space fires
  // INVOKED; further typing while the panel is active re-requests with
  // RETRIGGER (the reference manager flow).
  CompletionProvider completion_provider;
  std::function<bool(const std::string&)> completion_trigger_characters;
  // Decoration click callbacks (reference fireGestureEvents decoration hits).
  std::function<void(const std::string& url)> on_link_click;
  std::function<void(int32_t command_id)> on_codelens_click;
  // line is the logical line owning the icon, icon_id identifies the marker.
  std::function<void(uint32_t line, int32_t icon_id)> on_gutter_icon_click;
  std::function<void(uint32_t line, uint32_t column)> on_inlay_click;
  // Editor event bus (reference EditorEventBus): fired on document, caret,
  // selection, scroll, fold, and gesture changes.
  std::function<void()> on_text_changed;
  std::function<void(uint32_t line, uint32_t column)> on_cursor_changed;
  std::function<void(uint32_t start_line, uint32_t start_column, uint32_t end_line, uint32_t end_column)>
      on_selection_changed;
  std::function<void(float scroll_x, float scroll_y)> on_scroll_changed;
  std::function<void(size_t line)> on_fold_toggle;
  std::function<void(uint32_t line, uint32_t column)> on_long_press;
  std::function<void(uint32_t line, uint32_t column)> on_double_tap;
  // Newline action hook: invoked before inserting a newline; may return
  // replacement text (reference NewLineActionProvider).
  std::function<std::string(uint32_t line, uint32_t column)> newline_action;
  // Phantom text provider: returns ghost text shown at the end of the given
  // line (rendered as PHANTOM_TEXT). When `accept_phantom_on_tab` is true, Tab
  // commits the caret-line phantom text.
  std::function<std::string(uint32_t line)> phantom_text_provider;
  bool accept_phantom_on_tab = true;
  // Host decoration providers (reference DecorationProvider): invoked with the
  // visible line range during decoration refresh; results merge into the
  // editor's inlay hints, diagnostics, and code lens.
  std::vector<HostDecorationProvider> decoration_providers;
  // Invoked on Ctrl+F (or the platform search shortcut): the host may toggle
  // its find/replace bar. The component's built-in bar uses this to show.
  std::function<void()> on_toggle_search;
  // When set, the core computes a line-level diff against this original text
  // (reference diff presentation): added/removed lines get background colors
  // and the removed lines are shown in the gutter. Empty disables the diff.
  std::string original_text;
  // Render whitespace markers (space dots / tab arrows) and line-break
  // symbols. The reference platform exposes these as display options.
  bool render_whitespace = false;
  bool render_line_breaks = false;
  // Wrap long lines (character or word level); NONE keeps horizontal scroll.
  // The core WrapMode values map directly.
  int wrap_mode = 0;  // 0 = NONE, 1 = CHAR_BREAK, 2 = WORD_BREAK
  // Keep the gutter (line-number column) fixed while scrolling horizontally.
  bool sticky_gutter = false;
  // Scrollbar customization (reference ScrollbarConfig): track thickness and
  // visibility mode (0 = ALWAYS, 1 = TRANSIENT, 2 = NEVER). Zero thickness
  // keeps the core default.
  float scrollbar_thickness = 0.0F;
  int scrollbar_mode = 0;
  // Extra horizontal padding between the gutter and the text start.
  float content_start_padding = 0.0F;
  // Host gutter icons (breakpoints, errors) merged during decoration refresh.
  GutterIconProvider gutter_icon_provider;
};

// A HuxerUI code editor backed by the SweetEditor core (3dparty/SweetEditor).
//
// The component owns one SweetEditor EditorCore, bridges text measurement to
// HuxerUI's platform measurer, renders the EditorRenderModel through a Canvas,
// and forwards pointer and keyboard events into the core.
huxerui::View SweetEditor(SweetEditorOptions options = {});

}  // namespace sweetedit_huxer
