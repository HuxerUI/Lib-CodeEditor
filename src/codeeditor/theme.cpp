#include <huxerui/codeeditor/theme.h>

#include <sweeteditor/decoration.h>

#include <utility>

namespace huxerui::codeeditor {
namespace {

void DefineMarkdownHeadingStyles(EditorTheme& theme) {
  const auto heading = theme.text_styles.at(static_cast<std::uint32_t>(EditorStyle::Annotation)).foreground;
  for (std::uint32_t index = 0; index < 6; ++index) {
    theme.DefineTextStyle(markdown_heading_style_base + index,
                          {heading, Color::Transparent(), sweeteditor::FONT_STYLE_BOLD});
  }
}

void DefineSyntaxStyle(EditorTheme& theme, EditorStyle style, std::uint32_t argb,
                       std::int32_t font_style = sweeteditor::FONT_STYLE_NORMAL) {
  theme.DefineTextStyle(static_cast<std::uint32_t>(style), {Color::Argb32(argb), Color::Transparent(), font_style});
}

void DefinePresetDecorationStyles(EditorTheme& theme) {
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Url), {theme.link_color});
  constexpr std::array rainbow = {
      EditorStyle::Preprocessor, EditorStyle::Number, EditorStyle::Class, EditorStyle::String,
      EditorStyle::Function, EditorStyle::Keyword, EditorStyle::Type, EditorStyle::Builtin,
  };
  auto id = static_cast<std::uint32_t>(EditorStyle::RainbowFirst);
  for (auto style : rainbow) {
    theme.DefineTextStyle(id++, {theme.text_styles.at(static_cast<std::uint32_t>(style)).foreground});
  }
  DefineMarkdownHeadingStyles(theme);
}

void CompletePalette(EditorTheme& theme) {
  theme.selection_text_color = theme.text_color;
  theme.current_line_number_color = theme.text_color;
  theme.split_line_color = theme.separator_line_color;
  theme.scrollbar_track_color = theme.gutter_background;
  theme.scrollbar_thumb_color = theme.line_number_color;
  theme.scrollbar_thumb_color.alpha = 0.35F;
  theme.scrollbar_thumb_active_color = theme.line_number_color;
  theme.search_current_border_color = theme.search_current_bg_color;
  theme.search_current_border_color.alpha = 1.0F;
  theme.inlay_hint_icon_color = theme.inlay_hint_text_color;
  theme.phantom_text_color = theme.line_number_color;
  theme.invisible_character_color = theme.line_number_color;
  theme.linked_editing_active_color = theme.cursor_color;
  theme.linked_editing_inactive_color = theme.line_number_color;
  theme.bracket_highlight_border_color = theme.cursor_color;
  theme.selection_menu_background_color = theme.completion_bg_color;
  theme.selection_menu_text_color = theme.completion_label_color;
  theme.selection_menu_divider_color = theme.separator_line_color;
  DefineMarkdownHeadingStyles(theme);
}

}  // namespace

void EditorTheme::DefineTextStyle(std::uint32_t id, EditorTextStyle style) {
  text_styles.insert_or_assign(id, std::move(style));
}

