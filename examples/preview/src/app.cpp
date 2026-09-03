#include <huxerui/huxerui.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <map>
#include <memory>

#include <huxerui/codeeditor.h>

#include "sweetline_provider.h"

using namespace huxerui;

namespace {

using huxerui::codeeditor::CompletionContext;
using huxerui::codeeditor::CompletionItem;
using huxerui::codeeditor::CompletionItemKind;

CompletionItem MakeItem(
    std::string label,
    std::string detail,
    std::string insert_text,
    CompletionItemKind kind,
    std::string sort_key,
    bool snippet = false
) {
  CompletionItem item;
  item.label = std::move(label);
  item.detail = std::move(detail);
  item.insert_text = std::move(insert_text);
  item.kind = kind;
  item.sort_key = std::move(sort_key);
  item.insert_text_is_snippet = snippet;
  return item;
}

// Mirrors the SweetEditor demo completion provider: '.' triggers member
// completion synchronously; any other request filters keyword/identifier
// candidates by the identifier prefix at the caret and replaces that range.
std::vector<CompletionItem> ProvideCompletions(const CompletionContext& context) {
  if (context.trigger_kind == CompletionContext::TriggerKind::Character &&
      context.trigger_character == ".") {
    return {
        MakeItem("length", "size_t", "length()", CompletionItemKind::Property, "a_length"),
        MakeItem("push_back", "void push_back(T)", "push_back()", CompletionItemKind::Function, "b_push_back"),
        MakeItem("begin", "iterator", "begin()", CompletionItemKind::Function, "c_begin"),
        MakeItem("end", "iterator", "end()", CompletionItemKind::Function, "d_end"),
        MakeItem("size", "size_t", "size()", CompletionItemKind::Function, "e_size"),
    };
  }

  struct Candidate {
    const char* label;
    const char* detail;
    const char* insert_text;
    bool snippet;
    CompletionItemKind kind;
    const char* sort_key;
  };
  static constexpr Candidate kCandidates[] = {
      {"std::string", "class", "std::string", false, CompletionItemKind::Class, "a_string"},
      {"std::vector", "template class", "std::vector<>", false, CompletionItemKind::Class, "b_vector"},
      {"std::cout", "ostream", "std::cout", false, CompletionItemKind::Variable, "c_cout"},
      {"if", "snippet", "if (${1:condition}) {\n\t$0\n}", true, CompletionItemKind::Snippet, "d_if"},
      {"for", "snippet",
       "for (int ${1:i} = 0; ${1:i} < ${2:n}; ++${1:i}) {\n\t$0\n}", true, CompletionItemKind::Snippet, "e_for"},
      {"class", "snippet - class definition",
       "class ${1:ClassName} {\npublic:\n\t${1:ClassName}() {$2}\n\t~${1:ClassName}() {$3}\n$0\n};", true,
       CompletionItemKind::Snippet, "f_class"},
      {"return", "keyword", "return ", false, CompletionItemKind::Keyword, "g_return"},
  };

  const uint32_t start = context.word_start;
  const uint32_t end = context.word_end;
  if (start >= end || context.cursor_column < start || context.cursor_column > end ||
      end > context.line_text.size()) {
    return {};
  }
  const std::string pattern = context.line_text.substr(start, context.cursor_column - start);
  if (pattern.empty()) {
    return {};
  }
  std::vector<CompletionItem> items;
  for (const Candidate& candidate : kCandidates) {
    const std::string insert_text = candidate.insert_text;
    if (insert_text.rfind(pattern, 0) != 0 && std::string(candidate.label).rfind(pattern, 0) != 0) {
      continue;
    }
    CompletionItem item = MakeItem(
        candidate.label, candidate.detail, candidate.insert_text, candidate.kind, candidate.sort_key, candidate.snippet);
    item.has_text_edit = true;
    item.text_edit_start = start;
    item.text_edit_end = end;
    item.text_edit_text = candidate.insert_text;
    items.push_back(std::move(item));
  }
  return items;
}

}  // namespace

