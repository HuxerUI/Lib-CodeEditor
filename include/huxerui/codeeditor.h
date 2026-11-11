#pragma once

/// @file
/// @brief Declarative code editing for HuxerUI.
///
/// Link HuxerUI::CodeEditor. Create a controller with UseCodeEditor(), then
/// pass it to CodeEditor() and bind EditorEvents on the returned View.
/// The mounted editor owns the document session, layout, platform text input,
/// and provider requests.

#include <string>
#include <variant>

#include <huxerui/codeeditor/controller.h>
#include <huxerui/view.h>

namespace huxerui::codeeditor {

/// @brief Document, configuration, and provider inputs for one CodeEditor View.
///
/// Changes to settings, theme, and provider lists apply to the mounted session.
/// Keep shared configuration and provider instances stable across composition
/// until their inputs change.
struct EditorOptions {
  /// UTF-8 text or a shared document handle. Defaults to empty text.
  /// Changing the text value or document identity replaces the active document.
  std::variant<std::string, Document> document;

  /// Construction-time settings for gesture thresholds, fling behavior,
  /// undo history capacity, and key chord timeout.
  /// The default reveals the selection end after Select All on Android and iOS.
  sweeteditor::EditorOptions core_options = DefaultCoreOptions();

  /// Layout, editing, and interaction settings.
  EditorSettings settings;

  /// Explicit palette. An empty value selects EditorTheme::Dark().
  std::optional<EditorTheme> theme;

  /// Keyboard bindings and custom command handlers. An empty pointer selects
  /// EditorKeyMap::DefaultKeyMap().
  std::shared_ptr<const EditorKeyMap> key_map;

  /// Language information and editing rules supplied to the session.
  std::shared_ptr<const LanguageConfiguration> language_configuration;

  /// Application data copied into provider requests.
  std::shared_ptr<const EditorMetadata> metadata;

  /// Sources of highlighting and document decorations.
  std::vector<std::shared_ptr<DecorationProvider>> decoration_providers;

  /// Sources of asynchronous completion items.
  std::vector<std::shared_ptr<CompletionProvider>> completion_providers;

  /// Sources consulted in registration order for custom newline actions.
  std::vector<std::shared_ptr<NewLineActionProvider>> newline_action_providers;

 private:
  static sweeteditor::EditorOptions DefaultCoreOptions();
};

/// @brief Declares a focusable editor backed by a mounted SweetEditor session.
///
/// Place the View in a bounded viewport. EditorEvents::DocumentLoaded fires
/// when the initial document becomes available and after document replacement.
/// @param controller Handle to bind to this View for its mounted lifetime.
/// @param options Initial document and declarative editor configuration.
/// @pre Declare on the UI thread. Use one controller per mounted editor.
/// @return A View with editing, scrolling, and platform text input.
/// @throws std::invalid_argument If settings are invalid or the supplied
/// document handle is empty.
View CodeEditor(EditorController controller, EditorOptions options = {});

}  // namespace huxerui::codeeditor
