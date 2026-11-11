#pragma once

/// @file
/// @brief Shared document handles, settings, and input models for CodeEditor.
///
/// The document model uses UTF-8 text, zero-based logical lines, and UTF-16
/// code-unit columns. Editor geometry uses logical layout units.

#include <any>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <huxerui/file.h>
#include <huxerui/geometry.h>
#include <huxerui/resource.h>
#include <huxerui/text.h>
#include <sweeteditor/editor_core.h>

namespace huxerui::codeeditor {

/// Bit flags for active editor animation work.
using sweeteditor::AnimationFlag;
/// Controls indentation applied by the default newline action.
using sweeteditor::AutoIndentMode;
/// Parent, branch, and end positions of a bracket guide.
using sweeteditor::BracketGuide;
/// Opening and closing characters used for bracket rules.
using sweeteditor::BracketPair;
/// A line annotation with a label and command identifier.
using sweeteditor::CodeLensItem;
/// Controls how the line containing the caret is drawn.
using sweeteditor::CurrentLineRenderMode;
/// A line-local diagnostic span and severity.
using sweeteditor::Diagnostic;
/// A line-local occurrence highlight and its semantic kind.
using sweeteditor::DocumentHighlight;
/// Origin of a change, such as keyboard input, IME, or a controller command.
using sweeteditor::EditorActionSource;
/// Identifiers for built-in keyboard commands.
using sweeteditor::EditorBuiltinCommand;
/// Numeric key map command identifier.
using sweeteditor::EditorCommandId;
/// Source and target positions of a control-flow guide.
using sweeteditor::FlowGuide;
/// Controls when fold arrows appear in the gutter.
using sweeteditor::FoldArrowMode;
/// Inclusive logical line range with its collapsed state.
using sweeteditor::FoldRegion;
/// An icon identifier placed in a logical line's gutter.
using sweeteditor::GutterIcon;
/// Hit regions for selection handles relative to the caret anchors.
using sweeteditor::HandleConfig;
/// Kind and document position of the editor element under an input point.
using sweeteditor::HitTarget;
/// Start and end positions of an indentation guide.
using sweeteditor::IndentGuide;
/// Line-local text, icon, or color annotation displayed inline.
using sweeteditor::InlayHint;
/// Content kind and payload interpretation of an inlay hint.
using sweeteditor::InlayType;
/// Inclusive integer range used for logical line windows.
using sweeteditor::IntRange;
/// One or two key chords associated with a command ID.
using sweeteditor::KeyBinding;
/// A key code with its modifier flags.
using sweeteditor::KeyChord;
/// Key identifier accepted by the editor key map.
using sweeteditor::KeyCode;
/// Modifier flags used in editor key chords.
using sweeteditor::KeyModifier;
/// Line-local UTF-16 range and its link target.
using sweeteditor::LinkSpan;
/// Display-only text placed at a line-local UTF-16 column.
using sweeteditor::PhantomText;
/// Placement of a target line within the viewport.
using sweeteditor::ScrollBehavior;
/// Scrollbar visibility, geometry, input, and fade timing.
using sweeteditor::ScrollbarConfig;
/// Current viewport and content scroll metrics in logical layout units.
using sweeteditor::ScrollMetrics;
/// Document search query and match options.
using sweeteditor::SearchRequest;
/// Search status, match count, active match, and error information.
using sweeteditor::SearchState;
/// A horizontal separator attached to a logical line.
using sweeteditor::SeparatorGuide;
/// Highlight priority layer for style spans.
using sweeteditor::SpanLayer;
/// Line-local UTF-16 start and length with a registered style ID.
using sweeteditor::StyleSpan;
/// Document ranges edited together at a snippet tab stop.
using sweeteditor::TabStopGroup;
/// Replacement range with previous and new UTF-8 text.
using sweeteditor::TextChange;
/// Kind of text change reported for an editing action.
using sweeteditor::TextChangeKind;
/// A document range and its UTF-8 replacement text.
using sweeteditor::TextEdit;
/// Zero-based logical line and UTF-16 code-unit column.
using sweeteditor::TextPosition;
/// Pair of document positions with a half-open extent when ordered.
using sweeteditor::TextRange;
/// Controls which whitespace characters receive visible markers.
using sweeteditor::WhitespaceRenderMode;
/// Controls visual wrapping of logical lines.
using sweeteditor::WrapMode;

/// @brief Decorations grouped by zero-based logical line.
/// @tparam T Decoration type stored on each line.
template <class T>
using LineDecorations = std::map<std::size_t, std::vector<T>>;

/// Application-defined data copied into decoration, completion, and newline requests.
using EditorMetadata = std::any;
/// Resolves an icon ID to a HuxerUI image on the UI thread. Return an empty value
/// to use the editor's fallback presentation.
using EditorIconProvider = std::function<std::optional<ImageAsset>(std::int32_t)>;

/// @brief Language identity and optional editing overrides.
///
/// Empty indentation overrides use the editor settings. Omitted bracket
/// lists use the editor's current rules.
struct LanguageConfiguration {
  /// Language identifier passed to providers.
  std::string language_id;
  /// Bracket pairs used for matching. An engaged value replaces the current rules.
  std::optional<std::vector<BracketPair>> brackets;
  /// Bracket pairs inserted by automatic closing. An engaged empty list disables it.
  std::optional<std::vector<BracketPair>> auto_closing_pairs;
  /// Positive tab width in columns. An empty value uses EditorSettings::tab_size.
  std::optional<std::uint32_t> tab_size;
  /// Whether indentation uses spaces. An empty value uses EditorSettings::insert_spaces.
  std::optional<bool> insert_spaces;
};

namespace detail {
struct DocumentAccess;
struct EditorControllerState;
}  // namespace detail

/// @brief Shared handle to editable document storage.
///
/// Copies observe edits to the same document. Access a document on its editor's
/// UI thread once it is loaded. Use GetText() for an independent text snapshot.
class Document {
 public:
  /// @brief Creates an in-memory document.
  /// @param text Initial UTF-8 contents.
  explicit Document(std::string text = {});
  /// @brief Opens a file-backed UTF-8 document with in-memory edits.
  /// @param file Readable local file accessible by path on the current platform.
  /// @return A document backed by the file mapping for its lifetime.
  static Document FromFile(const File& file);

