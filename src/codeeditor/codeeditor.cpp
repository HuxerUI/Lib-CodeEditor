#include <huxerui/codeeditor.h>

#if defined(__ANDROID__)
#include <android/log.h>
#define CODEEDITOR_TRACE(...) __android_log_print(ANDROID_LOG_INFO, "CodeEditor", __VA_ARGS__)
#else
#include <cstdio>
#define CODEEDITOR_TRACE(...) std::fprintf(stderr, __VA_ARGS__)
#endif

#include <huxerui/huxerui.h>

#include <sweeteditor/decoration.h>
#include <sweeteditor/document.h>
#include <sweeteditor/editor_core.h>
#include <sweeteditor/visual.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace huxerui::codeeditor {

CodeEditorTheme CodeEditorTheme::Default() {
  CodeEditorTheme theme;
  theme.background = Color::Rgb(255, 255, 255);
  theme.gutter_background = Color::Rgb(242, 243, 245);
  theme.current_line_background = Color::Rgb(0, 0, 0, 0.06F);
  theme.separator_color = Color::Rgb(176, 183, 195);
  theme.text_foreground = Color::Rgb(31, 31, 31);
  theme.line_number_color = Color::Rgb(154, 160, 166);
  theme.caret_color = Color::Rgb(31, 31, 31);
  theme.link_color = Color::Rgb(76, 157, 255);
  theme.active_link_color = Color::Rgb(76, 157, 255);
  theme.codelens_color = Color::Rgb(52, 74, 115, 0.69F);
  theme.active_codelens_color = Color::Rgb(58, 95, 160);
  theme.selection_background = Color::Rgb(74, 144, 226, 0.33F);
  theme.search_match_background = Color::Rgb(245, 158, 11, 0.2F);
  theme.search_current_background = Color::Rgb(245, 158, 11, 0.33F);
  theme.bracket_match_background = Color::Rgb(15, 118, 110, 0.15F);
  theme.document_highlight_text = Color::Rgb(37, 99, 235, 0.08F);
  theme.document_highlight_read = Color::Rgb(37, 99, 235, 0.11F);
  theme.document_highlight_write = Color::Rgb(37, 99, 235, 0.16F);
  theme.ime_composition_underline = Color::Rgb(37, 99, 235);
  theme.diagnostic_error_underline = Color::Rgb(220, 38, 38);
  theme.diagnostic_warning_underline = Color::Rgb(217, 119, 6);
  theme.diagnostic_info_underline = Color::Rgb(14, 165, 233);
  theme.diagnostic_hint_underline = Color::Rgb(100, 116, 139);
  theme.diff_added_background = Color::Rgb(166, 226, 46, 0.12F);
  theme.diff_removed_background = Color::Rgb(249, 38, 114, 0.12F);
  theme.diff_added_gutter = Color::Rgb(166, 226, 46, 0.18F);
  theme.diff_removed_gutter = Color::Rgb(249, 38, 114, 0.18F);
  theme.indent_guide_color = Color::Rgb(200, 200, 200);
  theme.inlay_hint_background = Color::Rgb(59, 130, 246, 0.08F);
  theme.inlay_hint_text = Color::Rgb(52, 74, 115, 0.69F);
  theme.fold_placeholder_background = Color::Rgb(116, 141, 176, 0.18F);
  theme.fold_placeholder_text = Color::Rgb(40, 74, 112);
  theme.gutter_icon_color = Color::Rgb(38, 127, 153);
  theme.syntax_keyword = Color::Rgb(0, 0, 255);
  theme.syntax_type = Color::Rgb(38, 127, 153);
  theme.syntax_class = Color::Rgb(38, 127, 153);
  theme.syntax_function = Color::Rgb(121, 94, 38);
  theme.syntax_variable = Color::Rgb(0, 16, 128);
  theme.syntax_string = Color::Rgb(163, 21, 21);
  theme.syntax_number = Color::Rgb(9, 134, 88);
  theme.syntax_comment = Color::Rgb(0, 128, 0);
  theme.syntax_preprocessor = Color::Rgb(155, 79, 150);
  theme.syntax_builtin = Color::Rgb(0, 0, 255);
  theme.syntax_punctuation = Color::Rgb(119, 119, 119);
  theme.syntax_annotation = Color::Rgb(179, 92, 0);
  theme.syntax_url = Color::Rgb(11, 92, 173);
  theme.syntax_rainbow = {
      Color::Rgb(220, 50, 47),  Color::Rgb(38, 139, 210), Color::Rgb(133, 153, 0),
      Color::Rgb(181, 137, 0),  Color::Rgb(203, 75, 22),  Color::Rgb(108, 113, 196),
      Color::Rgb(42, 161, 152), Color::Rgb(211, 54, 130),
  };
  theme.completion_background = Color::Rgb(250, 251, 253, 0.94F);
  theme.completion_border = Color::Rgb(160, 168, 184, 0.19F);
  theme.completion_selected_background = Color::Rgb(59, 130, 246, 0.24F);
  theme.completion_label = Color::Rgb(31, 41, 55);
  theme.completion_detail = Color::Rgb(138, 148, 166);
  return theme;
}

CodeEditorTheme CodeEditorTheme::FromThemeSpec(const ThemeSpec& spec) {
  const ColorScheme& colors = spec.colors;
  const auto with_alpha = [](Color color, float alpha) {
    color.alpha = alpha;
    return color;
  };
  CodeEditorTheme theme;
  theme.background = colors.surface;
  theme.gutter_background = colors.surface_container_highest;
  theme.current_line_background = with_alpha(colors.on_surface, 0.06F);
  theme.separator_color = colors.outline;
  theme.text_foreground = colors.on_surface;
  theme.line_number_color = colors.on_surface_variant;
  theme.caret_color = colors.primary;
  theme.link_color = colors.primary;
  theme.active_link_color = colors.primary;
  theme.codelens_color = with_alpha(colors.on_surface_variant, 0.69F);
  theme.active_codelens_color = colors.primary;
  theme.selection_background = with_alpha(colors.primary, 0.33F);
  theme.search_match_background = with_alpha(colors.secondary, 0.30F);
  theme.search_current_background = with_alpha(colors.secondary, 0.45F);
  theme.bracket_match_background = with_alpha(colors.secondary, 0.18F);
  theme.document_highlight_text = with_alpha(colors.primary, 0.08F);
  theme.document_highlight_read = with_alpha(colors.primary, 0.11F);
  theme.document_highlight_write = with_alpha(colors.primary, 0.16F);
  theme.ime_composition_underline = colors.primary;
  theme.diagnostic_error_underline = colors.error;
  theme.diagnostic_warning_underline = with_alpha(colors.error, 0.75F);
  theme.diagnostic_info_underline = colors.primary;
  theme.diagnostic_hint_underline = colors.on_surface_variant;
  theme.diff_added_background = with_alpha(colors.primary, 0.10F);
  theme.diff_removed_background = with_alpha(colors.error, 0.10F);
  theme.diff_added_gutter = with_alpha(colors.primary, 0.16F);
  theme.diff_removed_gutter = with_alpha(colors.error, 0.16F);
  theme.indent_guide_color = with_alpha(colors.outline, 0.55F);
  theme.inlay_hint_background = with_alpha(colors.primary, 0.08F);
  theme.inlay_hint_text = with_alpha(colors.on_surface_variant, 0.85F);
  theme.fold_placeholder_background = with_alpha(colors.outline, 0.25F);
  theme.fold_placeholder_text = colors.on_surface;
  theme.gutter_icon_color = colors.secondary;
  theme.syntax_keyword = colors.primary;
  theme.syntax_type = colors.primary;
  theme.syntax_class = colors.secondary;
  theme.syntax_function = colors.primary;
  theme.syntax_variable = colors.on_surface;
  theme.syntax_string = colors.secondary;
  theme.syntax_number = colors.error;
  theme.syntax_comment = colors.on_surface_variant;
  theme.syntax_preprocessor = colors.error;
  theme.syntax_builtin = colors.primary;
  theme.syntax_punctuation = colors.on_surface_variant;
  theme.syntax_annotation = colors.secondary;
  theme.syntax_url = colors.primary;
  theme.syntax_rainbow = {
      colors.primary,   colors.secondary, colors.error,
      colors.primary,   colors.secondary, colors.error,
      colors.primary,   colors.secondary,
  };
  theme.completion_background = with_alpha(colors.surface, 0.94F);
  theme.completion_border = with_alpha(colors.outline, 0.25F);
  theme.completion_selected_background = with_alpha(colors.primary, 0.24F);
  theme.completion_label = colors.on_surface;
  theme.completion_detail = colors.on_surface_variant;
  return theme;
}

namespace {

namespace se = sweeteditor;
using namespace huxerui;

// ---- UTF-16 code units <-> UTF-8 -------------------------------------------
std::string Utf16ToUtf8(std::u16string_view input) {
  std::string out;
  out.reserve(input.size());
  for (std::size_t i = 0; i < input.size(); ++i) {
    char32_t cp = input[i];
    if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < input.size() && input[i + 1] >= 0xDC00 &&
        input[i + 1] <= 0xDFFF) {
      cp = 0x10000 + ((cp - 0xD800) << 10) + (input[i + 1] - 0xDC00);
      ++i;
    }
    if (cp < 0x80) {
      out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
      out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
      out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
  }
  return out;
}

// SweetEditor's UTF-16 string is std::basic_string<wchar_t> on Windows (where
// wchar_t is a 16-bit UTF-16 code unit) and std::u16string elsewhere; view it
// as UTF-16 code units for the UTF-8 conversion.
std::string Utf16ToUtf8(const se::U16String& input) {
  static_assert(sizeof(se::U16Char) == 2, "SweetEditor UTF-16 code units must be 16-bit");
  return Utf16ToUtf8(std::u16string_view(reinterpret_cast<const char16_t*>(input.data()), input.size()));
}

// ---- Decoration result -> SweetEditor core conversions --------------------

std::vector<std::pair<size_t, std::vector<se::StyleSpan>>> ToCoreSpanEntries(
    const CodeEditorLineEntries<CodeEditorStyleSpan>& spans
) {
  std::vector<std::pair<size_t, std::vector<se::StyleSpan>>> entries;
  for (const auto& [line, items] : spans) {
    std::vector<se::StyleSpan> converted;
    for (const CodeEditorStyleSpan& span : items) {
      if (span.length == 0) {
        continue;
      }
      converted.push_back({span.column, span.length, static_cast<uint32_t>(span.style)});
    }
    if (!converted.empty()) {
      entries.emplace_back(static_cast<size_t>(line), std::move(converted));
    }
  }
  return entries;
}

std::vector<std::pair<size_t, std::vector<se::InlayHint>>> ToCoreInlayEntries(
    const CodeEditorLineEntries<CodeEditorInlayHint>& hints
) {
  std::vector<std::pair<size_t, std::vector<se::InlayHint>>> entries;
  for (const auto& [line, items] : hints) {
    std::vector<se::InlayHint> converted;
    for (const CodeEditorInlayHint& hint : items) {
      converted.push_back({se::InlayType::TEXT, hint.column, 0, se::U8String(hint.text)});
    }
    if (!converted.empty()) {
      entries.emplace_back(static_cast<size_t>(line), std::move(converted));
    }
  }
  return entries;
}

std::vector<std::pair<size_t, std::vector<se::Diagnostic>>> ToCoreDiagnosticEntries(
    const CodeEditorLineEntries<CodeEditorDiagnostic>& diagnostics
) {
  std::vector<std::pair<size_t, std::vector<se::Diagnostic>>> entries;
  for (const auto& [line, items] : diagnostics) {
    std::vector<se::Diagnostic> converted;
    for (const CodeEditorDiagnostic& diagnostic : items) {
      converted.push_back(
          {diagnostic.column, diagnostic.length, static_cast<se::DiagnosticSeverity>(diagnostic.severity)}
      );
    }
    if (!converted.empty()) {
      entries.emplace_back(static_cast<size_t>(line), std::move(converted));
    }
  }
  return entries;
}

std::vector<std::pair<size_t, std::vector<se::CodeLensItem>>> ToCoreCodeLensEntries(
    const CodeEditorLineEntries<CodeEditorCodeLens>& lenses
) {
  std::vector<std::pair<size_t, std::vector<se::CodeLensItem>>> entries;
  for (const auto& [line, items] : lenses) {
    std::vector<se::CodeLensItem> converted;
    for (const CodeEditorCodeLens& lens : items) {
      converted.push_back({static_cast<int32_t>(lens.column), lens.command_id, se::U8String(lens.title)});
    }
    if (!converted.empty()) {
      entries.emplace_back(static_cast<size_t>(line), std::move(converted));
    }
  }
  return entries;
}

std::vector<std::pair<size_t, std::vector<se::LinkSpan>>> ToCoreLinkEntries(
    const CodeEditorLineEntries<CodeEditorLink>& links
) {
  std::vector<std::pair<size_t, std::vector<se::LinkSpan>>> entries;
  for (const auto& [line, items] : links) {
    std::vector<se::LinkSpan> converted;
    for (const CodeEditorLink& link : items) {
      converted.push_back({link.column, link.length, se::U8String(link.url)});
    }
    if (!converted.empty()) {
      entries.emplace_back(static_cast<size_t>(line), std::move(converted));
    }
  }
  return entries;
}

std::vector<std::pair<size_t, std::vector<se::GutterIcon>>> ToCoreGutterIconEntries(
    const CodeEditorLineEntries<CodeEditorGutterIcon>& icons
) {
  std::vector<std::pair<size_t, std::vector<se::GutterIcon>>> entries;
  for (const auto& [line, items] : icons) {
    std::vector<se::GutterIcon> converted;
    for (const CodeEditorGutterIcon& icon : items) {
      converted.push_back({icon.icon_id});
    }
    if (!converted.empty()) {
      entries.emplace_back(static_cast<size_t>(line), std::move(converted));
    }
  }
  return entries;
}

std::vector<std::pair<size_t, std::vector<se::DocumentHighlight>>> ToCoreDocumentHighlightEntries(
    const CodeEditorLineEntries<CodeEditorStyleSpan>& highlights
) {
  std::vector<std::pair<size_t, std::vector<se::DocumentHighlight>>> entries;
  for (const auto& [line, items] : highlights) {
    std::vector<se::DocumentHighlight> converted;
    for (const CodeEditorStyleSpan& span : items) {
      if (span.length == 0) {
        continue;
      }
      converted.push_back({span.column, span.length, se::DocumentHighlightKind::TEXT});
    }
    if (!converted.empty()) {
      entries.emplace_back(static_cast<size_t>(line), std::move(converted));
    }
  }
  return entries;
}

// SweetEditor style objects carry 0xAARRGGBB integers.
int32_t ToArgb(const Color& color) {
  const auto channel = [](float value) {
    const int scaled = static_cast<int>(value * 255.0F + 0.5F);
    return scaled < 0 ? 0 : (scaled > 255 ? 255 : scaled);
  };
  return (channel(color.alpha) << 24) | (channel(color.red) << 16) | (channel(color.green) << 8) | channel(color.blue);
}

// Registers the syntax token palette so CodeEditorStyle ids resolve to the
// theme's colors; safe to call again on theme changes (registerTextStyle
// replaces the previous style).
void RegisterSyntaxStyles(se::EditorCore& core, const CodeEditorTheme& theme) {
  const auto style = [](const Color& color) {
    return se::TextStyle{ToArgb(color), 0, se::FONT_STYLE_NORMAL};
  };
  core.registerTextStyle(static_cast<uint32_t>(CodeEditorStyle::Keyword), style(theme.syntax_keyword));
  core.registerTextStyle(static_cast<uint32_t>(CodeEditorStyle::Type), style(theme.syntax_type));
  core.registerTextStyle(static_cast<uint32_t>(CodeEditorStyle::Class), style(theme.syntax_class));
  core.registerTextStyle(static_cast<uint32_t>(CodeEditorStyle::Function), style(theme.syntax_function));
  core.registerTextStyle(static_cast<uint32_t>(CodeEditorStyle::Variable), style(theme.syntax_variable));
  core.registerTextStyle(static_cast<uint32_t>(CodeEditorStyle::String), style(theme.syntax_string));
  core.registerTextStyle(static_cast<uint32_t>(CodeEditorStyle::Number), style(theme.syntax_number));
  core.registerTextStyle(static_cast<uint32_t>(CodeEditorStyle::Comment), style(theme.syntax_comment));
  core.registerTextStyle(static_cast<uint32_t>(CodeEditorStyle::Preprocessor), style(theme.syntax_preprocessor));
  core.registerTextStyle(static_cast<uint32_t>(CodeEditorStyle::Builtin), style(theme.syntax_builtin));
  core.registerTextStyle(static_cast<uint32_t>(CodeEditorStyle::Punctuation), style(theme.syntax_punctuation));
  core.registerTextStyle(static_cast<uint32_t>(CodeEditorStyle::Annotation), style(theme.syntax_annotation));
  core.registerTextStyle(static_cast<uint32_t>(CodeEditorStyle::Url), style(theme.syntax_url));
  for (std::size_t index = 0; index < theme.syntax_rainbow.size(); ++index) {
    core.registerTextStyle(
        static_cast<uint32_t>(CodeEditorStyle::RainbowFirst) + index, style(theme.syntax_rainbow[index])
    );
  }
}

void MergeDecorations(CodeEditorDecorationResult& target, CodeEditorDecorationResult part) {
  const auto append_entries = []<typename T>(CodeEditorLineEntries<T>& dst, CodeEditorLineEntries<T>&& src) {
    dst.insert(dst.end(), std::make_move_iterator(src.begin()), std::make_move_iterator(src.end()));
  };
  append_entries(target.syntax_spans, std::move(part.syntax_spans));
  append_entries(target.overlay_spans, std::move(part.overlay_spans));
  append_entries(target.document_highlights, std::move(part.document_highlights));
  append_entries(target.inlay_hints, std::move(part.inlay_hints));
  append_entries(target.diagnostics, std::move(part.diagnostics));
  append_entries(target.code_lens, std::move(part.code_lens));
  append_entries(target.links, std::move(part.links));
  append_entries(target.gutter_icons, std::move(part.gutter_icons));
  append_entries(target.phantom_texts, std::move(part.phantom_texts));
  target.indent_guides.insert(
      target.indent_guides.end(), std::make_move_iterator(part.indent_guides.begin()),
      std::make_move_iterator(part.indent_guides.end())
  );
  target.fold_regions.insert(
      target.fold_regions.end(), std::make_move_iterator(part.fold_regions.begin()),
      std::make_move_iterator(part.fold_regions.end())
  );
  if (part.matched_bracket) {
    target.matched_bracket = part.matched_bracket;
  }
}

void ApplyDecorations(se::EditorCore& core, const CodeEditorDecorationResult& decorations) {
  core.clearHighlights(se::SpanLayer::SYNTAX);
  core.setBatchLineSpans(se::SpanLayer::SYNTAX, ToCoreSpanEntries(decorations.syntax_spans));
  core.clearHighlights(se::SpanLayer::OVERLAY);
  core.setBatchLineSpans(se::SpanLayer::OVERLAY, ToCoreSpanEntries(decorations.overlay_spans));
  if (decorations.document_highlights.empty()) {
    core.clearDocumentHighlights();
  } else {
    core.setBatchLineDocumentHighlights(ToCoreDocumentHighlightEntries(decorations.document_highlights));
  }
  core.setBatchLineInlayHints(ToCoreInlayEntries(decorations.inlay_hints));
  core.setBatchLineDiagnostics(ToCoreDiagnosticEntries(decorations.diagnostics));
  core.setBatchLineCodeLens(ToCoreCodeLensEntries(decorations.code_lens));
  core.setBatchLineLinks(ToCoreLinkEntries(decorations.links));
  core.setBatchLineGutterIcons(ToCoreGutterIconEntries(decorations.gutter_icons));
  std::vector<se::IndentGuide> guides;
  guides.reserve(decorations.indent_guides.size());
  for (const CodeEditorIndentGuide& guide : decorations.indent_guides) {
    if (guide.end_line < guide.start_line) {
      continue;
    }
    guides.push_back(
        {se::TextPosition{guide.start_line, guide.column}, se::TextPosition{guide.end_line, guide.column}}
    );
  }
  core.setIndentGuides(std::move(guides));
  // Fold regions are published once per document (providers set them on the
  // settled pass after a load); later refreshes legitimately omit them, and
  // overwriting with an empty set would collapse the fold UI on caret moves.
  if (!decorations.fold_regions.empty()) {
    std::vector<se::FoldRegion> folds;
    folds.reserve(decorations.fold_regions.size());
    for (const CodeEditorFoldRegion& fold : decorations.fold_regions) {
      if (fold.end_line <= fold.start_line) {
        continue;
      }
      folds.push_back({fold.start_line, fold.end_line, false});
    }
    core.setFoldRegions(std::move(folds));
  }
  if (decorations.matched_bracket) {
    core.setMatchedBrackets(
        se::TextPosition{decorations.matched_bracket->line, decorations.matched_bracket->column},
        se::TextPosition{decorations.matched_bracket->partner_line, decorations.matched_bracket->partner_column}
    );
  } else {
    core.clearMatchedBrackets();
  }
}

// ---- ARGB color ------------------------------------------------------------
Color Argb(int32_t argb) {
  const int a = (argb >> 24) & 0xFF;
  const int r = (argb >> 16) & 0xFF;
  const int g = (argb >> 8) & 0xFF;
  const int b = argb & 0xFF;
  return Color::Rgb(r, g, b, static_cast<float>(a) / 255.0F);
}

// ---- Font / style mapping (shared by the measurer and the renderer) --------
TextStyle MakeTextStyle(int32_t font_style, int32_t argb_color, float font_size) {
  Font font = Font::Monospace(font_size);
  if ((font_style & se::FONT_STYLE_BOLD) != 0) {
    font = font.WithWeight(FontWeight::Bold);
  }
  if ((font_style & se::FONT_STYLE_ITALIC) != 0) {
    font = font.WithSlant(FontSlant::Italic);
  }
  TextDecoration decoration = TextDecoration::None;
  if ((font_style & se::FONT_STYLE_STRIKETHROUGH) != 0) {
    decoration = TextDecoration::StrikeThrough;
  }
  return TextStyle{font, Argb(argb_color), decoration};
}

// ---- SweetEditor TextMeasurer bridge ---------------------------------------
class HuxeruiTextMeasurer final : public se::TextMeasurer {
public:
  explicit HuxeruiTextMeasurer(huxerui::TextMeasurer& measurer, float font_size)
      : measurer_(&measurer), font_size_(font_size) {}

