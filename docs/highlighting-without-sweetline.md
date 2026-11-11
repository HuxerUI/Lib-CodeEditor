# Custom syntax highlighting

A decoration provider can analyze a snapshot and publish syntax spans by logical line. This small provider highlights `return` at the start of each visible line. It uses UTF-16 offsets for the Core spans and requests replacement within the visible range.

```cpp
#include <huxerui/codeeditor.h>

namespace ce = huxerui::codeeditor;

class ReturnHighlighter final : public ce::DecorationProvider {
 public:
  std::set<ce::DecorationType> GetCapabilities() const override {
    return {ce::DecorationType::SyntaxHighlight};
  }

  huxerui::Task<void> ProvideDecorations(ce::DecorationContext context,
                                        ce::DecorationReceiver receiver) override {
    ce::DecorationResult result;
    result.syntax_spans.mode = ce::ApplyMode::ReplaceRange;
    const ce::Document snapshot(*context.document_text);
    for (auto line = context.visible_line_range.start; line <= context.visible_line_range.end; ++line) {
      const auto text = snapshot.GetLineText(line);
      if (text == "return" || text.starts_with("return ") || text.starts_with("return;")) {
        result.syntax_spans.values[line].push_back(
            {0, 6, static_cast<std::uint32_t>(ce::EditorStyle::Keyword)});
      }
    }
    receiver.Accept(std::move(result));
    co_return;
  }
};
```

Add a stable provider instance to `EditorOptions::decoration_providers` or call `AddDecorationProvider` from the first `EditorEvents::DocumentLoaded` event or a later UI callback. For longer analysis, use `RunWorker` or `WorkerSequence` with the owned snapshot, then publish through the receiver. Keep UI state reads and writes on the UI thread.

The [preview provider](../examples/preview/src/sweetline_provider.cpp) uses SweetLine for incremental viewport analysis, syntax, diagnostics, folds, and other adornments. It rebuilds from the snapshot if an earlier analysis was cancelled before reaching the requested revision.