  /// Returns the number of logical lines, including an empty trailing line.
  [[nodiscard]] std::size_t GetLineCount() const;
  /// @brief Copies a logical line as UTF-8 without its line ending.
  /// @param line Zero-based line index. Values past the end use the last line.
  [[nodiscard]] std::string GetLineText(std::size_t line) const;
  /// Returns an owned UTF-8 copy of the entire document.
  [[nodiscard]] std::string GetText() const;
  /// Compares shared document identity.
  bool operator==(const Document&) const = default;

 private:
  explicit Document(std::shared_ptr<sweeteditor::Document> core);
  std::shared_ptr<sweeteditor::Document> core_;
  friend struct detail::DocumentAccess;
};

/// @brief Value snapshot of the editor's layout, editing, and interaction settings.
struct EditorSettings {
  /// Positive base font size in logical layout units before applying scale.
  float editor_text_size = 14.0F;
  /// Font family for document text and gutter line numbers.
  FontFamily font_family = FontFamily::Monospace();
  /// Positive scale factor applied to the editor's base font size.
  float scale = 1.0F;
  /// Visibility policy for the gutter's fold arrows.
  FoldArrowMode fold_arrow_mode = FoldArrowMode::ALWAYS;
  /// Visual wrapping policy for logical lines.
  WrapMode wrap_mode = WrapMode::NONE;
  /// Fixed addition to line height in logical layout units.
  float line_spacing_add = 0.0F;
  /// Nonnegative multiplier applied to font height before line_spacing_add.
  float line_spacing_mult = 1.2F;
  /// Nonnegative inset between the gutter and text in logical layout units.
  float content_start_padding = 3.0F;
  /// Nonnegative line-number margin in logical layout units.
  float line_number_margin = 10.0F;
  /// Whether to draw a vertical line between the gutter and text.
  bool show_split_line = true;
  /// Whether the gutter remains pinned during horizontal scrolling.
  /// Defaults to false on Android and iOS, and true on desktop and Web.
  bool gutter_sticky = DefaultGutterSticky();
  /// Whether to display the gutter and its decorations.
  bool gutter_visible = true;
  /// Presentation of the logical line containing the caret.
  CurrentLineRenderMode current_line_render_mode = CurrentLineRenderMode::BACKGROUND;
  /// Policy for visible whitespace markers.
  WhitespaceRenderMode render_whitespace = WhitespaceRenderMode::NONE;
  /// Whether to display line-ending markers.
  bool render_line_breaks = false;
  /// Indentation applied by the default newline action.
  AutoIndentMode auto_indent_mode = AutoIndentMode::KEEP_INDENT;
  /// Whether Backspace in leading whitespace removes indentation by a tab stop.
  bool backspace_unindent = true;
  /// Whether document editing and platform text input are disabled.
  bool read_only = false;
  /// Positive default tab width in columns. Language rules may override it.
  std::uint32_t tab_size = 4;
  /// Whether indentation uses spaces by default. Language rules may override it.
  bool insert_spaces = true;
  /// Dedicated gutter icon capacity per line. Zero uses one icon in the line-number lane.
  std::uint32_t max_gutter_icons = 0;
  /// Hit areas in logical layout units on platforms that display selection handles.
  HandleConfig handles;
  /// Scrollbar geometry, visibility, and input policy in logical layout units.
  /// Defaults to a 6-unit persistent bar on desktop and a 5-unit transient bar on Android and iOS.
  ScrollbarConfig scrollbar = DefaultScrollbarConfig();
  /// Minimum interval in milliseconds between decoration requests during viewport motion.
  std::int64_t decoration_scroll_refresh_min_interval_ms = 16;
  /// Extra decoration analysis range on each side, measured in visible line counts.
  float decoration_overscan_viewport_multiplier = 1.5F;

