#include <catch2/catch_amalgamated.hpp>

#include <huxerui/codeeditor.h>
#include <huxerui/huxerui.h>
#include <huxerui/modifier.h>
#include <huxerui/testing/ui_test.h>

#include <chrono>

namespace {

using namespace huxerui;
using namespace huxerui::testing;
using namespace std::chrono_literals;
namespace ce = huxerui::codeeditor;

thread_local std::function<View()> root;

class TestApplication {
 public:
  explicit TestApplication(std::function<View()> view) { root = std::move(view); }
  ~TestApplication() { root = {}; }

  const Application app{[] { return root(); }, {.show_debug_overlay = false}};
};

class LinkProvider final : public ce::DecorationProvider {
 public:
  std::set<ce::DecorationType> GetCapabilities() const override { return {ce::DecorationType::Link}; }

  Task<void> ProvideDecorations(ce::DecorationContext context, ce::DecorationReceiver receiver) override {
    latest = std::move(receiver);
    visible = context.visible_line_range;
    co_return;
  }

  std::optional<ce::DecorationReceiver> latest;
  ce::IntRange visible;
};

class CompletionSource final : public ce::CompletionProvider {
 public:
  CompletionSource(ce::CompletionItem item, std::chrono::milliseconds delay) : item_(std::move(item)), delay_(delay) {}

  bool IsTriggerCharacter(std::string_view value) const override { return value == "."; }

  Task<ce::CompletionResult> ProvideCompletions(ce::CompletionContext context) override {
    co_await Delay(delay_);
    last_context = std::move(context);
    co_return ce::CompletionResult{{item_}};
  }

  std::optional<ce::CompletionContext> last_context;

 private:
  ce::CompletionItem item_;
  std::chrono::milliseconds delay_;
};

class NewLineSource final : public ce::NewLineActionProvider {
 public:
  explicit NewLineSource(std::optional<ce::NewLineAction> result) : result_(std::move(result)) {}
  std::optional<ce::NewLineAction> ProvideNewLineAction(const ce::NewLineContext& context) override {
    last_context = context;
    return result_;
  }

  std::optional<ce::NewLineContext> last_context;

 private:
  std::optional<ce::NewLineAction> result_;
};

}  // namespace

TEST_CASE("Unbound CodeEditor controllers reject work and expose empty queries") {
  const ce::EditorController editor;
  REQUIRE_FALSE(editor.GetDocument());
  REQUIRE_FALSE(editor.GetSettings());
  REQUIRE_FALSE(editor.GetTheme());
  REQUIRE_FALSE(editor.GetSelection());
  REQUIRE_FALSE(editor.GetCursorPosition());
  REQUIRE_FALSE(editor.LoadDocument(ce::Document("hello")));
  REQUIRE_FALSE(editor.InsertText("hello"));
  REQUIRE_FALSE(editor.SetCursorPosition({0, 0}));
  REQUIRE_FALSE(editor.Search({}));
  REQUIRE_FALSE(editor.RequestDecorationRefresh());
  REQUIRE_FALSE(editor.SelectAll());
  REQUIRE(editor.GetVisibleLineRange().isEmpty());
  REQUIRE(editor.GetTotalLineCount() == 0);
  REQUIRE(editor.GetLinkTargetAt({0, 0}).empty());
  REQUIRE(editor == ce::EditorController(editor));
  REQUIRE_FALSE(editor == ce::EditorController());
}