// The presets follow Swing's EditorTheme, with selection menu colors from Android.
EditorTheme EditorTheme::Dark() {
  EditorTheme theme;
  theme.background_color = Color::Argb32(0xFF1B1E24);
  theme.gutter_background = theme.background_color;
  theme.text_color = Color::Argb32(0xFFD7DEE9);
  theme.cursor_color = Color::Argb32(0xFF8FB8FF);
  theme.selection_color = Color::Argb32(0x553B4F72);
  theme.selection_text_color = Color::Argb32(0xFFFFFFFF);
  theme.line_number_color = Color::Argb32(0xFF5E6778);
  theme.current_line_number_color = Color::Argb32(0xFF9CB3D6);
  theme.current_line_color = Color::Argb32(0x503A4A66);
  theme.diff_added_background = Color::Argb32(0xFF1B3025);
  theme.diff_removed_background = Color::Argb32(0xFF342126);
  theme.diff_added_gutter = Color::Argb32(0xFF1B3025);
  theme.diff_removed_gutter = Color::Argb32(0xFF342126);
  theme.guide_color = Color::Argb32(0x8056617A);
  theme.separator_line_color = Color::Argb32(0xFF4A8F7A);
  theme.split_line_color = Color::Argb32(0x3356617A);
  theme.scrollbar_track_color = Color::Argb32(0x2AFFFFFF);
  theme.scrollbar_thumb_color = Color::Argb32(0x9A7282A0);
  theme.scrollbar_thumb_active_color = Color::Argb32(0xFFAABEDD);
  theme.composition_underline_color = Color::Argb32(0xFF7AA2F7);
  theme.inlay_hint_bg_color = Color::Argb32(0x223A4A66);
  theme.inlay_hint_text_color = Color::Argb32(0xC0AFC2E0);
  theme.inlay_hint_icon_color = Color::Argb32(0xCC9CB0CD);
  theme.gutter_icon_color = theme.inlay_hint_icon_color;
  theme.fold_placeholder_bg_color = Color::Argb32(0x36506C90);
  theme.fold_placeholder_text_color = Color::Argb32(0xFFE2ECFF);
  theme.phantom_text_color = Color::Argb32(0x8AA3B5D1);
  theme.invisible_character_color = Color::Argb32(0x486B7890);
  theme.diagnostic_error_color = Color::Argb32(0xFFF7768E);
  theme.diagnostic_warning_color = Color::Argb32(0xFFE0AF68);
  theme.diagnostic_info_color = Color::Argb32(0xFF7DCFFF);
  theme.diagnostic_hint_color = Color::Argb32(0xFF8FA3BF);
  theme.code_lens_color = Color::Argb32(0xC0AFC2E0);
  theme.code_lens_active_color = Color::Argb32(0xFF9CB3D6);
  theme.link_color = Color::Argb32(0xFF4C9DFF);
  theme.link_active_color = Color::Argb32(0xFF8FB8FF);
  theme.linked_editing_active_color = Color::Argb32(0xCC7AA2F7);
  theme.linked_editing_inactive_color = Color::Argb32(0x667AA2F7);
  theme.bracket_highlight_border_color = Color::Argb32(0xCC9ECE6A);
  theme.bracket_highlight_bg_color = Color::Argb32(0x2A9ECE6A);
  theme.search_match_bg_color = Color::Argb32(0x33E0AF68);
  theme.search_current_bg_color = Color::Argb32(0x55E0AF68);
  theme.search_current_border_color = Color::Argb32(0xFFE0AF68);
  theme.document_highlight_text_bg_color = Color::Argb32(0x1C7AA2F7);
  theme.document_highlight_read_bg_color = Color::Argb32(0x267AA2F7);
  theme.document_highlight_write_bg_color = Color::Argb32(0x337AA2F7);
  theme.completion_bg_color = Color::Argb32(0xF0252830);
  theme.completion_border_color = Color::Argb32(0x40607090);
  theme.completion_selected_bg_color = Color::Argb32(0x3D5580BB);
  theme.completion_label_color = Color::Argb32(0xFFD8DEE9);
  theme.completion_detail_color = Color::Argb32(0xFF7A8494);
  theme.selection_menu_background_color = theme.completion_bg_color;
  theme.selection_menu_text_color = theme.completion_label_color;
  theme.selection_menu_divider_color = Color::Argb32(0x33D8DEE9);

  DefineSyntaxStyle(theme, EditorStyle::Keyword, 0xFF7AA2F7, sweeteditor::FONT_STYLE_BOLD);
  DefineSyntaxStyle(theme, EditorStyle::String, 0xFF9ECE6A);
  DefineSyntaxStyle(theme, EditorStyle::Comment, 0xFF7A8294, sweeteditor::FONT_STYLE_ITALIC);
  DefineSyntaxStyle(theme, EditorStyle::Number, 0xFFFF9E64);
  DefineSyntaxStyle(theme, EditorStyle::Builtin, 0xFF7DCFFF);
  DefineSyntaxStyle(theme, EditorStyle::Type, 0xFFBB9AF7);
  DefineSyntaxStyle(theme, EditorStyle::Class, 0xFFE0AF68, sweeteditor::FONT_STYLE_BOLD);
  DefineSyntaxStyle(theme, EditorStyle::Function, 0xFF73DACA);
  DefineSyntaxStyle(theme, EditorStyle::Variable, 0xFFD7DEE9);
  DefineSyntaxStyle(theme, EditorStyle::Punctuation, 0xFFB0BED3);
  DefineSyntaxStyle(theme, EditorStyle::Annotation, 0xFF2AC3DE);
  DefineSyntaxStyle(theme, EditorStyle::Preprocessor, 0xFFF7768E);
  DefinePresetDecorationStyles(theme);
  return theme;
}

