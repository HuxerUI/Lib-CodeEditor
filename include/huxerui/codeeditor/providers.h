#pragma once

/// @file
/// @brief Decoration, completion, newline, and menu providers for CodeEditor.
///
/// Provider entry points run on the editor's UI thread. Decoration receivers
/// can also be retained and used by background work. Text positions use
/// zero-based logical lines and UTF-16 code-unit columns.

#include <functional>
#include <optional>
#include <set>
#include <string_view>

#include <huxerui/codeeditor/model.h>
#include <huxerui/task.h>
#include <huxerui/view.h>

namespace huxerui::codeeditor {

/// @brief Decoration families declared by a provider.
enum class DecorationType {
  /// Token styles derived from document syntax.
  SyntaxHighlight,
  /// Styles derived from semantic analysis.
  SemanticHighlight,
  /// Spans drawn in the overlay highlight layer.
  OverlayHighlight,
  /// Inline text, icon, or color annotations.
  InlayHint,
  /// Diagnostic ranges and severities.
  Diagnostic,
  /// Highlights for related document occurrences.
  DocumentHighlight,
  /// Collapsible logical line ranges.
  FoldRegion,
  /// Indentation guides.
  IndentGuide,
  /// Guides connecting related brackets.
  BracketGuide,
  /// Control-flow guides.
  FlowGuide,
  /// Horizontal separators.
  SeparatorGuide,
  /// Icons in the gutter.
  GutterIcon,
  /// Display-only inline text.
  PhantomText,
  /// Inline commands displayed above document text.
  CodeLens,
  /// Interactive document links.
  Link,
};

/// @brief How one decoration update combines with the current document decorations.
enum class ApplyMode {
  /// Adds values to the existing family.
  Merge,
  /// Replaces the family within the request's inclusive visible line range.
  ReplaceRange,
  /// Replaces the family throughout the document.
  ReplaceAll,
};

/// @brief Values and apply mode for one decoration family.
/// @tparam T Line-indexed decorations or a list of document ranges or guides.
template <class T>
struct DecorationUpdate {
  /// Values to apply. An empty replacement clears the corresponding range or family.
  T values;
  /// Operation applied to the values.
  ApplyMode mode = ApplyMode::Merge;
};

/// @brief Updates produced for one decoration request.
///
/// Each family is applied independently when the provider advertises it.
/// Across providers, the strongest mode is ReplaceAll, followed by ReplaceRange and Merge.
struct DecorationResult {
  /// Syntax spans keyed by logical line.
  DecorationUpdate<LineDecorations<StyleSpan>> syntax_spans;
  /// Semantic spans keyed by logical line.
  DecorationUpdate<LineDecorations<StyleSpan>> semantic_spans;
  /// Overlay spans keyed by logical line.
  DecorationUpdate<LineDecorations<StyleSpan>> overlay_spans;
  /// Inlay hints keyed by logical line.
  DecorationUpdate<LineDecorations<InlayHint>> inlay_hints;
  /// Diagnostics keyed by logical line.
  DecorationUpdate<LineDecorations<Diagnostic>> diagnostics;
  /// Occurrence highlights keyed by logical line.
  DecorationUpdate<LineDecorations<DocumentHighlight>> document_highlights;
  /// Indentation guides in document coordinates.
  DecorationUpdate<std::vector<IndentGuide>> indent_guides;
  /// Bracket guides in document coordinates.
  DecorationUpdate<std::vector<BracketGuide>> bracket_guides;
  /// Control-flow guides in document coordinates.
  DecorationUpdate<std::vector<FlowGuide>> flow_guides;
  /// Separators attached to logical lines.
  DecorationUpdate<std::vector<SeparatorGuide>> separator_guides;
  /// Fold regions with inclusive logical line bounds.
  DecorationUpdate<std::vector<FoldRegion>> fold_regions;
  /// Icons keyed by gutter line.
  DecorationUpdate<LineDecorations<GutterIcon>> gutter_icons;
  /// Display-only text keyed by logical line.
  DecorationUpdate<LineDecorations<PhantomText>> phantom_texts;
  /// Code lens items keyed by logical line.
  DecorationUpdate<LineDecorations<CodeLensItem>> code_lens_items;
  /// Links keyed by logical line.
  DecorationUpdate<LineDecorations<LinkSpan>> links;
};

/// @brief Snapshot supplied to a decoration provider for one request.
struct DecorationContext {
  /// Inclusive range of logical lines currently displayed.
  IntRange visible_line_range;
  /// Inclusive range to analyze, including any viewport overscan.
  IntRange requested_line_range;
  /// Number of logical lines in the active document.
  std::size_t total_line_count = 0;
  /// Active caret at request creation.
  TextPosition cursor_position;
  /// Ordered changes from previous_text_revision to text_revision.
  std::vector<TextChange> text_changes;
  /// Language rules at request creation, when supplied.
  std::optional<LanguageConfiguration> language_configuration;
  /// Application data at request creation.
  EditorMetadata editor_metadata;
  /// Owned, immutable UTF-8 text for this revision. It may be read on a worker thread.
  std::shared_ptr<const std::string> document_text;
  /// Identifier that changes when the session loads a document.
  std::uint64_t document_generation = 0;
  /// Whether the viewport has settled after scrolling or interaction.
  bool viewport_settled = true;
  /// Revision at the start of text_changes. Use document_text to rebuild when
  /// the provider's prior analysis has a different revision or generation.
  std::uint64_t previous_text_revision = 0;
  /// Revision represented by document_text.
  std::uint64_t text_revision = 0;
};

namespace detail {
struct DecorationDelivery;
struct DecorationReceiverAccess;
}  // namespace detail

/// @brief Copyable delivery handle for one decoration request.
///
/// A provider can send multiple updates and retain the receiver beyond
/// ProvideDecorations(). Accept() and IsCancelled() are safe on any thread.
class DecorationReceiver {
 public:
  /// @brief Queues a decoration update for delivery on the UI thread.
  /// @param result Update owned by the receiver after the call.
  /// @return True if queued, or false when the request is already cancelled.
  /// A request superseded before delivery discards its queued updates.
  bool Accept(DecorationResult result) const;

