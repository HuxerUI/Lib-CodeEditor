#include "decoration_pipeline.h"

#include <algorithm>
#include <atomic>
#include <iterator>

namespace huxerui::codeeditor {
namespace detail {

struct DecorationDelivery {
  std::atomic_bool cancelled{false};
  TaskScope scope;
  std::function<void(DecorationResult)> accept;
};

DecorationReceiver DecorationReceiverAccess::Create(TaskScope scope, std::function<void(DecorationResult)> accept) {
  auto delivery = std::make_shared<DecorationDelivery>();
  delivery->scope = std::move(scope);
  delivery->accept = std::move(accept);
  return DecorationReceiver(std::move(delivery));
}

void DecorationReceiverAccess::Cancel(const DecorationReceiver& receiver) {
  receiver.delivery_->cancelled.store(true, std::memory_order_relaxed);
}

}  // namespace detail

DecorationReceiver::DecorationReceiver(std::shared_ptr<detail::DecorationDelivery> delivery)
    : delivery_(std::move(delivery)) {}

bool DecorationReceiver::Accept(DecorationResult result) const {
  if (IsCancelled()) return false;
  delivery_->scope.Post([delivery = delivery_, result = std::move(result)]() mutable {
    if (!delivery->cancelled.load(std::memory_order_relaxed)) delivery->accept(std::move(result));
  });
  return true;
}

bool DecorationReceiver::IsCancelled() const noexcept {
  return !delivery_ || delivery_->cancelled.load(std::memory_order_relaxed);
}

namespace detail {
namespace {

template <class T>
void Append(std::vector<T>& target, const std::vector<T>& values) {
  target.insert(target.end(), values.begin(), values.end());
}

template <class T>
void Append(LineDecorations<T>& target, const LineDecorations<T>& values) {
  for (const auto& [line, items] : values) Append(target[line], items);
}

bool InRange(IntRange range, std::size_t line) {
  return !range.isEmpty() && line >= static_cast<std::size_t>(range.start) &&
         line <= static_cast<std::size_t>(range.end);
}

template <class Data>
bool Accumulate(DecorationUpdate<Data>& target, DecorationUpdate<Data> update, IntRange visible, auto anchor) {
  if (update.values.empty() && update.mode == ApplyMode::Merge) return false;
  if (update.mode == ApplyMode::ReplaceAll) {
    target.values = std::move(update.values);
  } else {
    if (update.mode == ApplyMode::ReplaceRange) {
      std::erase_if(target.values, [&](const auto& entry) { return InRange(visible, anchor(entry)); });
      std::erase_if(update.values, [&](const auto& entry) { return !InRange(visible, anchor(entry)); });
    }
    Append(target.values, update.values);
  }
  target.mode = std::max(target.mode, update.mode);
  return true;
}

template <class Data>
DecorationUpdate<Data> Merge(const std::vector<DecorationResult>& results, DecorationUpdate<Data> DecorationResult::*member) {
  DecorationUpdate<Data> merged;
  for (const auto& result : results) {
    const auto& update = result.*member;
    merged.mode = std::max(merged.mode, update.mode);
    Append(merged.values, update.values);
  }
  return merged;
}

template <class T, class Read, class Clear, class Write>
void ApplyLines(const std::vector<DecorationResult>& results, DecorationUpdate<LineDecorations<T>> DecorationResult::*member,
                DecorationResult& base, DecorationResult& published, std::size_t count, IntRange visible,
                Read read, Clear clear, Write write) {
  auto update = Merge(results, member);
  auto& previous = published.*member;
  auto& baseline = (base.*member).values;
  auto& values = update.values;
  std::erase_if(values, [count](const auto& entry) { return entry.first >= count; });

  if (update.mode == ApplyMode::ReplaceAll && previous.mode != ApplyMode::ReplaceAll) clear();

  // An empty replacement removes the previous snapshot's entries as well.
  if (update.mode == ApplyMode::ReplaceRange && !visible.isEmpty()) {
    for (std::size_t line = static_cast<std::size_t>(visible.start);
         line < count && InRange(visible, line); ++line) {
      values.try_emplace(line);
    }
  }
  if (update.mode == ApplyMode::ReplaceAll && previous.mode == ApplyMode::ReplaceAll) {
    for (const auto& [line, items] : previous.values) values.try_emplace(line);
  }

  std::vector<std::pair<std::size_t, std::vector<T>>> entries;
  entries.reserve(values.size());
  for (const auto& [line, items] : values) {
    if (update.mode == ApplyMode::Merge || (update.mode == ApplyMode::ReplaceRange && !InRange(visible, line))) {
      const auto original = baseline.try_emplace(line, read(line)).first;
      auto combined = original->second;
      Append(combined, items);
      entries.emplace_back(line, std::move(combined));
    } else {
      entries.emplace_back(line, items);
    }
  }
  if (!entries.empty()) write(std::move(entries));
  previous = std::move(update);
}

template <class T, class Read, class Anchor, class Write>
void ApplyArray(const std::vector<DecorationResult>& results, DecorationUpdate<std::vector<T>> DecorationResult::*member,
                DecorationResult& base, IntRange visible, Read read, Anchor anchor, Write write) {
  auto& baseline = base.*member;
  if (baseline.mode == ApplyMode::Merge) {
    baseline.values = read();
    baseline.mode = ApplyMode::ReplaceAll;
  }
  auto update = Merge(results, member);
  auto values = baseline.values;
  if (update.mode == ApplyMode::ReplaceRange) {
    std::erase_if(values, [&](const T& item) { return InRange(visible, anchor(item)); });
  } else if (update.mode == ApplyMode::ReplaceAll) {
    values.clear();
  }
  Append(values, update.values);
  write(std::move(values));
}

}  // namespace

DecorationBatch::DecorationBatch(IntRange visible, std::vector<std::set<DecorationType>> capabilities)
    : visible_(visible), capabilities_(std::move(capabilities)), results_(capabilities_.size()) {}

void DecorationBatch::Accept(std::size_t provider, DecorationResult result) {
  auto& target = results_[provider];
  const auto& types = capabilities_[provider];
  const auto apply = [&]<class Data>(DecorationType type, DecorationUpdate<Data> DecorationResult::*member, auto anchor) {
    if (types.contains(type) && Accumulate(target.*member, std::move(result.*member), visible_, anchor)) changed_.insert(type);
  };
  const auto line = [](const auto& entry) { return entry.first; };
  apply(DecorationType::SyntaxHighlight, &DecorationResult::syntax_spans, line);
  apply(DecorationType::SemanticHighlight, &DecorationResult::semantic_spans, line);
  apply(DecorationType::OverlayHighlight, &DecorationResult::overlay_spans, line);
  apply(DecorationType::InlayHint, &DecorationResult::inlay_hints, line);
  apply(DecorationType::Diagnostic, &DecorationResult::diagnostics, line);
  apply(DecorationType::DocumentHighlight, &DecorationResult::document_highlights, line);
  apply(DecorationType::GutterIcon, &DecorationResult::gutter_icons, line);
  apply(DecorationType::PhantomText, &DecorationResult::phantom_texts, line);
  apply(DecorationType::CodeLens, &DecorationResult::code_lens_items, line);
  apply(DecorationType::Link, &DecorationResult::links, line);
  apply(DecorationType::IndentGuide, &DecorationResult::indent_guides, [](const auto& guide) { return guide.start.line; });
  apply(DecorationType::BracketGuide, &DecorationResult::bracket_guides, [](const auto& guide) { return guide.parent.line; });
  apply(DecorationType::FlowGuide, &DecorationResult::flow_guides, [](const auto& guide) { return guide.start.line; });
  apply(DecorationType::SeparatorGuide, &DecorationResult::separator_guides, [](const auto& guide) { return guide.line; });
  apply(DecorationType::FoldRegion, &DecorationResult::fold_regions, [](const auto& fold) { return fold.start_line; });
}

void DecorationBatch::Apply(sweeteditor::EditorCore& core, const sweeteditor::Document& document, const Dispatch& dispatch) {
  const auto& decorations = document.getDecorations();
  const auto count = document.getLineCount();
  for (const auto type : std::exchange(changed_, {})) {
    switch (type) {
    case DecorationType::SyntaxHighlight:
      ApplyLines(results_, &DecorationResult::syntax_spans, base_, published_, count, visible_,
                 [&](std::size_t line) -> const auto& { return decorations.getLineSpans(line, SpanLayer::SYNTAX); },
                 [&] { dispatch(core.clearHighlights(SpanLayer::SYNTAX)); },
                 [&](auto entries) { dispatch(core.setBatchLineSpans(SpanLayer::SYNTAX, std::move(entries))); });
      break;
    case DecorationType::SemanticHighlight:
      ApplyLines(results_, &DecorationResult::semantic_spans, base_, published_, count, visible_,
                 [&](std::size_t line) -> const auto& { return decorations.getLineSpans(line, SpanLayer::SEMANTIC); },
                 [&] { dispatch(core.clearHighlights(SpanLayer::SEMANTIC)); },
                 [&](auto entries) { dispatch(core.setBatchLineSpans(SpanLayer::SEMANTIC, std::move(entries))); });
      break;
    case DecorationType::OverlayHighlight:
      ApplyLines(results_, &DecorationResult::overlay_spans, base_, published_, count, visible_,
                 [&](std::size_t line) -> const auto& { return decorations.getLineSpans(line, SpanLayer::OVERLAY); },
                 [&] { dispatch(core.clearHighlights(SpanLayer::OVERLAY)); },
                 [&](auto entries) { dispatch(core.setBatchLineSpans(SpanLayer::OVERLAY, std::move(entries))); });
      break;
    case DecorationType::InlayHint:
      ApplyLines(results_, &DecorationResult::inlay_hints, base_, published_, count, visible_,
                 [&](std::size_t line) -> const auto& { return decorations.getLineInlayHints(line); },
                 [&] { dispatch(core.clearInlayHints()); },
                 [&](auto entries) { dispatch(core.setBatchLineInlayHints(std::move(entries))); });
      break;
    case DecorationType::Diagnostic:
      ApplyLines(results_, &DecorationResult::diagnostics, base_, published_, count, visible_,
                 [&](std::size_t line) -> const auto& { return decorations.getLineDiagnostics(line); },
                 [&] { dispatch(core.clearDiagnostics()); },
                 [&](auto entries) { dispatch(core.setBatchLineDiagnostics(std::move(entries))); });
      break;
    case DecorationType::DocumentHighlight:
      ApplyLines(results_, &DecorationResult::document_highlights, base_, published_, count, visible_,
                 [&](std::size_t line) -> const auto& { return decorations.getLineDocumentHighlights(line); },
                 [&] { dispatch(core.clearDocumentHighlights()); },
                 [&](auto entries) { dispatch(core.setBatchLineDocumentHighlights(std::move(entries))); });
      break;
    case DecorationType::GutterIcon:
      ApplyLines(results_, &DecorationResult::gutter_icons, base_, published_, count, visible_,
                 [&](std::size_t line) -> const auto& { return decorations.getLineGutterIcons(line); },
                 [&] { dispatch(core.clearGutterIcons()); },
                 [&](auto entries) { dispatch(core.setBatchLineGutterIcons(std::move(entries))); });
      break;
    case DecorationType::PhantomText:
      ApplyLines(results_, &DecorationResult::phantom_texts, base_, published_, count, visible_,
                 [&](std::size_t line) -> const auto& { return decorations.getLinePhantomTexts(line); },
                 [&] { dispatch(core.clearPhantomTexts()); },
                 [&](auto entries) { dispatch(core.setBatchLinePhantomTexts(std::move(entries))); });
      break;
    case DecorationType::CodeLens:
      ApplyLines(results_, &DecorationResult::code_lens_items, base_, published_, count, visible_,
                 [&](std::size_t line) -> const auto& { return decorations.getLineCodeLens(line); },
                 [&] { dispatch(core.clearCodeLens()); },
                 [&](auto entries) { dispatch(core.setBatchLineCodeLens(std::move(entries))); });
      break;
    case DecorationType::Link:
      ApplyLines(results_, &DecorationResult::links, base_, published_, count, visible_,
                 [&](std::size_t line) -> const auto& { return decorations.getLineLinks(line); },
                 [&] { dispatch(core.clearLinks()); },
                 [&](auto entries) { dispatch(core.setBatchLineLinks(std::move(entries))); });
      break;
    case DecorationType::IndentGuide:
      ApplyArray(results_, &DecorationResult::indent_guides, base_, visible_,
                 [&]() -> const auto& { return decorations.getIndentGuides(); },
                 [](const IndentGuide& item) { return item.start.line; },
                 [&](auto entries) { dispatch(core.setIndentGuides(std::move(entries))); });
      break;
    case DecorationType::BracketGuide:
      ApplyArray(results_, &DecorationResult::bracket_guides, base_, visible_,
                 [&]() -> const auto& { return decorations.getBracketGuides(); },
                 [](const BracketGuide& item) { return item.parent.line; },
                 [&](auto entries) { dispatch(core.setBracketGuides(std::move(entries))); });
      break;
    case DecorationType::FlowGuide:
      ApplyArray(results_, &DecorationResult::flow_guides, base_, visible_,
                 [&]() -> const auto& { return decorations.getFlowGuides(); },
                 [](const FlowGuide& item) { return item.start.line; },
                 [&](auto entries) { dispatch(core.setFlowGuides(std::move(entries))); });
      break;
    case DecorationType::SeparatorGuide:
      ApplyArray(results_, &DecorationResult::separator_guides, base_, visible_,
                 [&]() -> const auto& { return decorations.getSeparatorGuides(); },
                 [](const SeparatorGuide& item) { return item.line; },
                 [&](auto entries) { dispatch(core.setSeparatorGuides(std::move(entries))); });
      break;
    case DecorationType::FoldRegion:
      ApplyArray(results_, &DecorationResult::fold_regions, base_, visible_,
                 [&]() -> const auto& { return decorations.getFoldRegions(); },
                 [](const FoldRegion& item) { return item.start_line; },
                 [&](auto entries) { dispatch(core.setFoldRegions(std::move(entries))); });
      break;
    }
  }
}

void DecorationBatch::Clear(DecorationType type, sweeteditor::EditorCore& core, const Dispatch& dispatch) {
  switch (type) {
  case DecorationType::SyntaxHighlight: dispatch(core.clearHighlights(SpanLayer::SYNTAX)); break;
  case DecorationType::SemanticHighlight: dispatch(core.clearHighlights(SpanLayer::SEMANTIC)); break;
  case DecorationType::OverlayHighlight: dispatch(core.clearHighlights(SpanLayer::OVERLAY)); break;
  case DecorationType::InlayHint: dispatch(core.clearInlayHints()); break;
  case DecorationType::Diagnostic: dispatch(core.clearDiagnostics()); break;
  case DecorationType::DocumentHighlight: dispatch(core.clearDocumentHighlights()); break;
  case DecorationType::GutterIcon: dispatch(core.clearGutterIcons()); break;
  case DecorationType::PhantomText: dispatch(core.clearPhantomTexts()); break;
  case DecorationType::CodeLens: dispatch(core.clearCodeLens()); break;
  case DecorationType::Link: dispatch(core.clearLinks()); break;
  case DecorationType::IndentGuide: dispatch(core.setIndentGuides({})); break;
  case DecorationType::BracketGuide: dispatch(core.setBracketGuides({})); break;
  case DecorationType::FlowGuide: dispatch(core.setFlowGuides({})); break;
  case DecorationType::SeparatorGuide: dispatch(core.setSeparatorGuides({})); break;
  case DecorationType::FoldRegion: dispatch(core.setFoldRegions({})); break;
  }
}

}  // namespace detail
}  // namespace huxerui::codeeditor
