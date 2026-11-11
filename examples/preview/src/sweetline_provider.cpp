#include "sweetline_provider.h"

#include <algorithm>
#include <string_view>

#include <sweeteditor/utility.h>
#include <sweetline/highlight.h>

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

}  // namespace

struct SweetLineDecorationProvider::Analysis {
  std::string syntax_json;
  std::string document_key;
  sl::SharedPtr<sl::HighlightEngine> engine;
  sl::SharedPtr<sl::Document> document;
  sl::SharedPtr<sl::DocumentAnalyzer> analyzer;
  std::uint64_t document_generation = 0;
  std::optional<std::uint64_t> revision;
  std::optional<std::vector<ce::FoldRegion>> folds;

  ce::DecorationResult Analyze(const ce::DecorationContext& context, std::stop_token stop = {});
};

SweetLineDecorationProvider::SweetLineDecorationProvider(std::string syntax_json, std::string document_key,
                                                         GutterIconSource gutter_icons)
    : analysis_(std::make_shared<Analysis>()), gutter_icons_(std::move(gutter_icons)) {
  analysis_->syntax_json = std::move(syntax_json);
  analysis_->document_key = std::move(document_key);
}

std::set<ce::DecorationType> SweetLineDecorationProvider::GetCapabilities() const {
  using Type = ce::DecorationType;
  return {Type::SyntaxHighlight, Type::OverlayHighlight, Type::IndentGuide, Type::FoldRegion, Type::DocumentHighlight,
          Type::Diagnostic, Type::InlayHint, Type::CodeLens, Type::Link, Type::GutterIcon};
}

