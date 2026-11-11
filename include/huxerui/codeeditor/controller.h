#pragma once

/// @file
/// @brief Imperative commands and queries for a mounted CodeEditor View.

#include <source_location>

#include <huxerui/codeeditor/events.h>
#include <huxerui/codeeditor/theme.h>

namespace huxerui::codeeditor {

namespace detail {
struct EditorControllerAccess;
}  // namespace detail

/// @brief UI-thread handle to one CodeEditor View.
///
/// Copies share the same binding. The View owns the editing session and
/// accepts commands from the first EditorEvents::DocumentLoaded notification.
/// A new mount requires a new controller. Before that notification and after
/// unmounting, commands return false and queries return empty or default values.
///
/// A successful command indicates that the session handled the request.
/// TextChanged reports the changes produced by a document edit.
class EditorController {
 public:
  /// @brief Creates a handle that can be bound to one CodeEditor View.
  /// Use UseCodeEditor() to retain a handle across composition.
  EditorController();

  /// @brief Replaces the active document and publishes EditorEvents::DocumentLoaded.
  /// Resets caret, selection, undo history, and the platform text-input session.
  /// @param document Shared document handle to display.
  bool LoadDocument(Document document) const;

  /// @brief Reads the active document handle.
  /// @return The shared document, or an empty value outside the mounted session.
  [[nodiscard]] std::optional<Document> GetDocument() const;

  /// @brief Applies a palette and registers its text styles for the mounted session.
  /// @param theme Colors, caret width, and syntax styles.
  /// @throws std::invalid_argument If the caret width is nonpositive or nonfinite.
  bool ApplyTheme(EditorTheme theme) const;

  /// @brief Returns a copy of the active palette, or an empty value outside the mounted session.
  [[nodiscard]] std::optional<EditorTheme> GetTheme() const;

  /// @brief Obtains the settings handle for the active session.
  /// @return An empty value outside the mounted session.
  [[nodiscard]] std::optional<EditorSettingsHandle> GetSettings() const;

  /// @brief Replaces keyboard bindings and custom command handlers.
  /// @param key_map Bindings to install for subsequent key input.
  bool SetKeyMap(EditorKeyMap key_map) const;

  /// @brief Returns a copy of the active key map, or an empty value outside the mounted session.
  [[nodiscard]] std::optional<EditorKeyMap> GetKeyMap() const;

  /// @brief Configures icon lookup for gutter decorations, inlay hints, and selection menus.
  /// @param provider Resolver on the UI thread. An empty resolver restores built-in presentation.
  bool SetEditorIconProvider(EditorIconProvider provider) const;

  /// @brief Inserts text at the caret or replaces the active selection.
  /// @param text UTF-8 text to insert.
  bool InsertText(std::string text) const;

  /// @brief Inserts text at a document position.
  /// @param position Zero-based logical line and UTF-16 column.
  /// @param text UTF-8 text to insert.
  bool InsertTextAt(TextPosition position, std::string text) const;

  /// @brief Replaces a document range with text.
  /// @param range Half-open range in the current document.
  /// @param text UTF-8 replacement text.
  bool ReplaceText(TextRange range, std::string text) const;

  /// @brief Removes the text in a document range.
  /// @param range Half-open range in the current document.
  bool DeleteText(TextRange range) const;

  /// @brief Applies a nonconflicting batch as one undoable edit.
  /// The first edit determines the resulting caret after the other edits are accounted for.
  /// @param edits Replacements expressed in the document coordinates before the batch.
  /// @return True if handled. Conflicting edits and read-only sessions reject the batch.
  bool ApplyTextEdits(std::vector<TextEdit> edits) const;

  /// @brief Moves the current line or selected line block up by one logical line.
  bool MoveLineUp() const;

  /// @brief Moves the current line or selected line block down by one logical line.
  bool MoveLineDown() const;

  /// @brief Duplicates the current line or selected line block above the original.
  bool CopyLineUp() const;

  /// @brief Duplicates the current line or selected line block below the original.
  bool CopyLineDown() const;

  /// @brief Removes the current line or selected line block.
  bool DeleteLine() const;

  /// @brief Inserts a new line before the caret line.
  bool InsertLineAbove() const;

  /// @brief Inserts a new line after the caret line.
  bool InsertLineBelow() const;

  /// @brief Reverses the most recent undoable document edit.
  bool Undo() const;

  /// @brief Reapplies the most recently undone document edit.
  bool Redo() const;

  /// @brief Reports whether the session has an undo history entry.
  [[nodiscard]] bool CanUndo() const;