TEST_CASE("UseCodeEditor retains a controller across updates and DocumentLoaded enables commands") {
  std::optional<ce::EditorController> composed_controller;
  const ce::Document initial("first\nsecond");
  const ce::Document next("next\npage");
  std::vector<std::string> loaded;
  bool initialized = false;
  const TestApplication app([&] {
    const auto editor = ce::UseCodeEditor();
    composed_controller = editor;
    const auto document = UseState(initial);
    const auto visible = UseState(true);
    const auto dark = UseState(false);
    return Column{
        Button("Recompose").OnClick([dark] { dark = true; }),
        Button("Load next").OnClick([document, next] { document = next; }),
        Button("Unmount").OnClick([visible] { visible = false; }),
        visible.Get() ? ce::CodeEditor(editor, {
            .document = document.Get(),
            .theme = dark.Get() ? ce::EditorTheme::Dark() : ce::EditorTheme::Light(),
        }).On<ce::EditorEvents::DocumentLoaded>([&, editor] {
          if (const auto active = editor.GetDocument()) {
            if (loaded.empty()) initialized = editor.SetMetadata(std::string("metadata"));
            loaded.push_back(active->GetText());
          }
        }).With(Grow{}) : View{},
    }.With(CrossAlign(CrossAxisAlignment::Stretch));
  });
  UiTest ui(app.app, {.viewport = {400, 300}});
  ui.Pump();

  REQUIRE(composed_controller);
  const auto editor = *composed_controller;
  REQUIRE(initialized);
  REQUIRE(loaded == std::vector<std::string>{"first\nsecond"});
  REQUIRE(editor.GetDocument() == initial);
  const auto settings = *editor.GetSettings();
  auto config = *settings.Get();
  config.tab_size = 8;
  REQUIRE(settings.Apply(config));
  REQUIRE(editor.InsertText("x"));
  REQUIRE(editor.CanUndo());

  ui.Find(UiSelector::Text("Recompose")).Tap();
  ui.Pump();
  REQUIRE(composed_controller == editor);
  REQUIRE(loaded.size() == 1);
  REQUIRE(editor.GetTheme() == ce::EditorTheme::Dark());
  REQUIRE(editor.GetDocument()->GetText() == "xfirst\nsecond");
  REQUIRE(editor.GetSettings()->Get()->tab_size == 8);

  ui.Find(UiSelector::Text("Load next")).Tap();
  ui.Pump();
  REQUIRE(loaded == std::vector<std::string>{"first\nsecond", "next\npage"});
  REQUIRE(editor.GetDocument() == next);
  REQUIRE(editor.GetSettings()->Get()->tab_size == 8);
  REQUIRE(std::any_cast<std::string>(editor.GetMetadata()) == "metadata");
  REQUIRE_FALSE(editor.CanUndo());

  ui.Find(UiSelector::Text("Unmount")).Tap();
  ui.Pump();
  REQUIRE_FALSE(editor.GetDocument());
  REQUIRE_FALSE(editor.GetSettings());
  REQUIRE_FALSE(settings.Get());
  REQUIRE_FALSE(settings.Apply(config));
  REQUIRE_FALSE(editor.LoadDocument(initial));
}