// Dark showcase theme: starts from the light reference and overrides every
// major surface, demonstrating partial theming on top of CodeEditorTheme.
huxerui::codeeditor::CodeEditorTheme MakeDarkTheme() {
  auto theme = huxerui::codeeditor::CodeEditorTheme::Default();
  theme.background = Color::Rgb(30, 30, 46);
  theme.gutter_background = Color::Rgb(24, 24, 37);
  theme.current_line_background = Color::Rgb(255, 255, 255, 0.06F);
  theme.separator_color = Color::Rgb(88, 91, 112);
  theme.text_foreground = Color::Rgb(205, 214, 244);
  theme.line_number_color = Color::Rgb(108, 112, 152);
  theme.caret_color = Color::Rgb(245, 224, 220);
  theme.link_color = Color::Rgb(137, 180, 250);
  theme.active_link_color = Color::Rgb(137, 220, 235);
  theme.codelens_color = Color::Rgb(166, 173, 200, 0.69F);
  theme.active_codelens_color = Color::Rgb(137, 180, 250);
  theme.selection_background = Color::Rgb(137, 180, 250, 0.33F);
  theme.bracket_match_background = Color::Rgb(137, 220, 235, 0.16F);
  theme.document_highlight_text = Color::Rgb(137, 180, 250, 0.10F);
  theme.document_highlight_read = Color::Rgb(137, 180, 250, 0.14F);
  theme.document_highlight_write = Color::Rgb(137, 180, 250, 0.20F);
  theme.indent_guide_color = Color::Rgb(88, 91, 112);
  theme.inlay_hint_background = Color::Rgb(137, 180, 250, 0.12F);
  theme.inlay_hint_text = Color::Rgb(166, 173, 200);
  theme.fold_placeholder_background = Color::Rgb(88, 91, 112, 0.30F);
  theme.fold_placeholder_text = Color::Rgb(205, 214, 244);
  theme.gutter_icon_color = Color::Rgb(137, 220, 235);
  theme.syntax_keyword = Color::Rgb(203, 166, 247);
  theme.syntax_type = Color::Rgb(239, 213, 128);
  theme.syntax_class = Color::Rgb(239, 213, 128);
  theme.syntax_function = Color::Rgb(137, 180, 250);
  theme.syntax_variable = Color::Rgb(205, 214, 244);
  theme.syntax_string = Color::Rgb(166, 218, 179);
  theme.syntax_number = Color::Rgb(250, 179, 135);
  theme.syntax_comment = Color::Rgb(108, 112, 152);
  theme.syntax_preprocessor = Color::Rgb(245, 139, 168);
  theme.syntax_builtin = Color::Rgb(137, 220, 235);
  theme.syntax_punctuation = Color::Rgb(148, 156, 188);
  theme.syntax_annotation = Color::Rgb(235, 160, 172);
  theme.syntax_url = Color::Rgb(137, 220, 235);
  theme.syntax_rainbow = {
      Color::Rgb(243, 139, 168), Color::Rgb(250, 179, 135), Color::Rgb(239, 213, 128),
      Color::Rgb(166, 218, 179), Color::Rgb(137, 220, 235), Color::Rgb(137, 180, 250),
      Color::Rgb(203, 166, 247), Color::Rgb(245, 194, 231),
  };
  theme.completion_background = Color::Rgb(24, 24, 37, 0.96F);
  theme.completion_border = Color::Rgb(88, 91, 112, 0.40F);
  theme.completion_selected_background = Color::Rgb(137, 180, 250, 0.28F);
  theme.completion_label = Color::Rgb(205, 214, 244);
  theme.completion_detail = Color::Rgb(108, 112, 152);
  return theme;
}

// Demo documents: each entry pairs a label with its source text and SweetLine
// syntax definition, mirroring the SweetEditor demo's file picker.
struct DemoFile {
  const char* key;
  const char* label;
  const char* text_resource;
  const char* syntax_resource;
};

constexpr DemoFile kDemoFiles[] = {
    {"demo.cpp", "C++", nullptr, "raw/syntaxes/cpp.json"},
    {"gc.cpp", "GC (1.9MB)", "raw/files/gc.cpp", "raw/syntaxes/cpp.json"},
    {"example.java", "Java", "raw/files/example.java", "raw/syntaxes/java.json"},
    {"example.kt", "Kotlin", "raw/files/example.kt", "raw/syntaxes/kotlin.json"},
    {"example.lua", "Lua", "raw/files/example.lua", "raw/syntaxes/lua.json"},
};

