#include "sweetline_highlighter.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sweetedit_huxer {
namespace {

// Gutter icon identifiers shared with the renderer.
constexpr int32_t kIconType = 1;
constexpr int32_t kIconAt = 2;
constexpr int32_t kCommandRun = 1;
constexpr int32_t kCommandDebug = 2;
// Rainbow bracket palette base style id (see RegisterStyles).
constexpr uint32_t kRainbowBase = 100;

sweeteditor::TextStyle Style(int32_t argb) {
  return sweeteditor::TextStyle{argb, 0, sweeteditor::FONT_STYLE_NORMAL};
}

// Case-insensitive substring search without allocating an uppercased copy
// (used per comment token on every decoration refresh).
size_t FindIgnoreCase(std::string_view text, const char* needle) {
  const size_t needle_length = std::char_traits<char>::length(needle);
  if (needle_length == 0 || text.size() < needle_length) {
    return std::string::npos;
  }
  for (size_t index = 0; index + needle_length <= text.size(); ++index) {
    bool matched = true;
    for (size_t offset = 0; offset < needle_length; ++offset) {
      if (std::toupper(static_cast<unsigned char>(text[index + offset])) !=
          static_cast<unsigned char>(needle[offset])) {
        matched = false;
        break;
      }
    }
    if (matched) {
      return index;
    }
  }
  return std::string::npos;
}

bool IsSingleLine(const sweetline::TokenSpan& token) {
  return token.range.start.line == token.range.end.line && token.range.end.column > token.range.start.column;
}

// TokenSpan.matched_text is not reliably populated; read the literal from the
// managed document instead (ASCII columns map 1:1 to UTF-8 bytes). Returns a
// view into the line text to avoid per-token allocations.
std::string_view TokenLiteral(const sweetline::Document& document, const sweetline::TokenSpan& token) {
  if (token.range.start.line >= document.getLineCount()) {
    return {};
  }
  const sweetline::DocumentLine& line = document.getLine(token.range.start.line);
  const size_t column = token.range.start.column;
  if (column >= line.text.size()) {
    return {};
  }
  const size_t length = token.range.end.column - token.range.start.column;
  return std::string_view(line.text).substr(column, length);
}

template <typename Value>
std::vector<std::pair<size_t, std::vector<Value>>> CollectEntries(std::map<size_t, std::vector<Value>>& entries) {
  std::vector<std::pair<size_t, std::vector<Value>>> result;
  result.reserve(entries.size());
  for (auto& [line, values] : entries) {
    result.emplace_back(line, std::move(values));
  }
  return result;
}

sweetline::LineRange ViewportRange(const sweeteditor::EditorCore& core) {
  const sweeteditor::IntRange visible = core.getVisibleLineRange();
  if (visible.isEmpty()) {
    return {0, 0};
  }
  const size_t start = static_cast<size_t>(std::max(0, visible.start));
  const size_t end = static_cast<size_t>(std::max(visible.start, visible.end));
  return {start, end - start + 1};
}

}  // namespace

SweetLineHighlighter::SweetLineHighlighter(std::string syntax_json, std::string initial_text, std::string document_key) {
  engine_ = std::make_shared<sweetline::HighlightEngine>();

  engine_->registerStyleName("keyword", kStyleKeyword);
  engine_->registerStyleName("type", kStyleType);
  engine_->registerStyleName("class", kStyleClass);
  engine_->registerStyleName("method", kStyleFunction);
  engine_->registerStyleName("function", kStyleFunction);
  engine_->registerStyleName("variable", kStyleVariable);
  engine_->registerStyleName("field", kStyleVariable);
  engine_->registerStyleName("string", kStyleString);
  engine_->registerStyleName("number", kStyleNumber);
  engine_->registerStyleName("comment", kStyleComment);
  engine_->registerStyleName("preprocessor", kStylePreprocessor);
  engine_->registerStyleName("macro", kStylePreprocessor);
  engine_->registerStyleName("builtin", kStyleBuiltin);
  engine_->registerStyleName("punctuation", kStylePunctuation);
  engine_->registerStyleName("annotation", kStyleAnnotation);
  engine_->registerStyleName("url", kStyleUrl);

  // Compile the syntax rule (registered by name) and wrap the managed document
  // in a DocumentAnalyzer — the incremental analyzer used by other platforms.
  // The document uri must carry a suffix the compiled rule matches, otherwise
  // loadDocument returns null and every later analysis dereferences it.
  engine_->compileSyntaxFromJson(syntax_json);
  if (document_key.empty()) {
    document_key = "demo.cpp";
  }
  document_ = std::make_shared<sweetline::Document>(document_key, initial_text);
  analyzer_ = engine_->loadDocument(document_);
}