TEST_CASE("CodeEditor replaces text-backed documents only when the declared text changes") {
  const ce::EditorController editor;
  std::size_t loads = 0;
  const TestApplication app([&] {
    const auto source = UseState(std::string("seed"));
    const auto wrap = UseState(false);
    const std::string& text = source;
    return Column{
        Button("Toggle wrapping").OnClick([wrap] { wrap = !wrap; }),
        Button("Replace text source").OnClick([source] { source = "replacement"; }),
        ce::CodeEditor(editor, {
            .document = text,
            .settings = {.wrap_mode = wrap ? ce::WrapMode::WORD_BREAK : ce::WrapMode::NONE},
        }).On<ce::EditorEvents::DocumentLoaded>([&loads] { ++loads; }).With(Grow{}),
    }.With(CrossAlign(CrossAxisAlignment::Stretch));
  });
  UiTest ui(app.app, {.viewport = {400, 300}});
  ui.Pump();

  const auto initial = editor.GetDocument();
  REQUIRE(initial);
  REQUIRE(initial->GetText() == "seed");
  REQUIRE(editor.GetTheme() == ce::EditorTheme::Dark());
  REQUIRE(loads == 1);
  REQUIRE(editor.SetCursorPosition({0, 4}));
  REQUIRE(editor.InsertText("!"));
  const auto caret = editor.GetCursorPosition();
  REQUIRE(caret == ce::TextPosition{0, 5});

  ui.Find(UiSelector::Text("Toggle wrapping")).Tap();
  ui.Pump();
  REQUIRE(editor.GetSettings()->Get()->wrap_mode == ce::WrapMode::WORD_BREAK);
  REQUIRE(editor.GetDocument() == initial);
  REQUIRE(editor.GetDocument()->GetText() == "seed!");
  REQUIRE(editor.GetCursorPosition() == caret);
  REQUIRE(editor.CanUndo());
  REQUIRE(loads == 1);

  const ce::Document opened("opened by a command");
  REQUIRE(editor.LoadDocument(opened));
  ui.Find(UiSelector::Text("Toggle wrapping")).Tap();
  ui.Pump();
  REQUIRE(editor.GetSettings()->Get()->wrap_mode == ce::WrapMode::NONE);
  REQUIRE(editor.GetDocument() == opened);
  REQUIRE(loads == 2);

  ui.Find(UiSelector::Text("Replace text source")).Tap();
  ui.Pump();
  REQUIRE(editor.GetDocument() != opened);
  REQUIRE(editor.GetDocument()->GetText() == "replacement");
  REQUIRE_FALSE(editor.CanUndo());
  REQUIRE(loads == 3);
}

TEST_CASE("CodeEditor dispatches one atomic IME batch with UTF-16 offsets") {
  const ce::EditorController editor;
  std::vector<ce::TextChangedEvent> changes;
  const TestApplication app([&] {
    return ce::CodeEditor(editor, {.document = "A\xf0\x9f\x99\x82" "BC"})
        .On<ce::EditorEvents::TextChanged>([&](const auto& change) { changes.push_back(change); });
  });
  UiTest ui(app.app);
  ui.TapAt({80, 12});
  REQUIRE(ui.ActiveTextInput());
  auto session = ui.ActiveTextInput()->session_id;

  const TextInputCommand select{.kind = TextInputCommandKind::SetSelection, .selection_after = TextSelection{1, 3}};
  const TextInputCommand replace{.kind = TextInputCommandKind::CommitText, .text = "x"};
  REQUIRE(ui.SendTextInput({session, {select, replace}}).result_code == TextInputResultCode::Ok);
  ui.Pump();
  REQUIRE(editor.GetDocument()->GetText() == "AxBC");
  REQUIRE(ui.ActiveTextInput()->selection.active == 2);
  REQUIRE(changes.size() == 1);
  REQUIRE(changes[0].source == ce::EditorActionSource::IME);
  REQUIRE(changes[0].kind == ce::TextChangeKind::REPLACEMENT);
  REQUIRE(changes[0].changes.size() == 1);
  REQUIRE(changes[0].changes[0].range == ce::TextRange{{0, 1}, {0, 3}});

  session = ui.ActiveTextInput()->session_id;
  const TextInputCommand invalid{.kind = TextInputCommandKind::SetSelection, .selection_after = TextSelection{900, 900}};
  REQUIRE(ui.SendTextInput({session, {replace, invalid}}).result_code == TextInputResultCode::Rejected);
  ui.Pump();
  REQUIRE(editor.GetDocument()->GetText() == "AxBC");
  REQUIRE(changes.size() == 1);

  REQUIRE(editor.Undo());
  REQUIRE(editor.GetDocument()->GetText() == "A\xf0\x9f\x99\x82" "BC");
  REQUIRE(editor.LoadDocument(ce::Document("fresh")));
  ui.Pump();
  REQUIRE(ui.ActiveTextInput());
  REQUIRE(ui.ActiveTextInput()->session_id != session);
  REQUIRE(ui.SendTextInput({session, {replace}}).result_code == TextInputResultCode::SessionMismatch);
  REQUIRE(editor.GetDocument()->GetText() == "fresh");
}