  /// Compares all setting values.
  bool operator==(const EditorSettings&) const = default;

 private:
  static bool DefaultGutterSticky();
  static ScrollbarConfig DefaultScrollbarConfig();
};

/// @brief UI-thread access to settings for one mounted editor.
///
/// Obtain this handle with EditorController::GetSettings(). Copies refer to
/// the same session for its mounted lifetime.
class EditorSettingsHandle {
 public:
  /// Returns a settings snapshot, or an empty value after the session unmounts.
  [[nodiscard]] std::optional<EditorSettings> Get() const;
  /// @brief Applies a settings snapshot to the mounted session.
  /// @param settings Desired layout, editing, and interaction settings.
  /// @return True if applied, or false after the session unmounts.
  /// @throws std::invalid_argument If a value violates its setting constraints.
  bool Apply(EditorSettings settings) const;

 private:
  explicit EditorSettingsHandle(std::weak_ptr<detail::EditorControllerState> state);
  std::weak_ptr<detail::EditorControllerState> state_;
  friend class EditorController;
};

class EditorController;

/// @brief Value map of keyboard bindings and custom commands.
class EditorKeyMap {
 public:
  /// Invoked on the UI thread with the binding and the mounted controller.
  using CommandHandler = std::function<void(const KeyBinding&, const EditorController&)>;

  /// @brief Adds a binding or replaces a conflicting chord sequence.
  /// @param binding One or two key chords and their command ID.
  /// @throws std::invalid_argument If the first chord is empty.
  void AddBinding(KeyBinding binding);
  /// @brief Associates a custom command handler with a key binding.
  /// @param binding Binding with a custom command ID. The NONE ID requests allocation.
  /// @param handler Callback to invoke for the command.
  /// @return Assigned custom command ID.
  /// @throws std::invalid_argument If the handler or first chord is empty, or the ID is reserved.
  /// @throws std::overflow_error If no custom command ID can be allocated.
  EditorCommandId RegisterCommand(KeyBinding binding, CommandHandler handler);
  /// Returns the bindings owned by this map. Mutating the map may invalidate the reference.
  [[nodiscard]] const std::vector<KeyBinding>& Bindings() const noexcept { return bindings_; }
  /// @brief Looks up a custom command callback.
  /// @param command Command ID to look up.
  /// @return An owned callback, or an empty callback for an unregistered command.
  [[nodiscard]] CommandHandler GetCommand(EditorCommandId command) const;

  /// Returns the default bindings for editing, navigation, and completion.
  static EditorKeyMap DefaultKeyMap();
  /// Returns a preset with VS Code-style editing and navigation shortcuts.
  static EditorKeyMap VSCode();

 private:
  std::vector<KeyBinding> bindings_;
  std::map<EditorCommandId, CommandHandler> commands_;
  EditorCommandId next_custom_command_ = sweeteditor::EDITOR_BUILTIN_COMMAND_MAX + 1;
};

}  // namespace huxerui::codeeditor