void SweetLineHighlighter::RegisterStyles(sweeteditor::EditorCore& core) {
  core.registerTextStyle(kStyleKeyword, Style(static_cast<int32_t>(0xFF0000FF)));
  core.registerTextStyle(kStyleType, Style(static_cast<int32_t>(0xFF267F99)));
  core.registerTextStyle(kStyleClass, Style(static_cast<int32_t>(0xFF267F99)));
  core.registerTextStyle(kStyleFunction, Style(static_cast<int32_t>(0xFF795E26)));
  core.registerTextStyle(kStyleVariable, Style(static_cast<int32_t>(0xFF001080)));
  core.registerTextStyle(kStyleString, Style(static_cast<int32_t>(0xFFA31515)));
  core.registerTextStyle(kStyleNumber, Style(static_cast<int32_t>(0xFF098658)));
  core.registerTextStyle(kStyleComment, Style(static_cast<int32_t>(0xFF008000)));
  core.registerTextStyle(kStylePreprocessor, Style(static_cast<int32_t>(0xFF9B4F96)));
  core.registerTextStyle(kStyleBuiltin, Style(static_cast<int32_t>(0xFF0000FF)));
  core.registerTextStyle(kStylePunctuation, Style(static_cast<int32_t>(0xFF777777)));
  core.registerTextStyle(kStyleAnnotation, Style(static_cast<int32_t>(0xFFB35C00)));
  core.registerTextStyle(kStyleUrl, Style(static_cast<int32_t>(0xFF0B5CAD)));
  // Rainbow bracket depth palette (style ids 100..107).
  static constexpr uint32_t kRainbowColors[8] = {
      0xFFDC322F, 0xFF268BD2, 0xFF859900, 0xFFB58900,
      0xFFCB4B16, 0xFF6C71C4, 0xFF2AA198, 0xFFD33682,
  };
  for (uint32_t index = 0; index < 8; ++index) {
    core.registerTextStyle(kRainbowBase + index, Style(static_cast<int32_t>(kRainbowColors[index])));
  }
}


// Parses a 0x-prefixed hex color literal (0xRRGGBB / 0xAARRGGBB) and appends a
// color-block inlay after the token. Returns true when a swatch was emitted.
bool TryAppendColorInlay(
    std::string_view literal,
    uint32_t column,
    uint32_t length,
    size_t line,
    std::map<size_t, std::vector<sweeteditor::InlayHint>>& inlay_hints
) {
  if (literal.size() <= 2 || literal[0] != '0' || (literal[1] != 'x' && literal[1] != 'X')) {
    return false;
  }
  std::string hex;
  hex.reserve(literal.size() - 2);
  for (size_t index = 2; index < literal.size(); ++index) {
    const char ch = literal[index];
    if (ch == '_' || ch == 'u' || ch == 'U' || ch == 'l' || ch == 'L') {
      continue;
    }
    hex.push_back(ch);
  }
  if (hex.empty()) {
    return false;
  }
  char* end = nullptr;
  const unsigned long long value = std::strtoull(hex.c_str(), &end, 16);
  if (end == nullptr || *end != '\0' || end == hex.c_str()) {
    return false;
  }
  inlay_hints[line].push_back({
      sweeteditor::InlayType::COLOR,
      column + length,
      static_cast<int32_t>(value),
      "",
  });
  return true;
}