constexpr size_t kDemoFileCount = sizeof(kDemoFiles) / sizeof(kDemoFiles[0]);

const char* kCppDemoText =
    "// HuxerUI SweetEditor demo\n"
    "// TODO: explore completion, folds, and diagnostics\n"
    "#include <cstdint>\n"
    "#include <string>\n"
    "#include <vector>\n"
    "\n"
    "// https://example.com/sweeteditor\n"
    "\n"
    "namespace geometry {\n"
    "\n"
    "using std::vector;\n"
    "\n"
    "constexpr int kMaxVertices = 128;\n"
    "constexpr float kPi = 3.14159F;\n"
    "constexpr uint32_t kPrimaryColor = 0xFF2563EB;\n"
    "constexpr uint32_t kErrorColor = 0xFFDC2626;\n"
    "constexpr uint32_t kBackground = 0xFF0F172A;\n"
    "\n"
    "struct Vec2 {\n"
    "  float x;\n"
    "  float y;\n"
    "\n"
    "  float LengthSquared() const {\n"
    "    return x * x + y * y;\n"
    "  }\n"
    "};\n"
    "\n"
    "enum class ShapeKind {\n"
    "  Circle,\n"
    "  Rectangle,\n"
    "  Triangle,\n"
    "};\n"
    "\n"
    "// FIXME: extract a shared base class for shapes\n"
    "class Shape {\n"
    "public:\n"
    "  explicit Shape(Vec2 origin) : origin_(origin) {}\n"
    "  virtual ~Shape() = default;\n"
    "\n"
    "  virtual float Area() const = 0;\n"
    "  virtual const char* Name() const = 0;\n"
    "\n"
    "protected:\n"
    "  Vec2 origin_;\n"
    "};\n"
    "\n"
    "class Circle final : public Shape {\n"
    "public:\n"
    "  Circle(Vec2 origin, float radius) : Shape(origin), radius_(radius) {}\n"
    "\n"
    "  float Area() const override {\n"
    "    return kPi * radius_ * radius_;\n"
    "  }\n"
    "\n"
    "  const char* Name() const override {\n"
    "    return \"Circle\";\n"
    "  }\n"
    "\n"
    "private:\n"
    "  float radius_;\n"
    "};\n"
    "\n"
    "class Rectangle final : public Shape {\n"
    "public:\n"
    "  Rectangle(Vec2 origin, float width, float height)\n"
    "      : Shape(origin), width_(width), height_(height) {}\n"
    "\n"
    "  float Area() const override {\n"
    "    return width_ * height_;\n"
    "  }\n"
    "\n"
    "  const char* Name() const override {\n"
    "    return \"Rectangle\";\n"
    "  }\n"
    "\n"
    "private:\n"
    "  float width_;\n"
    "  float height_;\n"
    "};\n"
    "\n"
    "float TotalArea(const vector<Shape*>& shapes) {\n"
    "  float total = 0.0F;\n"
    "  for (const Shape* shape : shapes) {\n"
    "    if (shape == nullptr) {\n"
    "      continue;\n"
    "    }\n"
    "    const float area = shape->Area();\n"
    "    if (area > 0.0F) {\n"
    "      total += area;\n"
    "    }\n"
    "  }\n"
    "  return total;\n"
    "}\n"
    "\n"
    "}  // namespace geometry\n"
    "\n"
    "int main() {\n"
    "  using namespace geometry;\n"
    "\n"
    "  vector<Shape*> shapes;\n"
    "  shapes.push_back(new Circle({1.0F, 2.0F}, 3.0F));\n"
    "  shapes.push_back(new Rectangle({0.0F, 0.0F}, 4.0F, 5.0F));\n"
    "\n"
    "  const float area = TotalArea(shapes);\n"
    "  for (const Shape* shape : shapes) {\n"
    "    delete shape;\n"
    "  }\n"
    "\n"
    "  return static_cast<int>(area);\n"
    "}\n";

struct DemoDocument {
  const char* key;
  const char* label;
  std::string text;
  std::string syntax;
};

struct DemoFileSelected : Event<void(std::size_t)> {};
struct DiffToggleRequested : Event<void()> {};
struct ThemeToggleRequested : Event<void()> {};
struct FontToggleRequested : Event<void()> {};
struct LigatureToggleRequested : Event<void()> {};
struct WrapToggleRequested : Event<void()> {};
struct StickyGutterToggleRequested : Event<void()> {};
struct DiffClosed : Event<void()> {};

