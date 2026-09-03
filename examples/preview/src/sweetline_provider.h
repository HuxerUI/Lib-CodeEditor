#pragma once

#include <huxerui/codeeditor.h>

#include <sweetline/highlight.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace demo {

// Optional SweetLine-backed decoration provider for the CodeEditor demo. It
// shows how a concrete highlighting engine plugs into the editor through the
// unified EditorDecorationProvider interface instead of being built in:
// the adapter keeps its own incremental analyzer and turns each refresh into
// syntax spans, indent guides, fold regions, rainbow brackets, bracket
// matching, document highlights, diagnostics, links, inlay hints, code lens,
// gutter icons, and phantom text.
class SweetLineDecorationProvider final : public huxerui::codeeditor::EditorDecorationProvider {
 public:
  using GutterIconSource =
      std::function<std::vector<std::pair<uint32_t, int32_t>>(uint32_t start_line, uint32_t end_line)>;
  using PhantomSource = std::function<std::string(uint32_t line)>;

  SweetLineDecorationProvider(
      std::string syntax_json, std::string initial_text, std::string document_key, GutterIconSource gutter_icons,
      PhantomSource phantom
  );

  huxerui::codeeditor::EditorDecorationResult ProvideDecorations(
      const huxerui::codeeditor::EditorDecorationContext& context
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