void SweetLineHighlighter::RefreshBracketDecorations(sweeteditor::EditorCore& core) {
  const sweetline::LineRange range = ViewportRange(core);
  if (range.line_count == 0 || analyzer_ == nullptr) {
    core.clearHighlights(sweeteditor::SpanLayer::OVERLAY);
    core.setBracketGuides({});
    return;
  }
  const sweetline::SharedPtr<sweetline::BracketPairResult> result = analyzer_->analyzeBracketPairsInLineRange(range);
  std::map<size_t, std::vector<sweeteditor::StyleSpan>> rainbow_spans;
  if (result) {
    size_t line = result->start_line;
    for (const sweetline::LineBracketPairs& line_pairs : result->lines) {
      for (const sweetline::BracketToken& token : line_pairs.tokens) {
        if (token.range.start.line != token.range.end.line) {
          continue;
        }
        const uint32_t column = static_cast<uint32_t>(token.range.start.column);
        const uint32_t length = static_cast<uint32_t>(token.range.end.column - token.range.start.column);
        if (length == 0) {
          continue;
        }
        rainbow_spans[line].push_back({
            column,
            length,
            kRainbowBase + (static_cast<uint32_t>(std::max(0, token.depth)) % 8),
        });
      }
      ++line;
    }
  }
  core.setBatchLineSpans(sweeteditor::SpanLayer::OVERLAY, CollectEntries(rainbow_spans));
  // Bracket connection lines come from the indent-guide pipeline (scope rules
  // track the brace pair at the indent column); emitting BracketGuides here as
  // well draws a second connector at the open-brace column whenever the brace
  // is not at the indent column.
  core.setBracketGuides({});
}

void SweetLineHighlighter::RefreshVisible(sweeteditor::EditorCore& core) {
  const sweetline::LineRange range = ViewportRange(core);
  if (range.line_count == 0 || analyzer_ == nullptr) {
    return;
  }
  // Analyze once and reuse the slice for span publishing and decorations to
  // avoid re-copying the viewport highlight data on every viewport change.
  const sweetline::SharedPtr<sweetline::DocumentHighlightSlice> slice = analyzer_->analyzeLineRange(range);
  if (slice) {
    PublishSlice(core, slice);
  }
  RefreshIndentGuides(core);
  RefreshDecorations(core, slice);
  RefreshDocumentHighlights(core, slice);
}

void SweetLineHighlighter::SetHostDecorationProviders(std::vector<HostDecorationProvider> providers) {
  host_decoration_providers_ = std::move(providers);
}

void SweetLineHighlighter::SetGutterIconProvider(GutterIconProvider provider) {
  gutter_icon_provider_ = std::move(provider);
}

void SweetLineHighlighter::PublishVisible(sweeteditor::EditorCore& core) {
  const sweetline::LineRange range = ViewportRange(core);
  if (range.line_count == 0 || analyzer_ == nullptr) {
    return;
  }
  // Pre-analyze a band around the viewport (reference decoration pipeline uses
  // an overscan context) so scrolling into adjacent lines hits the analyzer
  // cache instead of running fresh Oniguruma scans per frame.
  const size_t total_lines = document_->getLineCount();
  if (total_lines > 0) {
    sweetline::LineRange overscan = range;
    constexpr size_t kOverscanLines = 32;
    const size_t start = overscan.start_line > kOverscanLines ? overscan.start_line - kOverscanLines : 0;
    const size_t end = std::min(total_lines - 1, overscan.start_line + overscan.line_count + kOverscanLines - 1);
    overscan.start_line = start;
    overscan.line_count = end >= start ? end - start + 1 : 1;
    analyzer_->analyzeLineRange(overscan);
  }
  if (const sweetline::SharedPtr<sweetline::DocumentHighlightSlice> slice = analyzer_->getHighlightSlice(range); slice) {
    PublishSlice(core, slice);
  }
  // Indent guides (bracket connectors) are cheap and must track the viewport
  // immediately while scrolling; only the heavier decorations are deferred.
  RefreshIndentGuides(core);
}