EditorTheme EditorTheme::Light() {
  EditorTheme theme;
  theme.background_color = Color::Argb32(0xFFFAFBFD);
  theme.gutter_background = theme.background_color;
  theme.text_color = Color::Argb32(0xFF1F2937);
  theme.cursor_color = Color::Argb32(0xFF2563EB);
  theme.selection_color = Color::Argb32(0x4D60A5FA);
  theme.selection_text_color = Color::Argb32(0xFFFFFFFF);
  theme.line_number_color = Color::Argb32(0xFF8A94A6);
  theme.current_line_number_color = Color::Argb32(0xFF3A5FA0);
  theme.current_line_color = Color::Argb32(0x1A0D3B66);
  theme.diff_added_background = Color::Argb32(0x1F68B76B);
  theme.diff_removed_background = Color::Argb32(0x1FE5534B);
  theme.diff_added_gutter = Color::Argb32(0x2E68B76B);
  theme.diff_removed_gutter = Color::Argb32(0x2EE5534B);
  theme.guide_color = Color::Argb32(0x4029426B);
  theme.separator_line_color = Color::Argb32(0xFF2F855A);
  theme.split_line_color = Color::Argb32(0x1F29426B);
  theme.scrollbar_track_color = Color::Argb32(0x1F2A3B55);
  theme.scrollbar_thumb_color = Color::Argb32(0x80446C9C);
  theme.scrollbar_thumb_active_color = Color::Argb32(0xEE6A9AD0);
  theme.composition_underline_color = Color::Argb32(0xFF2563EB);
  theme.inlay_hint_bg_color = Color::Argb32(0x143B82F6);
  theme.inlay_hint_text_color = Color::Argb32(0xB0344A73);
  theme.inlay_hint_icon_color = Color::Argb32(0xB04B607E);
  theme.gutter_icon_color = theme.inlay_hint_icon_color;
  theme.fold_placeholder_bg_color = Color::Argb32(0x2E748DB0);
  theme.fold_placeholder_text_color = Color::Argb32(0xFF284A70);
  theme.phantom_text_color = Color::Argb32(0x8A4B607E);
  theme.invisible_character_color = Color::Argb32(0x405D6B82);
  theme.diagnostic_error_color = Color::Argb32(0xFFDC2626);
  theme.diagnostic_warning_color = Color::Argb32(0xFFD97706);
  theme.diagnostic_info_color = Color::Argb32(0xFF0EA5E9);
  theme.diagnostic_hint_color = Color::Argb32(0xFF64748B);
  theme.code_lens_color = Color::Argb32(0xFF4B607E);
  theme.code_lens_active_color = Color::Argb32(0xFF3A5FA0);
  theme.link_color = Color::Argb32(0xFF005FB8);
  theme.link_active_color = Color::Argb32(0xFF1D4ED8);
  theme.linked_editing_active_color = Color::Argb32(0xCC2563EB);
  theme.linked_editing_inactive_color = Color::Argb32(0x662563EB);
  theme.bracket_highlight_border_color = Color::Argb32(0xCC0F766E);
  theme.bracket_highlight_bg_color = Color::Argb32(0x260F766E);
  theme.search_match_bg_color = Color::Argb32(0x33F59E0B);
  theme.search_current_bg_color = Color::Argb32(0x55F59E0B);
  theme.search_current_border_color = Color::Argb32(0xFFD97706);
  theme.document_highlight_text_bg_color = Color::Argb32(0x142563EB);
  theme.document_highlight_read_bg_color = Color::Argb32(0x1C2563EB);
  theme.document_highlight_write_bg_color = Color::Argb32(0x282563EB);
  theme.completion_bg_color = Color::Argb32(0xF0FAFBFD);
  theme.completion_border_color = Color::Argb32(0x30A0A8B8);
  theme.completion_selected_bg_color = Color::Argb32(0x3D3B82F6);
  theme.completion_label_color = Color::Argb32(0xFF1F2937);
  theme.completion_detail_color = Color::Argb32(0xFF8A94A6);
  theme.selection_menu_background_color = theme.completion_bg_color;
  theme.selection_menu_text_color = theme.completion_label_color;
  theme.selection_menu_divider_color = Color::Argb32(0x331F2937);

  DefineSyntaxStyle(theme, EditorStyle::Keyword, 0xFF3559D6, sweeteditor::FONT_STYLE_BOLD);
  DefineSyntaxStyle(theme, EditorStyle::String, 0xFF0F7B6C);
  DefineSyntaxStyle(theme, EditorStyle::Comment, 0xFF7B8798, sweeteditor::FONT_STYLE_ITALIC);
  DefineSyntaxStyle(theme, EditorStyle::Number, 0xFFB45309);
  DefineSyntaxStyle(theme, EditorStyle::Builtin, 0xFF006E7F);
  DefineSyntaxStyle(theme, EditorStyle::Type, 0xFF6D28D9);
  DefineSyntaxStyle(theme, EditorStyle::Class, 0xFF9A3412, sweeteditor::FONT_STYLE_BOLD);
  DefineSyntaxStyle(theme, EditorStyle::Function, 0xFF0E7490);
  DefineSyntaxStyle(theme, EditorStyle::Variable, 0xFF1F2937);
  DefineSyntaxStyle(theme, EditorStyle::Punctuation, 0xFF6E82A0);
  DefineSyntaxStyle(theme, EditorStyle::Annotation, 0xFF0F766E);
  DefineSyntaxStyle(theme, EditorStyle::Preprocessor, 0xFFBE123C);
  DefinePresetDecorationStyles(theme);
  return theme;
}

