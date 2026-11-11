# CodeEditor API

The component returns a HuxerUI View. The mounted node owns the Core, platform text-input bridge, providers, and presentation. Use the public [controller](../include/huxerui/codeeditor/controller.h), [models](../include/huxerui/codeeditor/model.h), [providers](../include/huxerui/codeeditor/providers.h), and [events](../include/huxerui/codeeditor/events.h) from the UI thread. Lines and columns are zero-based. Columns and platform text offsets count UTF-16 code units. Strings are UTF-8. Text ranges are half-open. Decoration viewport line ranges are inclusive.

## Mounting and documents

Create a controller with `UseCodeEditor()` during composition and pass it to `CodeEditor(controller, options)`. The controller and its copies share one binding. The editor accepts commands from the first `DocumentLoaded` event. Before that event and after unmounting, mutations return false and queries return empty values. Create a new controller for a new mount.

`EditorOptions::document` accepts a UTF-8 string or a shared `Document` handle and defaults to empty text. The editor creates an in-memory document when it mounts with text or receives a different text value. Equal text inputs and copies of the same handle reuse the active document and its editing history. A different handle or a call to `LoadDocument` replaces the document and publishes `DocumentLoaded`. The mounted session keeps its settings, theme, keymap, metadata, and providers.

Create an in-memory document with `Document(text)` or open a local mapped file with `Document::FromFile(file)`. `GetDocument()` returns the current shared handle. Read it with `GetText()`, `GetLineText()`, and `GetLineCount()`. The document handle remains readable on the UI thread after the editor is unmounted.

## Settings, theme, and shortcuts

Set initial settings with `EditorOptions::settings`. To update them imperatively, read `GetSettings()->Get()`, change the desired fields, and call `Apply(settings)` on the settings handle. Common fields include `editor_text_size`, `font_family`, `scale`, `wrap_mode`, `gutter_sticky`, `tab_size`, and `read_only`. Handle hit regions, scrollbar geometry, and decoration request interval and overscan use the same settings object. `EditorOptions::core_options` supplies construction-time gesture thresholds, fling physics, undo capacity, and chord timeout.

Desktop scrollbars are 6 logical units thick and remain visible while content overflows. Android and iOS use a 5-unit bar with an expanded thumb hit region. The mobile bar fades in over 300 ms on scroll and fades out over 300 ms after 700 ms of inactivity. Override `EditorSettings::scrollbar` to set a different policy.

On Android and iOS, the gutter scrolls horizontally with the document by default and Select All reveals the end of the selection. Desktop and Web pin the gutter and keep the current viewport on Select All. Each side of the line-number area has a margin of 10 logical units. Text begins 3 logical units beyond the gutter, and the default row height is 1.2 times the font height.

CodeEditor defaults to `EditorTheme::Dark()`. The `Dark()` and `Light()` palettes follow SweetEditor on Android and Swing. Use `Everforest()` for the Everforest palette or `EditorTheme::FromThemeSpec()` to derive colors from a HuxerUI theme. Apply themes declaratively or through `ApplyTheme`. `DefineTextStyle` configures a theme's syntax palette. `RegisterTextStyle` and `RegisterBatchTextStyles` register styles for the mounted session. IDs 1 through 12 identify the standard syntax styles. IDs starting at 100 are available for custom styles. `SetEditorIconProvider` resolves gutter, inlay, and selection-menu icons.

`EditorKeyMap::DefaultKeyMap()` uses the Core's VS Code-style bindings. Configure one or two `KeyChord` values per `KeyBinding`. `RegisterCommand` associates a custom command ID with a handler receiving the binding and controller. `SetKeyMap` applies the map to the session.

## Editing and navigation

| Capability | Controller methods |
| --- | --- |
| Text and undo | `InsertText`, `InsertTextAt`, `ReplaceText`, `DeleteText`, `ApplyTextEdits`, line move/copy/insert/delete, `Undo`, `Redo`, `CanUndo`, `CanRedo` |
| Cursor and selection | `SetCursorPosition`, `SetSelection`, `SelectAll`, `GetCursorPosition`, `GetSelection`, `GetSelectedText`, `GetWordRangeAtCursor`, `GetWordAtCursor` |
| Scrolling | `GotoPosition`, `ScrollToLine`, `SetScroll`, `GetScrollMetrics`, `GetPositionRect`, `GetCursorRect` |
| Folding | `SetFoldRegions`, `FoldAt`, `UnfoldAt`, `ToggleFold`, `FoldAll`, `UnfoldAll`, `IsLineVisible` |
| Search | `Search(SearchRequest)`, `GetSearchState`, `FindNextSearchMatch`, `FindPreviousSearchMatch`, `ReplaceCurrentSearchMatch`, `ReplaceAllSearchMatches`, `ClearSearch` |
| Clipboard | `CopyToClipboard`, `CutToClipboard`, `PasteFromClipboard` use the HuxerUI platform clipboard |
| Snippets and diff | `InsertSnippet`, linked-editing tab stops, `SetDiffOriginal` |