void SweetLineHighlighter::RefreshVisibleDecorations(sweeteditor::EditorCore& core) {
  const sweetline::LineRange range = ViewportRange(core);
  if (range.line_count == 0 || analyzer_ == nullptr) {
    return;
  }
  const sweetline::SharedPtr<sweetline::DocumentHighlightSlice> slice = analyzer_->getHighlightSlice(range);
  RefreshDecorations(core, slice);
  RefreshDocumentHighlights(core, slice);
  RefreshBracketDecorations(core);
}

void SweetLineHighlighter::ApplyChanges(
    sweeteditor::EditorCore& core, const std::vector<sweeteditor::TextChange>& changes
) {
  const sweetline::LineRange range = ViewportRange(core);
  if (range.line_count == 0 || analyzer_ == nullptr) {
    return;
  }
  sweetline::SharedPtr<sweetline::DocumentHighlightSlice> slice;
  for (const sweeteditor::TextChange& change : changes) {
    const sweetline::TextRange change_range{
        sweetline::TextPosition{change.range.start.line, change.range.start.column},
        sweetline::TextPosition{change.range.end.line, change.range.end.column},
    };
    slice = analyzer_->analyzeIncrementalInLineRange(change_range, change.new_text, range);
  }
  if (slice) {
    PublishSlice(core, slice);
  }
  RefreshIndentGuides(core);
  RefreshDecorations(core, slice);
  RefreshDocumentHighlights(core, slice);
}

void SweetLineHighlighter::RefreshIndentGuides(sweeteditor::EditorCore& core) {
  const sweetline::LineRange range = ViewportRange(core);
  if (range.line_count == 0) {
    core.setIndentGuides({});
    core.setFoldRegions({});
    return;
  }
  if (analyzer_ == nullptr) {
    core.setIndentGuides({});
    core.setFoldRegions({});
    return;
  }
  const sweetline::SharedPtr<sweetline::IndentGuideResult> result = analyzer_->analyzeIndentGuidesInLineRange(range);
  std::vector<sweeteditor::IndentGuide> guides;
  if (result) {
    for (const sweetline::IndentGuideLine& guide : result->guide_lines) {
      if (guide.end_line < guide.start_line) {
        continue;
      }
      const size_t column = static_cast<size_t>(std::max(0, guide.column));
      guides.push_back({
          sweeteditor::TextPosition{static_cast<size_t>(guide.start_line), column},
          sweeteditor::TextPosition{static_cast<size_t>(guide.end_line), column},
      });
    }
  }
  core.setIndentGuides(std::move(guides));
}

void SweetLineHighlighter::PublishFoldRegions(sweeteditor::EditorCore& core) {
  if (fold_regions_initialized_ || analyzer_ == nullptr || document_ == nullptr) {
    return;
  }
  fold_regions_initialized_ = true;
  const size_t total_lines = document_->getLineCount();
  if (total_lines == 0) {
    return;
  }
  // Compute fold regions over the WHOLE document: the core turns them into
  // fold arrows and hit targets, so they must cover lines outside the first
  // viewport too.
  const sweetline::SharedPtr<sweetline::IndentGuideResult> full =
      analyzer_->analyzeIndentGuidesInLineRange({0, total_lines});
  std::vector<sweeteditor::FoldRegion> folds;
  if (full) {
    for (const sweetline::IndentGuideLine& guide : full->guide_lines) {
      if (guide.end_line > guide.start_line) {
        folds.push_back({
            static_cast<size_t>(guide.start_line),
            static_cast<size_t>(guide.end_line),
            false,
        });
      }
    }
  }
  core.setFoldRegions(std::move(folds));
}