TEST_CASE("CodeEditor combines decoration updates and rejects retired receivers") {
  const ce::EditorController editor;
  const auto first = std::make_shared<LinkProvider>();
  const auto second = std::make_shared<LinkProvider>();
  std::string document = "abcdefghij";
  for (int i = 0; i < 120; ++i) document += "\nabcdefghij";
  const TestApplication app([&] {
    return ce::CodeEditor(editor, {.document = document, .decoration_providers = {first, second}});
  });
  UiTest ui(app.app, {.viewport = {340, 150}});
  ui.PumpUntil([&] { return first->latest && second->latest; });
  REQUIRE(first->visible.end < 100);

  ce::DecorationResult merge;
  merge.links.values[0].push_back({0, 2, "first"});
  merge.links.values[100].push_back({0, 2, "offscreen"});
  REQUIRE(first->latest->Accept(std::move(merge)));
  ui.PumpUntil([&] { return editor.GetLinkTargetAt({0, 0}) == "first"; });

  ce::DecorationResult range;
  range.links.mode = ce::ApplyMode::ReplaceRange;
  range.links.values[0].push_back({3, 2, "second"});
  range.links.values[100].push_back({0, 2, "ignored"});
  REQUIRE(second->latest->Accept(std::move(range)));
  ui.PumpUntil([&] { return editor.GetLinkTargetAt({0, 3}) == "second"; });
  REQUIRE(editor.GetLinkTargetAt({0, 0}) == "first");
  REQUIRE(editor.GetLinkTargetAt({100, 0}) == "offscreen");

  ce::DecorationResult all;
  all.links.mode = ce::ApplyMode::ReplaceAll;
  all.links.values[0].push_back({6, 2, "replacement"});
  REQUIRE(second->latest->Accept(std::move(all)));
  ui.PumpUntil([&] { return editor.GetLinkTargetAt({0, 6}) == "replacement"; });
  REQUIRE(editor.GetLinkTargetAt({0, 0}) == "first");
  REQUIRE(editor.GetLinkTargetAt({0, 3}).empty());

  const auto retired = *second->latest;
  REQUIRE(editor.LoadDocument(ce::Document("replacement document")));
  REQUIRE(retired.IsCancelled());
  REQUIRE_FALSE(retired.Accept({}));
  REQUIRE(editor.GetLinkTargetAt({0, 6}).empty());
}

TEST_CASE("CodeEditor merges Task completions and applies the selected item as one edit batch") {
  const ce::EditorController editor;
  ce::CompletionItem slow{.label = "chosen", .insert_text = "ignored",
      .text_edit = ce::TextEdit{{{1, 0}, {1, 2}}, "value"},
      .additional_text_edits = {{{{0, 0}, {0, 0}}, "import\n"}}, .sort_key = "a"};
  const auto first = std::make_shared<CompletionSource>(std::move(slow), 70ms);
  const auto second = std::make_shared<CompletionSource>(
      ce::CompletionItem{.label = "other", .insert_text = "wrong", .sort_key = "z"}, 20ms);
  const TestApplication app([&] {
    return ce::CodeEditor(editor, {.document = "header\nab tail", .completion_providers = {first, second}});
  });
  UiTest ui(app.app, {.viewport = {420, 300}});
  ui.TapAt({80, 12});
  REQUIRE(editor.SetCursorPosition({1, 2}));
  ui.PressKey(Key::Space, {.control = true});
  ui.PumpUntil([&] { return first->last_context && second->last_context; });
  REQUIRE(first->last_context->trigger_kind == ce::CompletionTriggerKind::Invoked);
  REQUIRE(first->last_context->word_range == ce::TextRange{{1, 0}, {1, 2}});
  ui.PressKey(Key::Enter);
  REQUIRE(editor.GetDocument()->GetText() == "import\nheader\nvalue tail");
  REQUIRE(editor.Undo());
  REQUIRE(editor.GetDocument()->GetText() == "header\nab tail");

  REQUIRE(editor.ShowCompletionItems({ce::CompletionItem{.label = "!", .insert_text = "!"}}));
  ui.PressKey(Key::Tab);
  REQUIRE(editor.GetDocument()->GetText() == "header\nab! tail");
  first->last_context.reset();
  second->last_context.reset();
  REQUIRE(editor.TriggerCompletion());
  REQUIRE(editor.LoadDocument(ce::Document("next")));
  ui.Pump(100ms);
  REQUIRE_FALSE(first->last_context);
  REQUIRE_FALSE(second->last_context);
}

