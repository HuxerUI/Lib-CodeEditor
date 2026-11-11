#include <catch2/catch_amalgamated.hpp>

#include <huxerui/codeeditor.h>

namespace ce = huxerui::codeeditor;

TEST_CASE("CodeEditor rejects invalid declarative settings before mounting") {
  ce::EditorOptions options;
  SECTION("text size") { options.settings.editor_text_size = 0; }
  SECTION("tab stop") { options.settings.tab_size = 0; }
  SECTION("line spacing") { options.settings.line_spacing_mult = -1; }
  SECTION("decoration interval") { options.settings.decoration_scroll_refresh_min_interval_ms = -1; }
  REQUIRE_THROWS_AS(ce::CodeEditor(ce::EditorController{}, options), std::invalid_argument);
}

TEST_CASE("CodeEditor documents preserve UTF-8 content and line boundaries") {
  const ce::Document document("A\xf0\x9f\x99\x82\nsecond\n");
  REQUIRE(document.GetLineCount() == 3);
  REQUIRE(document.GetLineText(0) == "A\xf0\x9f\x99\x82");
  REQUIRE(document.GetLineText(1) == "second");
  REQUIRE(document.GetLineText(2).empty());
  REQUIRE(document.GetText() == "A\xf0\x9f\x99\x82\nsecond\n");
}