void SweetLineHighlighter::RefreshDecorations(
    sweeteditor::EditorCore& core, const sweetline::SharedPtr<sweetline::DocumentHighlightSlice>& slice
) {
  if (analyzer_ == nullptr) {
    core.setBatchLineInlayHints({});
    core.setBatchLineDiagnostics({});
    core.setBatchLineCodeLens({});
    core.setBatchLineSpans(sweeteditor::SpanLayer::SYNTAX, {});
    core.setBatchLineSpans(sweeteditor::SpanLayer::SEMANTIC, {});
    return;
  }
  sweetline::SharedPtr<sweetline::DocumentHighlightSlice> resolved = slice;
  if (!resolved) {
    resolved = analyzer_->getHighlightSlice(ViewportRange(core));
  }
  if (!resolved || resolved->lines.empty()) {
    core.setBatchLineInlayHints({});
    core.setBatchLineDiagnostics({});
    core.setBatchLineCodeLens({});
    core.setBatchLineLinks({});
    core.setBatchLineGutterIcons({});
    core.setSeparatorGuides({});
    return;
  }
  const sweetline::DocumentHighlightSlice& highlight_slice = *resolved;

  std::map<size_t, std::vector<sweeteditor::InlayHint>> inlay_hints;
  std::map<size_t, std::vector<sweeteditor::Diagnostic>> diagnostics;
  std::map<size_t, std::vector<sweeteditor::CodeLensItem>> codelens;
  std::map<size_t, std::vector<sweeteditor::LinkSpan>> links;
  std::map<size_t, std::vector<sweeteditor::GutterIcon>> gutter_icons;
  std::vector<sweeteditor::SeparatorGuide> separators;

  size_t line = highlight_slice.start_line;
  for (const sweetline::LineHighlight& line_highlight : highlight_slice.lines) {
    for (const sweetline::TokenSpan& token : line_highlight.spans) {
      if (!IsSingleLine(token)) {
        continue;
      }
      const uint32_t column = static_cast<uint32_t>(token.range.start.column);
      const uint32_t length = static_cast<uint32_t>(token.range.end.column - token.range.start.column);
      const std::string_view literal = TokenLiteral(*document_, token);

      if (token.style_id == static_cast<int32_t>(kStyleComment)) {
        const size_t fixme = FindIgnoreCase(literal, "FIXME");
        const size_t todo = FindIgnoreCase(literal, "TODO");
        if (fixme != std::string::npos) {
          diagnostics[line].push_back({column + static_cast<uint32_t>(fixme), 5, sweeteditor::DiagnosticSeverity::DIAG_ERROR});
        } else if (todo != std::string::npos) {
          diagnostics[line].push_back(
              {column + static_cast<uint32_t>(todo), 4, sweeteditor::DiagnosticSeverity::DIAG_WARNING});
        }

        // Separator guides from `// ===` or `// ---` comment rulers.
        size_t index = 0;
        while (index < literal.size() && literal[index] == '/') {
          ++index;
        }
        if (index < literal.size() && (literal[index] == '=' || literal[index] == '-')) {
          const char marker = literal[index];
          size_t count = 0;
          while (index < literal.size() && literal[index] == marker) {
            ++count;
            ++index;
          }
          if (count > 0) {
            separators.push_back({
                static_cast<int32_t>(line),
                marker == '=' ? sweeteditor::SeparatorStyle::DOUBLE : sweeteditor::SeparatorStyle::SINGLE,
                static_cast<int32_t>(count),
                static_cast<uint32_t>(literal.size()),
            });
          }
        }
        continue;
      }

      // Color-literal swatch: any 0x-prefixed hex token (ARGB, e.g. 0xFFAABBCC)
      // gets a color block inlay right after it (reference decoration provider
      // treats STYLE_COLOR tokens this way). Suffix separators like '_' or 'u'
      // in literals are stripped before parsing.
      if (TryAppendColorInlay(literal, column, length, line, inlay_hints)) {
        continue;
      }

      if (token.style_id == static_cast<int32_t>(kStyleKeyword)) {
        if (literal == "const") {
          inlay_hints[line].push_back({sweeteditor::InlayType::TEXT, column + length, 0, "immutable"});
        } else if (literal == "return") {
          inlay_hints[line].push_back({sweeteditor::InlayType::TEXT, column + length, 0, "value: "});
        } else if (literal == "case") {
          inlay_hints[line].push_back({sweeteditor::InlayType::TEXT, column + length, 0, "condition: "});
        } else if (literal == "class" || literal == "struct") {
          gutter_icons[line].push_back({kIconType});
          codelens[line].push_back({static_cast<int32_t>(column), kCommandRun, "Run"});
          codelens[line].push_back({static_cast<int32_t>(column), kCommandDebug, "Debug"});
        }
        continue;
      }

      if (token.style_id == static_cast<int32_t>(kStyleUrl)) {
        if (!literal.empty()) {
          links[line].push_back({column, length, std::string(literal)});
        }
        continue;
      }
    }
    ++line;
  }

  // Merge host decoration providers (reference DecorationProvider) into the
  // generated inlay hints, diagnostics, and code lens.
  if (!host_decoration_providers_.empty()) {
    const uint32_t start_line = static_cast<uint32_t>(highlight_slice.start_line);
    const uint32_t end_line = start_line + static_cast<uint32_t>(highlight_slice.lines.size()) - 1;
    for (const HostDecorationProvider& provider : host_decoration_providers_) {
      const HostDecorationResult result = provider(start_line, end_line);
      for (const HostInlayHint& hint : result.inlay_hints) {
        inlay_hints[hint.line].push_back({sweeteditor::InlayType::TEXT, hint.column, 0, hint.text});
      }
      for (const HostDiagnostic& diagnostic : result.diagnostics) {
        const sweeteditor::DiagnosticSeverity severity =
            diagnostic.severity == 0 ? sweeteditor::DiagnosticSeverity::DIAG_ERROR
            : diagnostic.severity == 2 ? sweeteditor::DiagnosticSeverity::DIAG_INFO
            : diagnostic.severity == 3 ? sweeteditor::DiagnosticSeverity::DIAG_HINT
                                       : sweeteditor::DiagnosticSeverity::DIAG_WARNING;
        diagnostics[diagnostic.line].push_back(
            {diagnostic.column, diagnostic.length, severity}
        );
      }
      for (const HostCodeLens& lens : result.codelens) {
        codelens[lens.line].push_back({static_cast<int32_t>(lens.column), lens.command_id, lens.title});
      }
    }
  }

  // Merge host gutter icons (breakpoints/errors) into the generated ones.
  if (gutter_icon_provider_) {
    const uint32_t start_line = static_cast<uint32_t>(highlight_slice.start_line);
    const uint32_t end_line = start_line + static_cast<uint32_t>(highlight_slice.lines.size()) - 1;
    for (const auto& [line, icon_id] : gutter_icon_provider_(start_line, end_line)) {
      gutter_icons[line].push_back({icon_id});
    }
  }

  core.setBatchLineInlayHints(CollectEntries(inlay_hints));
  core.setBatchLineDiagnostics(CollectEntries(diagnostics));
  core.setBatchLineCodeLens(CollectEntries(codelens));
  core.setBatchLineLinks(CollectEntries(links));
  core.setBatchLineGutterIcons(CollectEntries(gutter_icons));
  core.setSeparatorGuides(std::move(separators));
}