  float measureWidth(const se::U16String& text, int32_t font_style) override {
    if (!measurer_) {
      return 0.0F;
    }
    return measurer_->MeasureRun(Utf16ToUtf8(text), MakeTextStyle(font_style, 0, font_size_)).advance;
  }

  float measureInlayHintWidth(const se::U16String& text) override {
    if (!measurer_) {
      return 0.0F;
    }
    // SweetEditor's reference renderer draws inlay hints in a sans-serif face at 90% of the text size.
    const huxerui::Font hint_font = huxerui::Font::System(font_size_ * 0.9F);
    return measurer_
        ->MeasureRun(
            Utf16ToUtf8(text), huxerui::TextStyle{hint_font, huxerui::Color::Black(), huxerui::TextDecoration::None}
        )
        .advance;
  }

  float measureIconWidth(int32_t) override {
    return 0.0F;
  }

  se::FontMetrics getFontMetrics() override {
    if (!measurer_) {
      return {0.0F, 0.0F};
    }
    const huxerui::FontMetrics metrics = measurer_->Metrics(huxerui::Font::Monospace(font_size_));
    return {-metrics.ascent, metrics.descent};
  }

private:
  huxerui::TextMeasurer* measurer_;
  float font_size_;
};

// ---- Input mapping ---------------------------------------------------------
se::KeyModifier ToModifiers(const KeyModifiers& modifiers) {
  int value = 0;
  if (modifiers.shift) {
    value |= static_cast<int>(se::KeyModifier::SHIFT);
  }
  if (modifiers.control) {
    value |= static_cast<int>(se::KeyModifier::CTRL);
  }
  if (modifiers.alt) {
    value |= static_cast<int>(se::KeyModifier::ALT);
  }
  if (modifiers.meta) {
    value |= static_cast<int>(se::KeyModifier::META);
  }
  return static_cast<se::KeyModifier>(value);
}

se::KeyCode ToKeyCode(Key key) {
  switch (key) {
  case Key::Backspace:
    return se::KeyCode::BACKSPACE;
  case Key::Tab:
    return se::KeyCode::TAB;
  case Key::Enter:
    return se::KeyCode::ENTER;
  case Key::Escape:
    return se::KeyCode::ESCAPE;
  case Key::Space:
    return se::KeyCode::SPACE;
  case Key::Delete:
    return se::KeyCode::DELETE_KEY;
  case Key::ArrowLeft:
    return se::KeyCode::LEFT;
  case Key::ArrowRight:
    return se::KeyCode::RIGHT;
  case Key::ArrowUp:
    return se::KeyCode::UP;
  case Key::ArrowDown:
    return se::KeyCode::DOWN;
  case Key::Home:
    return se::KeyCode::HOME;
  case Key::End:
    return se::KeyCode::END;
  case Key::PageUp:
    return se::KeyCode::PAGE_UP;
  case Key::PageDown:
    return se::KeyCode::PAGE_DOWN;
  case Key::A:
    return se::KeyCode::A;
  case Key::C:
    return se::KeyCode::C;
  case Key::V:
    return se::KeyCode::V;
  case Key::X:
    return se::KeyCode::X;
  case Key::Y:
    return se::KeyCode::Y;
  case Key::Z:
    return se::KeyCode::Z;
  default:
    return se::KeyCode::NONE;
  }
}

se::KeyEvent ToKeyEvent(const KeyEvent& event) {
  se::KeyEvent out;
  out.modifiers = ToModifiers(event.modifiers);
  // The web adapter only fills `text` for unmodified single-character input,
  // so a non-empty `text` always means a printable character.
  if (!event.text.empty()) {
    out.key_code = se::KeyCode::NONE;
    out.text = event.text;
  } else {
    out.key_code = ToKeyCode(event.key);
  }
  return out;
}

se::GestureEvent ToGestureEvent(const PointerEvent& event) {
  se::GestureEvent out;
  out.points.push_back(se::PointF{event.position.x, event.position.y});
  out.modifiers = se::KeyModifier::NONE;
  const bool mouse = event.device_kind == PointerDeviceKind::Mouse || event.device_kind == PointerDeviceKind::Pen;
  switch (event.type) {
  case PointerEventType::Down:
    out.type = mouse ? se::EventType::MOUSE_DOWN : se::EventType::TOUCH_DOWN;
    break;
  case PointerEventType::Move:
    out.type = mouse ? se::EventType::MOUSE_MOVE : se::EventType::TOUCH_MOVE;
    break;
  case PointerEventType::Up:
    out.type = mouse ? se::EventType::MOUSE_UP : se::EventType::TOUCH_UP;
    break;
  case PointerEventType::Cancel:
    out.type = se::EventType::TOUCH_CANCEL;
    break;
  }
  return out;
}

se::GestureEvent ToWheelGestureEvent(const ScrollInputEvent& event) {
  se::GestureEvent out;
  out.type = se::EventType::MOUSE_WHEEL;
  out.wheel_delta_x = event.delta_x;
  out.wheel_delta_y = event.delta_y;
  return out;
}

// ---- Theme defaults --------------------------------------------------------

// Auto-closing bracket pairs enabled by default (reference platform ships
// with these); hosts may override via CodeEditorOptions.auto_closing_pairs.
const std::vector<std::pair<char32_t, char32_t>> kDefaultAutoClosingPairs = {
    {U'(', U')'},
    {U'{', U'}'},
    {U'[', U']'},
};

// Completion panel, matching the SweetEditor reference light theme and
// dimensions (Android CompletionPopupController / EditorTheme.java).
constexpr float kCompletionPanelWidth = 300.0F;
constexpr float kCompletionPanelMinWidth = 120.0F;
constexpr float kCompletionRowHeight = 32.0F;
constexpr float kCompletionPanelPaddingH = 4.0F;
constexpr float kCompletionPanelPaddingV = 6.0F;
constexpr float kCompletionRowPaddingH = 8.0F;
constexpr float kCompletionBadgeSize = 18.0F;
constexpr float kCompletionBadgeGap = 8.0F;
constexpr float kCompletionDetailGap = 8.0F;
constexpr float kCompletionGap = 4.0F;
constexpr float kCompletionFramePadding = 8.0F;
constexpr size_t kCompletionMaxVisible = 4;
constexpr float kCompletionLabelSize = 13.0F;
constexpr float kCompletionDetailSize = 11.0F;
constexpr float kCompletionBadgeSizePx = 10.0F;

int32_t CompletionKindColor(CompletionItemKind kind) {
  switch (kind) {
  case CompletionItemKind::Keyword:
    return 0xFFC678DD;
  case CompletionItemKind::Function:
    return 0xFF61AFEF;
  case CompletionItemKind::Variable:
    return 0xFFE5C07B;
  case CompletionItemKind::Class:
    return 0xFFE06C75;
  case CompletionItemKind::Interface:
    return 0xFF56B6C2;
  case CompletionItemKind::Module:
    return 0xFFD19A66;
  case CompletionItemKind::Property:
    return 0xFF98C379;
  case CompletionItemKind::Snippet:
    return 0xFFBE5046;
  case CompletionItemKind::Text:
  default:
    return 0xFF7A8494;
  }
}

const char* CompletionKindLetter(CompletionItemKind kind) {
  switch (kind) {
  case CompletionItemKind::Keyword:
    return "K";
  case CompletionItemKind::Function:
    return "F";
  case CompletionItemKind::Variable:
    return "V";
  case CompletionItemKind::Class:
    return "C";
  case CompletionItemKind::Interface:
    return "I";
  case CompletionItemKind::Module:
    return "M";
  case CompletionItemKind::Property:
    return "P";
  case CompletionItemKind::Snippet:
    return "S";
  case CompletionItemKind::Text:
  default:
    return "T";
  }
}

// Baseline that centers text (ascent above, descent below) on a row center.
// DrawTextRun places its origin on the alphabetic baseline.
float CenterBaseline(float center_y, const FontMetrics& metrics) {
  return center_y + (metrics.ascent - metrics.descent) * 0.5F;
}

// Converts "(){}[]" style pair strings into the core's bracket pair list.
std::vector<se::BracketPair> MakeBracketPairs(const std::vector<std::pair<char32_t, char32_t>>& pairs) {
  std::vector<se::BracketPair> result;
  result.reserve(pairs.size());
  for (const auto& [open, close] : pairs) {
    result.push_back({open, close});
  }
  return result;
}

se::EditorRenderColors MakeRenderColors(const CodeEditorTheme& theme) {
  se::EditorRenderColors colors;
  colors.text_foreground = ToArgb(theme.text_foreground);
  colors.link_foreground = ToArgb(theme.link_color);
  colors.active_link_foreground = ToArgb(theme.active_link_color);
  colors.codelens_foreground = ToArgb(theme.codelens_color);
  colors.active_codelens_foreground = ToArgb(theme.active_codelens_color);
  colors.diff_added_line_background = ToArgb(theme.diff_added_background);
  colors.diff_removed_line_background = ToArgb(theme.diff_removed_background);
  colors.diff_added_gutter_background = ToArgb(theme.diff_added_gutter);
  colors.diff_removed_gutter_background = ToArgb(theme.diff_removed_gutter);
  return colors;
}

se::EditorRangeEffectStyles MakeRangeEffectStyles(const CodeEditorTheme& theme) {
  se::EditorRangeEffectStyles styles;
  styles.selection.background_color = ToArgb(theme.selection_background);
  styles.search_match.background_color = ToArgb(theme.search_match_background);
  styles.search_current.background_color = ToArgb(theme.search_current_background);
  styles.bracket_match.background_color = ToArgb(theme.bracket_match_background);
  styles.ime_composition.underline_color = ToArgb(theme.ime_composition_underline);
  styles.ime_composition.underline_style = se::RangeEffectUnderlineStyle::SOLID;

  styles.document_highlight_text.background_color = ToArgb(theme.document_highlight_text);
  styles.document_highlight_read.background_color = ToArgb(theme.document_highlight_read);
  styles.document_highlight_write.background_color = ToArgb(theme.document_highlight_write);
  styles.diagnostic_error.underline_color = ToArgb(theme.diagnostic_error_underline);
  styles.diagnostic_error.underline_style = se::RangeEffectUnderlineStyle::WAVY;
  styles.diagnostic_warning.underline_color = ToArgb(theme.diagnostic_warning_underline);
  styles.diagnostic_warning.underline_style = se::RangeEffectUnderlineStyle::WAVY;
  styles.diagnostic_info.underline_color = ToArgb(theme.diagnostic_info_underline);
  styles.diagnostic_info.underline_style = se::RangeEffectUnderlineStyle::WAVY;
  styles.diagnostic_hint.underline_color = ToArgb(theme.diagnostic_hint_underline);
  styles.diagnostic_hint.underline_style = se::RangeEffectUnderlineStyle::WAVY;
  return styles;
}


// ---- Text input bridge (HuxerUI IME session -> SweetEditor core) -----------
class CodeEditorTextInputClient final : public TextInputClient, public TextSelectionClient {
public:
  CodeEditorTextInputClient(
      std::shared_ptr<se::EditorCore> core,
      std::shared_ptr<se::Document> document,
      std::function<void(const std::vector<se::TextChange>&)> on_content_changed,
      std::function<void()> on_change,
      std::function<void(const std::vector<se::TextChange>&)> on_text_committed,
      std::function<void()> on_edit,
      std::function<bool()> on_tab,
      bool read_only,
      CodeEditorTheme theme
  )
      : core_(std::move(core)),
        theme_(std::move(theme)),
        document_(std::move(document)),
        on_content_changed_(std::move(on_content_changed)),
        on_change_(std::move(on_change)),
        on_text_committed_(std::move(on_text_committed)),
        on_edit_(std::move(on_edit)),
        on_tab_(std::move(on_tab)),
        read_only_(read_only) {}

  TextInputConfiguration Configuration() const override {
    TextInputConfiguration configuration;
    configuration.type = TextInputType::Text;
    configuration.capitalization = TextCapitalization::None;
    configuration.action = TextInputAction::Newline;
    configuration.multiline = true;
    configuration.secure = false;
    configuration.autocorrect = false;
    configuration.read_only = read_only_;
    return configuration;
  }

