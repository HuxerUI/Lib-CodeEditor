#pragma once

#include <sweetedit_core/sweet_editor.h>

#include <sweeteditor/decoration.h>
#include <sweeteditor/editor_core.h>
#include <sweetline/highlight.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sweetedit_huxer {

// Style IDs shared between SweetLine's registerStyleName and SweetEditor's
// registerTextStyle so a highlight token's style_id resolves to one color.
enum SweetStyleId : uint32_t {
  kStyleKeyword = 1,
  kStyleType = 2,
  kStyleClass = 3,
  kStyleFunction = 4,
  kStyleVariable = 5,
  kStyleString = 6,
  kStyleNumber = 7,
  kStyleComment = 8,
  kStylePreprocessor = 9,
  kStyleBuiltin = 10,
  kStylePunctuation = 11,
  kStyleAnnotation = 12,
  kStyleUrl = 13,
};

// Bridges the SweetLine native highlighting engine to SweetEditor's decoration
// API using the managed, incremental DocumentAnalyzer and viewport slicing —
// the same shape the other SweetEditor platform integrations use.
class SweetLineHighlighter {
public:
  // document_key is used as the SweetLine document uri so loadDocument can
  // route the compiled syntax rule by file suffix; a mismatched uri makes
  // loadDocument return null (and the analyzer unusable).
  SweetLineHighlighter(std::string syntax_json, std::string initial_text, std::string document_key);

  void RegisterStyles(sweeteditor::EditorCore& core);
  // Analyze enough lines to cover the current viewport and publish those spans.
  void RefreshVisible(sweeteditor::EditorCore& core);
  // Syntax-highlight-only path for scrolling: publishes spans but defers
  // decorations/document highlights until the viewport settles.
  void PublishVisible(sweeteditor::EditorCore& core);
  // Recompute decorations, indent guides, and document highlights for the
  // current viewport slice (called once the viewport is stable).
  void RefreshVisibleDecorations(sweeteditor::EditorCore& core);
  // Host decoration providers merged during RefreshDecorations.
  void SetHostDecorationProviders(std::vector<HostDecorationProvider> providers);
  // Host gutter icons (breakpoints/errors) merged during decoration refresh.
  void SetGutterIconProvider(GutterIconProvider provider);
  // Publishes fold regions computed over the whole document (called once after
  // the document loads so fold arrows cover every foldable line).
  void PublishFoldRegions(sweeteditor::EditorCore& core);
  // Rainbow brackets: publishes depth-colored bracket spans and bracket guides
  // for the viewport (called with the stable viewport slice).
  void RefreshBracketDecorations(sweeteditor::EditorCore& core);
  // Incrementally re-analyze after edits and publish the updated viewport slice.
  void ApplyChanges(sweeteditor::EditorCore& core, const std::vector<sweeteditor::TextChange>& changes);
  // Re-resolve the bracket pair under the cursor and publish it to the editor.
  void RefreshBracketMatch(sweeteditor::EditorCore& core);
  // Re-highlight every token matching the word under the cursor in the viewport.
  void RefreshDocumentHighlights(
      sweeteditor::EditorCore& core, const sweetline::SharedPtr<sweetline::DocumentHighlightSlice>& slice
  );

private:
  void PublishSlice(
      sweeteditor::EditorCore& core, const sweetline::SharedPtr<sweetline::DocumentHighlightSlice>& slice
  );
  void RefreshIndentGuides(sweeteditor::EditorCore& core);
  void RefreshDecorations(
      sweeteditor::EditorCore& core, const sweetline::SharedPtr<sweetline::DocumentHighlightSlice>& slice
  );

  sweetline::SharedPtr<sweetline::HighlightEngine> engine_;
  sweetline::SharedPtr<sweetline::Document> document_;
  sweetline::SharedPtr<sweetline::DocumentAnalyzer> analyzer_;
  // Fold regions are published once; the core owns the user-visible fold state
  // afterwards (re-publishing would reset interactive toggles).
  bool fold_regions_initialized_{false};
  std::vector<HostDecorationProvider> host_decoration_providers_;
  GutterIconProvider gutter_icon_provider_;
};

}  // namespace sweetedit_huxer
