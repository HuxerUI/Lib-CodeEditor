#pragma once

/// @file
/// @brief Typed editing and interaction events for CodeEditor.
///
/// Subscribe with View::On<EditorEvents::...>(). Handlers run on the editor's
/// UI thread. Copy any data needed after a callback returns. Document
/// coordinates use zero-based logical lines and UTF-16 code-unit columns.
/// Gesture positions and scroll offsets use editor-local logical layout units.

#include <huxerui/codeeditor/providers.h>
#include <huxerui/event.h>

namespace huxerui::codeeditor {

/// @brief Document changes reported by a single editing action.
struct TextChangedEvent {
  /// Ordered replacements reported by the editing core.
  std::vector<TextChange> changes;
  /// Kind of edit that produced the changes.
  TextChangeKind kind = TextChangeKind::NONE;
  /// Origin of the edit, including keyboard, platform text input, or a controller command.
  EditorActionSource source = EditorActionSource::NONE;
};

/// @brief Updated caret position.
struct CursorChangedEvent {
  /// Active caret after the action.
  TextPosition cursor_position;
};

/// @brief Updated selection and caret.
struct SelectionChangedEvent {
  /// Whether the new selection is nonempty.
  bool has_selection = false;
  /// Anchor and active end when the selection is nonempty.
  std::optional<TextRange> selection;
  /// Active caret after the action.
  TextPosition cursor_position;
};

/// @brief Updated viewport offsets in logical layout units.
struct ScrollChangedEvent {
  /// Horizontal content offset.
  float scroll_x = 0.0F;
  /// Vertical content offset.
  float scroll_y = 0.0F;
};

/// @brief Updated editor scale.
struct ScaleChangedEvent {
  /// Multiplier applied to the base editor text size.
  float scale = 1.0F;
};

/// @brief Tap that toggled a fold.
struct FoldToggleEvent {
  /// First logical line of the fold.
  std::size_t line = 0;
  /// True for a gutter fold marker. False for an inline fold placeholder.
  bool is_gutter = false;
  /// Tap position relative to the editor.
  Point location_in_editor;
};

/// @brief Activation of a gutter icon.
struct GutterIconClickEvent {
  /// Logical line carrying the icon.
  std::size_t line = 0;
  /// ID supplied by the decoration.
  std::int32_t icon_id = 0;
  /// Tap position relative to the editor.
  Point location_in_editor;
};

/// @brief Activation of an inline hint.
struct InlayHintClickEvent {
  /// Logical line carrying the hint.
  std::size_t line = 0;
  /// UTF-16 column of the hint.
  std::size_t column = 0;
  /// Hint content kind.
  InlayType type = InlayType::TEXT;
  /// Icon ID or ARGB color depending on type. Text hints use zero.
  std::int32_t int_value = 0;
  /// Tap position relative to the editor.
  Point location_in_editor;
};

/// @brief Activation of an inline command.
struct CodeLensClickEvent {
  /// Logical line carrying the code lens item.
  std::size_t line = 0;
  /// UTF-16 column associated with the item.
  std::size_t column = 0;
  /// Command ID supplied by the decoration.
  std::int32_t command_id = 0;
  /// Tap position relative to the editor.
  Point location_in_editor;
};

/// @brief Activation of a document link.
struct LinkClickEvent {
  /// Logical line of the hit link.
  std::size_t line = 0;
  /// UTF-16 column of the hit position.
  std::size_t column = 0;
  /// Link target supplied by the decoration.
  std::string target;
  /// Activation position relative to the editor.
  Point location_in_editor;
};

/// @brief Touch long press in the editor.
struct LongPressEvent {
  /// Active caret after processing the gesture.
  TextPosition cursor_position;
  /// Touch position relative to the editor.
  Point location_in_editor;
};

/// @brief Double tap and its resulting selection.
struct DoubleTapEvent {
  /// Active caret after processing the gesture.
  TextPosition cursor_position;
  /// Whether the gesture left a nonempty selection.
  bool has_selection = false;
  /// Selection after the gesture, when present.
  std::optional<TextRange> selection;
  /// Tap position relative to the editor.
  Point location_in_editor;
};

/// @brief Context menu gesture before presenting the menu.
struct ContextMenuEvent {
  /// Active caret after processing the gesture.
  TextPosition cursor_position;
  /// Gesture position relative to the editor.
  Point location_in_editor;
};

/// @brief Activation of an application-supplied context menu action.
struct ContextMenuItemClickEvent {
  /// Selected item and its command ID.
  ContextMenuItem item;
  /// Context captured when the menu was requested.
  ContextMenuRequest request;
};

/// @brief Activation of an application-supplied touch selection menu action.
struct SelectionMenuItemClickEvent {
  /// Selected item and its command ID.
  SelectionMenuItem item;
};

/// @brief Event keys accepted by the View returned from CodeEditor().
struct EditorEvents {
  /// The document text changed.
  struct TextChanged : Event<void(const TextChangedEvent&)> {};
  /// The active caret position changed.
  struct CursorChanged : Event<void(const CursorChangedEvent&)> {};
  /// The selection changed, including when it was cleared.
  struct SelectionChanged : Event<void(const SelectionChangedEvent&)> {};
  /// The viewport scroll offsets changed.
  struct ScrollChanged : Event<void(const ScrollChangedEvent&)> {};
  /// The editor scale changed.
  struct ScaleChanged : Event<void(const ScaleChangedEvent&)> {};
  /// The initial mounted document or a replacement document is available.
  /// The controller accepts commands from this callback.
  struct DocumentLoaded : Event<void()> {};
  /// A fold was toggled by a tap.
  struct FoldToggled : Event<void(const FoldToggleEvent&)> {};
  /// A gutter icon was activated.
  struct GutterIconClicked : Event<void(const GutterIconClickEvent&)> {};
  /// An inlay hint was activated.
  struct InlayHintClicked : Event<void(const InlayHintClickEvent&)> {};
  /// A code lens item was activated.
  struct CodeLensClicked : Event<void(const CodeLensClickEvent&)> {};
  /// A link was tapped or its Open Link action was invoked.
  struct LinkClicked : Event<void(const LinkClickEvent&)> {};
  /// A touch long press was recognized.
  struct LongPressed : Event<void(const LongPressEvent&)> {};
  /// A double tap was recognized.
  struct DoubleTapped : Event<void(const DoubleTapEvent&)> {};
  /// A secondary pointer action requested the context menu.
  struct ContextMenu : Event<void(const ContextMenuEvent&)> {};
  /// A context menu command was delegated to the application.
  struct ContextMenuItemClicked : Event<void(const ContextMenuItemClickEvent&)> {};
  /// A touch selection menu command was delegated to the application.
  struct SelectionMenuItemClicked : Event<void(const SelectionMenuItemClickEvent&)> {};
  /// The editor inserted an accepted suggestion.
  struct InlineSuggestionAccepted : Event<void(const InlineSuggestion&)> {};
  /// An active suggestion was dismissed.
  struct InlineSuggestionDismissed : Event<void(const InlineSuggestion&)> {};
};

}  // namespace huxerui::codeeditor