std::array<DemoDocument, kDemoFileCount> LoadDemoDocuments(
    const RawAsset& cpp_syntax,
    const RawAsset& java_syntax,
    const RawAsset& kotlin_syntax,
    const RawAsset& lua_syntax,
    const RawAsset& java_file,
    const RawAsset& kotlin_file,
    const RawAsset& lua_file,
    const RawAsset& gc_file
) {
  return {{
      {kDemoFiles[0].key, kDemoFiles[0].label, kCppDemoText, cpp_syntax.ToString()},
      {kDemoFiles[1].key, kDemoFiles[1].label, gc_file.ToString(), cpp_syntax.ToString()},
      {kDemoFiles[2].key, kDemoFiles[2].label, java_file.ToString(), java_syntax.ToString()},
      {kDemoFiles[3].key, kDemoFiles[3].label, kotlin_file.ToString(), kotlin_syntax.ToString()},
      {kDemoFiles[4].key, kDemoFiles[4].label, lua_file.ToString(), lua_syntax.ToString()},
  }};
}

[[huxerui::composable]]
View DemoFileSelector(const std::array<DemoDocument, kDemoFileCount>& documents, std::size_t selected) {
  const EventEmitter events = UseEvents();
  const std::array<std::string_view, kDemoFileCount> labels = {
      documents[0].label, documents[1].label, documents[2].label, documents[3].label, documents[4].label,
  };
  // A compact dropdown keeps the header one row tall as the demo grows.
  return Select(labels, selected, [](std::string_view label) {
           return Text(std::string(label)).Key(label);
         })
      .Label("Demo file")
      .OnChanged([events](std::size_t index) { events.Emit<DemoFileSelected>(index); })
      .With(Padding(8.0F));
}

[[huxerui::composable]]
View DemoToolbar(
    bool diff_enabled, bool wrap_enabled, bool sticky_enabled, bool dark_theme, bool maple_font, bool ligature
) {
  const EventEmitter events = UseEvents();
  return Flow {
    Button(diff_enabled ? "Diff: On" : "Diff: Off")
        .OnClick([events] { events.Emit<DiffToggleRequested>(); }),
    Button(dark_theme ? "Theme: Dark" : "Theme: Light")
        .OnClick([events] { events.Emit<ThemeToggleRequested>(); }),
    Button(maple_font ? "Font: Maple" : "Font: System")
        .OnClick([events] { events.Emit<FontToggleRequested>(); }),
    Button(ligature ? "Ligature: On" : "Ligature: Off")
        .OnClick([events] { events.Emit<LigatureToggleRequested>(); }),
    Button(wrap_enabled ? "Wrap: On" : "Wrap: Off")
        .OnClick([events] { events.Emit<WrapToggleRequested>(); }),
    Button(sticky_enabled ? "Sticky: On" : "Sticky: Off")
        .OnClick([events] { events.Emit<StickyGutterToggleRequested>(); }),
  }.With(Spacing(8.0F), Padding(8.0F));
}

[[huxerui::composable]]
View DemoDiffPanel(bool visible, State<std::string> original_text) {
  if (!visible) {
    // An empty View contributes no children and no layout space, so the
    // collapsed diff panel cannot push a gap between the toolbar and editor.
    return View{};
  }
  const EventEmitter events = UseEvents();
  return Column {
    Row {
      Text("Compare against:")
          .Style(TextStyle{Font::System(12.0F), Color::Rgb(90, 96, 105), TextDecoration::None}),
      Button("Close").OnClick([events] { events.Emit<DiffClosed>(); }),
    }.With(Spacing(8.0F), Padding(4.0F)),
    TextField(TextEditingValue::FromText(original_text.Get()))
        .LineLimits(TextFieldLineLimits::MultiLine(3, 4))
        .Placeholder("Type the original text to diff against...")
        .OnChanged([original_text](const TextEditingValue& value) { original_text = value.text; })
        .With(Padding(4.0F)),
  }.With(CrossAlign(CrossAxisAlignment::Stretch));
}