TEST_CASE("CodeEditor routes custom chords, newline providers, folds and search through the mounted session") {
  const ce::EditorController editor;
  const auto skip = std::make_shared<NewLineSource>(std::nullopt);
  const auto action = std::make_shared<NewLineSource>(ce::NewLineAction{"\nextra"});
  const auto key_map = std::make_shared<ce::EditorKeyMap>(ce::EditorKeyMap::VSCode());
  unsigned invoked = 0;
  key_map->RegisterCommand({{ce::KeyModifier::CTRL, ce::KeyCode::K}, {ce::KeyModifier::CTRL, ce::KeyCode::C}},
                           [&](const auto&, const auto& controller) {
    ++invoked;
    controller.InsertText("!");
  });
  const TestApplication app([&] {
    return ce::CodeEditor(editor, {
        .document = "first\nword\nend", .key_map = key_map, .newline_action_providers = {skip, action},
    });
  });
  UiTest ui(app.app);
  ui.TapAt({80, 12});
  editor.SetCursorPosition({0, 0});
  ui.PressKey(Key::K, {.control = true});
  ui.PressKey(Key::C, {.control = true});
  REQUIRE(invoked == 1);
  REQUIRE(editor.GetDocument()->GetText() == "!first\nword\nend");

  ui.PressKey(Key::Enter);
  REQUIRE(skip->last_context->column == 1);
  REQUIRE(action->last_context->line_text == "!first");
  REQUIRE(editor.GetDocument()->GetText() == "!\nextrafirst\nword\nend");
  REQUIRE(editor.SetFoldRegions({{1, 3}}));
  REQUIRE(editor.FoldAt(1));
  REQUIRE_FALSE(editor.IsLineVisible(2));
  REQUIRE(editor.UnfoldAt(1));
  REQUIRE(editor.IsLineVisible(2));

  REQUIRE(editor.Search({.pattern = "word"}));
  REQUIRE(editor.FindNextSearchMatch());
  REQUIRE(editor.GetSelectedText() == "word");
  REQUIRE(editor.ReplaceCurrentSearchMatch("changed"));
  REQUIRE(editor.GetDocument()->GetLineText(2) == "changed");
}