  /// @brief Reports whether the session has a redo history entry.
  [[nodiscard]] bool CanRedo() const;

  /// @brief Writes the selected text to the platform clipboard.
  /// @return True if the platform clipboard accepted the selection.
  bool CopyToClipboard() const;

  /// @brief Reads text from the platform clipboard and inserts it at the caret or replaces the selection.
  /// @return True if the edit was handled. Returns false if the clipboard text is unavailable or editing is disabled.
  bool PasteFromClipboard() const;

  /// @brief Writes the selection to the platform clipboard and then removes it from the document.
  /// @return True if the selection was copied and the removal was handled.
  bool CutToClipboard() const;

  /// @brief Selects the entire current document.
  bool SelectAll() const;

  /// @brief Returns the selected text as UTF-8, or an empty string when no selection is available.
  [[nodiscard]] std::string GetSelectedText() const;

  /// @brief Sets the selection and makes its end the active caret.
  /// @param range Anchor and active end in document coordinates. A collapsed range sets the caret.
  bool SetSelection(TextRange range) const;

  /// @brief Reads the current noncollapsed selection.
  /// @return Anchor and active end, or an empty value when there is no selection.
  [[nodiscard]] std::optional<TextRange> GetSelection() const;

  /// @brief Moves the caret and clears the selection.
  /// @param position Zero-based logical line and UTF-16 column.
  bool SetCursorPosition(TextPosition position) const;

  /// @brief Returns the active caret position, or an empty value outside the mounted session.
  [[nodiscard]] std::optional<TextPosition> GetCursorPosition() const;

  /// @brief Returns the word boundary range at the active caret.
  /// @return A half-open range that may be collapsed, or an empty value outside the mounted session.
  [[nodiscard]] std::optional<TextRange> GetWordRangeAtCursor() const;

  /// @brief Returns the word at the caret as UTF-8, or an empty string when unavailable.
  [[nodiscard]] std::string GetWordAtCursor() const;

  /// @brief Moves the caret, clears selection, and scrolls to reveal the target.
  /// @param position Zero-based logical line and UTF-16 column.
  bool GotoPosition(TextPosition position) const;

  /// @brief Scrolls the viewport to a logical document line.
  /// @param line Zero-based target line. Values past the document end use the last line.
  /// @param behavior Alignment within the viewport, subject to scroll bounds.
  bool ScrollToLine(std::size_t line, ScrollBehavior behavior = ScrollBehavior::GOTO_CENTER) const;

  /// @brief Sets the viewport's horizontal and vertical scroll offsets.
  /// @param x Horizontal offset in logical layout units.
  /// @param y Vertical offset in logical layout units.
  /// @throws std::invalid_argument If either offset is nonfinite.
  bool SetScroll(float x, float y) const;

  /// @brief Returns a snapshot of scroll offsets and bounds, or an empty value outside the mounted session.
  [[nodiscard]] std::optional<ScrollMetrics> GetScrollMetrics() const;

  /// @brief Resolves a document position to an editor-local caret rectangle in logical layout units.
  /// @param position Zero-based logical line and UTF-16 column.
  /// @return The rectangle, or an empty value when the line or session is unavailable.
  [[nodiscard]] std::optional<Rect> GetPositionRect(TextPosition position) const;

  /// @brief Returns the active caret rectangle in editor-local logical layout units.
  /// @return An empty value outside the mounted session.
  [[nodiscard]] std::optional<Rect> GetCursorRect() const;

  /// @brief Toggles the fold anchored at a logical line.
  /// @param line Zero-based start line of the fold.
  bool ToggleFold(std::size_t line) const;

  /// @brief Collapses the fold anchored at a logical line.
  /// @param line Zero-based start line of the fold.
  bool FoldAt(std::size_t line) const;

  /// @brief Expands the fold anchored at a logical line.
  /// @param line Zero-based start line of the fold.
  bool UnfoldAt(std::size_t line) const;

  /// @brief Reports whether a valid logical line is visible with the current folds.
  /// @param line Zero-based line to query.
  [[nodiscard]] bool IsLineVisible(std::size_t line) const;

  /// @brief Collapses all configured folds.
  bool FoldAll() const;

  /// @brief Expands all configured folds.
  bool UnfoldAll() const;

  /// @brief Starts a document search and updates the search state.
  /// @param request UTF-8 query and search options.
  bool Search(SearchRequest request) const;

  /// @brief Selects and reveals the next available search match.
  bool FindNextSearchMatch() const;

  /// @brief Selects and reveals the previous available search match.
  bool FindPreviousSearchMatch() const;