EditorTheme EditorTheme::FromThemeSpec(const ThemeSpec& spec) {
  const ColorScheme& colors = spec.colors;
  const auto with_alpha = [](Color color, float alpha) {
    color.alpha = alpha;
    return color;
  };
  EditorTheme theme;
  theme.background_color = colors.surface;
  theme.gutter_background = colors.surface_container_highest;
  theme.current_line_color = with_alpha(colors.on_surface, 0.06F);
  theme.separator_line_color = colors.outline;
  theme.text_color = colors.on_surface;
  theme.line_number_color = colors.on_surface_variant;
  theme.cursor_color = colors.primary;
  theme.link_color = colors.primary;
  theme.link_active_color = colors.primary;
  theme.code_lens_color = with_alpha(colors.on_surface_variant, 0.69F);
  theme.code_lens_active_color = colors.primary;
  theme.selection_color = with_alpha(colors.primary, 0.33F);
  theme.search_match_bg_color = with_alpha(colors.secondary, 0.30F);
  theme.search_current_bg_color = with_alpha(colors.secondary, 0.45F);
  theme.bracket_highlight_bg_color = with_alpha(colors.secondary, 0.18F);
  theme.document_highlight_text_bg_color = with_alpha(colors.primary, 0.08F);
  theme.document_highlight_read_bg_color = with_alpha(colors.primary, 0.11F);
  theme.document_highlight_write_bg_color = with_alpha(colors.primary, 0.16F);
  theme.composition_underline_color = colors.primary;
  theme.diagnostic_error_color = colors.error;
  theme.diagnostic_warning_color = with_alpha(colors.error, 0.75F);
  theme.diagnostic_info_color = colors.primary;
  theme.diagnostic_hint_color = colors.on_surface_variant;
  theme.diff_added_background = with_alpha(colors.primary, 0.10F);
  theme.diff_removed_background = with_alpha(colors.error, 0.10F);
  theme.diff_added_gutter = with_alpha(colors.primary, 0.16F);
  theme.diff_removed_gutter = with_alpha(colors.error, 0.16F);
  theme.guide_color = with_alpha(colors.outline, 0.55F);
  theme.inlay_hint_bg_color = with_alpha(colors.primary, 0.08F);
  theme.inlay_hint_text_color = with_alpha(colors.on_surface_variant, 0.85F);
  theme.fold_placeholder_bg_color = with_alpha(colors.outline, 0.25F);
  theme.fold_placeholder_text_color = colors.on_surface;
  theme.gutter_icon_color = colors.secondary;
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Keyword), {colors.primary});
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Type), {colors.primary});
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Class), {colors.secondary});
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Function), {colors.primary});
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Variable), {colors.on_surface});
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::String), {colors.secondary});
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Number), {colors.error});
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Comment), {colors.on_surface_variant});
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Preprocessor), {colors.error});
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Builtin), {colors.primary});
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Punctuation), {colors.on_surface_variant});
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Annotation), {colors.secondary});
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Url), {colors.primary});
  const std::array<Color, 8> rainbow = {
      colors.primary,   colors.secondary, colors.error,
      colors.primary,   colors.secondary, colors.error,
      colors.primary,   colors.secondary,
  };
  for (std::size_t index = 0; index < rainbow.size(); ++index) {
    theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::RainbowFirst) + index, {rainbow[index]});
  }
  theme.completion_bg_color = with_alpha(colors.surface, 0.94F);
  theme.completion_border_color = with_alpha(colors.outline, 0.25F);
  theme.completion_selected_bg_color = with_alpha(colors.primary, 0.24F);
  theme.completion_label_color = colors.on_surface;
  theme.completion_detail_color = colors.on_surface_variant;
  CompletePalette(theme);
  return theme;
}

