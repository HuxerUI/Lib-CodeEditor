#include "sweetline_provider.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <string_view>

#if defined(__ANDROID__)
#include <android/log.h>
#define CE_TRACE(...) __android_log_print(ANDROID_LOG_INFO, "CodeEditor", __VA_ARGS__)
#else
#define CE_TRACE(...) std::fprintf(stderr, __VA_ARGS__)
#endif

namespace demo {
namespace {

namespace ce = huxerui::codeeditor;
namespace sl = sweetline;

// Style names registered on the SweetLine engine; the ids match the
// EditorStyle palette the editor registers on its core.
void RegisterEngineStyles(const sl::SharedPtr<sl::HighlightEngine>& engine) {
  const auto name = [&engine](const char* key, int32_t id) { engine->registerStyleName(key, static_cast<uint32_t>(id)); };
  name("keyword", static_cast<int32_t>(ce::EditorStyle::Keyword));
  name("type", static_cast<int32_t>(ce::EditorStyle::Type));
  name("class", static_cast<int32_t>(ce::EditorStyle::Class));
  name("method", static_cast<int32_t>(ce::EditorStyle::Function));
  name("function", static_cast<int32_t>(ce::EditorStyle::Function));
  name("variable", static_cast<int32_t>(ce::EditorStyle::Variable));
  name("field", static_cast<int32_t>(ce::EditorStyle::Variable));
  name("string", static_cast<int32_t>(ce::EditorStyle::String));
  name("number", static_cast<int32_t>(ce::EditorStyle::Number));
  name("comment", static_cast<int32_t>(ce::EditorStyle::Comment));
  name("preprocessor", static_cast<int32_t>(ce::EditorStyle::Preprocessor));
  name("macro", static_cast<int32_t>(ce::EditorStyle::Preprocessor));
  name("builtin", static_cast<int32_t>(ce::EditorStyle::Builtin));
  name("punctuation", static_cast<int32_t>(ce::EditorStyle::Punctuation));
  name("annotation", static_cast<int32_t>(ce::EditorStyle::Annotation));
  name("url", static_cast<int32_t>(ce::EditorStyle::Url));
}

size_t FindIgnoreCase(std::string_view text, const char* needle) {
  const std::string_view hay = text;
  const std::string_view sub{needle};
  if (sub.empty() || hay.size() < sub.size()) {
    return std::string::npos;
  }
  for (size_t index = 0; index + sub.size() <= hay.size(); ++index) {
    bool matched = true;
    for (size_t offset = 0; offset < sub.size(); ++offset) {
      const char left = hay[index + offset];
      const char right = sub[offset];
      const bool equal = left == right ||
          (left >= 'a' && left <= 'z' ? left - 'a' + 'A' : left) ==
              (right >= 'a' && right <= 'z' ? right - 'a' + 'A' : right);
      if (!equal) {
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

// TokenSpan and BracketToken both expose a single-line `.range`.
bool IsSingleLine(const auto& token) {
  return token.range.start.line == token.range.end.line;
}

std::string_view TokenLiteral(const sl::Document& document, const sl::TokenSpan& token) {
  if (token.range.start.line >= document.getLineCount()) {
    return {};
  }
  const sl::DocumentLine& line = document.getLine(token.range.start.line);
  const size_t column = token.range.start.column;
  if (column >= line.text.size()) {
    return {};
  }
  const size_t length = token.range.end.column - token.range.start.column;
  return std::string_view(line.text).substr(column, length);
}

// Parses a #RGB / #RRGGBB literal into a 0xAARRGGBB color for COLOR inlays.
bool ParseHexColor(std::string_view literal, int32_t& value) {
  if (literal.size() < 4 || literal.front() != '#') {
    return false;
  }
  const std::string_view digits = literal.substr(1);
  uint32_t result = 0;
  if (digits.size() == 3) {
    for (const char character : digits) {
      const int32_t nibble = character >= '0' && character <= '9'
          ? character - '0'
          : character >= 'a' && character <= 'f' ? character - 'a' + 10
          : character >= 'A' && character <= 'F' ? character - 'A' + 10
                                                 : -1;
      if (nibble < 0) {
        return false;
      }
      result = (result << 8) | ((nibble << 4) | nibble);
    }
    value = static_cast<int32_t>(0xFF000000u | result);
    return true;
  }
  if (digits.size() == 6) {
    for (const char character : digits) {
      const int32_t nibble = character >= '0' && character <= '9'
          ? character - '0'
          : character >= 'a' && character <= 'f' ? character - 'a' + 10
          : character >= 'A' && character <= 'F' ? character - 'A' + 10
                                                 : -1;
      if (nibble < 0) {
        return false;
      }
      result = (result << 4) | static_cast<uint32_t>(nibble);
    }
    value = static_cast<int32_t>(0xFF000000u | result);
    return true;
  }
  return false;
}

sl::LineRange OverscanRange(const sl::LineRange& range, size_t total_lines) {
  constexpr size_t kOverscanLines = 32;
  const size_t start = range.start_line > kOverscanLines ? range.start_line - kOverscanLines : 0;
  const size_t end = std::min(total_lines - 1, range.start_line + range.line_count + kOverscanLines - 1);
  return {start, end >= start ? end - start + 1 : 1};
}

}  // namespace

SweetLineDecorationProvider::SweetLineDecorationProvider(
    std::string syntax_json, std::string initial_text, std::string document_key, GutterIconSource gutter_icons,
    PhantomSource phantom
)
    : syntax_json_(std::move(syntax_json)),
      document_key_(document_key.empty() ? "demo.cpp" : std::move(document_key)),
      gutter_icons_(std::move(gutter_icons)),
      phantom_(std::move(phantom)) {
  engine_ = std::make_shared<sl::HighlightEngine>();
  RegisterEngineStyles(engine_);
  engine_->compileSyntaxFromJson(syntax_json_);
  RebuildDocument(initial_text);
}

void SweetLineDecorationProvider::RebuildDocument(const std::string& text) {
  document_ = std::make_shared<sl::Document>(document_key_, text);
  analyzer_ = engine_->loadDocument(document_);
  synced_text_ = text;
  fold_regions_published_ = false;
}

void SweetLineDecorationProvider::ProvideDecorations(
    const ce::EditorDecorationContext& context, ce::EditorDecorationReceiver& receiver
) {
  ce::EditorDecorationResult result;
  if (analyzer_ == nullptr || document_ == nullptr) {
    return;
  }
  if (context.visible_end_line < context.visible_start_line) {
    return;
  }

  const sl::LineRange viewport{
      context.visible_start_line,
      context.visible_end_line > context.visible_start_line ? context.visible_end_line - context.visible_start_line + 1 : 1,
  };

  // Keep the analyzer document in sync with the editor document: incremental
  // analysis for edits, full rebuild only when the text diverges with no
  // reported changes (first load, reload, or an external reset).
  // Slice produced by the incremental pass; the analyzer returns it directly
  // (the proven path — getHighlightSlice serves the last full-analysis cache,
  // which the incremental patch does not refresh).
  const char* analysis_path = "full";
  sl::SharedPtr<sl::DocumentHighlightSlice> incremental_slice;
  if (analyzer_ != nullptr && !context.text_changes.empty()) {
    for (const ce::TextChange& change : context.text_changes) {
      incremental_slice = analyzer_->analyzeIncrementalInLineRange(
          sl::TextRange{
              sl::TextPosition{change.range.start.line, change.range.start.column},
              sl::TextPosition{change.range.end.line, change.range.end.column},
          },
          sl::U8String(change.new_text), viewport
      );
    }
    if (context.document_text != nullptr) {
      synced_text_ = *context.document_text;
    }
    analysis_path = "incr";
    CE_TRACE("provider: incremental changes=%zu", context.text_changes.size());
  } else if (context.document_text != nullptr && *context.document_text != synced_text_) {
    RebuildDocument(*context.document_text);
    analysis_path = "rebuild";
    CE_TRACE("provider: rebuilt document");
  }
  const size_t total_lines = document_->getLineCount();
  sl::SharedPtr<sl::DocumentHighlightSlice> slice;
  if (incremental_slice) {
    slice = std::move(incremental_slice);
  } else {
    analyzer_->analyzeLineRange(OverscanRange(viewport, total_lines));
    slice = analyzer_->getHighlightSlice(viewport);
  }

  // Syntax spans (fast path, kept up to date while scrolling).
  if (slice) {
    size_t line = slice->start_line;
    for (const sl::LineHighlight& line_highlight : slice->lines) {
      std::vector<sweeteditor::StyleSpan> spans;
      for (const sl::TokenSpan& token : line_highlight.spans) {
        if (token.style_id <= 0 || !IsSingleLine(token)) {
          continue;
        }
        const uint32_t column = static_cast<uint32_t>(token.range.start.column);
        const uint32_t length = static_cast<uint32_t>(token.range.end.column - token.range.start.column);
        if (length == 0) {
          continue;
        }
        spans.push_back({column, length, static_cast<uint32_t>(token.style_id)});
      }
      if (!spans.empty()) {
        result.syntax_spans.emplace_back(static_cast<uint32_t>(line), std::move(spans));
      }
      ++line;
    }
  }

  // Indent guides follow the viewport immediately.
  if (const sl::SharedPtr<sl::IndentGuideResult> guides = analyzer_->analyzeIndentGuidesInLineRange(viewport)) {
    for (const sl::IndentGuideLine& guide : guides->guide_lines) {
      if (guide.end_line < guide.start_line) {
        continue;
      }
      result.indent_guides.push_back(
          {static_cast<uint32_t>(guide.start_line), static_cast<uint32_t>(guide.end_line), static_cast<uint32_t>(std::max(0, guide.column))}
      );
    }
  }

  // Fold regions cover the whole document; publish once (the editor owns the
  // interactive fold state afterwards).
  if (!fold_regions_published_ && context.viewport_settled && total_lines > 0) {
    if (const sl::SharedPtr<sl::IndentGuideResult> full = analyzer_->analyzeIndentGuidesInLineRange({0, total_lines})) {
      for (const sl::IndentGuideLine& guide : full->guide_lines) {
        if (guide.end_line > guide.start_line) {
          result.fold_regions.push_back({static_cast<uint32_t>(guide.start_line), static_cast<uint32_t>(guide.end_line)});
        }
      }
      // Only mark published when folds were actually found: an early attempt
      // with a not-yet-populated analyzer or an empty document must retry.
      if (!result.fold_regions.empty()) {
        fold_regions_published_ = true;
      }
    }
  }

  // Bracket pair under the cursor.
  if (const sl::SharedPtr<sl::BracketPairResult> brackets = analyzer_->analyzeBracketPairsInLineRange(viewport)) {
    for (const sl::LineBracketPairs& line_pairs : brackets->lines) {
      for (const sl::BracketToken& token : line_pairs.tokens) {
        if (token.match_state != sl::BracketMatchState::MATCHED) {
          continue;
        }
        const bool at_start = token.range.start.line == context.cursor_line &&
            token.range.start.column == context.cursor_column;
        const bool at_end =
            token.range.end.line == context.cursor_line && token.range.end.column == context.cursor_column;
        if (!at_start && !at_end) {
          continue;
        }
        result.matched_bracket_open = sweeteditor::TextPosition{
            token.range.start.line, token.range.start.column,
        };
        result.matched_bracket_close = sweeteditor::TextPosition{
            token.partner_range.start.line, token.partner_range.start.column,
        };
      }
      if (result.matched_bracket_open) {
        break;
      }
    }
  }

  // Heavy decorations wait for the settled viewport.
  if (context.viewport_settled && slice) {
    // Rainbow brackets over the viewport.
    if (const sl::SharedPtr<sl::BracketPairResult> brackets = analyzer_->analyzeBracketPairsInLineRange(viewport)) {
      size_t line = brackets->start_line;
      for (const sl::LineBracketPairs& line_pairs : brackets->lines) {
        std::vector<sweeteditor::StyleSpan> rainbow;
        for (const sl::BracketToken& token : line_pairs.tokens) {
          if (!IsSingleLine(token)) {
            continue;
          }
          const uint32_t column = static_cast<uint32_t>(token.range.start.column);
          const uint32_t length = static_cast<uint32_t>(token.range.end.column - token.range.start.column);
          if (length == 0) {
            continue;
          }
          const uint32_t depth = static_cast<uint32_t>(std::max(0, token.depth)) % 8;
          rainbow.push_back({column, length, static_cast<uint32_t>(ce::EditorStyle::RainbowFirst) + depth});
        }
        if (!rainbow.empty()) {
          result.overlay_spans.emplace_back(static_cast<uint32_t>(line), std::move(rainbow));
        }
        ++line;
      }
    }

    // Word-under-cursor document highlights: reuse the token under the caret
    // as the word to match.
    std::string word;
    if (slice) {
      size_t line = slice->start_line;
      for (const sl::LineHighlight& line_highlight : slice->lines) {
        for (const sl::TokenSpan& token : line_highlight.spans) {
          if (!IsSingleLine(token)) {
            continue;
          }
          if (token.range.start.line == context.cursor_line && token.range.start.column <= context.cursor_column &&
              context.cursor_column <= token.range.end.column) {
            const std::string_view literal = TokenLiteral(*document_, token);
            const bool identifier = !literal.empty() &&
                ((literal.front() >= 'A' && literal.front() <= 'Z') ||
                 (literal.front() >= 'a' && literal.front() <= 'z') || literal.front() == '_');
            if (identifier) {
              word = std::string(literal);
            }
          }
        }
        ++line;
      }
    }
    if (!word.empty() && slice) {
      size_t line = slice->start_line;
      for (const sl::LineHighlight& line_highlight : slice->lines) {
        std::vector<sweeteditor::DocumentHighlight> highlights;
        for (const sl::TokenSpan& token : line_highlight.spans) {
          if (!IsSingleLine(token) || TokenLiteral(*document_, token) != word) {
            continue;
          }
          const uint32_t column = static_cast<uint32_t>(token.range.start.column);
          const uint32_t length = static_cast<uint32_t>(token.range.end.column - token.range.start.column);
          if (length > 0) {
            highlights.push_back({column, length, sweeteditor::DocumentHighlightKind::TEXT});
          }
        }
        if (!highlights.empty()) {
          result.document_highlights.emplace_back(static_cast<uint32_t>(line), std::move(highlights));
        }
        ++line;
      }
    }

    // Token-shaped decorations: TODO/FIXME diagnostics, color inlays, URL
    // links, and code lens on declarations.
    if (slice) {
      size_t line = slice->start_line;
      for (const sl::LineHighlight& line_highlight : slice->lines) {
        std::vector<sweeteditor::InlayHint> inlays;
        std::vector<sweeteditor::Diagnostic> diagnostics;
        std::vector<sweeteditor::CodeLensItem> lenses;
        std::vector<sweeteditor::LinkSpan> links;
        std::vector<sweeteditor::GutterIcon> icons;
        for (const sl::TokenSpan& token : line_highlight.spans) {
          if (!IsSingleLine(token)) {
            continue;
          }
          const uint32_t column = static_cast<uint32_t>(token.range.start.column);
          const uint32_t length = static_cast<uint32_t>(token.range.end.column - token.range.start.column);
          const std::string_view literal = TokenLiteral(*document_, token);

          if (token.style_id == static_cast<int32_t>(ce::EditorStyle::Comment)) {
            const size_t fixme = FindIgnoreCase(literal, "FIXME");
            const size_t todo = FindIgnoreCase(literal, "TODO");
            if (fixme != std::string::npos) {
              diagnostics.push_back(
                  {column + static_cast<uint32_t>(fixme), 5, sweeteditor::DiagnosticSeverity::DIAG_ERROR}
              );
            } else if (todo != std::string::npos) {
              diagnostics.push_back(
                  {column + static_cast<uint32_t>(todo), 4, sweeteditor::DiagnosticSeverity::DIAG_WARNING}
              );
            }
          } else if (token.style_id == static_cast<int32_t>(ce::EditorStyle::String)) {
            int32_t color = 0;
            if (ParseHexColor(literal, color)) {
              // COLOR inlays are not exposed through the decoration interface
              // yet; skip until the editor adds color-swatch support.
              static_cast<void>(color);
            }
          } else if (token.style_id == static_cast<int32_t>(ce::EditorStyle::Url)) {
            if (!literal.empty()) {
              links.push_back({column, length, std::string(literal)});
            }
          } else if (token.style_id == static_cast<int32_t>(ce::EditorStyle::Class) && length > 0) {
            lenses.push_back({static_cast<int32_t>(column), 1, "Run"});
            icons.push_back({1});
          }
        }
        if (!inlays.empty()) {
          result.inlay_hints.emplace_back(static_cast<uint32_t>(line), std::move(inlays));
        }
        if (!diagnostics.empty()) {
          result.diagnostics.emplace_back(static_cast<uint32_t>(line), std::move(diagnostics));
        }
        if (!lenses.empty()) {
          result.code_lens.emplace_back(static_cast<uint32_t>(line), std::move(lenses));
        }
        if (!links.empty()) {
          result.links.emplace_back(static_cast<uint32_t>(line), std::move(links));
        }
        if (!icons.empty()) {
          result.gutter_icons.emplace_back(static_cast<uint32_t>(line), std::move(icons));
        }
        ++line;
      }
    }
  }

  // Host gutter icons (breakpoints) and the demo phantom suggestion.
  if (gutter_icons_) {
    const auto icons = gutter_icons_(context.visible_start_line, context.visible_end_line);
    for (const auto& [line, icon_id] : icons) {
      result.gutter_icons.emplace_back(line, std::vector<sweeteditor::GutterIcon>{{icon_id}});
    }
  }
  if (phantom_) {
    const std::string text = phantom_(0);
    if (!text.empty()) {
      result.phantom_texts.emplace_back(0U, std::vector<sweeteditor::PhantomText>{{0, text}});
    }
  }
  CE_TRACE(
      "provider: spans=%zu lines guides=%zu folds=%zu settled=%d", result.syntax_spans.size(),
      result.indent_guides.size(), result.fold_regions.size(), context.viewport_settled ? 1 : 0
  );
  CE_TRACE(
      "provider: path=%s lines=%zu guides=%zu folds=%zu ch=%zu", analysis_path, result.syntax_spans.size(),
      result.indent_guides.size(), result.fold_regions.size(), context.text_changes.size()
  );
  receiver.Accept(std::move(result));
}

}  // namespace demo