  /// @brief Replaces the current search match.
  /// @param replacement UTF-8 replacement text.
  bool ReplaceCurrentSearchMatch(std::string replacement) const;

  /// @brief Replaces the matches of the active search.
  /// @param replacement UTF-8 replacement text.
  bool ReplaceAllSearchMatches(std::string replacement) const;

  /// @brief Clears the query, matches, and any selection owned by the current match.
  bool ClearSearch() const;

  /// @brief Returns the active search status, match information, and any error.
  /// @return A snapshot, or an empty value outside the mounted session.
  [[nodiscard]] std::optional<SearchState> GetSearchState() const;

  /// @brief Updates language rules and requests fresh decorations.
  /// @param configuration Language information and supplied bracket and indentation overrides.
  /// @throws std::invalid_argument If the configuration supplies a zero tab size.
  bool SetLanguageConfiguration(std::optional<LanguageConfiguration> configuration) const;

  /// @brief Returns the configured language information, or an empty value when absent.
  [[nodiscard]] std::optional<LanguageConfiguration> GetLanguageConfiguration() const;

  /// @brief Updates the application data sent to providers and requests fresh decorations.
  /// @param metadata Owned data. An empty std::any clears the metadata.
  bool SetMetadata(EditorMetadata metadata) const;

  /// @brief Returns a copy of the provider metadata, or an empty std::any when absent.
  [[nodiscard]] EditorMetadata GetMetadata() const;

  /// @brief Registers a provider and schedules a decoration refresh.
  /// @param provider Shared provider instance with declared decoration capabilities.
  /// @return True if registered. Empty or already registered instances return false.
  bool AddDecorationProvider(std::shared_ptr<DecorationProvider> provider) const;

  /// @brief Unregisters a provider, clears its declared decoration families, and refreshes remaining providers.
  /// @param provider The registered instance to remove.
  bool RemoveDecorationProvider(const std::shared_ptr<DecorationProvider>& provider) const;

  /// @brief Retires outstanding decoration requests and schedules a fresh request for the viewport.
  bool RequestDecorationRefresh() const;

  /// @brief Registers a source for subsequent completion requests.
  /// @param provider Shared provider instance. Empty or already registered instances are rejected.
  bool AddCompletionProvider(std::shared_ptr<CompletionProvider> provider) const;

  /// @brief Unregisters a completion provider and dismisses outstanding completion.
  /// @param provider The registered instance to remove.
  bool RemoveCompletionProvider(const std::shared_ptr<CompletionProvider>& provider) const;

  /// @brief Adds a newline provider to the end of the registration order.
  /// @param provider Shared provider instance. Empty or already registered instances are rejected.
  bool AddNewLineActionProvider(std::shared_ptr<NewLineActionProvider> provider) const;

  /// @brief Unregisters a newline provider.
  /// @param provider The registered instance to remove.
  bool RemoveNewLineActionProvider(const std::shared_ptr<NewLineActionProvider>& provider) const;

  /// @brief Starts an explicit completion request at the caret.
  /// Requests keyboard focus on the editor before showing candidates.
  /// @return True if a request was started. Returns false during composition, in read-only mode, or without providers.
  bool TriggerCompletion() const;

  /// @brief Replaces the current completion request with application-supplied items.
  /// @param items Items to display. An empty list dismisses completion.
  /// @return True if items were shown.
  bool ShowCompletionItems(std::vector<CompletionItem> items) const;

  /// @brief Closes the completion panel and cancels pending completion requests.
  /// @return True if a panel or request was active.
  bool DismissCompletion() const;

  /// @brief Sets the View factory for completion rows.
  /// @param renderer Factory called on the UI thread. An empty factory restores the default rows.
  bool SetCompletionItemRenderer(CompletionItemRenderer renderer) const;

  /// @brief Sets the provider used to build the next context menu.
  /// @param provider Callback on the UI thread. An empty callback restores the default menu.
  bool SetContextMenuItemProvider(ContextMenuItemProvider provider) const;

  /// @brief Sets the provider used to build the next touch selection toolbar.
  /// @param provider Callback on the UI thread. An empty callback restores the default actions.
  bool SetSelectionMenuItemProvider(SelectionMenuItemProvider provider) const;

  /// @brief Displays suggested text at a document position until it is accepted or dismissed.
  /// @param suggestion Insertion position and nonempty UTF-8 text.
  bool ShowInlineSuggestion(InlineSuggestion suggestion) const;

  /// @brief Dismisses the current suggestion and publishes EditorEvents::InlineSuggestionDismissed.
  /// @return True if a suggestion was active.
  bool DismissInlineSuggestion() const;

