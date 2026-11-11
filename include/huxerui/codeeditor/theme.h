#pragma once

/// @file
/// @brief Visual palettes and text styles for CodeEditor.

#include <array>
#include <cstdint>
#include <map>

#include <huxerui/color.h>
#include <huxerui/theme.h>

namespace huxerui::codeeditor {

/// First of the six style IDs used for Markdown heading levels.
inline constexpr std::uint32_t markdown_heading_style_base = 200;

/// @brief Standard style IDs for syntax highlighting and built-in palettes.
enum class EditorStyle : std::int32_t {
  /// Language keyword.
  Keyword = 1,
  /// String literal.
  String = 2,
  /// Source comment.
  Comment = 3,
  /// Numeric literal.
  Number = 4,
  /// Built-in language symbol.
  Builtin = 5,
  /// Type name.
  Type = 6,
  /// Class name.
  Class = 7,
  /// Function or method name.
  Function = 8,
  /// Variable name.
  Variable = 9,
  /// Operator, delimiter, or punctuation token.
  Punctuation = 10,
  /// Language annotation.
  Annotation = 11,
  /// Preprocessor directive.
  Preprocessor = 12,
  /// Start of the extended style ID range.
  UserBase = 100,
  /// First color used for nested brackets.
  RainbowFirst = UserBase,
  /// Last color used for nested brackets.
  RainbowLast = 107,
  /// URL token.
  Url = 108,
};

/// @brief Colors and font attributes for a registered syntax style.
struct EditorTextStyle {
  /// Foreground color for text using the style.
  Color foreground;
  /// Background behind the styled text.
  Color background = Color::Transparent();
  /// Bitmask of SweetEditor font attributes such as bold, italic, and strikethrough.
  std::int32_t font_style = 0;

  /// Compares all text style values.
  bool operator==(const EditorTextStyle&) const = default;
};

/// @brief Visual palette for document text, editor chrome, and decorations.
///
/// Start with Light(), Dark(), Everforest(), or FromThemeSpec() for a complete palette.
/// CodeEditor uses Dark() by default.
struct EditorTheme {
  /// Editor text-area background.
  Color background_color;
  /// Background behind line numbers and gutter decorations.
  Color gutter_background;
  /// Fill or border color for the line containing the caret.
  Color current_line_color;
  /// Color of horizontal separator guides.
  Color separator_line_color;

  /// Default document text color.
  Color text_color;
  /// Gutter color for inactive line numbers.
  Color line_number_color;
  /// Caret and selection-handle color.
  Color cursor_color;
  /// Positive, finite caret width in logical layout units.
  float caret_width = 2.0F;

  /// Foreground color for links.
  Color link_color;
  /// Foreground color for the active link.
  Color link_active_color;
  /// Foreground color for code lens items.
  Color code_lens_color;
  /// Foreground color for the active code lens item.
  Color code_lens_active_color;

  /// Background fill for selected text.
  Color selection_color;
  /// Background of search matches other than the active match.
  Color search_match_bg_color;
  /// Background of the active search match.
  Color search_current_bg_color;
  /// Background of a matching bracket.
  Color bracket_highlight_bg_color;
  /// Background of text occurrences.
  Color document_highlight_text_bg_color;
  /// Background of read occurrences.
  Color document_highlight_read_bg_color;
  /// Background of write occurrences.
  Color document_highlight_write_bg_color;
  /// Underline color for active platform text-input composition.
  Color composition_underline_color;

  /// Underline color for error diagnostics.
  Color diagnostic_error_color;
  /// Underline color for warning diagnostics.
  Color diagnostic_warning_color;
  /// Underline color for informational diagnostics.
  Color diagnostic_info_color;
  /// Underline color for hint diagnostics.
  Color diagnostic_hint_color;