View EditorStatusBar(State<std::string> cursor_status, State<std::string> selection_status) {
  return Row {
    Text(cursor_status).Style(TextStyle{Font::Monospace(12.0F), Color::Rgb(31, 35, 40), TextDecoration::None}),
    Text(selection_status).Style(TextStyle{Font::Monospace(12.0F), Color::Rgb(31, 35, 40), TextDecoration::None}),
  }.With(Spacing(16.0F), Padding(8.0F));
}

huxerui::codeeditor::CodeEditorOptions MakeEditorOptions(
    const DemoDocument& document,
    bool diff_enabled,
    const std::string& diff_original,
    bool wrap_enabled,
    bool sticky_enabled,
    bool dark_theme,
    bool maple_font,
    bool ligature,
    State<std::vector<uint32_t>> breakpoints
) {
  huxerui::codeeditor::CodeEditorOptions options;
  options.initial_text = document.text;
  options.document_key = document.key;
  options.completion_provider = ProvideCompletions;
  options.completion_trigger_characters = [](const std::string& character) {
    return character == "." || character == ":";
  };
  // Syntax highlighting plugs in through the decoration interface: the demo
  // wires SweetLine (plus its breakpoints and phantom suggestion) as one
  // optional provider instead of the editor shipping an engine. Providers are
  // memoized per document key: building one compiles the grammar and creates
  // a fresh analyzer, which must not repeat on every recomposition.
  static std::map<std::string, std::shared_ptr<demo::SweetLineDecorationProvider>> provider_cache;
  auto provider = provider_cache.find(document.key);
  if (provider == provider_cache.end()) {
    auto created = std::make_shared<demo::SweetLineDecorationProvider>(
        document.syntax,
        document.text,
        document.key,
        [breakpoints](uint32_t start_line, uint32_t end_line) {
          std::vector<std::pair<uint32_t, int32_t>> icons;
          for (uint32_t line : breakpoints.Get()) {
            if (line >= start_line && line <= end_line) {
              icons.emplace_back(line, 2);
            }
          }
          return icons;
        },
        [](uint32_t line) { return line == 0 ? std::string(" // TODO: implement") : std::string(); }
    );
    provider = provider_cache.emplace(document.key, std::move(created)).first;
  }
  options.decoration_providers.push_back(provider->second);
  // Explicit theme override demonstrates live restyling; empty follows the
  // ambient HuxerUI theme.
  if (dark_theme) {
    options.theme = MakeDarkTheme();
  }
  // Maple Mono ships as an Android asset font; "@noliga" is the bridge's
  // convention for disabling ligatures at the shaping level.
  if (maple_font) {
    options.font_family = ligature ? "MapleMono" : "MapleMono@noliga";
  }
  options.original_text = diff_enabled ? diff_original : std::string();
  options.wrap_mode = wrap_enabled ? 2 : 0;
  options.sticky_gutter = sticky_enabled;
  return options;
}