// Everforest Dark - a warm, low-contrast forest palette tuned for long
// editing sessions. Values follow the canonical Everforest Dark scheme.
EditorTheme EditorTheme::Everforest() {
  EditorTheme theme;
  constexpr auto Rgb = [](int red, int green, int blue, float alpha = 1.0F) {
    return Color::Rgb(red, green, blue, alpha);
  };
  // Surfaces: bg0 / bg0_h / bg1 / bg2 / bg3
  theme.background_color = Rgb(30, 35, 38);
  theme.gutter_background = Rgb(35, 42, 46);
  theme.current_line_color = Rgb(46, 56, 60, 0.6F);
  theme.separator_line_color = Rgb(133, 146, 137);
  // Text
  theme.text_color = Rgb(211, 198, 170);   // fg
  theme.line_number_color = Rgb(133, 146, 137); // grey
  theme.cursor_color = Rgb(211, 198, 170);
  // Links and code lens
  theme.link_color = Rgb(127, 187, 179);        // blue
  theme.link_active_color = Rgb(131, 192, 146); // aqua
  theme.code_lens_color = Rgb(133, 146, 137, 0.9F);
  theme.code_lens_active_color = Rgb(127, 187, 179);
  // Selection and highlights
  theme.selection_color = Rgb(61, 72, 77, 0.55F);        // bg_visual
  theme.search_match_bg_color = Rgb(219, 188, 127, 0.22F);  // yellow
  theme.search_current_bg_color = Rgb(219, 188, 127, 0.38F);
  theme.bracket_highlight_bg_color = Rgb(131, 192, 146, 0.22F);  // aqua
  theme.document_highlight_text_bg_color = Rgb(127, 187, 179, 0.10F);
  theme.document_highlight_read_bg_color = Rgb(127, 187, 179, 0.14F);
  theme.document_highlight_write_bg_color = Rgb(127, 187, 179, 0.20F);
  theme.composition_underline_color = Rgb(131, 192, 146);
  // Diagnostics
  theme.diagnostic_error_color = Rgb(230, 126, 128);  // red
  theme.diagnostic_warning_color = Rgb(219, 188, 127);  // yellow
  theme.diagnostic_info_color = Rgb(127, 187, 179);  // blue
  theme.diagnostic_hint_color = Rgb(133, 146, 137);  // grey
  // Diff
  theme.diff_added_background = Rgb(167, 192, 128, 0.16F);   // green
  theme.diff_removed_background = Rgb(230, 126, 128, 0.16F); // red
  theme.diff_added_gutter = Rgb(167, 192, 128, 0.24F);
  theme.diff_removed_gutter = Rgb(230, 126, 128, 0.24F);
  // Decorations
  theme.guide_color = Rgb(69, 79, 84);  // bg3
  theme.inlay_hint_bg_color = Rgb(69, 79, 84, 0.6F);
  theme.inlay_hint_text_color = Rgb(157, 169, 160);
  theme.fold_placeholder_bg_color = Rgb(69, 79, 84, 0.6F);
  theme.fold_placeholder_text_color = Rgb(157, 169, 160);
  theme.gutter_icon_color = Rgb(131, 192, 146);  // aqua
  // Syntax palette
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Keyword), {Rgb(230, 126, 128)});     // red
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Type), {Rgb(131, 192, 146)});        // aqua
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Class), {Rgb(127, 187, 179)});       // blue
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Function), {Rgb(219, 188, 127)});    // yellow
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Variable), {Rgb(211, 198, 170)});    // fg
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::String), {Rgb(167, 192, 128)});      // green
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Number), {Rgb(230, 152, 117)});      // orange
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Comment), {Rgb(133, 146, 137)});     // grey
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Preprocessor), {Rgb(230, 152, 117)});  // orange
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Builtin), {Rgb(127, 187, 179)});     // blue
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Punctuation), {Rgb(157, 169, 160)}); // grey1
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Annotation), {Rgb(214, 153, 182)});  // purple
  theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::Url), {Rgb(127, 187, 179)});         // blue
  const std::array<Color, 8> rainbow = {
      Rgb(230, 126, 128), Rgb(230, 152, 117), Rgb(219, 188, 127), Rgb(167, 192, 128),
      Rgb(131, 192, 146), Rgb(127, 187, 179), Rgb(214, 153, 182), Rgb(133, 146, 137),
  };
  for (std::size_t index = 0; index < rainbow.size(); ++index) {
    theme.DefineTextStyle(static_cast<std::uint32_t>(EditorStyle::RainbowFirst) + index, {rainbow[index]});
  }
  // Completion panel
  theme.completion_bg_color = Rgb(35, 42, 46, 0.96F);
  theme.completion_border_color = Rgb(157, 169, 160, 0.25F);
  theme.completion_selected_bg_color = Rgb(127, 187, 179, 0.26F);
  theme.completion_label_color = Rgb(211, 198, 170);
  theme.completion_detail_color = Rgb(157, 169, 160);
  CompletePalette(theme);
  return theme;
}

}  // namespace huxerui::codeeditor