  TextInputState State() const override {
    std::optional<TextRange> composition;
    if (composition_start_ >= 0 && composition_end_ >= composition_start_) {
      composition = TextRange{composition_start_, composition_end_};
    }
    return {
        session_id_,
        revision_,
        content_revision_,
        Selection(),
        composition,
    };
  }

  TextInputState BeginTextInput(TextInputSessionId session_id) override {
    session_id_ = session_id;
    return State();
  }

  // The runtime requires revision_ to advance on every observable selection change
  // (for example, a click or drag that moves the caret), not only on text edits.
  void NotifySelectionChanged() {
    ++revision_;
  }

  void NotifyContentChanged(const std::vector<se::TextChange>& changes) {
    ++telemetry_notify_calls;
    ++revision_;
    ++content_revision_;
    if (on_content_changed_) {
      on_content_changed_(changes);
    }
    // Live diff: re-run the comparison after every edit while a diff original
    // is active so added/removed rows track typing. computeDiff emits no text
    // changes, only a redraw request (the hook invalidates the frame).
    if (on_diff_edit_) {
      on_diff_edit_();
    }
  }

  // Selection/content edits performed outside the normal input pipeline (word
  // selection, drag-handle extension, clipboard actions) must mark the render
  // model dirty and schedule a repaint.
  void NotifyEdit() {
    on_change_();
    if (on_edit_) {
      on_edit_();
    }
  }

  // Live pipeline counters surfaced through the component telemetry hint.
  uint64_t telemetry_apply_calls = 0;
  uint64_t telemetry_notify_calls = 0;
  CodeEditorTheme theme_;

  void SetTheme(const CodeEditorTheme& theme) { theme_ = theme; }

  TextInputApplyResult ApplyTextInput(const TextInputCommandBatch& batch) override {
    ++telemetry_apply_calls;
    CODEEDITOR_TRACE("client: ApplyTextInput commands=%zu session=%s", batch.commands.size(),
                     batch.session_id == session_id_ ? "ok" : "MISMATCH");
    if (batch.session_id != session_id_) {
      return {TextInputResultCode::SessionMismatch, TextInputSyncAction::None};
    }
    if (read_only_) {
      return {TextInputResultCode::ReadOnly, TextInputSyncAction::None};
    }

    bool changed = false;
    std::vector<se::TextChange> changes;
    for (const TextInputCommand& command : batch.commands) {
      changed = ApplyCommand(command, changes) || changed;
    }
    if (changed) {
      NotifyContentChanged(changes);
      NotifyEdit();
      if (on_text_committed_) {
        on_text_committed_(changes);
      }
    }
    return {TextInputResultCode::Ok, TextInputSyncAction::None};
  }

  TextInputContext QueryTextInputContext(TextInputSessionId session_id, TextOffset, TextOffset) const override {
    TextInputContext result;
    result.session_id = session_id;
    if (session_id != session_id_) {
      result.result_code = TextInputResultCode::SessionMismatch;
      return result;
    }
    const se::U16String text = document_->getU16Text();
    result.result_code = TextInputResultCode::Ok;
    result.slice_start = 0;
    result.total_length = static_cast<TextOffset>(text.size());
    result.text = document_->getU8Text();
    result.selection = Selection();
    if (composition_start_ >= 0 && composition_end_ >= composition_start_) {
      result.composition = TextRange{composition_start_, composition_end_};
    }
    return result;
  }

  TextInputGeometry QueryTextInputGeometry(TextInputSessionId session_id, TextRange) const override {
    TextInputGeometry result;
    result.session_id = session_id;
    if (session_id != session_id_) {
      result.result_code = TextInputResultCode::SessionMismatch;
      return result;
    }
    const se::CursorRect cursor = core_->getCursorScreenRect();
    result.result_code = TextInputResultCode::Ok;
    result.caret = Rect{cursor.x, cursor.y, theme_.caret_width, cursor.height};
    return result;
  }

  TextInputPositionResult QueryTextInputPosition(TextInputSessionId session_id, Point point) const override {
    TextInputPositionResult result;
    result.session_id = session_id;
    if (session_id != session_id_ || document_ == nullptr) {
      result.result_code = TextInputResultCode::SessionMismatch;
      return result;
    }
    // Report the caret's document offset: the platform uses this to anchor
    // IME candidate windows. A precise screen->position hit test would need
    // the layout internals; the caret offset is what Android queries between
    // composition updates.
    const se::TextPosition cursor = core_->getCursorPosition();
    result.result_code = TextInputResultCode::Ok;
    result.position = {
        static_cast<TextOffset>(document_->getCharIndexFromPosition(cursor)),
        TextAffinity::Downstream,
    };
    return result;
  }

  TextInputKeyResult HandleTextKey(const KeyEvent& event) override {
    // Tab reaches the editor only through this hook: the runtime normally
    // reserves Tab for focus traversal, but gives a focused text input the
    // first chance. The holder decides between snippet tab-stop jumps,
    // phantom-text accept, and completion navigation; a plain indent Tab is
    // left to the core's key handling (which also runs here via on_tab_).
    if (event.type == KeyEventType::Down && event.key == Key::Tab && !event.repeat) {
      if (on_tab_ && on_tab_()) {
        return TextInputKeyResult::Handled;
      }
    }
    return TextInputKeyResult::Unhandled;
  }

  bool CanPerformTextEditingAction(TextEditingAction action, PlatformClipboard* clipboard) const override {
    switch (action) {
    case TextEditingAction::Cut:
    case TextEditingAction::Copy:
      return clipboard != nullptr && !core_->getSelection().isCollapsed();
    case TextEditingAction::Paste:
      return clipboard != nullptr && !read_only_;
    case TextEditingAction::SelectAll:
      return true;
    }
    return false;
  }

  bool PerformTextEditingAction(TextEditingAction action, PlatformClipboard* clipboard) override {
    if (clipboard != nullptr) {
      clipboard_ = clipboard;
    }
    if (clipboard == nullptr) {
      return false;
    }
    switch (action) {
    case TextEditingAction::Copy:
    case TextEditingAction::Cut: {
      const se::TextRange selection = core_->getSelection().normalized();
      if (selection.isCollapsed()) {
        return false;
      }
      const std::string text = document_->getU8Text(selection);
      if (!clipboard->WriteText(text)) {
        return false;
      }
      if (action == TextEditingAction::Cut) {
        if (read_only_) {
          return false;
        }
        const se::EditorActionResult result = core_->replaceText(selection, "");
        if (!result.text_changes.empty()) {
          NotifyContentChanged(result.text_changes);
          NotifyEdit();
        }
      }
      return true;
    }
    case TextEditingAction::Paste: {
      if (read_only_) {
        return false;
      }
      const std::optional<std::string> text = clipboard->ReadText();
      if (!text.has_value() || text->empty()) {
        return false;
      }
      const se::EditorActionResult result = core_->replaceText(core_->getSelection().normalized(), *text);
      if (!result.text_changes.empty()) {
        NotifyContentChanged(result.text_changes);
        NotifyEdit();
      }
      return true;
    }
    case TextEditingAction::SelectAll: {
      const se::EditorActionResult result = core_->selectAll();
      if (result.selection_changed || result.cursor_changed) {
        NotifySelectionChanged();
        NotifyEdit();
      }
      return true;
    }
    }
    return false;
  }

  // Context-menu clipboard actions; the clipboard pointer is cached from the
  // runtime's editing-action calls (the platform adapter outlives the session).
  bool CopySelection() {
    if (clipboard_ == nullptr || core_->getSelection().isCollapsed()) {
      return false;
    }
    return clipboard_->WriteText(document_->getU8Text(core_->getSelection().normalized()));
  }

  bool PasteFromClipboard() {
    if (clipboard_ == nullptr || read_only_) {
      return false;
    }
    const std::optional<std::string> text = clipboard_->ReadText();
    if (!text.has_value() || text->empty()) {
      return false;
    }
    const se::EditorActionResult result = core_->replaceText(core_->getSelection().normalized(), *text);
    if (!result.text_changes.empty()) {
      NotifyContentChanged(result.text_changes);
      NotifyEdit();
    }
    return true;
  }

  void SetDiffEditHook(std::function<void()> hook) {
    on_diff_edit_ = std::move(hook);
  }

  void EndTextInput(TextInputSessionId, TextInputEndReason) override {
    if (ime_session_open_) {
      core_->endImeSession(ime_session_id_);
      ime_session_open_ = false;
      composition_start_ = -1;
      composition_end_ = -1;
    }
  }

  // TextSelectionClient: the editor core owns pointer gestures, double-tap
  // word selection, and drag-handle extension through its gesture pipeline.
  // These runtime callbacks return false so the HuxerUI selection overlay
  // (handles + toolbar) stays hidden — the editor draws its own handles and
  // would otherwise render duplicates that ghost while dragging.
  bool SelectWord(Point) override {
    return false;
  }

  bool ExtendSelection(Point, bool) override {
    return false;
  }

  bool QuerySelectionGeometry(Rect&, Rect&) const override {
    return false;
  }

  Color SelectionHandleColor() const noexcept override {
    return Argb(0xFF1F6FEB);
  }

private:
  TextSelection Selection() const {
    const se::TextRange selection = core_->getSelection();
    return {
        static_cast<TextOffset>(document_->getCharIndexFromPosition(selection.start)),
        static_cast<TextOffset>(document_->getCharIndexFromPosition(selection.end)),
        TextAffinity::Downstream,
    };
  }

  // Opens the core IME session on demand (one session per HuxerUI session).
  bool EnsureImeSession() {
    if (ime_session_open_) {
      return true;
    }
    const se::ImeState state = core_->beginImeSession(se::ImeMutationModel::COMMAND);
    if (state.result_code != se::ImeResultCode::OK) {
      return false;
    }
    ime_session_id_ = state.session_id;
    ime_session_open_ = true;
    return true;
  }

  // Maps a UTF-16 document range to the core's IME coordinate space.
  se::ImeOffsetRange ToImeRange(TextOffset start, TextOffset end) {
    se::ImeOffsetRange range;
    range.coordinate_space = se::ImeCoordinateSpace::DOCUMENT;
    range.start_utf16 = static_cast<int64_t>(std::max<TextOffset>(0, start));
    range.end_utf16 = static_cast<int64_t>(std::max<TextOffset>(0, end));
    return range;
  }

  // Applies a core IME command batch; captures resulting text changes and the
  // active composition range (rendered with the IME underline style).
  bool SendImeCommands(std::vector<se::ImeCommand> commands, std::vector<se::TextChange>& changes) {
    if (!EnsureImeSession()) {
      return false;
    }
    se::ImeCommandBatch batch;
    batch.session_id = ime_session_id_;
    batch.commands = std::move(commands);
    const se::EditorActionResult result = core_->applyImeCommands(batch);
    if (!result.text_changes.empty()) {
      changes.insert(changes.end(), result.text_changes.begin(), result.text_changes.end());
    }
    TrackComposition(result.ime_state);
    return result.handled;
  }

  // Updates the local UTF-16 composition span from the core's IME state so
  // State() can report it to the platform (inline preview/underline).
  void TrackComposition(const se::ImeState& state) {
    const se::ImeOffsetRange& range = state.composition_range;
    if (range.start_utf16 < 0 || range.end_utf16 < range.start_utf16) {
      composition_start_ = -1;
      composition_end_ = -1;
      return;
    }
    composition_start_ = static_cast<TextOffset>(range.start_utf16);
    composition_end_ = static_cast<TextOffset>(range.end_utf16);
  }

  bool ApplyCommand(const TextInputCommand& command, std::vector<se::TextChange>& changes) {
    const auto capture = [&changes](const se::EditorActionResult& result) {
      changes.insert(changes.end(), result.text_changes.begin(), result.text_changes.end());
    };
    switch (command.kind) {
    case TextInputCommandKind::CommitText:
      if (!command.text.empty()) {
        capture(core_->insertText(command.text));
        return true;
      }
      return false;
    case TextInputCommandKind::SetSelection:
      if (command.selection_after.has_value()) {
        const TextSelection& selection = *command.selection_after;
        const se::TextPosition start =
            document_->getPositionFromCharIndex(static_cast<size_t>(std::max<TextOffset>(0, selection.anchor)));
        const se::TextPosition end =
            document_->getPositionFromCharIndex(static_cast<size_t>(std::max<TextOffset>(0, selection.active)));
        core_->setSelection({start, end});
        return true;
      }
      return false;
    case TextInputCommandKind::DeleteSurrounding: {
      bool changed = false;
      for (TextOffset index = 0; index < command.delete_before; ++index) {
        capture(core_->backspace());
        changed = true;
      }
      for (TextOffset index = 0; index < command.delete_after; ++index) {
        capture(core_->deleteForward());
        changed = true;
      }
      return changed;
    }
    case TextInputCommandKind::BeginComposition: {
      // Replaces the current selection with the composition text and marks the
      // range as an active composition (rendered with the IME underline style).
      if (command.text.empty()) {
        return false;
      }
      const TextSelection selection = Selection();
      const TextRange range = selection.Range();
      std::vector<se::ImeCommand> ime_commands;
      se::ImeCommand begin;
      begin.kind = se::ImeCommandKind::BEGIN_COMPOSITION;
      begin.target_range = ToImeRange(range.start, range.end);
      begin.text = command.text;
      ime_commands.push_back(std::move(begin));
      return SendImeCommands(std::move(ime_commands), changes);
    }
    case TextInputCommandKind::UpdateComposition: {
      if (command.text.empty()) {
        return false;
      }
      // Chinese/Japanese IMEs replace the whole composition string on each
      // step, so no target range is needed: the core treats the update as a
      // full-text replacement of the active composition.
      std::vector<se::ImeCommand> ime_commands;
      se::ImeCommand update;
      update.kind = se::ImeCommandKind::UPDATE_COMPOSITION;
      update.text = command.text;
      ime_commands.push_back(std::move(update));
      return SendImeCommands(std::move(ime_commands), changes);
    }
    case TextInputCommandKind::FinishComposition: {
      std::vector<se::ImeCommand> ime_commands;
      se::ImeCommand finish;
      finish.kind = se::ImeCommandKind::FINISH_COMPOSITION;
      if (!command.text.empty()) {
        finish.text = command.text;
      }
      ime_commands.push_back(std::move(finish));
      return SendImeCommands(std::move(ime_commands), changes);
    }
    case TextInputCommandKind::CancelComposition: {
      std::vector<se::ImeCommand> ime_commands;
      se::ImeCommand cancel;
      cancel.kind = se::ImeCommandKind::CANCEL_COMPOSITION;
      ime_commands.push_back(std::move(cancel));
      return SendImeCommands(std::move(ime_commands), changes);
    }
    }
    return false;
  }