  /// @brief Reports whether an inline suggestion is active.
  [[nodiscard]] bool IsInlineSuggestionShowing() const;

  /// @brief Registers a text style for use by decoration spans.
  /// @param id Style identifier referenced by StyleSpan.
  /// @param style Colors and font attributes to associate with the identifier.
  bool RegisterTextStyle(std::uint32_t id, EditorTextStyle style) const;

  /// @brief Registers several text styles together.
  /// @param styles Style definitions keyed by the IDs referenced by StyleSpan.
  bool RegisterBatchTextStyles(std::map<std::uint32_t, EditorTextStyle> styles) const;

  /// @brief Replaces one layer of highlight spans on a line.
  /// @param line Zero-based logical line.
  /// @param layer Highlight layer to update.
  /// @param spans UTF-16 spans and their style IDs. An empty list clears the layer on the line.
  bool SetLineSpans(std::size_t line, SpanLayer layer, std::vector<StyleSpan> spans) const;

  /// @brief Replaces one layer of highlight spans on the supplied lines.
  /// @param layer Highlight layer to update.
  /// @param spans Replacement spans keyed by logical line. An empty list clears that line's layer.
  bool SetBatchLineSpans(SpanLayer layer, LineDecorations<StyleSpan> spans) const;

  /// @brief Replaces inlay hints on a logical line.
  /// @param line Zero-based line to update.
  /// @param hints Replacement decorations. An empty list clears this family on the line.
  bool SetLineInlayHints(std::size_t line, std::vector<InlayHint> hints) const;

  /// @brief Replaces inlay hints on the supplied lines.
  /// @param hints Replacements keyed by logical line. An empty list clears the family on that line.
  bool SetBatchLineInlayHints(LineDecorations<InlayHint> hints) const;

  /// @brief Replaces phantom text on a logical line.
  /// @param line Zero-based line to update.
  /// @param phantoms Replacement decorations. An empty list clears this family on the line.
  bool SetLinePhantomTexts(std::size_t line, std::vector<PhantomText> phantoms) const;

  /// @brief Replaces phantom text on the supplied lines.
  /// @param phantoms Replacements keyed by logical line. An empty list clears the family on that line.
  bool SetBatchLinePhantomTexts(LineDecorations<PhantomText> phantoms) const;

  /// @brief Replaces gutter icons on a logical line.
  /// @param line Zero-based line to update.
  /// @param icons Replacement decorations. An empty list clears this family on the line.
  bool SetLineGutterIcons(std::size_t line, std::vector<GutterIcon> icons) const;

  /// @brief Replaces gutter icons on the supplied lines.
  /// @param icons Replacements keyed by logical line. An empty list clears the family on that line.
  bool SetBatchLineGutterIcons(LineDecorations<GutterIcon> icons) const;

  /// @brief Replaces code lens items on a logical line.
  /// @param line Zero-based line to update.
  /// @param items Replacement decorations. An empty list clears this family on the line.
  bool SetLineCodeLens(std::size_t line, std::vector<CodeLensItem> items) const;

  /// @brief Replaces code lens items on the supplied lines.
  /// @param items Replacements keyed by logical line. An empty list clears the family on that line.
  bool SetBatchLineCodeLens(LineDecorations<CodeLensItem> items) const;

  /// @brief Replaces link ranges on a logical line.
  /// @param line Zero-based line to update.
  /// @param links Replacement decorations. An empty list clears this family on the line.
  bool SetLineLinks(std::size_t line, std::vector<LinkSpan> links) const;

  /// @brief Replaces link ranges on the supplied lines.
  /// @param links Replacements keyed by logical line. An empty list clears the family on that line.
  bool SetBatchLineLinks(LineDecorations<LinkSpan> links) const;

  /// @brief Replaces diagnostics on a logical line.
  /// @param line Zero-based line to update.
  /// @param diagnostics Replacement decorations. An empty list clears this family on the line.
  bool SetLineDiagnostics(std::size_t line, std::vector<Diagnostic> diagnostics) const;

  /// @brief Replaces diagnostics on the supplied lines.
  /// @param diagnostics Replacements keyed by logical line. An empty list clears the family on that line.
  bool SetBatchLineDiagnostics(LineDecorations<Diagnostic> diagnostics) const;

  /// @brief Replaces occurrence highlights on a logical line.
  /// @param line Zero-based line to update.
  /// @param highlights Replacement decorations. An empty list clears this family on the line.
  bool SetLineDocumentHighlights(std::size_t line, std::vector<DocumentHighlight> highlights) const;