Pass original document coordinates to `ApplyTextEdits`. The Core checks for conflicting edits and applies the batch as one history operation. Build search UI around the search methods and inspect `SearchState` for matches, errors, and result status. Clipboard operations report the platform service's availability and result. Use `SetDiffOriginal` to set the UTF-8 comparison text. Pass `std::nullopt` to clear the diff or an empty string to compare against an empty document.

## Providers

Register provider instances in `EditorOptions` or through the controller. `LanguageConfiguration` supplies a language ID and optional bracket, auto-closing, and indentation settings. `EditorMetadata` is host-owned `std::any` data copied into each context.

### Decorations

`DecorationProvider::GetCapabilities` declares the families a provider supplies. `ProvideDecorations` receives the visible and requested line ranges, an owned text snapshot, the document generation, incremental changes, and the language and metadata. `requested_line_range` adds overscan for analysis. Each `DecorationReceiver` can deliver multiple results from any thread. Delivery is queued on the UI thread. `IsCancelled` becomes true when the request is superseded, the provider is removed, the document is replaced, or the editor unmounts.

A result can update syntax, semantic and overlay spans, hints, diagnostics, document highlights, fold regions, four guide families, gutter icons, phantom text, code lens, and links. For each family, `Merge` appends, `ReplaceRange` replaces entries in the visible line range, and `ReplaceAll` replaces the whole family. For concurrent snapshots the strongest mode is `ReplaceAll`, then `ReplaceRange`, then `Merge`. Providers can replay changes if their analyzed revision matches `previous_text_revision`, or rebuild from `document_text`.

Use `RequestDecorationRefresh` when host data such as breakpoints changes. Hosts can also set per-line and batch decorations directly and clear individual families. See [custom highlighting](highlighting-without-sweetline.md) for a small provider.

### Completion and newline

`CompletionProvider::ProvideCompletions` returns `Task<CompletionResult>`. Results from current providers merge by `sort_key` or label. Set `is_incomplete` when a provider needs to refine the list on more input. `IsTriggerCharacter` handles automatic triggers. Use `TriggerCompletion` to focus the editor and request candidates, `ShowCompletionItems` to display supplied items, and `DismissCompletion` to close the popup.

`SetCompletionItemRenderer` takes a factory that receives the candidate and its selection state and returns a HuxerUI `View` for the row. Use non-focusable content so keyboard input stays on the editor. The editor supplies row selection, activation, and scrolling. Pass an empty factory to restore the default rows.

A completion commits `text_edit` first, followed in priority by `insert_text` and `label`. Only `text_edit` supplies a replacement range. Plain-text completion and `additional_text_edits` share a Core edit batch. `InsertTextFormat::Snippet` delegates expansion and tab stops to Core linked editing.

`NewLineActionProvider` is synchronous. Providers run in registration order and the first returned action supplies the text for the Enter command. An empty optional continues to the next provider or the default indentation behavior.

## Events and presentation

Bind events with `.On<EditorEvents::...>(handler)` on the returned View. `TextChanged` carries incremental edits, change kind, and action source. Cursor, selection, scroll, and scale events carry their current values. Fold, gutter icon, inlay, code lens, link, long-press, double-tap, and context menu events include editor-local coordinates where applicable.

Right click opens a HuxerUI context menu. Touch selection opens a horizontal toolbar centered on the visible selection. Use `SetContextMenuItemProvider` and `SetSelectionMenuItemProvider` to supply complete action lists for each show cycle. Custom actions emit typed item-click events. The built-in Open Link action emits `LinkClicked` for the host to open the target.

Use `ShowInlineSuggestion` to preview text at a document position. Tab accepts it and Escape dismisses it. Text changes and cursor movement also dismiss it. The View reports acceptance and dismissal through `EditorEvents`.

The HuxerUI text-input bridge submits each platform text-input command batch to Core in one transaction. The retained node consumes the Core result to update platform text-input state, schedule animation, dispatch events, and invalidate paint. Geometry queries use node-local layout coordinates, and HuxerUI applies the window transform.