  /// Reports whether the request has been retired or its session has ended.
  [[nodiscard]] bool IsCancelled() const noexcept;

 private:
  explicit DecorationReceiver(std::shared_ptr<detail::DecorationDelivery> delivery);
  std::shared_ptr<detail::DecorationDelivery> delivery_;
  friend struct detail::DecorationReceiverAccess;
};

/// @brief Produces syntax, semantic, or presentation decorations for a document.
///
/// The session retains registered providers. Use owned request data when
/// moving analysis to a worker thread.
class DecorationProvider {
 public:
  /// Destroys the provider after its last owner releases it.
  virtual ~DecorationProvider() = default;

  /// Returns the families this provider can publish. Read when registering the provider.
  [[nodiscard]] virtual std::set<DecorationType> GetCapabilities() const = 0;

  /// @brief Starts processing a viewport decoration request.
  /// @param context Owned request snapshot with document text and revision information.
  /// @param receiver Delivery handle for publishing updates.
  /// @return A task that runs on the UI thread and can be cancelled when superseded.
  virtual Task<void> ProvideDecorations(DecorationContext context, DecorationReceiver receiver) = 0;
};

/// @brief Semantic category used by the completion panel to present an item.
enum class CompletionItemKind : std::int32_t {
  /// Language keyword.
  Keyword = 0,
  /// Callable symbol.
  Function = 1,
  /// Named variable.
  Variable = 2,
  /// Class type.
  Class = 3,
  /// Interface type.
  Interface = 4,
  /// Module or package.
  Module = 5,
  /// Member property.
  Property = 6,
  /// Snippet template.
  Snippet = 7,
  /// Plain text candidate.
  Text = 8,
};

/// @brief Interpretation of a completion item's insertion text.
enum class InsertTextFormat : std::int32_t {
  /// Inserts the text literally.
  PlainText = 1,
  /// Expands a snippet and activates any linked tab stops.
  Snippet = 2,
};

/// @brief Reason the session requests completions.
enum class CompletionTriggerKind {
  /// Explicitly requested by a shortcut or controller command.
  Invoked,
  /// Requested after input matched a provider's trigger.
  Character,
  /// Refines the previous completion request after more input.
  Retrigger,
};

/// @brief One candidate and the document edits applied when it is accepted.
///
/// text_edit supplies an explicit replacement range. Otherwise insert_text or
/// label is inserted at the caret. Additional edits use pre-acceptance coordinates.
struct CompletionItem {
  /// Primary text shown in the panel and fallback insertion text.
  std::string label;
  /// Secondary text shown alongside the label.
  std::string detail;
  /// Optional UTF-8 insertion text used when text_edit is absent.
  std::optional<std::string> insert_text;
  /// Interpretation of the text to insert or replace.
  InsertTextFormat insert_text_format = InsertTextFormat::PlainText;
  /// Explicit replacement, taking precedence over insert_text and label.
  std::optional<TextEdit> text_edit;
  /// Additional nonconflicting edits to apply with the primary edit.
  std::vector<TextEdit> additional_text_edits;
  /// Optional matching text for provider logic or custom presentation.
  std::string filter_text;
  /// Sort value for the merged list. An empty value uses label.
  std::string sort_key;
  /// Category used to choose a visual indicator.
  CompletionItemKind kind = CompletionItemKind::Text;
};

/// @brief Caret, line, and application context for a completion request.
struct CompletionContext {
  /// Reason for the request.
  CompletionTriggerKind trigger_kind = CompletionTriggerKind::Invoked;
  /// UTF-8 input that triggered completion. Empty for an explicit request or retrigger.
  std::string trigger_character;
  /// Active caret position.
  TextPosition cursor_position;
  /// Contents of the caret's logical line as UTF-8.
  std::string line_text;
  /// Word boundary range at the caret.
  TextRange word_range;
  /// Language configuration at request creation, when supplied.
  std::optional<LanguageConfiguration> language_configuration;
  /// Application data at request creation.
  EditorMetadata editor_metadata;
};

/// @brief Candidates returned by one completion provider.
struct CompletionResult {
  /// Items to merge into the active completion list.
  std::vector<CompletionItem> items;
  /// Whether more input can require another request even when this result is empty.
  bool is_incomplete = false;
};

/// @brief Asynchronous source of completion candidates.
class CompletionProvider {
 public:
  /// Destroys the provider after its last owner releases it.
  virtual ~CompletionProvider() = default;