void SweetLineHighlighter::RefreshDocumentHighlights(
    sweeteditor::EditorCore& core, const sweetline::SharedPtr<sweetline::DocumentHighlightSlice>& slice
) {
  const sweetline::LineRange range = ViewportRange(core);
  if (range.line_count == 0 || analyzer_ == nullptr) {
    core.clearDocumentHighlights();
    return;
  }
  const sweeteditor::TextPosition cursor = core.getCursorPosition();
  const std::string word = core.getWordAtCursor();
  if (word.empty()) {
    core.clearDocumentHighlights();
    return;
  }

  sweetline::SharedPtr<sweetline::DocumentHighlightSlice> resolved = slice;
  if (!resolved) {
    resolved = analyzer_->getHighlightSlice(range);
  }
  if (!resolved) {
    return;
  }
  const sweetline::DocumentHighlightSlice& highlight_slice = *resolved;
  std::map<size_t, std::vector<sweeteditor::DocumentHighlight>> highlights;
  size_t line = highlight_slice.start_line;
  for (const sweetline::LineHighlight& line_highlight : highlight_slice.lines) {
    for (const sweetline::TokenSpan& token : line_highlight.spans) {
      if (!IsSingleLine(token)) {
        continue;
      }
      if (TokenLiteral(*document_, token) != word) {
        continue;
      }
      const uint32_t column = static_cast<uint32_t>(token.range.start.column);
      const uint32_t length = static_cast<uint32_t>(token.range.end.column - token.range.start.column);
      const bool is_cursor = token.range.start.line == cursor.line && token.range.start.column <= cursor.column &&
                             cursor.column <= token.range.end.column;
      highlights[line].push_back({
          column,
          length,
          is_cursor ? sweeteditor::DocumentHighlightKind::WRITE : sweeteditor::DocumentHighlightKind::TEXT,
      });
    }
    ++line;
  }
  core.setBatchLineDocumentHighlights(CollectEntries(highlights));
}

