#include <catch2/catch_amalgamated.hpp>

#include <huxerui/codeeditor.h>
#include <sweeteditor/decoration.h>

namespace ce = huxerui::codeeditor;

TEST_CASE("CodeEditor presets follow the platform palette and syntax styles") {
  const auto light = ce::EditorTheme::Light();
  const auto dark = ce::EditorTheme::Dark();
  using huxerui::Color;
  REQUIRE_FALSE(light == dark);
  REQUIRE_FALSE(dark == ce::EditorTheme::Everforest());
  REQUIRE(dark.background_color == Color::Argb32(0xFF1B1E24));
  REQUIRE(light.background_color == Color::Argb32(0xFFFAFBFD));
  REQUIRE(dark.gutter_background == dark.background_color);
  REQUIRE(light.gutter_background == light.background_color);
  REQUIRE(dark.diff_added_background == Color::Argb32(0xFF1B3025));
  REQUIRE(dark.diff_removed_background == Color::Argb32(0xFF342126));
  REQUIRE(light.diff_added_background == Color::Argb32(0x1F68B76B));
  REQUIRE(light.diff_removed_background == Color::Argb32(0x1FE5534B));
  REQUIRE(light.code_lens_color == Color::Argb32(0xFF4B607E));
  REQUIRE(dark.link_active_color == Color::Argb32(0xFF8FB8FF));
  REQUIRE(static_cast<int>(ce::EditorStyle::Keyword) == 1);
  REQUIRE(static_cast<int>(ce::EditorStyle::String) == 2);
  REQUIRE(static_cast<int>(ce::EditorStyle::Comment) == 3);
  REQUIRE(static_cast<int>(ce::EditorStyle::Preprocessor) == 12);
  for (unsigned id = 1; id <= 12; ++id) {
    REQUIRE(light.text_styles.contains(id));
    REQUIRE(dark.text_styles.contains(id));
  }
  const auto keyword = static_cast<std::uint32_t>(ce::EditorStyle::Keyword);
  const auto comment = static_cast<std::uint32_t>(ce::EditorStyle::Comment);
  REQUIRE(dark.text_styles.at(keyword).foreground == Color::Argb32(0xFF7AA2F7));
  REQUIRE(light.text_styles.at(keyword).foreground == Color::Argb32(0xFF3559D6));
  REQUIRE(dark.text_styles.at(keyword).font_style == sweeteditor::FONT_STYLE_BOLD);
  REQUIRE(light.text_styles.at(comment).font_style == sweeteditor::FONT_STYLE_ITALIC);
}

TEST_CASE("CodeEditor derives colors from the ambient HuxerUI theme") {
  const huxerui::ThemeSpec spec;
  const auto derived = ce::EditorTheme::FromThemeSpec(spec);
  REQUIRE(derived.background_color == spec.colors.surface);
  REQUIRE(derived.text_color == spec.colors.on_surface);
  REQUIRE_FALSE(derived.text_color == derived.background_color);
}