  /// @brief Checks whether the supplied input should start completion.
  /// @param character UTF-8 text from the triggering edit.
  /// @return True to request completion automatically.
  [[nodiscard]] virtual bool IsTriggerCharacter(std::string_view character) const = 0;

  /// @brief Produces candidates for a document position.
  /// @param context Owned snapshot for the request.
  /// @return A task yielding items on the UI thread. Superseded results are discarded.
  virtual Task<CompletionResult> ProvideCompletions(CompletionContext context) = 0;
};

/// Builds the content of one completion row on the UI thread.
///
/// The editor supplies selection, activation, and scrolling for the row. Return
/// non-focusable content to keep keyboard input on the editor. The item is
/// borrowed for the call. Capture any data the returned View needs by value.
using CompletionItemRenderer = std::function<View(const CompletionItem&, bool selected)>;

/// @brief Context supplied when the editor handles a newline command.
struct NewLineContext {
  /// Zero-based logical line at the caret.
  std::size_t line_number = 0;
  /// Zero-based UTF-16 column at the caret.
  std::size_t column = 0;
  /// Contents of the caret line as UTF-8.
  std::string line_text;
  /// Active language configuration, when supplied.
  std::optional<LanguageConfiguration> language_configuration;
  /// Application data for the editor session.
  EditorMetadata editor_metadata;
};

/// @brief Override for the editor's default newline action.
struct NewLineAction {
  /// UTF-8 text to insert. An empty string consumes the command without inserting text.
  std::string text;
};

/// @brief Synchronous source of newline actions consulted in registration order.
class NewLineActionProvider {
 public:
  /// Destroys the provider after its last owner releases it.
  virtual ~NewLineActionProvider() = default;

  /// @brief Computes a custom newline action on the UI thread.
  /// @param context Borrowed caret and line information for this call.
  /// @return A replacement action, or an empty value to continue to the next provider or the default action.
  virtual std::optional<NewLineAction> ProvideNewLineAction(const NewLineContext& context) = 0;
};

/// @brief Display-only suggestion that becomes a document edit when accepted.
struct InlineSuggestion {
  /// Zero-based logical line where the suggestion appears.
  std::size_t line = 0;
  /// Zero-based UTF-16 column where the suggestion appears.
  std::size_t column = 0;
  /// UTF-8 text displayed and inserted on acceptance.
  std::string text;
};

/// @brief Gesture that requested a context menu.
enum class ContextMenuTriggerKind {
  /// Touch long press.
  LongPress,
  /// Secondary pointer action.
  RightClick,
};

/// @brief Context captured when the editor requests a menu.
struct ContextMenuRequest {
  /// Gesture that initiated the request.
  ContextMenuTriggerKind trigger_kind = ContextMenuTriggerKind::RightClick;
  /// Active caret after processing the gesture.
  TextPosition cursor_position;
  /// Gesture location in editor-local logical layout units.
  Point location_in_editor;
  /// Whether the editor has a nonempty selection.
  bool has_selection = false;
  /// Selection at request creation, when present.
  std::optional<TextRange> selection;
  /// Editor element found at the gesture location.
  HitTarget hit_target;
  /// Link target at the hit point, or an empty string.
  std::string link_target;
};

/// @brief Menu entry shown for an editor context request.
struct ContextMenuItem {
  /// Command ID. Built-in IDs include copy, cut, paste, delete, select_all,
  /// open_link, and copy_link. Use other IDs for application actions.
  std::string id;
  /// Primary menu label.
  std::string label;
  /// Optional text displayed alongside the primary label.
  std::string secondary_label;
  /// Whether the item can be invoked.
  bool enabled = true;
  /// Optional HuxerUI image shown beside the label.
  std::optional<ImageAsset> icon;
};

/// @brief Consecutive context-menu entries separated from other nonempty sections.
struct ContextMenuSection {
  /// Entries in presentation order.
  std::vector<ContextMenuItem> items;
};

/// Builds context-menu sections on the UI thread. Return an empty list to suppress the menu.
using ContextMenuItemProvider = std::function<std::vector<ContextMenuSection>(const ContextMenuRequest&)>;

/// @brief Menu entry associated with a touch selection.
struct SelectionMenuItem {
  /// Command ID. Built-in IDs include copy, cut, paste, delete, and select_all.
  std::string id;
  /// Text shown in the selection menu.
  std::string label;
  /// Whether the item can be invoked.
  bool enabled = true;
  /// Optional ID resolved by EditorIconProvider.
  std::optional<std::int32_t> icon_id;
};

/// Builds menu entries on the UI thread for the current selection.
using SelectionMenuItemProvider = std::function<std::vector<SelectionMenuItem>(const TextRange&)>;

}  // namespace huxerui::codeeditor