  std::shared_ptr<se::EditorCore> core_;
  std::shared_ptr<se::Document> document_;
  std::function<void(const std::vector<se::TextChange>&)> on_content_changed_;
  std::function<void()> on_change_;
  std::function<void(const std::vector<se::TextChange>&)> on_text_committed_;
  std::function<void()> on_edit_;
  std::function<bool()> on_tab_;
  std::function<void()> on_diff_edit_;
  bool read_only_;
  PlatformClipboard* clipboard_{nullptr};
  TextInputSessionId session_id_ = 0;
  std::uint64_t revision_ = 0;
  std::uint64_t content_revision_ = 0;
  // SweetEditor core IME session id (distinct from the HuxerUI session id).
  uint64_t ime_session_id_{0};
  bool ime_session_open_{false};
  // Active composition span in UTF-16 document offsets (-1 when none).
  TextOffset composition_start_{-1};
  TextOffset composition_end_{-1};
};

// ---- Editor holder ---------------------------------------------------------
class EditorHolder {
public:
  EditorHolder(
      TextMeasurer& measurer,
      const CodeEditorOptions& options,
      std::function<void()> invalidate,
      huxerui::EventEmitter events,
      CodeEditorController controller
  )
      : font_size_(options.font_size),
        invalidate_(std::move(invalidate)),
        events_(std::move(events)),
        controller_(std::move(controller)),
        completion_provider_(options.completion_provider),
        completion_trigger_characters_(options.completion_trigger_characters),
        theme_(options.theme.value_or(CodeEditorTheme::Default())) {
    font_metrics_ = measurer.Metrics(Font::Monospace(font_size_));
    char_width_ = measurer.MeasureRun("0", TextStyle{Font::Monospace(font_size_), Color::Black(), TextDecoration::None})
                      .advance;
    completion_label_metrics_ = measurer.Metrics(Font::System(kCompletionLabelSize));
    completion_detail_metrics_ = measurer.Metrics(Font::System(kCompletionDetailSize));
    const TextStyle badge_style{Font::System(kCompletionBadgeSizePx).WithWeight(FontWeight::Bold), Color::White(),
                                TextDecoration::None};
    completion_badge_metrics_ = measurer.Metrics(Font::System(kCompletionBadgeSizePx));
    completion_badge_char_advance_ = measurer.MeasureRun("W", badge_style).advance;

    se::EditorOptions core_options;
    core_ = std::make_shared<se::EditorCore>(
        std::make_shared<HuxeruiTextMeasurer>(measurer, font_size_), core_options);
    theme_ = options.theme.value_or(CodeEditorTheme::Default());
    core_->setEditorRenderColors(MakeRenderColors(theme_));
    core_->setEditorRangeEffectStyles(MakeRangeEffectStyles(theme_));
    core_->setLineSpacing(options.line_spacing_add, options.line_spacing_mult);
    core_->setReadOnly(options.read_only);

    // Editing feel aligned with the reference platform: keep the previous
    // line's indent on newline, backspace unindents to the tab stop, Tab
    // inserts spaces up to the next stop, and typed opening brackets insert
    // their closing partner (core default bracket pairs () {} []).
    core_->setTabSize(options.tab_size > 0 ? options.tab_size : 4);
    core_->setAutoIndentMode(se::AutoIndentMode::KEEP_INDENT);
    core_->setBackspaceUnindent(options.backspace_unindent);
    core_->setInsertSpaces(options.insert_spaces);
    // The core disables auto-closing when the pair list is empty; the reference
    // platform ships with () {} [] enabled, so apply that default unless the
    // host explicitly configured a list.
    const std::vector<std::pair<char32_t, char32_t>>& closing_pairs =
        options.auto_closing_pairs.empty()
        ? kDefaultAutoClosingPairs
        : options.auto_closing_pairs;
    core_->setAutoClosingPairs(MakeBracketPairs(closing_pairs));
    core_->setRenderWhitespace(
        options.render_whitespace ? se::WhitespaceRenderMode::ALL : se::WhitespaceRenderMode::NONE
    );
    core_->setRenderLineBreaks(options.render_line_breaks);
    // Gutter icons share the line-number lane: with max_gutter_icons == 0 the
    // core reports overlay mode (icon replaces the line number); the reference
    // demo sets 1 so the icon renders beside the number.
    core_->setMaxGutterIcons(1);
    // Display options: wrap mode (0=NONE/1=CHAR_BREAK/2=WORD_BREAK) and sticky
    // gutter (line numbers stay fixed during horizontal scroll).
    current_wrap_mode_ = options.wrap_mode;
    current_sticky_gutter_ = options.sticky_gutter;
    if (options.wrap_mode >= 1 && options.wrap_mode <= 2) {
      core_->setWrapMode(
          options.wrap_mode == 1 ? se::WrapMode::CHAR_BREAK : se::WrapMode::WORD_BREAK
      );
    }
    core_->setGutterSticky(options.sticky_gutter);
    if (options.content_start_padding > 0.0F) {
      core_->setContentStartPadding(options.content_start_padding);
    }
    // Scrollbar customization (defaults are sensible; hosts may override).
    if (options.scrollbar_thickness > 0.0F || options.scrollbar_mode != 0) {
      se::ScrollbarConfig scrollbar;
      scrollbar.thickness = options.scrollbar_thickness > 0.0F
          ? options.scrollbar_thickness
          : 12.0F;
      scrollbar.mode = static_cast<se::ScrollbarMode>(std::clamp(options.scrollbar_mode, 0, 2));
      core_->setScrollbarConfig(scrollbar);
    }

    document_ = std::make_shared<se::LineArrayDocument>(se::U8String(options.initial_text));
    core_->loadDocument(document_);
    // Optional diff presentation against a provided original text (the demo
    // feeds each file's pristine content so edits light up added/removed rows).
    // The original is tracked so the component can switch the diff on/off
    // without rebuilding the holder (which would discard the live edits).
    current_diff_original_ = options.original_text;
    if (!current_diff_original_.empty()) {
      core_->computeDiff(current_diff_original_);
    }

    RegisterSyntaxStyles(*core_, theme_);
    providers_ = options.decoration_providers;
    // Initial decoration pass: publishes whole-document fold regions and
    // lights the first viewport once providers are attached.
    RefreshDecorations(true);

    text_input_client_ = std::make_shared<CodeEditorTextInputClient>(
        core_, document_,
        [this](const std::vector<se::TextChange>& changes) {
          RecordPendingChanges(changes);
          RefreshDecorations(true);
        },
        invalidate_,
        [this](const std::vector<se::TextChange>& changes) { UpdateCompletion(changes); },
        [this]() {
          model_dirty_ = true;
          if (invalidate_) {
            invalidate_();
          }
        },
        [this]() { return HandleTab(); },
        options.read_only,
        theme_);
    text_input_client_->SetDiffEditHook([this]() {
      if (current_diff_original_.empty()) {
        return;
      }
      core_->computeDiff(current_diff_original_);
      model_dirty_ = true;
      if (invalidate_) {
        invalidate_();
      }
    });

    newline_action_ = options.newline_action;
    accept_phantom_on_tab_ = options.accept_phantom_on_tab;
  }

  // Enables/disables the diff presentation against `original` without touching
  // the live document: recomputes the line-level diff (or clears it) and
  // repaints. Used by the host toggle so edits survive switching the diff.
  const std::string& CurrentDiffOriginal() const {
    return current_diff_original_;
  }

  // Runtime sync for display options changed after construction (wrap mode,
  // sticky gutter): the host toggles them without rebuilding the holder.
  void SyncDisplayOptions(int wrap_mode, bool sticky_gutter) {
    if (wrap_mode != current_wrap_mode_) {
      current_wrap_mode_ = wrap_mode;
      if (wrap_mode >= 1 && wrap_mode <= 2) {
        core_->setWrapMode(
            wrap_mode == 1 ? se::WrapMode::CHAR_BREAK : se::WrapMode::WORD_BREAK
        );
      } else {
        core_->setWrapMode(se::WrapMode::NONE);
      }
      AfterCoreAction({});
    }
    if (sticky_gutter != current_sticky_gutter_) {
      current_sticky_gutter_ = sticky_gutter;
      core_->setGutterSticky(sticky_gutter);
      AfterCoreAction({});
    }
  }

  void SetDiffOriginal(std::string original) {
    if (original == current_diff_original_) {
      return;
    }
    current_diff_original_ = std::move(original);
    se::EditorActionResult result = current_diff_original_.empty()
        ? core_->clearDiff()
        : core_->computeDiff(current_diff_original_);
    AfterCoreAction(result);
  }

  // ---- Find & replace (reference search module) ----------------------------

  // Runs a fresh search; empty pattern clears the search state.
  void RunSearch(const std::string& pattern) {
    se::SearchRequest request;
    request.pattern = pattern;
    if (!pattern.empty()) {
      se::EditorActionResult result = core_->search(request);
      AfterCoreAction(result);
    } else {
      se::EditorActionResult result = core_->clearSearch();
      AfterCoreAction(result);
    }
  }

  void FindNext() {
    se::EditorActionResult result = core_->findNextSearchMatch();
    AfterCoreAction(result);
  }

  void FindPrevious() {
    se::EditorActionResult result = core_->findPreviousSearchMatch();
    AfterCoreAction(result);
  }

  void ReplaceCurrent(const std::string& replacement) {
    if (replacement.empty()) {
      FindNext();
      return;
    }
    se::EditorActionResult result = core_->replaceCurrentSearchMatch(replacement);
    AfterCoreAction(result);
  }

  void ReplaceAll(const std::string& replacement) {
    se::EditorActionResult result = core_->replaceAllSearchMatches(replacement);
    AfterCoreAction(result);
  }

  void CloseSearch() {
    se::EditorActionResult result = core_->clearSearch();
    AfterCoreAction(result);
  }

  // Full document text as UTF-8 (controller Text()).
  std::string Text() const {
    return document_ ? document_->getU8Text() : std::string();
  }

  // Moves the caret to a zero-based line/column (controller SetCursor()).
  bool SetCursor(uint32_t line, uint32_t column) {
    se::EditorActionResult result = core_->setCursorPosition(se::TextPosition{line, column});
    AfterCoreAction(result);
    return result.handled;
  }

  // Common post-core-action refresh (text/selection/scroll invalidation).
  void AfterCoreAction(const se::EditorActionResult& result) {
    if (!result.text_changes.empty()) {
      text_input_client_->NotifyContentChanged(result.text_changes);
      RefreshDecorations(true);
      FireTextChanged();
    } else if (result.selection_changed || result.cursor_changed || result.scroll_changed) {
      text_input_client_->NotifySelectionChanged();
      RefreshDecorations(true);
    }
    model_dirty_ = true;
    if (invalidate_) {
      invalidate_();
    }
  }

  // ---- Context menu (long-press / double-tap) ------------------------------

  void ShowContextMenu(const se::PointF& point) {
    context_menu_.visible = true;
    context_menu_.anchor = point;
    context_menu_.entries = {
        {"Cut", 0},
        {"Copy", 1},
        {"Paste", 2},
        {"Select All", 3},
        {"Find", 4},
        {"Fold All", 5},
        {"Unfold All", 6},
    };
    // Simple anchored sizing: 150px wide, menu row height, clamped to the
    // viewport so the panel never floats off-screen.
    const float width = 150.0F;
    const float row_height = 30.0F;
    const float height = row_height * static_cast<float>(context_menu_.entries.size()) + 8.0F;
    const float x = std::clamp(point.x, 0.0F, std::max(0.0F, viewport_.width - width));
    const float y = std::clamp(point.y, 0.0F, std::max(0.0F, viewport_.height - height));
    context_menu_.panel_rect = {x, y, width, height};
  }

  void DismissContextMenu() {
    context_menu_.visible = false;
    context_menu_.entries.clear();
  }

  // Returns true when the point hits the open context menu (down swallows it).
  bool HitContextMenu(const se::PointF& point, int& out_command) const {
    if (!context_menu_.visible) {
      return false;
    }
    const se::Rect& rect = context_menu_.panel_rect;
    if (point.x < rect.origin.x || point.x > rect.origin.x + rect.width ||
        point.y < rect.origin.y || point.y > rect.origin.y + rect.height) {
      return false;
    }
    const float row_height = 30.0F;
    const size_t index = static_cast<size_t>((point.y - rect.origin.y - 4.0F) / row_height);
    if (index < context_menu_.entries.size()) {
      out_command = context_menu_.entries[index].command;
      return true;
    }
    return false;
  }

  void RunContextCommand(int command) {
    switch (command) {
    case 0: {
      const se::EditorActionResult result = core_->deleteText(core_->getSelection().normalized());
      AfterCoreAction(result);
      break;
    }
    case 1: {
      // Copy via the runtime clipboard through the text input client.
      text_input_client_->CopySelection();
      break;
    }
    case 2:
      text_input_client_->PasteFromClipboard();
      break;
    case 3: {
      const se::EditorActionResult result = core_->selectAll();
      AfterCoreAction(result);
      break;
    }
    case 4:
      controller_.ToggleSearch();
      break;
    case 5:
      AfterCoreAction(core_->foldAll());
      break;
    case 6:
      AfterCoreAction(core_->unfoldAll());
      break;
    default:
      break;
    }
    DismissContextMenu();
  }

  void DrawContextMenu(PaintContext& paint) {
    if (!context_menu_.visible) {
      return;
    }
    const se::Rect& rect = context_menu_.panel_rect;
    paint.DrawRect(ToRect(rect), Argb(0xFFFFFFFF), CornerRadii(6.0F));
    paint.DrawBorder(ToRect(rect), Argb(0xFFDDDDDD), StrokeStyle{1.0F}, CornerRadii(6.0F));
    const float row_height = 30.0F;
    const TextStyle item_style{Font::System(13.0F), Argb(0xFF1F1F1F), TextDecoration::None};
    float y = rect.origin.y + 4.0F;
    for (const ContextMenuEntry& entry : context_menu_.entries) {
      const float baseline = y + (row_height + font_metrics_.descent - font_metrics_.ascent) * 0.5F;
      paint.DrawTextRun(
          Rect{rect.origin.x + 10.0F, y, rect.width - 20.0F, row_height},
          Point{rect.origin.x + 10.0F, baseline},
          entry.label,
          item_style);
      y += row_height;
    }
  }

  // Builds the multi-touch GestureEvent for the current pointer event. Touch
  // pointers are aggregated in press order so a two-finger move reaches the
  // core as a two-point TOUCH_MOVE and is recognized as SCALE (pinch zoom).
  se::GestureEvent BuildTouchGestureEvent(const PointerEvent& event) {
    se::GestureEvent out;
    out.modifiers = se::KeyModifier::NONE;
    const se::PointF point{event.position.x, event.position.y};
    switch (event.type) {
    case PointerEventType::Down: {
      const bool first_pointer = active_pointers_.empty();
      auto found = std::find_if(active_pointers_.begin(), active_pointers_.end(),
                                [&](const auto& entry) { return entry.first == event.pointer_id; });
      if (found == active_pointers_.end()) {
        active_pointers_.emplace_back(event.pointer_id, point);
      } else {
        found->second = point;
      }
      out.type = first_pointer ? se::EventType::TOUCH_DOWN : se::EventType::TOUCH_POINTER_DOWN;
      if (active_pointers_.size() >= 2) {
        pinch_last_distance_ = active_pointers_[0].second.distance(active_pointers_[1].second);
      }
      break;
    }
    case PointerEventType::Move: {
      for (auto& entry : active_pointers_) {
        if (entry.first == event.pointer_id) {
          entry.second = point;
          break;
        }
      }
      if (active_pointers_.size() >= 2) {
        // Two fingers: report the distance ratio directly. DIRECT_SCALE skips
        // the core's touch state machine and applies the scale immediately.
        const float distance = active_pointers_[0].second.distance(active_pointers_[1].second);
        if (pinch_last_distance_ > 0.0F && distance > 0.0F) {
          out.type = se::EventType::DIRECT_SCALE;
          out.direct_scale = distance / pinch_last_distance_;
        }
        pinch_last_distance_ = distance;
        break;
      }
      out.type = se::EventType::TOUCH_MOVE;
      break;
    }
    case PointerEventType::Up:
    case PointerEventType::Cancel: {
      active_pointers_.erase(std::remove_if(active_pointers_.begin(), active_pointers_.end(),
                                            [&](const auto& entry) { return entry.first == event.pointer_id; }),
                             active_pointers_.end());
      pinch_last_distance_ = 0.0F;
      out.type = active_pointers_.empty() ? se::EventType::TOUCH_UP : se::EventType::TOUCH_POINTER_UP;
      break;
    }
    }
    for (const auto& entry : active_pointers_) {
      out.points.push_back(entry.second);
    }
    if (out.points.empty()) {
      out.points.push_back(point);
    }
    return out;
  }

  bool HandlePointer(const PointerEvent& event) {
    if (event.type == PointerEventType::Down) {
      suppress_focus_on_up_ = false;
    }
    // A down inside the open context menu executes the tapped entry; a down
    // outside dismisses it (then falls through to the editor).
    if (event.type == PointerEventType::Down && context_menu_.visible) {
      int command = -1;
      if (HitContextMenu(se::PointF{event.position.x, event.position.y}, command)) {
        RunContextCommand(command);
        return true;
      }
      DismissContextMenu();
    }
    // Scrollbar thumb dragging takes priority over the editor gestures.
    if (scrollbar_dragging_) {
      if (event.type == PointerEventType::Move) {
        DragScrollbar(event.position);
        return true;
      }
      if (event.type == PointerEventType::Up || event.type == PointerEventType::Cancel) {
        scrollbar_dragging_ = false;
        return true;
      }
    } else if (event.type == PointerEventType::Down) {
      bool vertical = false;
      if (HitScrollbarThumb(event.position, vertical)) {
        scrollbar_dragging_ = true;
        scrollbar_drag_vertical_ = vertical;
        const Rect thumb = ToRect(vertical ? cached_scrollbar_v_.thumb : cached_scrollbar_h_.thumb);
        scrollbar_drag_offset_ = vertical ? event.position.y - thumb.y : event.position.x - thumb.x;
        return true;
      }
    }
    if (event.type == PointerEventType::Down && completion_.visible && !completion_.items.empty()) {
      size_t row = 0;
      if (HitCompletionPanel(event.position, row)) {
        completion_.selected = row;
        ConfirmCompletion();
        return true;
      }
      // A tap outside the panel dismisses it, then falls through to the editor.
      DismissCompletion();
    }
    try {
      const bool touch = event.device_kind == PointerDeviceKind::Touch;
      const se::EditorActionResult result =
          core_->handleGestureEvent(touch ? BuildTouchGestureEvent(event) : ToGestureEvent(event));
      if (result.selection_changed || result.cursor_changed) {
        text_input_client_->NotifySelectionChanged();
        RefreshDecorations(true);
        FireCaretEvents();
      }
      // Decoration hits (reference fireGestureEvents): fold toggles are
      // handled inline; the rest dispatch to host callbacks. Taps on command
      // areas (gutter icons, code lens, links, inlay hints, fold controls)
      // must not focus the editor or raise the keyboard.
      if (result.gesture_type == se::GestureType::TAP && result.hit_target.type != se::HitTargetType::NONE) {
        const se::HitTarget& target = result.hit_target;
        suppress_focus_on_up_ = true;
        switch (target.type) {
        case se::HitTargetType::FOLD_GUTTER:
        case se::HitTargetType::FOLD_PLACEHOLDER:
          // The core's gesture pipeline already folded the line
          // (intent.toggle_fold -> toggleFoldAtInternal); do NOT toggle again
          // here or the double toggle cancels out. Just refresh and notify.
          RefreshDecorations(false);
          FireFoldToggle(target.line);
          // Folding changes the visible line set, so the cached render model
          // must be rebuilt on the next frame.
          model_dirty_ = true;
          return true;
        case se::HitTargetType::LINK:
          events_.Emit<CodeEditorEvents::LinkClicked>(LinkTextAt(target.line, target.column));
          return true;
        case se::HitTargetType::CODELENS:
          events_.Emit<CodeEditorEvents::CodeLensClicked>(target.icon_id);
          return true;
        case se::HitTargetType::GUTTER_ICON:
          events_.Emit<CodeEditorEvents::GutterIconClicked>(static_cast<uint32_t>(target.line), target.icon_id);
          return true;
        case se::HitTargetType::INLAY_HINT_TEXT:
        case se::HitTargetType::INLAY_HINT_ICON:
        case se::HitTargetType::INLAY_HINT_COLOR:
          events_.Emit<CodeEditorEvents::InlayClicked>(
              static_cast<uint32_t>(target.line), static_cast<uint32_t>(target.column)
          );
          return true;
        default:
          break;
        }
      }
      FirePointerEvents(result);
      if (result.needs_redraw) {
        model_dirty_ = true;
      }
      if (result.needsAnimation()) {
        animation_pending_ = true;
        if (invalidate_) {
          invalidate_();
        }
      }
      return result.needs_redraw;
    } catch (const std::exception&) {
      return false;
    }
  }