TEST_CASE("CodeEditor reports wheel, inline suggestion and context menu actions on the mounted view") {
  const ce::EditorController editor;
  std::string text = "first";
  for (int i = 0; i < 60; ++i) text += "\nline";
  unsigned scroll_events = 0;
  unsigned accepted = 0;
  unsigned dismissed = 0;
  std::optional<ce::ContextMenuRequest> requested;
  std::optional<ce::ContextMenuItemClickEvent> selected;
  const TestApplication app([&] {
    return ce::CodeEditor(editor, {.document = text})
        .On<ce::EditorEvents::ScrollChanged>([&](const auto&) { ++scroll_events; })
        .On<ce::EditorEvents::InlineSuggestionAccepted>([&](const auto&) { ++accepted; })
        .On<ce::EditorEvents::InlineSuggestionDismissed>([&](const auto&) { ++dismissed; })
        .On<ce::EditorEvents::ContextMenuItemClicked>([&](const auto& event) { selected = event; });
  });
  UiTest ui(app.app, {.viewport = {400, 240}});
  ui.TapAt({80, 10}, {.device_kind = PointerDeviceKind::Mouse});
  REQUIRE(editor.SetCursorPosition({0, 5}));
  REQUIRE(editor.ShowInlineSuggestion({0, 5, " suffix"}));
  REQUIRE(ui.SendScroll({.position = {110, 40}, .delta_y = 120}).y > 0);
  ui.Pump();
  REQUIRE(scroll_events > 0);
  REQUIRE(editor.IsInlineSuggestionShowing());
  ui.PressKey(Key::Tab);
  REQUIRE(editor.GetDocument()->GetLineText(0) == "first suffix");
  REQUIRE(accepted == 1);
  REQUIRE(dismissed == 0);

  REQUIRE(editor.ShowInlineSuggestion({0, 12, " more"}));
  ui.PressKey(Key::Escape);
  REQUIRE_FALSE(editor.IsInlineSuggestionShowing());
  REQUIRE(accepted == 1);
  REQUIRE(dismissed == 1);

  REQUIRE(editor.SetContextMenuItemProvider([&](const ce::ContextMenuRequest& request) {
    requested = request;
    return std::vector<ce::ContextMenuSection>{{{{"custom", "Custom action"}}}};
  }));
  const Point location{100, 65};
  ui.SendPointer({.type = PointerEventType::Down, .pointer_id = 7, .position = location,
                  .changed_button = PointerButton::Secondary, .pressed_buttons = PointerButton::Secondary});
  ui.SendPointer({.type = PointerEventType::Up, .pointer_id = 7, .position = location,
                  .changed_button = PointerButton::Secondary});
  ui.Pump();
  REQUIRE(requested);
  REQUIRE(requested->trigger_kind == ce::ContextMenuTriggerKind::RightClick);
  REQUIRE(requested->location_in_editor == location);
  ui.Find(UiSelector::Text("Custom action")).Tap();
  REQUIRE(selected);
  REQUIRE(selected->item.id == "custom");
  REQUIRE(selected->request.location_in_editor == location);
}

TEST_CASE("CodeEditor draws hover underlines for links and CodeLens") {
  const ce::EditorController editor;
  auto theme = ce::EditorTheme::Light();
  theme.link_active_color = theme.link_color;
  theme.code_lens_active_color = theme.code_lens_color;
  const TestApplication app([&] {
    return ce::CodeEditor(editor, {.document = std::string("Link target\nCodeLens target"), .theme = theme});
  });
  UiTest ui(app.app, {.viewport = {400, 240}});
  REQUIRE(editor.SetLineLinks(0, {{0, 4, "https://example.com"}}));
  REQUIRE(editor.SetLineCodeLens(1, {{0, 1, "Run"}}));
  ui.Pump();
  ui.TapAt({200, 100}, {.device_kind = PointerDeviceKind::Mouse});

  const auto link = editor.GetPositionRect({0, 0});
  const auto code = editor.GetPositionRect({1, 0});
  REQUIRE(link);
  REQUIRE(code);
  const auto hover = [&](Point point) {
    ui.SendPointer({.type = PointerEventType::Move, .pointer_id = 1,
                    .position = point, .device_kind = PointerDeviceKind::Mouse});
    ui.Pump();
  };
  hover({link->x + 2.0F, link->y + link->height * 0.5F});
  const auto idle = ui.CaptureSnapshot();
  ui.SendKey({.type = KeyEventType::Down, .key = Key::ControlLeft, .modifiers = {.control = true}});
  ui.Pump();
  REQUIRE(ui.CaptureSnapshot() != idle);
  ui.SendKey({.type = KeyEventType::Up, .key = Key::ControlLeft});
  ui.Pump();
  REQUIRE(ui.CaptureSnapshot() == idle);

  hover({code->x + 2.0F, code->y - code->height * 0.5F});
  REQUIRE(ui.CaptureSnapshot() != idle);
  hover({200.0F, 100.0F});
  REQUIRE(ui.CaptureSnapshot() == idle);
}

