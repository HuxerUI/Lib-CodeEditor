#include <huxerui/huxerui.h>

#include <algorithm>
#include <cstdio>

#include "sweet_editor/sweet_editor.h"

using namespace huxerui;

namespace {

using sweetedit_huxer::CompletionContext;
using sweetedit_huxer::CompletionItem;
using sweetedit_huxer::CompletionItemKind;

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

View App() {
  // The runtime applies SafeAreaPadding to the application root. A plain
  // container (not the editor canvas) consumes it, so the canvas becomes a
  // child whose event coordinates are mapped into its local space and stay
  // aligned with its paint coordinates (on edge-to-edge Android the root
  // would otherwise consume the status-bar inset and shift tap hit-testing).
  auto current_file = UseState(0);
  // Diff presentation toggle: when on, the editor diffs the live text against
  // an editable comparison text (defaults to the pristine demo content).
  auto diff_enabled = UseState(false);
  auto diff_original = UseState(std::string());
  // Display options: wrap long lines and keep the gutter fixed while scrolling
  // horizontally (reference editor settings).
  auto wrap_enabled = UseState(false);
  auto sticky_enabled = UseState(false);
  // Breakpoint demo: tap the gutter icon of a line to toggle a breakpoint
  // marker (rendered as a filled circle via icon_id 2).
  auto breakpoints = UseState(std::vector<uint32_t>());

  // Load every demo syntax + file resource unconditionally (hook order stable).
  const RawAsset cpp_syntax = UseRawResource(RawResource("app", "raw/syntaxes/cpp.json"));
  const RawAsset java_syntax = UseRawResource(RawResource("app", "raw/syntaxes/java.json"));
  const RawAsset kotlin_syntax = UseRawResource(RawResource("app", "raw/syntaxes/kotlin.json"));
  const RawAsset lua_syntax = UseRawResource(RawResource("app", "raw/syntaxes/lua.json"));
  const RawAsset java_file = UseRawResource(RawResource("app", "raw/files/example.java"));
  const RawAsset kotlin_file = UseRawResource(RawResource("app", "raw/files/example.kt"));
  const RawAsset lua_file = UseRawResource(RawResource("app", "raw/files/example.lua"));
  const RawAsset gc_file = UseRawResource(RawResource("app", "raw/files/gc.cpp"));

  const int index = current_file.Get();
  const std::string texts[kDemoFileCount] = {
      kCppDemoText,
      gc_file.ToString(),
      java_file.ToString(),
      kotlin_file.ToString(),
      lua_file.ToString(),
  };
  const std::string syntaxes[kDemoFileCount] = {
      cpp_syntax.ToString(),
      cpp_syntax.ToString(),
      java_syntax.ToString(),
      kotlin_syntax.ToString(),
      lua_syntax.ToString(),
  };

  sweetedit_huxer::SweetEditorOptions options;
  options.initial_text = texts[static_cast<size_t>(index)];
  options.syntax_json = syntaxes[static_cast<size_t>(index)];
  options.document_key = kDemoFiles[static_cast<size_t>(index)].key;
  options.completion_provider = ProvideCompletions;
  options.completion_trigger_characters = [](const std::string& ch) { return ch == "." || ch == ":"; };
  const ToastHandle toast = UseToast();
  options.on_link_click = [toast](const std::string& url) {
    toast.Show("Link: " + url);
  };
  options.on_codelens_click = [toast](int32_t command_id) {
    toast.Show("CodeLens: " + std::to_string(command_id));
  };
  options.on_gutter_icon_click = [toast, breakpoints](uint32_t line, int32_t icon_id) {
    std::vector<uint32_t> lines = breakpoints.Get();
    const auto found = std::find(lines.begin(), lines.end(), line);
    if (found != lines.end()) {
      lines.erase(found);
      toast.Show("Breakpoint removed at line " + std::to_string(line + 1));
    } else {
      lines.push_back(line);
      toast.Show("Breakpoint set at line " + std::to_string(line + 1));
    }
    breakpoints = std::move(lines);
  };
  options.gutter_icon_provider = [breakpoints](uint32_t start_line, uint32_t end_line) {
    std::vector<std::pair<uint32_t, int32_t>> icons;
    for (uint32_t line : breakpoints.Get()) {
      if (line >= start_line && line <= end_line) {
        icons.emplace_back(line, 2);  // icon_id 2 = breakpoint (filled circle)
      }
    }
    return icons;
  };
  options.on_inlay_click = [toast](uint32_t line, uint32_t column) {
    toast.Show("Inlay: " + std::to_string(line + 1) + ":" + std::to_string(column + 1));
  };

  // Status bar: live caret position, selection length, and fold toggles.
  auto cursor_status = UseState(std::string("Ln 1, Col 1"));
  auto selection_status = UseState(std::string());
  options.on_cursor_changed = [cursor_status](uint32_t line, uint32_t column) {
    cursor_status = "Ln " + std::to_string(line + 1) + ", Col " + std::to_string(column + 1);
  };
  options.on_selection_changed =
      [cursor_status, selection_status](uint32_t, uint32_t, uint32_t end_line, uint32_t end_column) {
        selection_status = "Selection Ln " + std::to_string(end_line + 1) + ", Col " +
                           std::to_string(end_column + 1);
      };
  options.on_text_changed = [] {
    std::fprintf(stderr, "[sweetedit] text changed\n");
  };
  options.on_scroll_changed = [](float x, float y) {
    std::fprintf(stderr, "[sweetedit] scroll %.0f,%.0f\n", x, y);
  };
  options.on_fold_toggle = [](size_t line) {
    std::fprintf(stderr, "[sweetedit] fold toggled at line %zu\n", line);
  };
  options.on_long_press = [](uint32_t line, uint32_t column) {
    std::fprintf(stderr, "[sweetedit] long press %u:%u\n", line, column);
  };
  options.on_double_tap = [](uint32_t line, uint32_t column) {
    std::fprintf(stderr, "[sweetedit] double tap %u:%u\n", line, column);
  };

  // Copilot-style ghost text: suggest a TODO comment at the end of the first
  // line; Tab accepts it (the component's phantom pipeline commits it).
  options.phantom_text_provider = [](uint32_t line) {
    if (line == 0) {
      return std::string(" // TODO: implement");
    }
    return std::string();
  };
  options.accept_phantom_on_tab = true;
  // Diff against the comparison text when the toggle is on.
  options.original_text = diff_enabled.Get() ? diff_original.Get() : std::string();
  // Display toggles.
  options.wrap_mode = wrap_enabled.Get() ? 2 : 0;  // 2 = WORD_BREAK
  options.sticky_gutter = sticky_enabled.Get();

  std::vector<View> file_buttons;
  file_buttons.reserve(kDemoFileCount);
  for (size_t i = 0; i < kDemoFileCount; ++i) {
    file_buttons.push_back(
        Button(kDemoFiles[i].label)
            .On<ViewEvents::Click>([current_file, diff_enabled, diff_original, texts, i] {
              current_file = static_cast<int>(i);
              if (diff_enabled.Get()) {
                diff_original = texts[static_cast<size_t>(i)];
              }
            })
    );
  }

  std::vector<View> children;
  children.reserve(6);
  children.push_back(Row { std::move(file_buttons) }.With(Spacing(8.0F), Padding(8.0F)));
  children.push_back(Row {
    Button(diff_enabled.Get() ? "Diff: On" : "Diff: Off")
        .On<ViewEvents::Click>([diff_enabled, diff_original, texts, index] {
          const bool turning_on = !diff_enabled.Get();
          diff_enabled = turning_on;
          diff_original = turning_on ? texts[static_cast<size_t>(index)] : std::string();
        }),
    Button(wrap_enabled.Get() ? "Wrap: On" : "Wrap: Off")
        .On<ViewEvents::Click>([wrap_enabled] { wrap_enabled = !wrap_enabled.Get(); }),
    Button(sticky_enabled.Get() ? "Sticky: On" : "Sticky: Off")
        .On<ViewEvents::Click>([sticky_enabled] { sticky_enabled = !sticky_enabled.Get(); }),
  }.With(Spacing(8.0F), Padding(8.0F)));
  if (diff_enabled.Get()) {
    children.push_back(
        Row {
          Text("Compare against:")
              .Style(TextStyle{Font::System(12.0F), Color::Rgb(90, 96, 105), TextDecoration::None}),
          Button("Close").On<ViewEvents::Click>([diff_enabled, diff_original] {
            diff_enabled = false;
            diff_original = std::string();
          }),
        }.With(Spacing(8.0F), Padding(4.0F))
    );
    children.push_back(
        TextField(TextEditingValue::FromText(diff_original.Get()))
            .LineLimits(TextFieldLineLimits::MultiLine(3, 4))
            .Placeholder("Type the original text to diff against...")
            .OnChanged([diff_original](const TextEditingValue& value) { diff_original = value.text; })
            .With(Padding(4.0F))
    );
  }
  children.push_back(sweetedit_huxer::SweetEditor(options));
  children.push_back(Row {
    Text(cursor_status.Get()).Style(TextStyle{Font::Monospace(12.0F), Color::Rgb(31, 35, 40), TextDecoration::None}),
    Text(selection_status.Get()).Style(TextStyle{Font::Monospace(12.0F), Color::Rgb(31, 35, 40), TextDecoration::None}),
  }.With(Spacing(16.0F), Padding(8.0F)));

  return Column { std::move(children) }.With(CrossAlign(CrossAxisAlignment::Stretch));
}

// The debug overlay is enabled explicitly so it works in release builds too.
const Application application{
    App,
    {
        .window = {.title = "sweetedit_huxer"},
        .show_debug_overlay = true,
    },
};