  /// @brief Replaces occurrence highlights on the supplied lines.
  /// @param highlights Replacements keyed by logical line. An empty list clears the family on that line.
  bool SetBatchLineDocumentHighlights(LineDecorations<DocumentHighlight> highlights) const;

  /// @brief Replaces the document's indent guides.
  /// @param guides Guide ranges in document coordinates.
  bool SetIndentGuides(std::vector<IndentGuide> guides) const;

  /// @brief Replaces the document's bracket guides.
  /// @param guides Parent, branch, and end positions in document coordinates.
  bool SetBracketGuides(std::vector<BracketGuide> guides) const;

  /// @brief Replaces the document's control-flow guides.
  /// @param guides Guide endpoints in document coordinates.
  bool SetFlowGuides(std::vector<FlowGuide> guides) const;

  /// @brief Replaces the document's separator guides.
  /// @param guides Separator positions and styles for individual logical lines.
  bool SetSeparatorGuides(std::vector<SeparatorGuide> guides) const;

  /// @brief Replaces the document's fold regions.
  /// @param regions Fold start and end lines.
  bool SetFoldRegions(std::vector<FoldRegion> regions) const;

  /// @brief Clears document highlight spans.
  /// @param layer Layer to clear. An empty value clears all span layers.
  bool ClearHighlights(std::optional<SpanLayer> layer = std::nullopt) const;

  /// @brief Removes the document's inlay hints.
  bool ClearInlayHints() const;

  /// @brief Removes phantom text decorations.
  bool ClearPhantomTexts() const;

  /// @brief Removes the document's gutter icons.
  bool ClearGutterIcons() const;

  /// @brief Removes the document's code lens items.
  bool ClearCodeLens() const;

  /// @brief Removes link decorations.
  bool ClearLinks() const;

  /// @brief Removes indent, bracket, control-flow, and separator guides.
  bool ClearGuides() const;

  /// @brief Removes the document's diagnostic ranges.
  bool ClearDiagnostics() const;

  /// @brief Removes the document's occurrence highlights.
  bool ClearDocumentHighlights() const;

  /// @brief Clears the decorations currently applied to the document.
  /// Registered providers can publish new decorations on later requests.
  bool ClearAllDecorations() const;

  /// @brief Returns the inclusive range of logical lines in the viewport.
  /// @return An empty range when the viewport or session is unavailable.
  [[nodiscard]] IntRange GetVisibleLineRange() const;

  /// @brief Returns the document's logical line count, or zero outside the mounted session.
  [[nodiscard]] std::size_t GetTotalLineCount() const;

  /// @brief Reads the link target at a document position.
  /// @param position Zero-based logical line and UTF-16 column.
  /// @return The target text, or an empty string when the position has no link.
  [[nodiscard]] std::string GetLinkTargetAt(TextPosition position) const;

  /// @brief Expands a snippet at the caret or selection and activates its tab stops.
  /// @param snippet UTF-8 snippet source containing any linked placeholders.
  bool InsertSnippet(std::string snippet) const;

  /// @brief Starts synchronized editing of existing document ranges.
  /// @param groups Tab-stop groups in document coordinates.
  bool StartLinkedEditing(std::vector<TabStopGroup> groups) const;

  /// @brief Reports whether the session has active linked editing.
  [[nodiscard]] bool IsInLinkedEditing() const;

  /// @brief Moves to the next linked-editing tab stop.
  bool LinkedEditingNext() const;

  /// @brief Moves to the previous linked-editing tab stop.
  bool LinkedEditingPrevious() const;

  /// @brief Ends linked editing and leaves the document edits in place.
  bool CancelLinkedEditing() const;

  /// @brief Sets the comparison text for inline diff rendering.
  /// @param original UTF-8 baseline. An empty optional clears the diff.
  /// An empty string compares against an empty document.
  bool SetDiffOriginal(std::optional<std::string> original) const;

  /// Compares controller identity. Copies of one handle compare equal.
  bool operator==(const EditorController&) const = default;

 private:
  std::shared_ptr<detail::EditorControllerState> state_;
  friend struct detail::EditorControllerAccess;
};

/// @brief Retains a CodeEditor controller in the current composition scope.
/// @param location Call site identifying the retained handle. Usually supplied by the default argument.
/// @pre Call during composition on the UI thread.
/// @return A controller to pass as the first argument of CodeEditor().
EditorController UseCodeEditor(const std::source_location& location = std::source_location::current());

}  // namespace huxerui::codeeditor