  bool HandleKey(const KeyEvent& event) {
    if (event.type == KeyEventType::Down) {
      // Line operations (reference keymap): Alt+Up/Down move the line,
      // Shift+Alt+Up/Down copy it, Ctrl+Shift+K deletes it. The huxerui Key
      // enum lacks K, so Ctrl+Shift+K is matched through the text payload.
      if (event.modifiers.alt && !event.modifiers.control && !event.modifiers.meta) {
        if (event.key == Key::ArrowUp || event.key == Key::ArrowDown) {
          const bool copy = event.modifiers.shift;
          const se::EditorActionResult result = event.key == Key::ArrowUp
              ? (copy ? core_->copyLineUp() : core_->moveLineUp())
              : (copy ? core_->copyLineDown() : core_->moveLineDown());
          AfterCoreAction(result);
          return result.handled;
        }
      }
      if (!event.modifiers.alt && event.modifiers.control && event.modifiers.shift &&
          (event.text == "k" || event.text == "K")) {
        const se::EditorActionResult result = core_->deleteLine();
        AfterCoreAction(result);
        return result.handled;
      }
      // Escape dismisses the context menu first.
      if (event.key == Key::Escape && context_menu_.visible) {
        DismissContextMenu();
        if (invalidate_) {
          invalidate_();
        }
        return true;
      }
      // Tab reaches the editor through the text-input key hook (runtime gives a
      // focused text input first chance before focus traversal); it handles
      // linked-editing jumps, phantom accept, completion navigation, and plain
      // indentation in one place. Keep the direct path for platforms that do
      // deliver Tab to the component (e.g. desktop textareas).
      if (event.key == Key::Tab && !event.repeat && HandleTab()) {
        return true;
      }
      // Panel navigation takes priority over the core (reference
      // handleAndroidKeyCode: Enter confirms, Escape dismisses, arrows move).
      // Enter may arrive as a key or as '\r'/'\n' text depending on platform.
      const bool is_enter = event.key == Key::Enter || event.text == "\r" || event.text == "\n";
      if (completion_.visible && !completion_.items.empty()) {
        if (is_enter) {
          ConfirmCompletion();
          return true;
        }
        switch (event.key) {
        case Key::Escape:
          DismissCompletion();
          return true;
        case Key::ArrowUp:
          MoveCompletionSelection(-1);
          return true;
        case Key::ArrowDown:
          MoveCompletionSelection(1);
          return true;
        default:
          break;
        }
      }
      // Host newline action overrides the core's newline insertion
      // (reference NewLineActionProvider).
      if (is_enter && newline_action_) {
        const se::TextPosition cursor = core_->getCursorPosition();
        const std::string replacement = newline_action_(
            static_cast<uint32_t>(cursor.line), static_cast<uint32_t>(cursor.column)
        );
        if (!replacement.empty()) {
          const se::EditorActionResult result =
              core_->replaceText(core_->getSelection().normalized(), replacement);
          if (!result.text_changes.empty()) {
            text_input_client_->NotifyContentChanged(result.text_changes);
            RefreshDecorations(true);
            FireTextChanged();
          }
          model_dirty_ = true;
          if (invalidate_) {
            invalidate_();
          }
          return true;
        }
      }
      if (event.modifiers.control && event.key == Key::Space && completion_provider_) {
        TriggerCompletion(CompletionContext::TriggerKind::Invoked, "");
        return true;
      }
      // Ctrl+F (the web adapter delivers printable characters through
      // `text`; the key enum has no letter keys beyond the edit shortcuts).
      if (event.modifiers.control && event.text == "f") {
        controller_.ToggleSearch();
        return true;
      }
    }

    // The SweetEditor core resolves key chords without a down/up distinction, so
    // a KeyUp would repeat the same edit (double newline / double delete).
    if (event.type != KeyEventType::Down) {
      return false;
    }
    try {
      const se::EditorActionResult result = core_->handleKeyEvent(ToKeyEvent(event));
      if (!result.text_changes.empty()) {
        text_input_client_->NotifyContentChanged(result.text_changes);
        RefreshDecorations(true);
        FireTextChanged();
        UpdateCompletion(result.text_changes);
      } else if (result.selection_changed || result.cursor_changed) {
        // Caret moves without text changes dismiss the panel (reference manager
        // dismisses whenever the edit result carries no text changes).
        if (completion_.visible || completion_.request_active) {
          DismissCompletion();
        }
        text_input_client_->NotifySelectionChanged();
        RefreshDecorations(true);
        FireCaretEvents();
      }
      if (result.needs_redraw) {
        model_dirty_ = true;
      }
      return result.needs_redraw;
    } catch (const std::exception&) {
      return false;
    }
  }

  bool HandleScroll(const ScrollInputEvent& event) {
    const se::EditorActionResult result = core_->handleGestureEvent(ToWheelGestureEvent(event));
    if (result.selection_changed || result.cursor_changed) {
      text_input_client_->NotifySelectionChanged();
    }
    if (result.scroll_changed) {
      const se::ViewState view = core_->getViewState();
      events_.Emit<CodeEditorEvents::ScrollChanged>(view.scroll_x, view.scroll_y);
    }
    if (result.needs_redraw) {
      model_dirty_ = true;
    }
    if (result.needsAnimation()) {
      animation_pending_ = true;
      if (invalidate_) {
        invalidate_();
      }
    }
    return result.needs_redraw;
  }

  // Toggled by the legacy host path; retained frame scheduling now owns blinking.
  void TickBlink() {
    if (!focused_) {
      blink_on_ = true;
      return;
    }
    blink_on_ = !blink_on_;
    if (invalidate_) {
      invalidate_();
    }
  }

  std::shared_ptr<CodeEditorTextInputClient> TextInputClient() const {
    return text_input_client_;
  }

  bool IsFocused() const noexcept {
    return focused_;
  }

  bool HasActiveAnimation() const noexcept {
    return animation_pending_;
  }

  bool HasPendingPaintWork() const noexcept {
    return model_dirty_ || decorations_pending_;
  }

  // Whether the first non-empty viewport highlight slice has been published.
  bool HighlightPublished() const noexcept {
    return highlight_published_;
  }

  // The first frames after mount keep flowing until the initial highlight slice
  // is published (an unfocused editor schedules no follow-up frame otherwise).
  // Bounded so a permanently empty viewport cannot spin frames forever.
  bool ShouldBootstrapHighlight() noexcept {
    if (highlight_published_ || highlight_bootstrap_attempts_ >= kHighlightBootstrapFrames) {
      return false;
    }
    ++highlight_bootstrap_attempts_;
    return true;
  }

  [[nodiscard]] const CodeEditorTheme& Theme() const noexcept {
    return theme_;
  }

  // Applies a resolved theme change without resetting retained state.
  void ApplyTheme(CodeEditorTheme theme) {
    if (theme == theme_) {
      return;
    }
    theme_ = std::move(theme);
    if (core_) {
      RegisterSyntaxStyles(*core_, theme_);
      core_->setEditorRenderColors(MakeRenderColors(theme_));
      core_->setEditorRangeEffectStyles(MakeRangeEffectStyles(theme_));
    }
    if (text_input_client_) {
      text_input_client_->SetTheme(theme_);
    }
    model_dirty_ = true;
    if (invalidate_) {
      invalidate_();
    }
  }

  // ---- Decoration provider pipeline -----------------------------------------

  // Buffers incremental edits so providers receive them on the next refresh.
  void RecordPendingChanges(const std::vector<se::TextChange>& changes) {
    for (const se::TextChange& change : changes) {
      CodeEditorTextChange pending;
      pending.start_line = static_cast<uint32_t>(change.range.start.line);
      pending.start_column = static_cast<uint32_t>(change.range.start.column);
      pending.end_line = static_cast<uint32_t>(change.range.end.line);
      pending.end_column = static_cast<uint32_t>(change.range.end.column);
      pending.new_text = change.new_text;
      pending_changes_.push_back(std::move(pending));
    }
  }

  // Collects results from every registered provider and applies the merged
  // decoration set to the core.
  void RefreshDecorations(bool settled) {
    if (!core_ || !document_) {
      return;
    }
    synced_document_text_ = document_->getU8Text();
    CodeEditorDecorationContext context;
    const se::IntRange visible = core_->getVisibleLineRange();
    context.visible_start_line =
        visible.isEmpty() ? 0U : static_cast<uint32_t>(std::max(0, visible.start));
    context.visible_end_line = visible.isEmpty() ? 0U : static_cast<uint32_t>(std::max(0, visible.end));
    context.total_line_count = static_cast<uint32_t>(document_->getLineCount());
    const se::TextPosition cursor = core_->getCursorPosition();
    context.cursor_line = static_cast<uint32_t>(cursor.line);
    context.cursor_column = static_cast<uint32_t>(cursor.column);
    context.viewport_settled = settled;
    context.document_text = &synced_document_text_;
    context.text_changes = std::move(pending_changes_);
    pending_changes_.clear();

    CodeEditorDecorationResult merged;
    for (const std::shared_ptr<CodeEditorDecorationProvider>& provider : providers_) {
      if (!provider) {
        continue;
      }
      MergeDecorations(merged, provider->ProvideDecorations(context));
    }
    cached_phantom_entries_ = merged.phantom_texts;
    ApplyDecorations(*core_, merged);
    model_dirty_ = true;
    CODEEDITOR_TRACE(
        "refresh: providers=%zu spans=%zu settled=%d", providers_.size(), merged.syntax_spans.size(), settled ? 1 : 0
    );
  }

  // Whether the latest tap hit a command area (gutter icon, code lens, link,
  // inlay hint, fold control) that must not focus the editor.
  bool SuppressFocusOnPointerUp() const noexcept {
    return suppress_focus_on_up_;
  }

  bool AdvanceFrame(double timestamp) {
    bool changed = false;
    if (animation_pending_) {
      const se::EditorActionResult result = core_->tickAnimations();
      if (result.needs_redraw || result.scroll_changed || result.animation_flags != 0) {
        model_dirty_ = true;
        changed = true;
      }
      animation_pending_ = result.needsAnimation();
    }
    if (focused_ && (last_blink_timestamp_ < 0.0 || timestamp - last_blink_timestamp_ >= 0.5)) {
      blink_on_ = !blink_on_;
      last_blink_timestamp_ = timestamp;
      changed = true;
    }
    return changed;
  }

  void SetFocused(bool focused) {
    focused_ = focused;
    if (!focused) {
      blink_on_ = true;
    }
    if (invalidate_) {
      invalidate_();
    }
  }

  // ---- Editor event bus (reference EditorEventBus) -------------------------

  void FireCaretEvents() {
    const se::TextPosition cursor = core_->getCursorPosition();
    events_.Emit<CodeEditorEvents::CursorChanged>(static_cast<uint32_t>(cursor.line), static_cast<uint32_t>(cursor.column));
    const se::TextRange selection = core_->getSelection();
    events_.Emit<CodeEditorEvents::SelectionChanged>(
        static_cast<uint32_t>(selection.start.line),
        static_cast<uint32_t>(selection.start.column),
        static_cast<uint32_t>(selection.end.line),
        static_cast<uint32_t>(selection.end.column)
    );
  }

  void FirePointerEvents(const se::EditorActionResult& result) {
    switch (result.gesture_type) {
    case se::GestureType::LONG_PRESS:
      events_.Emit<CodeEditorEvents::LongPressed>(
          static_cast<uint32_t>(result.cursor_after.line), static_cast<uint32_t>(result.cursor_after.column)
      );
      ShowContextMenu(result.tap_point);
      break;
    case se::GestureType::DOUBLE_TAP:
      events_.Emit<CodeEditorEvents::DoubleTapped>(
          static_cast<uint32_t>(result.cursor_after.line), static_cast<uint32_t>(result.cursor_after.column)
      );
      ShowContextMenu(result.tap_point);
      break;
    default:
      break;
    }
  }

  void FireFoldToggle(size_t line) {
    events_.Emit<CodeEditorEvents::FoldToggled>(line);
  }

  void FireTextChanged() {
    events_.Emit<CodeEditorEvents::TextChanged>();
  }

  // Applies phantom (ghost) text for the visible lines (reference phantom text
  // decoration / copilot inline suggestion preview). Returns true when the
  // published phantom set changed (model rebuild required).
  bool ApplyPhantomTexts(int32_t start_line, int32_t end_line) {
    if (start_line < 0 || end_line < start_line) {
      return false;
    }
    std::map<uint32_t, std::string> next;
    for (const auto& [line, items] : cached_phantom_entries_) {
      if (static_cast<int32_t>(line) < start_line || static_cast<int32_t>(line) > end_line) {
        continue;
      }
      for (const CodeEditorPhantomText& phantom : items) {
        if (!phantom.text.empty()) {
          next[line] = phantom.text;
        }
      }
    }
    if (next == cached_phantom_) {
      return false;
    }
    cached_phantom_ = std::move(next);
    if (cached_phantom_.empty()) {
      core_->clearPhantomTexts();
      return true;
    }
    std::vector<std::pair<size_t, std::vector<se::PhantomText>>> entries;
    for (const auto& [line, text] : cached_phantom_) {
      const uint32_t column = document_->getLineColumns(line);
      entries.push_back({line, {{column, text}}});
    }
    core_->setBatchLinePhantomTexts(std::move(entries));
    return true;
  }

  // Single Tab handler shared by the direct component key path and the
  // text-input key hook: advance a snippet's linked editing, accept the
  // caret-line phantom text, move the completion selection, or insert
  // indentation through the core. Tab is always consumed while the editor is
  // focused so the runtime's focus traversal never steals it.
  bool HandleTab() {
    if (core_->isInLinkedEditing()) {
      const se::EditorActionResult result = core_->linkedEditingNextTabStop();
      if (result.selection_changed || result.cursor_changed) {
        text_input_client_->NotifySelectionChanged();
        model_dirty_ = true;
        if (invalidate_) {
          invalidate_();
        }
        return true;
      }
    }
    if (AcceptPhantomText()) {
      return true;
    }
    if (completion_.visible && !completion_.items.empty()) {
      MoveCompletionSelection(1);
      return true;
    }
    // Plain Tab inserts indentation (spaces up to the tab stop when
    // insert_spaces is on); run the core's own key handling for it.
    se::KeyEvent tab_event;
    tab_event.key_code = se::KeyCode::TAB;
    const se::EditorActionResult result = core_->handleKeyEvent(tab_event);
    if (!result.text_changes.empty()) {
      text_input_client_->NotifyContentChanged(result.text_changes);
      RefreshDecorations(true);
      FireTextChanged();
      UpdateCompletion(result.text_changes);
    } else if (result.selection_changed || result.cursor_changed) {
      text_input_client_->NotifySelectionChanged();
    }
    model_dirty_ = true;
    if (invalidate_) {
      invalidate_();
    }
    return true;
  }

  // Commits the caret-line phantom text (copilot accept via Tab).
  bool AcceptPhantomText() {
    if (!accept_phantom_on_tab_) {
      return false;
    }
    const se::TextPosition cursor = core_->getCursorPosition();
    std::string text;
    for (const auto& [line, items] : cached_phantom_entries_) {
      if (line != static_cast<uint32_t>(cursor.line)) {
        continue;
      }
      for (const CodeEditorPhantomText& phantom : items) {
        if (!phantom.text.empty()) {
          text = phantom.text;
        }
      }
    }
    if (text.empty()) {
      return false;
    }
    const se::EditorActionResult result = core_->insertText(text);
    if (!result.text_changes.empty()) {
      text_input_client_->NotifyContentChanged(result.text_changes);
      RefreshDecorations(true);
      FireTextChanged();
    }
    model_dirty_ = true;
    if (invalidate_) {
      invalidate_();
    }
    return true;
  }

  // ---- Scrollbar thumb dragging (P3) ---------------------------------------

  bool HitScrollbarThumb(const Point& position, bool& out_vertical) const {
    const auto hit = [&position](const se::ScrollbarModel& bar) {
      const se::Rect& thumb = bar.thumb;
      return position.x >= thumb.origin.x && position.x <= thumb.origin.x + thumb.width &&
             position.y >= thumb.origin.y && position.y <= thumb.origin.y + thumb.height;
    };
    if (cached_scrollbar_v_.visible && cached_scrollbar_v_.thumb.width > 0.0F && hit(cached_scrollbar_v_)) {
      out_vertical = true;
      return true;
    }
    if (cached_scrollbar_h_.visible && cached_scrollbar_h_.thumb.height > 0.0F && hit(cached_scrollbar_h_)) {
      out_vertical = false;
      return true;
    }
    return false;
  }