TEST_CASE("CodeEditor settles composition when settings switch to read-only") {
  const ce::EditorController editor;
  const TestApplication app([&] { return ce::CodeEditor(editor, {.document = "end"}); });
  UiTest ui(app.app);
  ui.TapAt({85, 12});
  const auto active = ui.ActiveTextInput();
  REQUIRE(active);
  const TextInputCommand update{.kind = TextInputCommandKind::UpdateComposition,
                                .target = huxerui::TextRange{0, 0}, .text = "pre"};
  REQUIRE(ui.SendTextInput({active->session_id, {update}}).result_code == TextInputResultCode::Ok);
  REQUIRE(ui.ActiveTextInput()->composition == huxerui::TextRange{0, 3});

  const auto handle = *editor.GetSettings();
  auto settings = *handle.Get();
  settings.read_only = true;
  REQUIRE(handle.Apply(settings));
  ui.Pump();
  REQUIRE_FALSE(ui.ActiveTextInput());
  REQUIRE(editor.GetDocument()->GetText() == "preend");
  REQUIRE_FALSE(editor.InsertText("x"));
  REQUIRE(ui.SendTextInput({active->session_id, {update}}).result_code == TextInputResultCode::SessionMismatch);

  settings.read_only = false;
  REQUIRE(handle.Apply(settings));
  ui.Pump();
  REQUIRE(ui.ActiveTextInput());
  REQUIRE(ui.ActiveTextInput()->session_id != active->session_id);
  REQUIRE_FALSE(ui.ActiveTextInput()->composition);
}

#if defined(_WIN32)
TEST_CASE("Desktop selection endpoints allow clicks on the next line") {
  const ce::EditorController editor;
  const TestApplication app([&] { return ce::CodeEditor(editor, {.document = "alpha beta\nsecond line"}); });
  UiTest ui(app.app);
  ui.Pump();

  REQUIRE(editor.SetSelection({{0, 0}, {0, 5}}));
  ui.Pump();
  REQUIRE(editor.GetSelectedText() == "alpha");
  const auto endpoint = editor.GetPositionRect({0, 5});
  const auto next_line = editor.GetPositionRect({1, 0});
  REQUIRE(endpoint);
  REQUIRE(next_line);

  ui.TapAt({endpoint->x + 2.0F, next_line->y + next_line->height / 2.0F}, {.device_kind = PointerDeviceKind::Mouse});
  REQUIRE_FALSE(editor.GetSelection());
  const auto caret = editor.GetCursorPosition();
  REQUIRE(caret);
  REQUIRE(caret->line == 1);
}
#endif

TEST_CASE("Touch selection shows an editor-owned menu and delivers custom actions") {
  const ce::EditorController editor;
  std::optional<ce::SelectionMenuItemClickEvent> selected;
  const TestApplication app([&] {
    return ce::CodeEditor(editor, {.document = "alpha beta"})
        .On<ce::EditorEvents::SelectionMenuItemClicked>([&](const auto& event) { selected = event; });
  });
  UiTest ui(app.app, {.viewport = {420, 250}});
  ui.Pump();
  REQUIRE(editor.SetSelectionMenuItemProvider([](const ce::TextRange&) {
    return std::vector<ce::SelectionMenuItem>{{"custom", "Touch action"}};
  }));
  const auto position = editor.GetPositionRect({0, 2});
  REQUIRE(position);
  const Point touch{position->x, position->y + position->height / 2};
  ui.TapAt(touch);
  ui.TapAt(touch);
  ui.PumpUntil([&] { return ui.Find(UiSelector::Text("Touch action")).Exists(); });
  REQUIRE(editor.GetSelectedText() == "alpha");
  ui.Find(UiSelector::Text("Touch action")).Tap();
  REQUIRE(selected);
  REQUIRE(selected->item.id == "custom");
  ui.Pump();
  REQUIRE_FALSE(ui.Find(UiSelector::Text("Touch action")).Exists());
}