ce::DecorationResult SweetLineDecorationProvider::Analysis::Analyze(const ce::DecorationContext& context,
                                                                    std::stop_token stop) {
  ce::DecorationResult result;
  if (stop.stop_requested() || !context.document_text || context.visible_line_range.isEmpty()) return result;
  if (!engine) {
    auto next = std::make_shared<sl::HighlightEngine>();
    RegisterEngineStyles(next);
    next->compileSyntaxFromJson(syntax_json);
    engine = std::move(next);
  }
  const auto visible = context.visible_line_range;
  const sl::LineRange viewport{static_cast<std::size_t>(visible.start), static_cast<std::size_t>(visible.length())};
  const auto requested = context.requested_line_range;
  const sl::LineRange overscan{static_cast<std::size_t>(requested.start), static_cast<std::size_t>(requested.length())};
  sl::SharedPtr<sl::DocumentHighlightSlice> slice;
  if (!revision || context.document_generation != document_generation || *revision != context.text_revision) {
    const bool incremental = analyzer && context.document_generation == document_generation &&
        revision == context.previous_text_revision && !context.text_changes.empty();
    revision.reset();
    folds.reset();
    if (incremental) {
      for (const auto& change : context.text_changes) {
        if (stop.stop_requested()) return result;
        slice = analyzer->analyzeIncrementalInLineRange(
            {{change.range.start.line, change.range.start.column}, {change.range.end.line, change.range.end.column}},
            change.new_text, overscan);
      }
    } else {
      engine->removeDocument(document_key);
      document = std::make_shared<sl::Document>(document_key, *context.document_text);
      analyzer = engine->loadDocument(document);
    }
    document_generation = context.document_generation;
    revision = context.text_revision;
  }
  if (!analyzer || stop.stop_requested()) return result;
  if (!slice) slice = analyzer->analyzeLineRange(overscan);
  const std::size_t total_lines = document->getLineCount();
  result.syntax_spans.mode = ce::ApplyMode::ReplaceRange;
  result.indent_guides.mode = ce::ApplyMode::ReplaceAll;

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
        result.syntax_spans.values.emplace(line, std::move(spans));
      }
      ++line;
    }
  }

  // Indent guides follow the viewport immediately.
  if (const sl::SharedPtr<sl::IndentGuideResult> guides = analyzer->analyzeIndentGuidesInLineRange(viewport)) {
    for (const sl::IndentGuideLine& guide : guides->guide_lines) {
      if (guide.start_line < 0 || guide.end_line < guide.start_line) {
        continue;
      }
      const auto column = static_cast<std::size_t>(std::max(0, guide.column));
      result.indent_guides.values.push_back({{static_cast<std::size_t>(guide.start_line), column},
                                            {static_cast<std::size_t>(guide.end_line), column}});
    }
  }

  if (stop.stop_requested()) return {};
  if (context.viewport_settled) {
    if (!folds) {
      folds.emplace();
      if (const auto full = analyzer->analyzeIndentGuidesInLineRange({0, total_lines})) {
        for (const auto& guide : full->guide_lines) {
          if (guide.start_line >= 0 && guide.end_line > guide.start_line) {
            folds->push_back({static_cast<std::size_t>(guide.start_line), static_cast<std::size_t>(guide.end_line)});
          }
        }
      }
    }
    result.fold_regions = {*folds, ce::ApplyMode::ReplaceAll};
  }

  // Heavy decorations wait for the settled viewport.
  if (context.viewport_settled && slice) {
    result.overlay_spans.mode = ce::ApplyMode::ReplaceRange;
    result.document_highlights.mode = ce::ApplyMode::ReplaceRange;
    result.inlay_hints.mode = ce::ApplyMode::ReplaceRange;
    result.diagnostics.mode = ce::ApplyMode::ReplaceRange;
    result.code_lens_items.mode = ce::ApplyMode::ReplaceRange;
    result.links.mode = ce::ApplyMode::ReplaceRange;
    result.gutter_icons.mode = ce::ApplyMode::ReplaceRange;
    // Rainbow brackets over the viewport.
    if (const sl::SharedPtr<sl::BracketPairResult> brackets = analyzer->analyzeBracketPairsInLineRange(viewport)) {
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
          result.overlay_spans.values.emplace(line, std::move(rainbow));
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
          if (token.range.start.line == context.cursor_position.line && token.range.start.column <= context.cursor_position.column &&
              context.cursor_position.column <= token.range.end.column) {
            const std::string_view literal = TokenLiteral(*document, token);
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
          if (!IsSingleLine(token) || TokenLiteral(*document, token) != word) {
            continue;
          }
          const uint32_t column = static_cast<uint32_t>(token.range.start.column);
          const uint32_t length = static_cast<uint32_t>(token.range.end.column - token.range.start.column);
          if (length > 0) {
            highlights.push_back({column, length, sweeteditor::DocumentHighlightKind::TEXT});
          }
        }
        if (!highlights.empty()) {
          result.document_highlights.values.emplace(line, std::move(highlights));
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
          const std::string_view literal = TokenLiteral(*document, token);

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
              inlays.push_back({ce::InlayType::COLOR, column, color, {}});
            }
          } else if (token.style_id == static_cast<int32_t>(ce::EditorStyle::Url)) {
            if (!literal.empty()) {
              links.push_back({column, length, std::string(literal)});
            }
          } else if (token.style_id == static_cast<int32_t>(ce::EditorStyle::Class) && length > 0) {
            lenses.push_back({static_cast<int32_t>(column), 1, "Run"});
            icons.push_back({kTypeGutterIcon});
          } else if (token.style_id == static_cast<int32_t>(ce::EditorStyle::Annotation) && length > 0) {
            icons.push_back({kAnnotationGutterIcon});
          }
        }
        if (!inlays.empty()) {
          result.inlay_hints.values.emplace(line, std::move(inlays));
        }
        if (!diagnostics.empty()) {
          result.diagnostics.values.emplace(line, std::move(diagnostics));
        }
        if (!lenses.empty()) {
          result.code_lens_items.values.emplace(line, std::move(lenses));
        }
        if (!links.empty()) {
          result.links.values.emplace(line, std::move(links));
        }
        if (!icons.empty()) {
          result.gutter_icons.values.emplace(line, std::move(icons));
        }
        ++line;
      }
    }
  }

  return result;
}

huxerui::Task<void> SweetLineDecorationProvider::ProvideDecorations(ce::DecorationContext context,
                                                                   ce::DecorationReceiver receiver) {
  const auto visible = context.visible_line_range;
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
  auto result = analysis_->Analyze(context);
#else
  auto result = co_await worker_.Run([](std::stop_token stop, std::shared_ptr<Analysis> analysis,
                                       ce::DecorationContext request) {
    return analysis->Analyze(request, stop);
  }, analysis_, std::move(context));
#endif
  if (receiver.IsCancelled()) co_return;
  if (gutter_icons_ && result.gutter_icons.mode == ce::ApplyMode::ReplaceRange) {
    for (const auto& [line, icon] : gutter_icons_(visible.start, visible.end)) {
      auto& icons = result.gutter_icons.values[line];
      icons.insert(icons.begin(), ce::GutterIcon{icon});
    }
  }
  receiver.Accept(std::move(result));
}

}  // namespace demo