  void DragScrollbar(const Point& position) {
    const se::ScrollMetrics metrics = core_->getScrollMetrics();
    const se::ScrollbarModel& bar = scrollbar_drag_vertical_ ? cached_scrollbar_v_ : cached_scrollbar_h_;
    const se::Rect track = bar.track;
    const se::Rect thumb = bar.thumb;
    const float track_extent = scrollbar_drag_vertical_ ? track.height : track.width;
    const float thumb_extent = scrollbar_drag_vertical_ ? thumb.height : thumb.width;
    const float pointer =
        scrollbar_drag_vertical_ ? position.y - track.origin.y : position.x - track.origin.x;
    const float max_scroll = scrollbar_drag_vertical_ ? metrics.max_scroll_y : metrics.max_scroll_x;
    const float movable = std::max(1.0F, track_extent - thumb_extent);
    const float ratio = std::clamp((pointer - scrollbar_drag_offset_) / movable, 0.0F, 1.0F);
    const float scroll_x = scrollbar_drag_vertical_ ? metrics.scroll_x : ratio * max_scroll;
    const float scroll_y = scrollbar_drag_vertical_ ? ratio * max_scroll : metrics.scroll_y;
    const se::EditorActionResult result = core_->setScroll(scroll_x, scroll_y);
    if (result.needs_redraw) {
      model_dirty_ = true;
      if (invalidate_) {
        invalidate_();
      }
    }
  }

  // Reads the link text under a hit column from the document line (the core
  // reports the hit position; the URL continues to the next delimiter).
  std::string LinkTextAt(size_t line, size_t column) const {
    const se::U16String source_text = document_->getLineU16Text(line);
    const std::u16string_view text(reinterpret_cast<const char16_t*>(source_text.data()), source_text.size());
    if (column >= text.size()) {
      return {};
    }
    size_t end = column;
    while (end < text.size()) {
      const char16_t ch = text[end];
      if (ch == u' ' || ch == u'\t' || ch == u'(' || ch == u')' || ch == u'"' || ch == u'\'' || ch == u';' ||
          ch == u',' || ch == u'<' || ch == u'>' || ch == u'{' || ch == u'}') {
        break;
      }
      ++end;
    }
    return Utf16ToUtf8(text.substr(column, end - column));
  }

  void Render(PaintContext& paint, Size size) {
    const se::Size viewport{size.width, size.height};
    if (viewport_.width != viewport.width || viewport_.height != viewport.height) {
      viewport_ = viewport;
      core_->setViewport(viewport);
      // A viewport change (e.g. IME insets resizing the editor) invalidates the
      // visible range and scrollbar geometry; force a rebuild. Refresh the
      // decorations BEFORE the rebuild so the newly revealed slice paints with
      // its highlighting in this very frame — rebuilding first would render
      // one unhighlighted frame and repaint after (visible flash every time
      // the keyboard opens or closes).
      RefreshDecorations(false);
      const se::IntRange resized = core_->getVisibleLineRange();
      if (!resized.isEmpty()) {
        highlight_published_ = true;
      }
      last_visible_range_ = resized;
      decorations_pending_ = true;
      model_dirty_ = true;
      if (invalidate_) {
        invalidate_();
      }
    }

    // Rebuild the render model first: buildRenderModel also refreshes the
    // core's visible line range, so the viewport slice below is always current.
    if (model_dirty_) {
      // buildRenderModel appends into the model, so a fresh (empty) model is
      // required each rebuild; reusing the cached instance would accumulate
      // stale lines/decorations and ghost on every update.
      cached_model_ = se::EditorRenderModel{};
      core_->buildRenderModel(cached_model_);
      model_dirty_ = false;
    }
    const se::EditorRenderModel& model = cached_model_;

    // Re-publish the viewport highlight slice when the visible range changed
    // (scroll) or on the very first non-empty viewport. Syntax highlighting and
    // indent guides are applied immediately; decorations/document highlights are
    // deferred until the viewport settles so fast scrolling does not pay their
    // per-frame cost. Publishing marks the model dirty so the next frame
    // rebuilds with the spans; the OnFrame bootstrap keeps frames flowing until
    // that first publish lands.
    const se::IntRange visible = core_->getVisibleLineRange();
    const bool first_highlight = !highlight_published_ && !visible.isEmpty();
    if (first_highlight || visible.start != last_visible_range_.start || visible.end != last_visible_range_.end) {
      if (first_highlight || !visible.isEmpty()) {
        RefreshDecorations(false);
        if (first_highlight) {
          highlight_published_ = true;
        }
        decorations_pending_ = true;
        model_dirty_ = true;
        // The model above was built before these decorations landed; request a
        // follow-up frame so the highlighted rebuild is guaranteed even when
        // the bounded bootstrap has already exhausted its frame budget.
        if (invalidate_) {
          invalidate_();
        }
      }
      last_visible_range_ = visible;
    } else if (decorations_pending_) {
      RefreshDecorations(true);
      decorations_pending_ = false;
      model_dirty_ = true;
      if (invalidate_) {
        invalidate_();
      }
    }

    RenderModel(paint, size, model);

    cached_scrollbar_v_ = model.vertical_scrollbar;
    cached_scrollbar_h_ = model.horizontal_scrollbar;

    if (!visible.isEmpty() && !decorations_pending_ && !cached_phantom_entries_.empty()) {
      if (ApplyPhantomTexts(visible.start, visible.end)) {
        model_dirty_ = true;
      }
    }

    // The completion panel floats above the document, anchored to the caret
    // (reference: cached cursor rect updated every frame in onDraw).
    if (completion_.visible && !completion_.items.empty()) {
      if (model.cursor.visible) {
        completion_.last_cursor = model.cursor.position;
        completion_.last_cursor_height = model.cursor.height;
      }
      const se::Rect panel = ComputeCompletionPanelRect(completion_.last_cursor, completion_.last_cursor_height);
      completion_.panel_rect = panel;
      DrawCompletionPanel(paint, panel);
    }

    // The context menu floats above everything, drawn last.
    DrawContextMenu(paint);
  }

private:
  static Rect ToRect(const se::Rect& rect) {
    return Rect{rect.origin.x, rect.origin.y, rect.width, rect.height};
  }

  float LineHeight(const se::EditorRenderModel& model) const {
    return model.cursor.height > 0.0F ? model.cursor.height : font_metrics_.LineHeight();
  }

  void RenderModel(PaintContext& paint, Size size, const se::EditorRenderModel& model) {
    const float width = size.width;
    const float height = size.height;
    const float ascent = font_metrics_.ascent;
    const float line_height = ascent + font_metrics_.descent;

    paint.DrawRect(Rect{0.0F, 0.0F, width, height}, theme_.background);

    // The gutter (line-number column) sits on its own background strip; the
    // core crops text runs with a margin that hides under this strip, so it
    // must be painted or cropped characters bleed into the line-number area.
    if (model.split_x > 0.0F) {
      paint.DrawRect(Rect{0.0F, 0.0F, model.split_x, height}, theme_.gutter_background);
      // The split line separates the gutter from the text area (reference
      // renderer draws it when splitLineVisible is set).
      if (model.split_line_visible) {
        paint.DrawRect(Rect{model.split_x - 1.0F, 0.0F, 1.0F, height}, Argb(0xFFE3E5E8));
      }
    }

    if (model.current_line_render_mode == se::CurrentLineRenderMode::BACKGROUND) {
      paint.DrawRect(Rect{0.0F, model.current_line.y, width, LineHeight(model)}, theme_.current_line_background);
    }

    for (const se::RangeEffectRenderItem& effect : model.range_effects) {
      if (effect.style.background_color != 0) {
        paint.DrawRect(ToRect(effect.rect), Argb(effect.style.background_color));
      }
    }

    const TextStyle line_number_style{Font::Monospace(font_size_), theme_.line_number_color, TextDecoration::None};
    for (const se::VisualLine& line : model.lines) {
      // line_number_position.y is the text baseline; the row background spans
      // from the line top (baseline - ascent) down the full line height.
      if (line.line_background_color != 0) {
        paint.DrawRect(Rect{model.split_x, line.line_number_position.y - ascent, width - model.split_x, line_height},
                       Argb(line.line_background_color));
      }
      if (line.line_number >= 0) {
        const std::string number = std::to_string(line.line_number);
        const float number_width = char_width_ * static_cast<float>(number.size());
        paint.DrawTextRun(
            Rect{line.line_number_position.x, line.line_number_position.y - ascent, number_width, line_height},
            Point{line.line_number_position.x, line.line_number_position.y},
            number,
            line_number_style);
      }
    }
    // Clip everything that lives in the text area right of the gutter: runs,
    // structure guides (bracket/indent connectors), and range-effect
    // underlines (diagnostics/IME squiggles). Gutter icons and fold markers
    // are painted outside the clip because they belong to the gutter lane.
    if (model.split_x < width) {
      paint.PushClip(Rect{model.split_x, 0.0F, width - model.split_x, height});
      for (const se::VisualLine& line : model.lines) {
        for (const se::VisualRun& run : line.runs) {
          DrawRun(paint, run, line_height);
        }
      }
      for (const se::GuideSegment& segment : model.guide_segments) {
        DrawGuideSegment(paint, segment);
      }
      for (const se::RangeEffectRenderItem& effect : model.range_effects) {
        if (effect.style.underline_color != 0 && effect.style.underline_style != se::RangeEffectUnderlineStyle::NONE) {
          DrawUnderline(paint, effect.rect, effect.style.underline_color, effect.style.underline_style);
        }
      }
      paint.PopClip();
    } else {
      for (const se::VisualLine& line : model.lines) {
        for (const se::VisualRun& run : line.runs) {
          DrawRun(paint, run, line_height);
        }
      }
      for (const se::GuideSegment& segment : model.guide_segments) {
        DrawGuideSegment(paint, segment);
      }
      for (const se::RangeEffectRenderItem& effect : model.range_effects) {
        if (effect.style.underline_color != 0 && effect.style.underline_style != se::RangeEffectUnderlineStyle::NONE) {
          DrawUnderline(paint, effect.rect, effect.style.underline_color, effect.style.underline_style);
        }
      }
    }

    for (const se::GutterIconRenderItem& icon : model.gutter_icons) {
      DrawGutterIcon(paint, icon);
    }
    for (const se::FoldMarkerRenderItem& marker : model.fold_markers) {
      DrawFoldMarker(paint, marker);
    }

    const bool caret_visible = model.cursor.visible && focused_ && blink_on_;
    if (caret_visible) {
      paint.DrawRect(
          Rect{model.cursor.position.x, model.cursor.position.y, theme_.caret_width, model.cursor.height},
          theme_.caret_color);
    }

    // Handles are drawn here (single owner) because the HuxerUI selection
    // overlay is disabled; the core's gesture pipeline handles dragging.
    if (model.selection_start_handle.visible) {
      DrawSelectionHandle(paint, model.selection_start_handle);
    }
    if (model.selection_end_handle.visible) {
      DrawSelectionHandle(paint, model.selection_end_handle);
    }

    if (model.vertical_scrollbar.visible) {
      DrawScrollbar(paint, model.vertical_scrollbar);
    }
    if (model.horizontal_scrollbar.visible) {
      DrawScrollbar(paint, model.horizontal_scrollbar);
    }
  }

  void DrawSelectionHandle(PaintContext& paint, const se::SelectionHandle& handle) {
    const float radius = 6.0F;
    const float cx = handle.position.x;
    // getPositionScreenCoord resolves the logical line's line_number_position.y,
    // which is the line TOP (screen y, scroll already applied) — not the text
    // baseline. The stem spans the full line height; the teardrop sits below
    // the line bottom (reference drag-handle anchor).
    const float line_top = handle.position.y;
    const float line_bottom = handle.position.y + handle.height;
    paint.DrawRect(Rect{cx - 1.0F, line_top, 2.0F, std::max(1.0F, line_bottom - line_top)}, Argb(0xFF1F6FEB));
    paint.DrawCircle(Point{cx, line_bottom + radius * 0.8F}, radius, Argb(0xFF1F6FEB));
  }

  void DrawScrollbar(PaintContext& paint, const se::ScrollbarModel& bar) {
    const Rect thumb = ToRect(bar.thumb);
    if (thumb.width <= 0.0F || thumb.height <= 0.0F) {
      return;
    }
    paint.DrawRect(thumb, Color{0.0F, 0.0F, 0.0F, std::clamp(bar.alpha, 0.0F, 1.0F) * 0.45F}, CornerRadii(4.0F));
  }

  void DrawRun(PaintContext& paint, const se::VisualRun& run, float line_height) {
    const float ascent = font_metrics_.ascent;
    const float top = run.y - ascent;
    const float height = ascent + font_metrics_.descent;

    if (run.type == se::VisualRunType::INLAY_HINT) {
      DrawInlayHint(paint, run, top, height);
      return;
    }
    if (run.type == se::VisualRunType::FOLD_PLACEHOLDER) {
      DrawFoldPlaceholder(paint, run, top, height);
      return;
    }
    if (run.type == se::VisualRunType::WHITESPACE) {
      // Space marker: a faint dot centered on the glyph cell.
      const float center_y = run.y - ascent * 0.35F;
      const float spacing = std::max(2.0F, char_width_ * 0.5F);
      for (float x = run.x + spacing; x < run.x + run.width; x += char_width_) {
        paint.DrawCircle(Point{x, center_y}, 1.2F, Color{0.0F, 0.0F, 0.0F, 0.25F});
      }
      return;
    }
    if (run.type == se::VisualRunType::TAB) {
      // Tab marker: a right arrow at the glyph start.
      const float mid_y = run.y - ascent * 0.5F;
      const float x = run.x + 2.0F;
      Path path;
      path.MoveTo(Point{x, mid_y});
      path.LineTo(Point{x + char_width_ * 0.6F, mid_y});
      path.LineTo(Point{x + char_width_ * 0.45F, mid_y - 3.0F});
      path.MoveTo(Point{x + char_width_ * 0.6F, mid_y});
      path.LineTo(Point{x + char_width_ * 0.45F, mid_y + 3.0F});
      paint.StrokePath(path, Color{0.0F, 0.0F, 0.0F, 0.3F}, StrokeStyle{1.0F});
      return;
    }
    if (run.type == se::VisualRunType::NEWLINE) {
      // Line-break symbol: a left-pointing hook like the reference renderer.
      const float mid_y = run.y - ascent * 0.5F;
      const float x = run.x + 2.0F;
      Path path;
      path.MoveTo(Point{x, mid_y});
      path.LineTo(Point{x + 8.0F, mid_y});
      path.LineTo(Point{x + 8.0F, mid_y + 4.0F});
      path.LineTo(Point{x + 4.0F, mid_y + 4.0F});
      paint.StrokePath(path, Color{0.0F, 0.0F, 0.0F, 0.3F}, StrokeStyle{1.0F});
      return;
    }
    if (run.text.empty()) {
      return;
    }

    if (run.style.background_color != 0) {
      paint.DrawRect(Rect{run.x, top, run.width, height}, Argb(run.style.background_color));
    }

    // HuxerUI text runs must not contain line breaks; strip any that slip
    // through (for example multi-line snippet expansion) defensively.
    std::string text = Utf16ToUtf8(run.text);
    const size_t line_break = text.find_first_of("\r\n");
    if (line_break != std::string::npos) {
      text.resize(line_break);
    }

    if (run.type == se::VisualRunType::PHANTOM_TEXT) {
      TextStyle phantom{Font::Monospace(font_size_).WithSlant(FontSlant::Italic), Color{0.6F, 0.6F, 0.6F, 0.6F},
                        TextDecoration::None};
      paint.DrawTextRun(Rect{run.x, top, run.width, height}, Point{run.x, run.y}, text, phantom);
      return;
    }

    const TextStyle style = MakeTextStyle(run.style.font_style, run.style.color, font_size_);
    paint.DrawTextRun(Rect{run.x, top, run.width, height}, Point{run.x, run.y}, text, style);
  }

  void DrawInlayHint(PaintContext& paint, const se::VisualRun& run, float top, float height) {
    const float margin = run.margin;
    if (run.color_value != 0) {
      paint.DrawRect(Rect{run.x + margin, top, height, height}, Argb(run.color_value));
      return;
    }
    const Rect background{run.x + margin, top, run.width - margin * 2.0F, height};
    paint.DrawRect(background, theme_.inlay_hint_background, CornerRadii(height * 0.2F));
    if (!run.text.empty()) {
      const float text_x = run.x + margin + run.padding;
      const TextStyle style{Font::System(font_size_ * 0.9F), theme_.inlay_hint_text, TextDecoration::None};
      paint.DrawTextRun(Rect{text_x, top, run.width, height}, Point{text_x, run.y}, Utf16ToUtf8(run.text), style);
    }
  }

  void DrawFoldPlaceholder(PaintContext& paint, const se::VisualRun& run, float top, float height) {
    const float margin = run.margin;
    const Rect background{run.x + margin, top, run.width - margin * 2.0F, height};
    paint.DrawRect(background, theme_.fold_placeholder_background, CornerRadii(height * 0.2F));
    if (!run.text.empty()) {
      const float text_x = run.x + margin + run.padding;
      const TextStyle style{Font::Monospace(font_size_), theme_.fold_placeholder_text, TextDecoration::None};
      paint.DrawTextRun(Rect{text_x, top, run.width, height}, Point{text_x, run.y}, Utf16ToUtf8(run.text), style);
    }
  }