[[huxerui::composable]]
View CodeEditorDemo() {
  auto current_file = UseState<std::size_t>(0);
  auto diff_enabled = UseState(false);
  auto diff_original = UseState(std::string());
  auto wrap_enabled = UseState(false);
  auto sticky_enabled = UseState(false);
  auto dark_theme = UseState(false);
  auto maple_font = UseState(false);
  auto ligature = UseState(true);
  auto breakpoints = UseState(std::vector<uint32_t>());
  auto cursor_status = UseState(std::string("Ln 1, Col 1"));
  auto selection_status = UseState(std::string());

  const RawAsset cpp_syntax = UseRawResource(RawResource("app", "raw/syntaxes/cpp.json"));
  const RawAsset java_syntax = UseRawResource(RawResource("app", "raw/syntaxes/java.json"));
  const RawAsset kotlin_syntax = UseRawResource(RawResource("app", "raw/syntaxes/kotlin.json"));
  const RawAsset lua_syntax = UseRawResource(RawResource("app", "raw/syntaxes/lua.json"));
  const RawAsset java_file = UseRawResource(RawResource("app", "raw/files/example.java"));
  const RawAsset kotlin_file = UseRawResource(RawResource("app", "raw/files/example.kt"));
  const RawAsset lua_file = UseRawResource(RawResource("app", "raw/files/example.lua"));
  const RawAsset gc_file = UseRawResource(RawResource("app", "raw/files/gc.cpp"));
  const auto documents = LoadDemoDocuments(
      cpp_syntax, java_syntax, kotlin_syntax, lua_syntax, java_file, kotlin_file, lua_file, gc_file);
  const ToastHandle toast = UseToast();
  const huxerui::codeeditor::CodeEditorController controller = huxerui::codeeditor::UseCodeEditorController();
  const DemoDocument& document = documents[current_file.Get()];
  const auto options = MakeEditorOptions(
      document, diff_enabled.Get(), diff_original.Get(), wrap_enabled.Get(), sticky_enabled.Get(), dark_theme.Get(),
      maple_font.Get(), ligature.Get(), breakpoints);

  return Column {
    DemoFileSelector(documents, current_file.Get())
        .On<DemoFileSelected>([current_file, diff_enabled, diff_original, documents](std::size_t index) {
          current_file = index;
          if (diff_enabled.Get()) {
            diff_original = documents[index].text;
          }
        }),
    DemoToolbar(
        diff_enabled.Get(), wrap_enabled.Get(), sticky_enabled.Get(), dark_theme.Get(), maple_font.Get(),
        ligature.Get())
        .On<DiffToggleRequested>([current_file, diff_enabled, diff_original, documents] {
          const bool enabled = !diff_enabled.Get();
          diff_enabled = enabled;
          diff_original = enabled ? documents[current_file.Get()].text : std::string();
        })
        .On<WrapToggleRequested>([wrap_enabled] { wrap_enabled = !wrap_enabled.Get(); })
        .On<ThemeToggleRequested>([dark_theme] { dark_theme = !dark_theme.Get(); })
        .On<FontToggleRequested>([maple_font] { maple_font = !maple_font.Get(); })
        .On<LigatureToggleRequested>([ligature] { ligature = !ligature.Get(); })
        .On<StickyGutterToggleRequested>([sticky_enabled] { sticky_enabled = !sticky_enabled.Get(); }),
    DemoDiffPanel(diff_enabled.Get(), diff_original)
        .On<DiffClosed>([diff_enabled, diff_original] {
          diff_enabled = false;
          diff_original = std::string();
        }),
    huxerui::codeeditor::CodeEditor(options, controller)
        .On<huxerui::codeeditor::CodeEditorEvents::LinkClicked>([toast](const std::string& url) {
          toast.Show("Link: " + url);
        })
        .On<huxerui::codeeditor::CodeEditorEvents::CodeLensClicked>([toast](int32_t command_id) {
          toast.Show("CodeLens: " + std::to_string(command_id));
        })
        .On<huxerui::codeeditor::CodeEditorEvents::GutterIconClicked>([toast, breakpoints](uint32_t line, int32_t) {
          std::vector<uint32_t> lines = breakpoints.Get();
          const auto found = std::find(lines.begin(), lines.end(), line);
          if (found == lines.end()) {
            lines.push_back(line);
            toast.Show("Breakpoint set at line " + std::to_string(line + 1));
          } else {
            lines.erase(found);
            toast.Show("Breakpoint removed at line " + std::to_string(line + 1));
          }
          breakpoints = std::move(lines);
        })
        .On<huxerui::codeeditor::CodeEditorEvents::InlayClicked>([toast](uint32_t line, uint32_t column) {
          toast.Show("Inlay: " + std::to_string(line + 1) + ":" + std::to_string(column + 1));
        })
        .On<huxerui::codeeditor::CodeEditorEvents::CursorChanged>([cursor_status](uint32_t line, uint32_t column) {
          cursor_status = "Ln " + std::to_string(line + 1) + ", Col " + std::to_string(column + 1);
        })
        .On<huxerui::codeeditor::CodeEditorEvents::SelectionChanged>(
            [selection_status](uint32_t, uint32_t, uint32_t line, uint32_t column) {
              selection_status = "Selection Ln " + std::to_string(line + 1) + ", Col " +
                                 std::to_string(column + 1);
            }
        )
        .With(Grow{}),
    EditorStatusBar(cursor_status, selection_status),
  }.With(CrossAlign(CrossAxisAlignment::Stretch));
}

View App() {
  return CodeEditorDemo();
}

const Application application{
    App,
    {
        .window = {.title = "CodeEditor"},
        .show_debug_overlay = true,
    },
};
