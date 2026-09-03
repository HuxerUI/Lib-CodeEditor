#pragma once

#include <huxerui/codeeditor.h>

#include <sweetline/highlight.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace demo {

// Optional SweetLine-backed decoration provider for the CodeEditor demo,
// demonstrating the receiver pattern through the unified
// EditorDecorationProvider interface.
class SweetLineDecorationProvider final : public huxerui::codeeditor::EditorDecorationProvider {
 public:
  using GutterIconSource =
      std::function<std::vector<std::pair<size_t, int32_t>>(size_t start_line, size_t end_line)>;
  using PhantomSource = std::function<std::string(size_t line)>;

  SweetLineDecorationProvider(
      std::string syntax_json, std::string initial_text, std::string document_key, GutterIconSource gutter_icons,
      PhantomSource phantom
  );

  void ProvideDecorations(
      const huxerui::codeeditor::EditorDecorationContext& context,
      huxerui::codeeditor::EditorDecorationReceiver& receiver
  ) override;

 private:
  void RebuildDocument(const std::string& text);

  std::string syntax_json_;
  std::string document_key_;
  std::string synced_text_;
  sweetline::SharedPtr<sweetline::HighlightEngine> engine_;
  sweetline::SharedPtr<sweetline::Document> document_;
  sweetline::SharedPtr<sweetline::DocumentAnalyzer> analyzer_;
  bool fold_regions_published_{false};
  GutterIconSource gutter_icons_;
  PhantomSource phantom_;
};

}  // namespace demo