  void DrawGutterIcon(PaintContext& paint, const se::GutterIconRenderItem& icon) {
    const Rect rect = ToRect(icon.rect);
    const Point center{rect.x + rect.width * 0.5F, rect.y + rect.height * 0.5F};
    // icon_id drives the shape: 1 = type marker (diamond), 2 = execution point
    // (filled circle), anything else = plain dot. The demo emits 1 for
    // class/struct rows; hosts may reuse the ids for breakpoints/errors.
    if (icon.icon_id == 1) {
      Path diamond;
      diamond.MoveTo(Point{center.x, rect.y});
      diamond.LineTo(Point{rect.x + rect.width, center.y});
      diamond.LineTo(Point{center.x, rect.y + rect.height});
      diamond.LineTo(Point{rect.x, center.y});
      diamond.Close();
      paint.StrokePath(diamond, theme_.gutter_icon_color, StrokeStyle{1.5F});
      return;
    }
    if (icon.icon_id == 2) {
      paint.DrawCircle(center, rect.width * 0.5F, theme_.gutter_icon_color);
      return;
    }
    paint.DrawCircle(center, rect.width * 0.35F, theme_.gutter_icon_color);
  }

  void DrawFoldMarker(PaintContext& paint, const se::FoldMarkerRenderItem& marker) {
    const Rect rect = ToRect(marker.rect);
    Path path;
    if (marker.fold_state == se::FoldState::COLLAPSED) {
      path.MoveTo(Point{rect.x + rect.width * 0.35F, rect.y + rect.height * 0.2F});
      path.LineTo(Point{rect.x + rect.width * 0.75F, rect.y + rect.height * 0.5F});
      path.LineTo(Point{rect.x + rect.width * 0.35F, rect.y + rect.height * 0.8F});
    } else {
      path.MoveTo(Point{rect.x + rect.width * 0.2F, rect.y + rect.height * 0.35F});
      path.LineTo(Point{rect.x + rect.width * 0.5F, rect.y + rect.height * 0.75F});
      path.LineTo(Point{rect.x + rect.width * 0.8F, rect.y + rect.height * 0.35F});
    }
    paint.StrokePath(path, theme_.line_number_color, StrokeStyle{1.5F});
  }

  void DrawUnderline(
      PaintContext& paint, const se::Rect& rect, int32_t color, se::RangeEffectUnderlineStyle style
  ) {
    const Rect r = ToRect(rect);
    const float baseline = r.y + r.height - 1.0F;
    if (style == se::RangeEffectUnderlineStyle::WAVY) {
      Path path;
      constexpr float kAmplitude = 1.5F;
      constexpr float kWavelength = 5.0F;
      path.MoveTo(Point{r.x, baseline});
      bool up = true;
      for (float x = r.x + kWavelength; x < r.x + r.width; x += kWavelength) {
        path.LineTo(Point{x, baseline + (up ? -kAmplitude : kAmplitude)});
        up = !up;
      }
      path.LineTo(Point{r.x + r.width, baseline + (up ? -kAmplitude : kAmplitude)});
      paint.StrokePath(path, Argb(color), StrokeStyle{1.0F});
      return;
    }
    if (style == se::RangeEffectUnderlineStyle::DASHED) {
      constexpr float kDash = 4.0F;
      constexpr float kGap = 3.0F;
      float x = r.x;
      while (x < r.x + r.width) {
        const float end = std::min(x + kDash, r.x + r.width);
        paint.DrawRect(Rect{x, baseline, end - x, 1.5F}, Argb(color));
        x = end + kGap;
      }
      return;
    }
    paint.DrawRect(Rect{r.x, baseline, r.width, 1.5F}, Argb(color));
  }

  void DrawGuideSegment(PaintContext& paint, const se::GuideSegment& segment) {
    const Color color =
        segment.type == se::GuideType::SEPARATOR ? theme_.separator_color : theme_.indent_guide_color;
    // Separator and bracket lines are lighter than indent guides (reference
    // renderer uses a separate paint for separators).
    const float width = segment.type == se::GuideType::SEPARATOR ? 1.0F : 1.0F;
    const auto stroke = [&](const Point& a, const Point& b) {
      Path path;
      path.MoveTo(a);
      path.LineTo(b);
      paint.StrokePath(path, color, StrokeStyle{width});
    };

    // DOUBLE style draws a pair of parallel lines straddling the segment.
    if (segment.style == se::GuideStyle::DOUBLE) {
      constexpr float kOffset = 1.5F;
      if (segment.direction == se::GuideDirection::HORIZONTAL) {
        stroke({segment.start.x, segment.start.y - kOffset}, {segment.end.x, segment.end.y - kOffset});
        stroke({segment.start.x, segment.start.y + kOffset}, {segment.end.x, segment.end.y + kOffset});
      } else {
        stroke({segment.start.x - kOffset, segment.start.y}, {segment.end.x - kOffset, segment.end.y});
        stroke({segment.start.x + kOffset, segment.start.y}, {segment.end.x + kOffset, segment.end.y});
      }
      return;
    }

    // DASHED style: short dashes along the segment (used by separator guides).
    if (segment.style == se::GuideStyle::DASHED) {
      const float dx = segment.end.x - segment.start.x;
      const float dy = segment.end.y - segment.start.y;
      const float length = std::hypot(dx, dy);
      if (length <= 0.0F) {
        return;
      }
      constexpr float kDash = 6.0F;
      constexpr float kGap = 4.0F;
      const float ux = dx / length;
      const float uy = dy / length;
      float distance = 0.0F;
      while (distance < length) {
        const float end = std::min(distance + kDash, length);
        stroke(
            {segment.start.x + ux * distance, segment.start.y + uy * distance},
            {segment.start.x + ux * end, segment.start.y + uy * end}
        );
        distance = end + kGap;
      }
      return;
    }

    // Solid line; when the segment carries an arrow, draw the shaft plus a
    // small arrowhead at the end (flow/return guides).
    if (segment.arrow_end) {
      constexpr float kArrowLength = 8.0F;
      constexpr float kArrowAngle = 0.49F;  // ~28 degrees
      const float dx = segment.end.x - segment.start.x;
      const float dy = segment.end.y - segment.start.y;
      const float length = std::hypot(dx, dy);
      if (length <= kArrowLength) {
        return;
      }
      const float ratio = (length - kArrowLength) / length;
      const Point shaft_end{
          segment.start.x + dx * ratio,
          segment.start.y + dy * ratio,
      };
      stroke({segment.start.x, segment.start.y}, shaft_end);
      // Arrowhead: two short lines back from the tip at ±angle.
      const float ux = dx / length;
      const float uy = dy / length;
      const float cos_a = std::cos(kArrowAngle);
      const float sin_a = std::sin(kArrowAngle);
      // Left wing: rotate (ux, uy) by -angle.
      const float left_x = segment.end.x - kArrowLength * (ux * cos_a + uy * sin_a);
      const float left_y = segment.end.y - kArrowLength * (-ux * sin_a + uy * cos_a);
      stroke(shaft_end, {left_x, left_y});
      // Right wing: rotate (ux, uy) by +angle.
      const float right_x = segment.end.x - kArrowLength * (ux * cos_a - uy * sin_a);
      const float right_y = segment.end.y - kArrowLength * (ux * sin_a + uy * cos_a);
      stroke(shaft_end, {right_x, right_y});
      return;
    }

    stroke({segment.start.x, segment.start.y}, {segment.end.x, segment.end.y});
  }

  // ---- Code completion ----------------------------------------------------

  static bool IsCompletionWordChar(char16_t ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' ||
           ch > 0x7F;
  }

  bool IsTriggerCharacter(const std::string& ch) const {
    return completion_trigger_characters_ && completion_trigger_characters_(ch);
  }

  void UpdateCompletion(const std::vector<se::TextChange>& changes) {
    FireTextChanged();
    if (!completion_provider_) {
      return;
    }
    const bool completion_active = completion_.request_active;
    // The reference manager dismisses when the result carries no text change
    // or more than one (a single character edit is the only live trigger).
    if (changes.empty() || changes.size() != 1) {
      DismissCompletion();
      return;
    }
    const std::string& new_text = changes.front().new_text;
    if (new_text.size() == 1 && IsTriggerCharacter(new_text)) {
      TriggerCompletion(CompletionContext::TriggerKind::Character, new_text);
    } else if (completion_active || new_text.size() == 1) {
      // Any printable character re-requests while active, and also opens the
      // panel from scratch so typing an identifier (for example `for`) shows
      // matching candidates immediately.
      TriggerCompletion(CompletionContext::TriggerKind::Retrigger, "");
    } else {
      completion_.request_active = false;
      if (completion_.visible) {
        DismissCompletion();
      }
    }
  }

  void TriggerCompletion(CompletionContext::TriggerKind kind, const std::string& trigger_character) {
    completion_.request_active = true;
    completion_.last_trigger_kind = kind;
    completion_.last_trigger_char = trigger_character;
    ExecuteCompletionRefresh();
  }

  void DismissCompletion() {
    if (!completion_.visible && !completion_.request_active) {
      return;
    }
    completion_.visible = false;
    completion_.request_active = false;
    completion_.items.clear();
    if (invalidate_) {
      invalidate_();
    }
  }

  void ExecuteCompletionRefresh() {
    if (!completion_.request_active || !completion_provider_) {
      DismissCompletion();
      return;
    }
    const CompletionContext context = BuildCompletionContext(completion_.last_trigger_kind, completion_.last_trigger_char);
    std::vector<CompletionItem> items = completion_provider_(context);
    std::stable_sort(items.begin(), items.end(), [](const CompletionItem& a, const CompletionItem& b) {
      const std::string& key_a = !a.sort_key.empty() ? a.sort_key : a.label;
      const std::string& key_b = !b.sort_key.empty() ? b.sort_key : b.label;
      return key_a < key_b;
    });
    if (items.empty()) {
      DismissCompletion();
      return;
    }
    completion_.items = std::move(items);
    completion_.selected = 0;
    completion_.visible = true;
    if (invalidate_) {
      invalidate_();
    }
  }

  CompletionContext BuildCompletionContext(CompletionContext::TriggerKind kind, const std::string& trigger_character) {
    CompletionContext context;
    context.trigger_kind = kind;
    context.trigger_character = trigger_character;
    const se::TextPosition cursor = core_->getCursorPosition();
    context.cursor_line = static_cast<uint32_t>(cursor.line);
    context.cursor_column = static_cast<uint32_t>(cursor.column);
    const se::U16String source_line = document_->getLineU16Text(cursor.line);
    const std::u16string_view line(reinterpret_cast<const char16_t*>(source_line.data()), source_line.size());
    context.line_text = Utf16ToUtf8(line);
    size_t start = cursor.column;
    while (start > 0 && IsCompletionWordChar(line[start - 1])) {
      --start;
    }
    size_t end = cursor.column;
    while (end < line.size() && IsCompletionWordChar(line[end])) {
      ++end;
    }
    context.word_start = static_cast<uint32_t>(start);
    context.word_end = static_cast<uint32_t>(end);
    return context;
  }

  void ConfirmCompletion() {
    if (!completion_.visible || completion_.selected >= completion_.items.size()) {
      return;
    }
    const CompletionItem item = completion_.items[completion_.selected];
    DismissCompletion();

    const std::string text = !item.insert_text.empty() ? item.insert_text : item.label;
    const bool is_snippet = item.insert_text_is_snippet;
    se::EditorActionResult result;
    try {
      if (item.has_text_edit) {
        const se::TextPosition cursor = core_->getCursorPosition();
        const std::string edit_text = !item.text_edit_text.empty() ? item.text_edit_text : text;
        const se::TextRange range{{cursor.line, item.text_edit_start}, {cursor.line, item.text_edit_end}};
        if (is_snippet) {
          // Reference behavior: clear the replacement range first, then expand
          // the snippet template from the caret.
          result = core_->replaceText(range, "");
          if (!result.text_changes.empty() && !edit_text.empty()) {
            result = core_->insertSnippet(edit_text);
          }
        } else {
          result = core_->replaceText(range, edit_text);
        }
      } else if (is_snippet) {
        result = core_->insertSnippet(text);
      } else {
        result = core_->insertText(text);
      }
    } catch (const std::exception&) {
      return;
    }
    if (!result.text_changes.empty()) {
      text_input_client_->NotifyContentChanged(result.text_changes);
      model_dirty_ = true;
    }
  }

  void MoveCompletionSelection(int delta) {
    if (!completion_.visible || completion_.items.empty()) {
      return;
    }
    const int count = static_cast<int>(completion_.items.size());
    const int next = std::max(0, std::min(count - 1, static_cast<int>(completion_.selected) + delta));
    if (static_cast<size_t>(next) != completion_.selected) {
      completion_.selected = static_cast<size_t>(next);
      if (invalidate_) {
        invalidate_();
      }
    }
  }

  bool HitCompletionPanel(const Point& position, size_t& out_row) const {
    if (!completion_.visible || completion_.items.empty()) {
      return false;
    }
    const se::Rect& rect = completion_.panel_rect;
    if (position.x < rect.origin.x || position.x >= rect.origin.x + rect.width || position.y < rect.origin.y ||
        position.y >= rect.origin.y + rect.height) {
      return false;
    }
    const float row = (position.y - rect.origin.y - kCompletionPanelPaddingV) / kCompletionRowHeight;
    if (row < 0.0F) {
      return false;
    }
    const size_t index = static_cast<size_t>(row);
    if (index >= completion_.items.size()) {
      return false;
    }
    out_row = index;
    return true;
  }

  se::Rect ComputeCompletionPanelRect(const se::PointF& cursor, float cursor_height) const {
    const float width = std::min(
        kCompletionPanelWidth,
        std::max(kCompletionPanelMinWidth, viewport_.width - kCompletionFramePadding * 2.0F));
    const size_t rows = std::min(completion_.items.size(), kCompletionMaxVisible);
    const float height = kCompletionRowHeight * static_cast<float>(rows) + kCompletionPanelPaddingV * 2.0F;
    const float x = std::max(
        kCompletionFramePadding,
        std::min(cursor.x, std::max(kCompletionFramePadding, viewport_.width - kCompletionFramePadding - width)));
    float y = cursor.y + cursor_height + kCompletionGap;
    if (y + height > viewport_.height - kCompletionFramePadding) {
      // Not enough room below the caret: place the panel above it.
      y = cursor.y - kCompletionGap - height;
      if (y < kCompletionFramePadding) {
        y = kCompletionFramePadding;
      }
    }
    return se::Rect{{x, y}, width, height};
  }

  void DrawCompletionPanel(PaintContext& paint, const se::Rect& rect) {
    const Rect panel{rect.origin.x, rect.origin.y, rect.width, rect.height};
    paint.DrawRect(panel, theme_.completion_background, CornerRadii(12.0F));
    paint.DrawBorder(panel, theme_.completion_border, StrokeStyle{1.0F}, CornerRadii(12.0F));

    const float content_left = rect.origin.x + kCompletionPanelPaddingH + kCompletionRowPaddingH;
    float row_center_y = rect.origin.y + kCompletionPanelPaddingV + kCompletionRowHeight * 0.5F;
    const size_t rows = std::min(completion_.items.size(), kCompletionMaxVisible);
    for (size_t i = 0; i < rows; ++i) {
      const CompletionItem& item = completion_.items[i];
      const bool selected = (i == completion_.selected);
      if (selected) {
        paint.DrawRect(
            Rect{rect.origin.x + kCompletionPanelPaddingH, row_center_y - kCompletionRowHeight * 0.5F,
                 rect.width - kCompletionPanelPaddingH * 2.0F, kCompletionRowHeight},
            theme_.completion_selected_background,
            CornerRadii(6.0F));
      }

      // Kind badge: 18x18 rounded square with a single bold letter.
      const float badge_center_y = row_center_y;
      const float badge_x = content_left;
      paint.DrawRect(
          Rect{badge_x, badge_center_y - kCompletionBadgeSize * 0.5F, kCompletionBadgeSize, kCompletionBadgeSize},
          Argb(CompletionKindColor(item.kind)),
          CornerRadii(4.0F));
      const std::string letter = CompletionKindLetter(item.kind);
      const TextStyle badge_style{Font::System(kCompletionBadgeSizePx).WithWeight(FontWeight::Bold), Argb(0xFFFFFFFF),
                                  TextDecoration::None};
      const float badge_baseline = CenterBaseline(badge_center_y, completion_badge_metrics_);
      const float badge_text_x = badge_x + (kCompletionBadgeSize - completion_badge_char_advance_) * 0.5F;
      paint.DrawTextRun(
          Rect{badge_text_x, badge_center_y - kCompletionBadgeSize * 0.5F, kCompletionBadgeSize, kCompletionBadgeSize},
          Point{badge_text_x, badge_baseline},
          letter,
          badge_style);

      // Label, then a right-aligned detail when space allows.
      const float label_x = content_left + kCompletionBadgeSize + kCompletionBadgeGap;
      const float label_baseline = CenterBaseline(badge_center_y, completion_label_metrics_);
      paint.DrawTextRun(
          Rect{label_x, row_center_y - kCompletionRowHeight * 0.5F,
               rect.width - (label_x - rect.origin.x) - kCompletionRowPaddingH, kCompletionRowHeight},
          Point{label_x, label_baseline},
          item.label,
          TextStyle{Font::System(kCompletionLabelSize), theme_.completion_label, TextDecoration::None});

      if (!item.detail.empty()) {
        // Estimated advance (reference renders an 11sp detail at the row end).
        const float detail_advance = kCompletionDetailSize * 0.55F * static_cast<float>(item.detail.size());
        const float detail_right = rect.origin.x + rect.width - kCompletionPanelPaddingH - kCompletionRowPaddingH;
        const float detail_x = detail_right - detail_advance;
        if (detail_x > label_x + kCompletionDetailGap) {
          const float detail_baseline = CenterBaseline(badge_center_y, completion_detail_metrics_);
          paint.DrawTextRun(
              Rect{detail_x, row_center_y - kCompletionRowHeight * 0.5F, detail_advance, kCompletionRowHeight},
              Point{detail_x, detail_baseline},
              item.detail,
              TextStyle{Font::System(kCompletionDetailSize), theme_.completion_detail, TextDecoration::None});
        }
      }

      row_center_y += kCompletionRowHeight;
    }
  }