  /// Text-area background for added lines in a diff.
  Color diff_added_background;
  /// Text-area background for removed lines in a diff.
  Color diff_removed_background;
  /// Gutter background for added lines in a diff.
  Color diff_added_gutter;
  /// Gutter background for removed lines in a diff.
  Color diff_removed_gutter;

  /// Color of indent, bracket, and control-flow guides.
  Color guide_color;
  /// Background of inline hints.
  Color inlay_hint_bg_color;
  /// Foreground of inline hint labels.
  Color inlay_hint_text_color;
  /// Background of inline fold placeholders.
  Color fold_placeholder_bg_color;
  /// Foreground of inline fold placeholders.
  Color fold_placeholder_text_color;
  /// Color for built-in gutter icon rendering.
  Color gutter_icon_color;

  /// Background of the completion panel.
  Color completion_bg_color;
  /// Border of the completion panel.
  Color completion_border_color;
  /// Background of the selected completion row.
  Color completion_selected_bg_color;
  /// Primary label color in completion rows.
  Color completion_label_color;
  /// Secondary text color in completion rows.
  Color completion_detail_color;

  /// Selected text color. Use Color::Transparent() to inherit the run color.
  Color selection_text_color = Color::Transparent();
  /// Gutter color for the line number at the caret.
  Color current_line_number_color = Color::Rgb(31, 35, 40);
  /// Vertical divider color between the gutter and text.
  Color split_line_color = Color::Rgb(176, 183, 195);
  /// Scrollbar track color.
  Color scrollbar_track_color = Color::Transparent();
  /// Scrollbar thumb color.
  Color scrollbar_thumb_color = Color::Rgb(31, 35, 40, 0.32F);
  /// Scrollbar thumb color during active interaction.
  Color scrollbar_thumb_active_color = Color::Rgb(31, 35, 40, 0.6F);
  /// Outline of the active search match.
  Color search_current_border_color = Color::Rgb(245, 158, 11);
  /// Color of the built-in inline hint icon.
  Color inlay_hint_icon_color = Color::Rgb(99, 102, 241);
  /// Color of display-only text and inline suggestions.
  Color phantom_text_color = Color::Rgb(100, 116, 139);
  /// Color of whitespace and line-ending markers.
  Color invisible_character_color = Color::Rgb(100, 116, 139);
  /// Outline of the active linked-editing range.
  Color linked_editing_active_color = Color::Rgb(37, 99, 235);
  /// Outline of the other linked-editing ranges.
  Color linked_editing_inactive_color = Color::Rgb(100, 116, 139);
  /// Outline of a matching bracket.
  Color bracket_highlight_border_color = Color::Rgb(15, 118, 110);
  /// Surface color of the touch selection toolbar.
  Color selection_menu_background_color = Color::White();
  /// Label color of the touch selection toolbar.
  Color selection_menu_text_color = Color::Rgb(31, 35, 40);
  /// Separator color of the touch selection toolbar.
  Color selection_menu_divider_color = Color::Rgb(176, 183, 195);
  /// Text styles indexed by the IDs used in decoration spans.
  std::map<std::uint32_t, EditorTextStyle> text_styles;

  /// @brief Adds or replaces a text style in this palette.
  /// @param id Style identifier used by StyleSpan.
  /// @param style Colors and font attributes for that identifier.
  void DefineTextStyle(std::uint32_t id, EditorTextStyle style);
  /// Returns the built-in light palette based on SweetEditor platform colors.
  static EditorTheme Light();
  /// Returns the default dark palette based on SweetEditor platform colors.
  static EditorTheme Dark();
  /// @brief Builds a palette from a HuxerUI theme.
  /// @param spec Theme specification supplying the ambient color roles.
  /// @return An editor palette derived from those roles.
  static EditorTheme FromThemeSpec(const ThemeSpec& spec);
  /// Returns the built-in Everforest dark palette.
  static EditorTheme Everforest();

  /// Compares palette values and the registered style map.
  bool operator==(const EditorTheme&) const = default;
};

}  // namespace huxerui::codeeditor