void SweetLineHighlighter::RefreshBracketMatch(sweeteditor::EditorCore& core) {
  const sweetline::LineRange range = ViewportRange(core);
  if (range.line_count == 0 || analyzer_ == nullptr) {
    core.clearMatchedBrackets();
    return;
  }
  const sweetline::SharedPtr<sweetline::BracketPairResult> result = analyzer_->analyzeBracketPairsInLineRange(range);
  if (!result) {
    core.clearMatchedBrackets();
    return;
  }

  const sweeteditor::TextPosition cursor = core.getCursorPosition();
  for (const sweetline::LineBracketPairs& line : result->lines) {
    for (const sweetline::BracketToken& token : line.tokens) {
      if (token.match_state != sweetline::BracketMatchState::MATCHED) {
        continue;
      }
      const bool at_start = token.range.start.line == cursor.line && token.range.start.column == cursor.column;
      const bool at_end = token.range.end.line == cursor.line && token.range.end.column == cursor.column;
      if (!at_start && !at_end) {
        continue;
      }
      core.setMatchedBrackets(
          sweeteditor::TextPosition{token.range.start.line, token.range.start.column},
          sweeteditor::TextPosition{token.partner_range.start.line, token.partner_range.start.column});
      return;
    }
  }
  core.clearMatchedBrackets();
}

void SweetLineHighlighter::PublishSlice(
    sweeteditor::EditorCore& core, const sweetline::SharedPtr<sweetline::DocumentHighlightSlice>& slice
) {
  std::vector<std::pair<size_t, std::vector<sweeteditor::StyleSpan>>> entries;
  size_t line = slice->start_line;
  for (const sweetline::LineHighlight& line_highlight : slice->lines) {
    std::vector<sweeteditor::StyleSpan> spans;
    for (const sweetline::TokenSpan& token : line_highlight.spans) {
      if (token.style_id <= 0) {
        continue;
      }
      if (token.range.start.line != token.range.end.line) {
        continue;
      }
      if (token.range.end.column <= token.range.start.column) {
        continue;
      }
      spans.push_back({
          static_cast<uint32_t>(token.range.start.column),
          static_cast<uint32_t>(token.range.end.column - token.range.start.column),
          static_cast<uint32_t>(token.style_id),
      });
    }
    if (!spans.empty()) {
      entries.push_back({line, std::move(spans)});
    }
    ++line;
  }

  core.clearHighlights(sweeteditor::SpanLayer::SYNTAX);
  core.setBatchLineSpans(sweeteditor::SpanLayer::SYNTAX, std::move(entries));
}

}  // namespace sweetedit_huxer