  std::shared_ptr<se::EditorCore> core_;
  std::shared_ptr<se::Document> document_;
  CodeEditorTheme theme_;
  std::vector<std::shared_ptr<CodeEditorDecorationProvider>> providers_;
  std::vector<CodeEditorTextChange> pending_changes_;
  std::string synced_document_text_;
  CodeEditorLineEntries<CodeEditorPhantomText> cached_phantom_entries_;
  std::shared_ptr<CodeEditorTextInputClient> text_input_client_;
  std::function<void()> invalidate_;
  se::Size viewport_;
  se::IntRange last_visible_range_{0, -1};
  FontMetrics font_metrics_;
  float font_size_;
  float char_width_;
  CompletionProvider completion_provider_;
  std::function<bool(const std::string&)> completion_trigger_characters_;
  FontMetrics completion_label_metrics_;
  FontMetrics completion_detail_metrics_;
  FontMetrics completion_badge_metrics_;
  float completion_badge_char_advance_{0.0F};

  CodeEditorController controller_;
  huxerui::EventEmitter events_;
  std::function<std::string(uint32_t, uint32_t)> newline_action_;
  bool accept_phantom_on_tab_{true};
  std::string current_diff_original_;
  int current_wrap_mode_{0};
  bool current_sticky_gutter_{false};
  std::map<uint32_t, std::string> cached_phantom_;

  struct CompletionState {
    bool visible{false};
    bool request_active{false};
    std::vector<CompletionItem> items;
    size_t selected{0};
    CompletionContext::TriggerKind last_trigger_kind{CompletionContext::TriggerKind::Invoked};
    std::string last_trigger_char;
    se::Rect panel_rect{};
    se::PointF last_cursor{};
    float last_cursor_height{0.0F};
  };
  CompletionState completion_;

  // Context menu (double-tap / long-press): a small floating panel with
  // clipboard and selection actions, drawn on the canvas like the completion
  // panel (no host overlay needed).
  struct ContextMenuEntry {
    const char* label;
    int command;  // 0=cut, 1=copy, 2=paste, 3=select-all, 4=find
  };
  struct ContextMenuState {
    bool visible{false};
    se::PointF anchor{};
    se::Rect panel_rect{};
    std::vector<ContextMenuEntry> entries;
  };
  ContextMenuState context_menu_;

  // Active touch pointers (pointer_id -> position) in press order. The editor
  // aggregates multi-touch into one multi-point SweetEditor GestureEvent so the
  // core can recognize two-finger pinch-to-zoom.
  std::vector<std::pair<std::int64_t, se::PointF>> active_pointers_;
  float pinch_last_distance_{0.0F};

  bool focused_{false};
  bool blink_on_{true};
  double last_blink_timestamp_{-1.0};
  // Core-managed animations (fling momentum / edge scroll) are pending.
  bool animation_pending_{false};
  se::EditorRenderModel cached_model_;
  bool model_dirty_{true};
  bool decorations_pending_{false};
  bool highlight_published_{false};
  bool suppress_focus_on_up_{false};
  static constexpr unsigned kHighlightBootstrapFrames = 16;
  unsigned highlight_bootstrap_attempts_{0};
  // Scrollbar thumb dragging state.
  bool scrollbar_dragging_{false};
  bool scrollbar_drag_vertical_{true};
  float scrollbar_drag_offset_{0.0F};
  se::ScrollbarModel cached_scrollbar_v_;
  se::ScrollbarModel cached_scrollbar_h_;
};

struct SearchBridge {
  std::function<void(const std::string&)> run_search;
  std::function<void()> find_next;
  std::function<void()> find_previous;
  std::function<void(const std::string&)> replace_current;
  std::function<void(const std::string&)> replace_all;
  std::function<void()> close;
};

}  // namespace

namespace detail {

struct CodeEditorControllerState {
  std::function<void(const std::string&, const std::string&)> load_document;
  std::function<std::string()> get_text;
  std::function<bool(uint32_t, uint32_t)> set_cursor;
  std::function<void(const std::string&)> run_search;
  std::function<void()> find_next;
  std::function<void()> find_previous;
  std::function<void(const std::string&)> replace_current;
  std::function<void(const std::string&)> replace_all;
  std::function<void()> clear_search;
  std::function<void()> toggle_search;
};

struct CodeEditorControllerAccess {
  static const std::shared_ptr<CodeEditorControllerState>& State(const CodeEditorController& controller) noexcept {
    return controller.state_;
  }
};

}  // namespace detail

CodeEditorController::CodeEditorController()
    : state_(std::make_shared<detail::CodeEditorControllerState>()) {}

bool CodeEditorController::IsConnected() const noexcept {
  return detail::CodeEditorControllerAccess::State(*this)->load_document != nullptr;
}

bool CodeEditorController::LoadDocument(const std::string& document_key, const std::string& text) const {
  const auto& state = detail::CodeEditorControllerAccess::State(*this);
  if (!state->load_document) {
    return false;
  }
  state->load_document(document_key, text);
  return true;
}

std::string CodeEditorController::Text() const {
  const auto& state = detail::CodeEditorControllerAccess::State(*this);
  return state->get_text ? state->get_text() : std::string();
}

bool CodeEditorController::SetCursor(std::uint32_t line, std::uint32_t column) const {
  const auto& state = detail::CodeEditorControllerAccess::State(*this);
  return state->set_cursor ? state->set_cursor(line, column) : false;
}

bool CodeEditorController::RunSearch(const std::string& pattern) const {
  const auto& state = detail::CodeEditorControllerAccess::State(*this);
  if (!state->run_search) {
    return false;
  }
  state->run_search(pattern);
  return true;
}

bool CodeEditorController::FindNext() const {
  const auto& state = detail::CodeEditorControllerAccess::State(*this);
  if (!state->find_next) {
    return false;
  }
  state->find_next();
  return true;
}

bool CodeEditorController::FindPrevious() const {
  const auto& state = detail::CodeEditorControllerAccess::State(*this);
  if (!state->find_previous) {
    return false;
  }
  state->find_previous();
  return true;
}

bool CodeEditorController::ReplaceCurrent(const std::string& replacement) const {
  const auto& state = detail::CodeEditorControllerAccess::State(*this);
  if (!state->replace_current) {
    return false;
  }
  state->replace_current(replacement);
  return true;
}

bool CodeEditorController::ReplaceAll(const std::string& replacement) const {
  const auto& state = detail::CodeEditorControllerAccess::State(*this);
  if (!state->replace_all) {
    return false;
  }
  state->replace_all(replacement);
  return true;
}

bool CodeEditorController::ClearSearch() const {
  const auto& state = detail::CodeEditorControllerAccess::State(*this);
  if (!state->clear_search) {
    return false;
  }
  state->clear_search();
  return true;
}

bool CodeEditorController::ToggleSearch() const {
  const auto& state = detail::CodeEditorControllerAccess::State(*this);
  if (!state->toggle_search) {
    return false;
  }
  state->toggle_search();
  return true;
}

struct CodeEditorBehavior {
  huxerui::TextMeasurer* measurer = nullptr;
  CodeEditorOptions options;
  CodeEditorController controller;
  huxerui::EventEmitter events;

  struct Extension final : NodeExtension {
    Extension(MountedNode& node, const CodeEditorBehavior& behavior)
        : measurer_(behavior.measurer),
          options_(behavior.options),
          events_(behavior.events),
          controller_(behavior.controller),
          holder_(std::make_shared<EditorHolder>(
              *behavior.measurer,
              behavior.options,
              [this] { InvalidatePaint(); },
              behavior.events,
              behavior.controller
          )),
          document_key_(behavior.options.document_key) {
      BindController(behavior.controller);
      static_cast<void>(node);
    }

    void BindController(CodeEditorController controller) {
      auto& state = detail::CodeEditorControllerAccess::State(controller);
      state->load_document = [this](const std::string& key, const std::string& text) {
        document_key_ = key;
        options_.initial_text = text;
        options_.document_key = key;
        holder_ =
            std::make_shared<EditorHolder>(*measurer_, options_, [this] { InvalidatePaint(); }, events_, controller_);
        InvalidatePaint();
      };
      state->get_text = [this] { return holder_->Text(); };
      state->set_cursor = [this](uint32_t line, uint32_t column) { return holder_->SetCursor(line, column); };
      state->run_search = [this](const std::string& text) { holder_->RunSearch(text); };
      state->find_next = [this] { holder_->FindNext(); };
      state->find_previous = [this] { holder_->FindPrevious(); };
      state->replace_current = [this](const std::string& text) { holder_->ReplaceCurrent(text); };
      state->replace_all = [this](const std::string& text) { holder_->ReplaceAll(text); };
      state->clear_search = [this] { holder_->CloseSearch(); };
    }

    void Update(MountedNode& node, const CodeEditorBehavior& behavior) {
      static_cast<void>(node);
      measurer_ = behavior.measurer;
      options_ = behavior.options;
      controller_ = behavior.controller;
      BindController(behavior.controller);
      // Reconcile declarative style changes without resetting the editor.
      holder_->ApplyTheme(behavior.options.theme.value_or(holder_->Theme()));
      if (behavior.options.document_key != document_key_) {
        document_key_ = behavior.options.document_key;
        holder_ = std::make_shared<EditorHolder>(
            *behavior.measurer,
            behavior.options,
            [this] { InvalidatePaint(); },
            behavior.events,
            behavior.controller
        );
        BindController(behavior.controller);
        return;
      }
      if (behavior.options.original_text != holder_->CurrentDiffOriginal()) {
        holder_->SetDiffOriginal(behavior.options.original_text);
      }
      holder_->SyncDisplayOptions(behavior.options.wrap_mode, behavior.options.sticky_gutter);
    }

    PointerResult OnPointer(MountedNode& node, const PointerEvent& event) override {
      static_cast<void>(node);
      const bool changed = holder_->HandlePointer(event);
      if (changed) {
        InvalidatePaint();
      }
      if (event.type == PointerEventType::Down) {
        return PointerResult::Capture;
      }
      return changed ? PointerResult::Handled : PointerResult::Ignored;
    }

    bool OnKey(MountedNode& node, const KeyEvent& event) override {
      static_cast<void>(node);
      if (holder_->HandleKey(event)) {
        InvalidatePaint();
        return true;
      }
      return false;
    }

    void OnFocusChanged(MountedNode& node, bool focused) override {
      static_cast<void>(node);
      holder_->SetFocused(focused);
      InvalidatePaint();
    }

#if defined(CODEEDITOR_HAS_SHOULD_COMMIT_FOCUS_ON_POINTER_UP)
    bool ShouldCommitFocusOnPointerUp() const noexcept override {
      return !holder_->SuppressFocusOnPointerUp();
    }
#endif

    FrameResult OnFrame(MountedNode& node, const FrameInfo& frame) override {
      static_cast<void>(node);
      const bool changed = holder_->AdvanceFrame(frame.timestamp);
      if (changed) {
        InvalidatePaint();
      }
      // Keep frames flowing until the first highlight slice is published: an
      // unfocused editor otherwise schedules no follow-up frame and the first
      // paint (which may run before the viewport has size) never repaints.
      // Bounded so a permanently empty viewport cannot spin frames forever.
      if (holder_->ShouldBootstrapHighlight()) {
        InvalidatePaint();
        return {.needs_frame = true};
      }
      if (holder_->HasActiveAnimation() || holder_->HasPendingPaintWork()) {
        return {.needs_frame = true};
      }
      if (holder_->IsFocused()) {
        return {.wake_after = 0.5};
      }
      return {};
    }

    void PaintAboveContent(const MountedNode& node, PaintContext& context) const override {
      const Size size = node.LayoutSize();
      // Clip the editor painting to its own bounds so cursor, focus line,
      // selection backgrounds, and other overdraw cannot bleed into the gutter
      // lane or outside the component layout.
      context.PushClip(Rect{0.0F, 0.0F, size.width, size.height});
      holder_->Render(context, size);
      context.PopClip();
    }

    std::shared_ptr<TextInputClient> GetTextInputClient() noexcept override {
      return holder_->TextInputClient();
    }

    TextSelectionClient* GetTextSelectionClient() noexcept override {
      // The editor renders selection handles and highlights itself inside its
      // own clip; leaving the window-level selection overlay enabled would draw
      // them in host coordinates without clipping and bleed onto other views.
      return nullptr;
    }

    bool HitTest(MountedNode& node, Point position) const override {
      const Rect bounds = node.Bounds();
      return position.x >= bounds.x && position.x <= bounds.x + bounds.width &&
             position.y >= bounds.y && position.y <= bounds.y + bounds.height;
    }

    huxerui::TextMeasurer* measurer_ = nullptr;
    CodeEditorOptions options_;
    huxerui::EventEmitter events_;
    CodeEditorController controller_;
    std::shared_ptr<EditorHolder> holder_;
    std::string document_key_;
  };
};

View CodeEditorSearchBar(
    State<std::string> search_text,
    State<std::string> replace_text,
    State<bool> visible,
    CodeEditorController controller
) {
  return Column {
    Row {
      TextField(TextEditingValue::FromText(search_text.Get()))
          .Placeholder("Find")
          .OnChanged([search_text, controller](const TextEditingValue& value) {
            search_text = value.text;
            controller.RunSearch(value.text);
          })
          .OnSubmitted([controller] { controller.FindNext(); })
          .With(Grow{}),
      Button("Prev").OnClick([controller] { controller.FindPrevious(); }),
      Button("Next").OnClick([controller] { controller.FindNext(); }),
      Button("Close").OnClick([visible, controller] {
        controller.ClearSearch();
        visible = false;
      }),
    }.With(Spacing(4.0F), Padding(4.0F)),
    Row {
      TextField(TextEditingValue::FromText(replace_text.Get()))
          .Placeholder("Replace")
          .OnChanged([replace_text](const TextEditingValue& value) { replace_text = value.text; })
          .With(Grow{}),
      Button("Replace").OnClick([replace_text, controller] {
        controller.ReplaceCurrent(replace_text.Get());
      }),
      Button("All").OnClick([replace_text, controller] {
        controller.ReplaceAll(replace_text.Get());
      }),
    }.With(Spacing(4.0F), Padding(4.0F)),
  }.With(CrossAlign(CrossAxisAlignment::Stretch));
}

[[huxerui::composable]]
View CodeEditor(CodeEditorOptions options, CodeEditorController controller) {
  TextMeasurer& measurer = UseTextMeasurer();
  // An empty document key keeps one default document so callers never have to
  // invent a storage key. The theme resolves from the ambient HuxerUI theme
  // unless explicitly overridden, so the editor follows Theme changes live.
  CodeEditorOptions effective_options = options;
  if (effective_options.document_key.empty()) {
    effective_options.document_key = "codeeditor:default";
  }
  // Priority: explicit options.theme, then a CodeEditorTheme placed in the
  // environment through Theme{ThemeDefinition{}.Set(CodeEditorTheme{...})},
  // then the ambient ThemeSpec.
  if (!effective_options.theme) {
    const CodeEditorTheme& environment_style = UseEnvironment<CodeEditorTheme>();
    if (!(environment_style == CodeEditorTheme::Default())) {
      effective_options.theme = environment_style;
    }
  }
  effective_options.theme = effective_options.theme.value_or(CodeEditorTheme::FromThemeSpec(UseTheme()));

  // Search is composed outside the retained editor node; the editor itself only owns editing semantics.
  auto search_visible = UseState(false);
  auto search_text = UseState(std::string());
  auto replace_text = UseState(std::string());
  const EventEmitter events = UseEvents();
  detail::CodeEditorControllerAccess::State(controller)->toggle_search =
      [search_visible] { search_visible = !search_visible.Get(); };
  View editor = Canvas([](PaintContext&, Size) {}).With(
      CodeEditorBehavior{&measurer, std::move(effective_options), controller, events}, Focusable{}
  );
  if (!search_visible.Get()) {
    return editor;
  }
  return Column {
    CodeEditorSearchBar(search_text, replace_text, search_visible, controller),
    std::move(editor).With(Grow{}),
  }.With(CrossAlign(CrossAxisAlignment::Stretch));
}

}  // namespace huxerui::codeeditor
