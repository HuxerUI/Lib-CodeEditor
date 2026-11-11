#include <huxerui/huxerui.h>

#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <huxerui/codeeditor.h>
#include <sweeteditor/utility.h>

#include <app_resources.h>

#include "sweetline_provider.h"

using namespace huxerui;

namespace {

namespace ce = huxerui::codeeditor;

using huxerui::codeeditor::CompletionContext;
using huxerui::codeeditor::CompletionItem;
using huxerui::codeeditor::CompletionItemKind;

constexpr char kForSnippet[] = "for (int ${1:i} = 0; ${1:i} < ${2:n}; ++${1:i}) {\n\t$0\n}";

bool IsDemoIdentifierCharacter(char16_t character) {
  return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
         (character >= '0' && character <= '9') || character == '_';
}

std::vector<CompletionItem> CollectDemoCompletions(const CompletionContext& context) {
  struct Candidate {
    const char* label;
    const char* detail;
    const char* insert_text;
    CompletionItemKind kind;
    const char* sort_key;
    bool snippet = false;
  };
  static constexpr Candidate kCandidates[] = {
      {"std::string", "class", "std::string", CompletionItemKind::Class, "a_string"},
      {"std::vector", "template class", "std::vector<>", CompletionItemKind::Class, "b_vector"},
      {"std::cout", "ostream", "std::cout", CompletionItemKind::Variable, "c_cout"},
      {"if", "snippet", "if (${1:condition}) {\n\t$0\n}", CompletionItemKind::Snippet, "d_if", true},
      {"for", "snippet", kForSnippet, CompletionItemKind::Snippet, "e_for", true},
      {"class", "snippet - class definition",
       "class ${1:ClassName} {\npublic:\n\t${1:ClassName}() {$2}\n\t~${1:ClassName}() {$3}\n$0\n};",
       CompletionItemKind::Snippet, "f_class", true},
      {"return", "keyword", "return ", CompletionItemKind::Keyword, "g_return"},
  };
  static constexpr Candidate kMemberCandidates[] = {
      {"length", "size_t", "length()", CompletionItemKind::Property, "a_length"},
      {"push_back", "void push_back(T)", "push_back()", CompletionItemKind::Function, "b_push_back"},
      {"begin", "iterator", "begin()", CompletionItemKind::Function, "c_begin"},
      {"end", "iterator", "end()", CompletionItemKind::Function, "d_end"},
      {"size", "size_t", "size()", CompletionItemKind::Function, "e_size"},
  };

  sweeteditor::U16String line;
  sweeteditor::StrUtil::convertUTF8ToUTF16(context.line_text, line);
  const auto caret = context.cursor_position;
  if (caret.column > line.size()) return {};
  ce::TextRange replacement = context.word_range;
  if (replacement.start.column >= line.size() || !IsDemoIdentifierCharacter(line[replacement.start.column])) {
    replacement = {caret, caret};
  }
  const auto start = replacement.start.column;
  if (start > caret.column || replacement.end.column > line.size() || replacement.end.column < caret.column) return {};

  const bool member_access = start > 0 && line[start - 1] == '.';
  std::string prefix;
  sweeteditor::StrUtil::convertUTF16ToUTF8(line.substr(start, caret.column - start), prefix);
  if (!member_access && prefix.empty() && context.trigger_kind != ce::CompletionTriggerKind::Invoked) return {};

  const auto candidates = member_access ? std::span<const Candidate>(kMemberCandidates)
                                        : std::span<const Candidate>(kCandidates);
  std::vector<CompletionItem> items;
  for (const Candidate& candidate : candidates) {
    if (!std::string_view(candidate.label).starts_with(prefix)) continue;
    items.push_back({
        .label = candidate.label,
        .detail = candidate.detail,
        .insert_text_format = candidate.snippet ? ce::InsertTextFormat::Snippet : ce::InsertTextFormat::PlainText,
        .text_edit = ce::TextEdit{replacement, candidate.insert_text},
        .sort_key = candidate.sort_key,
        .kind = candidate.kind,
    });
  }
  return items;
}

class DemoCompletionProvider final : public ce::CompletionProvider {
 public:
  bool IsTriggerCharacter(std::string_view character) const override {
    return character == "." || (character.size() == 1 && IsDemoIdentifierCharacter(character.front()));
  }

  Task<ce::CompletionResult> ProvideCompletions(CompletionContext context) override {
    co_return ce::CompletionResult{CollectDemoCompletions(context), true};
  }
};

ce::CompletionItemRenderer DemoCompletionRows(const ce::EditorTheme& theme) {
  return [label = theme.completion_label_color, detail = theme.completion_detail_color, accent = theme.cursor_color](
      const CompletionItem& item, bool selected) {
    return Column {
      Text(item.label).Style({Font::Monospace(12.0F), selected ? accent : label}),
      item.detail.empty() ? View{} : Text(item.detail).Style({Font::System(10.0F), detail}),
    }.With(Padding(EdgeInsets::Symmetric(12.0F, 0.0F)), Spacing(1.0F), ClipChildren());
  };
}

struct DemoFile {
  const char* label;
  const char* language;
};

constexpr DemoFile kDemoFiles[] = {
    {"demo.cpp", "C++"},
    {"gc.cpp", "C++"},
    {"example.java", "Java"},
    {"example.kt", "Kotlin"},
    {"example.lua", "Lua"},
    {"diff: example.java", "Java"},
};

const char* kCppDemoText =
    "// CodeEditor Preview\n"
    "// TODO: explore completion, folds, and diagnostics\n"
    "#include <cstdint>\n"
    "#include <string>\n"
    "#include <vector>\n"
    "\n"
    "// https://example.com/codeeditor\n"
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
  std::string text;
  std::string syntax;
  std::optional<std::string> diff_original = std::nullopt;

  bool operator==(const DemoDocument&) const = default;
};

using DemoDocuments = std::array<DemoDocument, std::size(kDemoFiles)>;

std::string MakeJavaDiffSample(std::string text) {
  const std::string newline = text.find("\r\n") != std::string::npos ? "\r\n" : "\n";
  const auto replace = [&text](std::string_view from, std::string_view to) {
    if (const auto position = text.find(from); position != std::string::npos) {
      text.replace(position, from.size(), to);
    }
  };
  replace("import java.util.Map;", "import java.util.Map;" + newline + "import java.util.Set;");
  replace("import java.io.*;" + newline, "");
  replace("private static final int MAX_SIZE = 100;", "private static final int MAX_SIZE = 256;");
  return text;
}

DemoDocuments LoadDemoDocuments(const Resources& resources) {
  const std::string cpp = resources.GetRawResource(app::raw::syntaxes_cpp_json).ReadString();
  const std::string java = resources.GetRawResource(app::raw::files_example_java).ReadString();
  const std::string java_syntax = resources.GetRawResource(app::raw::syntaxes_java_json).ReadString();
  return {{
      {kDemoFiles[0].label, kCppDemoText, cpp},
      {kDemoFiles[1].label, resources.GetRawResource(app::raw::files_gc_cpp).ReadString(), cpp},
      {kDemoFiles[2].label, java, java_syntax},
      {kDemoFiles[3].label, resources.GetRawResource(app::raw::files_example_kt).ReadString(),
       resources.GetRawResource(app::raw::syntaxes_kotlin_json).ReadString()},
      {kDemoFiles[4].label, resources.GetRawResource(app::raw::files_example_lua).ReadString(),
       resources.GetRawResource(app::raw::syntaxes_lua_json).ReadString()},
      {kDemoFiles[2].label, MakeJavaDiffSample(java), java_syntax, java},
  }};
}

Color WithOpacity(Color color, float alpha) {
  color.alpha = alpha;
  return color;
}

ThemeDefinition PreviewTheme(bool dark) {
  auto theme = dark ? FlatDarkThemeSpec() : FlatLightThemeSpec();
  auto& colors = theme.colors;
  if (dark) {
    colors.primary = Color::Rgb(137, 180, 250);
    colors.primary_container = Color::Rgb(49, 64, 96);
    colors.on_primary_container = Color::Rgb(221, 233, 255);
    colors.background = colors.surface_container = Color::Rgb(24, 24, 37);
    colors.surface = Color::Rgb(30, 30, 46);
    colors.surface_container_high = Color::Rgb(37, 38, 54);
    colors.surface_container_highest = Color::Rgb(49, 50, 68);
    colors.on_surface = Color::Rgb(205, 214, 244);
    colors.on_surface_variant = Color::Rgb(166, 173, 200);
    colors.outline = Color::Rgb(63, 65, 86);
    colors.error = Color::Rgb(243, 139, 168);
  } else {
    colors.surface_container = Color::Rgb(246, 248, 252);
    colors.surface_container_high = Color::Rgb(238, 242, 248);
    colors.outline = Color::Rgb(211, 219, 231);
  }
  theme.typography.body_medium = 13.0F;
  theme.typography.label_large = 12.0F;
  theme.interactions.focus_ring.color = colors.primary;
  auto definition = FlatThemeDefinition(theme);
  definition.Set(DividerStyle{.color = colors.outline});
  definition.Set(ButtonStyle{
      .background = colors.surface_container_high,
      .label_style = TextStyle{Font::System(12.0F).WithWeight(FontWeight::Medium), colors.on_surface},
      .disabled_background = colors.surface_container,
      .disabled_label = WithOpacity(colors.on_surface_variant, 0.45F),
      .padding = EdgeInsets::Symmetric(10.0F, 5.0F),
      .minimum_height = 30.0F,
      .corner_radii = CornerRadii{5.0F},
      .indication = theme.interactions.indication,
  });
  definition.Set(IconButtonStyle{
      .foreground = colors.on_surface_variant,
      .disabled_foreground = WithOpacity(colors.on_surface_variant, 0.4F),
      .icon_size = 16.0F,
      .minimum_interactive_size = 32.0F,
      .state_layer_size = 30.0F,
      .corner_radius = 5.0F,
      .indication = theme.interactions.indication,
  });
  definition.Set(ChipStyle{
      .background = colors.surface_container_high,
      .selected_background = colors.primary_container,
      .label_style = TextStyle{Font::System(12.0F), colors.on_surface},
      .selected_label = colors.on_primary_container,
      .border = {Color::Transparent(), 0.0F},
      .padding = EdgeInsets::Symmetric(9.0F, 5.0F),
      .minimum_height = 30.0F,
      .corner_radii = CornerRadii{5.0F},
      .indication = theme.interactions.indication,
      .selected_indication = theme.interactions.indication,
  });
  definition.Set(SelectStyle{
      .background = colors.surface,
      .foreground = colors.on_surface,
      .border = {colors.outline, 1.0F},
      .indicator = colors.on_surface_variant,
      .popup_background = colors.surface,
      .active_item_background = colors.surface_container_high,
      .selected_item_background = colors.primary_container,
      .trigger_padding = EdgeInsets::Symmetric(10.0F, 5.0F),
      .minimum_width = 96.0F,
      .minimum_height = 32.0F,
      .minimum_item_height = 32.0F,
      .indicator_size = 14.0F,
      .corner_radii = CornerRadii{5.0F},
  });
  definition.Set(MenuStyle{
      .background = colors.surface_container_high,
      .foreground = colors.on_surface,
      .icon_tint = colors.on_surface_variant,
      .item_indication = theme.interactions.indication,
      .separator_color = colors.outline,
      .separator_mode = MenuSeparatorMode::BetweenSections,
      .item_padding = EdgeInsets::Symmetric(10.0F, 5.0F),
      .minimum_item_height = 30.0F,
  });
  auto fields = TextFieldStyle::Default();
  fields.show_label = false;
  fields.standard = {
      .background = colors.surface,
      .border = colors.outline,
      .hovered_border = colors.on_surface_variant,
      .focused_border = colors.primary,
      .corner_radii = CornerRadii{5.0F},
      .minimum_height = 32.0F,
  };
  fields.text_style = TextStyle{Font::System(13.0F), colors.on_surface};
  fields.placeholder_style = TextStyle{Font::System(13.0F), colors.on_surface_variant};
  fields.padding = EdgeInsets::Symmetric(9.0F, 5.0F);
  fields.caret = colors.primary;
  fields.selection = WithOpacity(colors.primary, 0.3F);
  fields.composition = colors.primary;
  definition.Set(fields);
  return definition;
}

VectorAsset OutlineIcon(Path path) {
  return VectorAsset::Create({16.0F, 16.0F}, [path = std::move(path)](VectorBuilder& builder) {
    builder.StrokePath(path, Color::White(), StrokeStyle{.width = 1.5F, .cap = StrokeCap::Round, .join = StrokeJoin::Round});
  });
}

const auto kUndoIcon = OutlineIcon(Path().MoveTo({6, 3}).LineTo({2, 7}).LineTo({6, 11})
    .MoveTo({2, 7}).LineTo({10, 7}).CubicTo({15, 7}, {15, 13}, {11, 13}));
const auto kRedoIcon = OutlineIcon(Path().MoveTo({10, 3}).LineTo({14, 7}).LineTo({10, 11})
    .MoveTo({14, 7}).LineTo({6, 7}).CubicTo({1, 7}, {1, 13}, {5, 13}));
const auto kFindIcon = OutlineIcon(Path().MoveTo({11, 6.5F})
    .ArcTo({4.5F, 4.5F}, 0, ArcSize::Small, ArcDirection::Clockwise, {2, 6.5F})
    .ArcTo({4.5F, 4.5F}, 0, ArcSize::Small, ArcDirection::Clockwise, {11, 6.5F})
    .MoveTo({10, 10}).LineTo({14, 14}));
const auto kPreviousIcon = OutlineIcon(Path().MoveTo({4, 10}).LineTo({8, 6}).LineTo({12, 10}));
const auto kNextIcon = OutlineIcon(Path().MoveTo({4, 6}).LineTo({8, 10}).LineTo({12, 6}));
const auto kCloseIcon = OutlineIcon(Path().MoveTo({4, 4}).LineTo({12, 12}).MoveTo({12, 4}).LineTo({4, 12}));
const auto kSettingsIcon = OutlineIcon(Path().MoveTo({2, 4}).LineTo({6, 4}).MoveTo({9, 4}).LineTo({14, 4})
    .MoveTo({6, 2}).LineTo({6, 6}).MoveTo({2, 8}).LineTo({10, 8}).MoveTo({13, 8}).LineTo({14, 8})
    .MoveTo({10, 6}).LineTo({10, 10}).MoveTo({2, 12}).LineTo({5, 12}).MoveTo({8, 12}).LineTo({14, 12})
    .MoveTo({5, 10}).LineTo({5, 14}));
const auto kActionsIcon = VectorAsset::Create({16.0F, 16.0F}, [](VectorBuilder& builder) {
  for (const float x : {3.0F, 7.0F, 11.0F}) {
    builder.FillPath(Path::RoundedRect({x, 7.0F, 2.0F, 2.0F}, CornerRadii{1.0F}), Color::White());
  }
});

View ToolIcon(const VectorAsset& icon, const char* label, std::function<void()> action, bool enabled = true) {
  return IconButton(icon, label).OnClick(std::move(action)).With(Enabled(enabled), Tooltip(label));
}

enum class SearchMode { Closed, Find, Replace };

struct SearchFilters {
  bool match_case = false;
  bool whole_word = false;
  bool regex = false;
  bool operator==(const SearchFilters&) const = default;
};

struct DemoSearch {
  ce::EditorController editor;
  ViewHandle find_button;
  ViewHandle field_focus;
  State<SearchMode> mode;
  State<TextEditingValue> query;
  State<TextEditingValue> replacement;
  State<SearchFilters> filters;
  State<std::optional<ce::SearchState>> result;
  State<std::string> status;

  SearchMode Mode() const { return mode; }

  void Open(SearchMode next) const {
    const bool visible = Mode() != SearchMode::Closed;
    mode = next;
    if (visible) field_focus.RequestFocus();
  }

  void Close() const {
    mode = SearchMode::Closed;
    editor.ClearSearch();
    result = std::nullopt;
    find_button.RequestFocus();
  }

  void Refresh() const {
    if (Mode() == SearchMode::Closed || query->composition) return;
    if (query->text.empty()) {
      editor.ClearSearch();
    } else {
      editor.Search({.pattern = query->text,
                     .options = {.case_sensitive = filters->match_case, .whole_word = filters->whole_word,
                                 .use_regex = filters->regex}});
    }
    result = editor.GetSearchState();
  }

  void Navigate(bool forward) const {
    if (query->composition) return;
    if (forward) {
      editor.FindNextSearchMatch();
    } else {
      editor.FindPreviousSearchMatch();
    }
    result = editor.GetSearchState();
  }

  void Replace(bool all) const {
    if (query->composition || replacement->composition) return;
    const auto matches = editor.GetSearchState();
    if (!matches || matches->pattern != query->text || matches->match_count == 0) return;
    if (all) {
      if (editor.ReplaceAllSearchMatches(replacement->text)) {
        status = "Replaced " + std::to_string(matches->match_count) +
            (matches->match_count == 1 ? " match" : " matches");
      }
    } else if (editor.ReplaceCurrentSearchMatch(replacement->text)) {
      status = "Replaced current match";
    }
  }
};

View SearchFilter(const DemoSearch& search, const char* label, const char* tooltip, bool SearchFilters::*filter) {
  const SearchFilters& filters = search.filters;
  return Chip(label, filters.*filter)
      .OnChanged([filters = search.filters, filter](bool enabled) {
        filters.Update([filter, enabled](SearchFilters& next) { next.*filter = enabled; });
      })
      .With(Tooltip(tooltip));
}

[[huxerui::composable]]
View SearchPanel(DemoSearch search, State<std::size_t> document_revision) {
  Lifecycle([focus = search.field_focus] { focus.RequestFocus(); });
  const SearchFilters& options = search.filters;
  Lifecycle([search] { search.Refresh(); }, search.query->text, search.query->composition.has_value(),
            options, document_revision);

  const auto& colors = UseTheme().colors;
  const bool compact = UseViewportClass() == ViewportClass::Compact;
  const bool replacing = search.Mode() == SearchMode::Replace;
  const std::optional<ce::SearchState>& matches = search.result;
  const bool can_navigate = matches && matches->pattern == search.query->text && matches->match_count > 0 &&
      !search.query->composition;
  const bool can_replace = can_navigate && !search.replacement->composition;
  std::string count;
  std::string error;
  if (matches && !search.query->text.empty()) {
    if (!matches->error_message.empty()) {
      error = matches->error_message;
    } else if (matches->has_current_match) {
      count = std::to_string(matches->current_index + 1) + " / " + std::to_string(matches->match_count);
    } else if (matches->match_count == 0) {
      count = "No matches";
    } else {
      count = std::to_string(matches->match_count) + " matches";
    }
  }

  const auto query = TextField(search.query)
      .Label("Find in document")
      .Placeholder("Find in document")
      .InputConfiguration({.action = TextInputAction::Search, .autocorrect = false})
      .OnChanged([value = search.query](const TextEditingValue& next) { value = next; })
      .OnSubmitted([search] { search.Navigate(true); })
      .On<ViewEvents::KeyIntercept>([search](const KeyEvent& event) {
        if (event.type != KeyEventType::Down || event.key != Key::Enter || !event.modifiers.shift ||
            search.query->composition) {
          return false;
        }
        search.Navigate(false);
        return true;
      })
      .With(search.field_focus, Grow(), Semantics{.role = SemanticRole::SearchField});
  const View filters = Row {
    SearchFilter(search, "Aa", "Match case", &SearchFilters::match_case),
    SearchFilter(search, "Word", "Match whole word", &SearchFilters::whole_word),
    SearchFilter(search, ".*", "Use regular expression", &SearchFilters::regex),
  }.With(Spacing(4.0F), CrossAlign(CrossAxisAlignment::Center));
  const auto match_count = Text(count)
      .With(FontSize(11.0F), Foreground(colors.on_surface_variant), Frame{.min_width = 70.0F},
            Semantics{.live_region = SemanticLiveRegion::Polite});
  const View replace_toggle = Chip("Replace", replacing).OnChanged([mode = search.mode](bool enabled) {
    mode = enabled ? SearchMode::Replace : SearchMode::Find;
  }).With(Tooltip("Show or hide the replacement field"));
  const auto close = ToolIcon(kCloseIcon, "Close find (Esc)", [search] { search.Close(); });

  return Column {
    Row {
      query,
      compact ? View{} : filters,
      compact ? View{} : match_count,
      ToolIcon(kPreviousIcon, "Previous match (Shift+Enter)", [search] { search.Navigate(false); }, can_navigate),
      ToolIcon(kNextIcon, "Next match (Enter)", [search] { search.Navigate(true); }, can_navigate),
      compact ? View{} : replace_toggle,
      close,
    }.With(Spacing(5.0F), CrossAlign(CrossAxisAlignment::Center)),
    compact ? Row {
      filters, Spacer().With(Grow()), match_count, replace_toggle,
    }.With(Spacing(5.0F), CrossAlign(CrossAxisAlignment::Center)) : View{},
    replacing ? Row {
      TextField(search.replacement)
          .Label("Replace with")
          .Placeholder("Replace with")
          .InputConfiguration({.autocorrect = false})
          .OnChanged([value = search.replacement](const TextEditingValue& next) { value = next; })
          .OnSubmitted([search] { search.Replace(false); })
          .With(Grow()),
      Button("Replace").OnClick([search] { search.Replace(false); })
          .With(Enabled(can_replace && matches->has_current_match)),
      Button("Replace all").OnClick([search] { search.Replace(true); }).With(Enabled(can_replace)),
    }.With(Spacing(5.0F), CrossAlign(CrossAxisAlignment::Center)) : View{},
    error.empty() ? View{} : Text(error).With(FontSize(12.0F), Foreground(colors.error)),
  }.With(Padding(EdgeInsets::Symmetric(12.0F, 6.0F)), Spacing(5.0F),
         CrossAlign(CrossAxisAlignment::Stretch), Background(colors.surface_container));
}

[[huxerui::composable]]
View EditorStatusBar(const char* language, State<std::string> status, State<ce::TextPosition> cursor,
                     State<std::optional<ce::TextRange>> selection) {
  const auto& colors = UseTheme().colors;
  const bool compact = UseViewportClass() == ViewportClass::Compact;
  std::string selected;
  if (const std::optional<ce::TextRange>& range = selection; range) {
    if (range->start.line == range->end.line) {
      selected = std::to_string(range->end.column - range->start.column) + " selected";
    } else {
      selected = std::to_string(range->end.line - range->start.line + 1) + " lines selected";
    }
  }
  return Row {
    Text(status).With(Grow(), ClipChildren(), Semantics{.live_region = SemanticLiveRegion::Polite}),
    Text(language).With(Foreground(colors.on_surface)),
    Text::Format("Ln {}, Col {}", cursor->line + 1, cursor->column + 1),
    compact || selected.empty() ? View{} : Text(std::move(selected)),
  }.With(Spacing(14.0F), Padding(EdgeInsets::Symmetric(12.0F, 5.0F)),
         Frame{.height = 29.0F}, FontSize(11.0F), Foreground(colors.on_surface_variant),
         CrossAlign(CrossAxisAlignment::Center), Background(colors.surface_container));
}

struct DemoSettings {
  bool dark = true;
  bool maple = false;
  bool ligatures = true;
  bool wrap = false;
  bool sticky = ce::EditorSettings{}.gutter_sticky;
  bool custom_completion_rows = false;

  bool operator==(const DemoSettings&) const = default;
};

struct EditHistory {
  bool can_undo = false;
  bool can_redo = false;

  bool operator==(const EditHistory&) const = default;
};

[[huxerui::composable]]
View DemoToolbar(
    DemoSearch search, State<DemoSettings> settings, State<EditHistory> history, std::size_t selected,
    std::function<void(std::size_t)> select
) {
  const auto menu = UseMenu();
  const auto tasks = UseTaskScope();
  const auto actions_anchor = UseViewHandle();
  const auto settings_anchor = UseViewHandle();
  const bool compact = UseViewportClass() == ViewportClass::Compact;
  const auto editor = search.editor;
  const auto undo = [editor] { editor.Undo(); };
  const auto redo = [editor] { editor.Redo(); };
  const auto show_actions = [menu, actions_anchor, search, editor, tasks, history, compact, undo, redo] {
    std::vector<MenuEntry> items;
    if (compact) {
      items.emplace_back(MenuItem("Undo", undo).Enabled(history->can_undo));
      items.emplace_back(MenuItem("Redo", redo).Enabled(history->can_redo));
      items.emplace_back(MenuSection{});
    }
    items.emplace_back(MenuItem("Find and replace", [search] { search.Open(SearchMode::Replace); }));
    items.emplace_back(MenuSection{});
    items.emplace_back(MenuItem("Show completions", [editor, tasks] {
      tasks.Post([editor] { editor.TriggerCompletion(); });
    }));
    items.emplace_back(MenuItem("Insert for-loop snippet", [editor] { editor.InsertSnippet(kForSnippet); }));
    items.emplace_back(MenuSection{});
    items.emplace_back(MenuItem("Fold all", [editor] { editor.FoldAll(); }));
    items.emplace_back(MenuItem("Unfold all", [editor] { editor.UnfoldAll(); }));
    menu.Show(actions_anchor, std::move(items), {.placement = {.alignment = AnchorAlignment::End}});
  };
  const auto show_settings = [menu, settings_anchor, settings] {
    const auto toggle = [settings](const char* label, bool DemoSettings::*field) {
      const DemoSettings& current = settings;
      return MenuItem(label, [settings, field] {
        settings.Update([field](DemoSettings& next) { next.*field = !(next.*field); });
      }).Checked(current.*field);
    };
    menu.Show(settings_anchor, {
      toggle("Dark theme", &DemoSettings::dark),
      MenuSection{},
      toggle("Maple Mono", &DemoSettings::maple),
      toggle("Font ligatures", &DemoSettings::ligatures).Enabled(settings->maple),
      MenuSection{},
      toggle("Word wrap", &DemoSettings::wrap),
      toggle("Sticky gutter", &DemoSettings::sticky),
      MenuSection{},
      toggle("Custom completion rows", &DemoSettings::custom_completion_rows),
    }, {.placement = {.alignment = AnchorAlignment::End}});
  };
  const auto find = [search] {
    search.Open(search.Mode() == SearchMode::Replace ? SearchMode::Replace : SearchMode::Find);
  };
  const auto separator = Divider(Axis::Vertical).With(Frame{.height = 18.0F});
  const auto& colors = UseTheme().colors;
  auto picker = Select(kDemoFiles, selected, [](const DemoFile& file) {
    return Text(file.label).With(FontSize(13.0F)).Key(file.label);
  }).Label("Sample file").OnChanged(std::move(select));
  return Row {
    compact ? std::move(picker).With(Grow()) : std::move(picker).With(Frame{.width = 176.0F}),
    compact ? View{} : Spacer().With(Grow()),
    compact ? View{} : Row {
      ToolIcon(kUndoIcon, "Undo (Ctrl+Z)", undo, history->can_undo),
      ToolIcon(kRedoIcon, "Redo (Ctrl+Y)", redo, history->can_redo),
      separator,
    }.With(Spacing(3.0F), CrossAlign(CrossAxisAlignment::Center)),
    ToolIcon(kFindIcon, "Find (Ctrl+F)", find).With(
        search.find_button,
        Background(search.Mode() == SearchMode::Closed ? Color::Transparent() : colors.primary_container),
        CornerRadius(5.0F)),
    separator,
    ToolIcon(kActionsIcon, "Editor actions", show_actions).With(actions_anchor),
    ToolIcon(kSettingsIcon, "Editor settings", show_settings).With(settings_anchor),
  }.With(Spacing(5.0F), Padding(EdgeInsets::Symmetric(12.0F, 8.0F)),
         CrossAlign(CrossAxisAlignment::Center), Background(colors.surface_container));
}

ce::EditorOptions MakeEditorOptions(
    const DemoDocument& document,
    const DemoSettings& settings,
    const FontFamily& font_family,
    State<std::vector<std::size_t>> breakpoints,
    std::map<std::string, std::shared_ptr<demo::SweetLineDecorationProvider>>& provider_cache,
    const std::shared_ptr<DemoCompletionProvider>& completion
) {
  ce::EditorOptions options;
  options.completion_providers.push_back(completion);
  auto provider = provider_cache.find(document.key);
  if (provider == provider_cache.end()) {
    auto created = std::make_shared<demo::SweetLineDecorationProvider>(
        document.syntax,
        document.key,
        [breakpoints](size_t start_line, size_t end_line) {
          std::vector<std::pair<size_t, int32_t>> icons;
          const std::vector<std::size_t>& lines = breakpoints;
          for (std::size_t line : lines) {
            if (line >= start_line && line <= end_line) {
              icons.emplace_back(line, demo::kBreakpointGutterIcon);
            }
          }
          return icons;
        }
    );
    provider = provider_cache.emplace(document.key, std::move(created)).first;
  }
  options.decoration_providers.push_back(provider->second);
  options.theme = settings.dark ? ce::EditorTheme::Dark() : ce::EditorTheme::Light();
  options.settings.font_family = font_family;
  options.settings.wrap_mode = settings.wrap ? ce::WrapMode::WORD_BREAK : ce::WrapMode::NONE;
  options.settings.gutter_sticky = settings.sticky;
  options.settings.current_line_render_mode = ce::CurrentLineRenderMode::BORDER;
  options.settings.max_gutter_icons = 1;
  return options;
}

FontFamily LoadMapleFamily(bool ligature) {
  return FontFamily::FromResources({
      ligature ? app::raw::fonts_MapleMono_ttf : app::raw::fonts_MapleMonoNL_ttf,
  });
}

[[huxerui::composable]]
View EditorDemo(State<DemoSettings> settings, DemoSearch search) {
  const auto current_file = UseState<std::size_t>(0);
  const auto current_document = UseState(std::optional<ce::Document>{});
  const auto breakpoints = UseState(std::vector<std::size_t>{});
  using ProvidersByDocument = std::map<std::string, std::shared_ptr<demo::SweetLineDecorationProvider>>;
  const std::shared_ptr<ProvidersByDocument> providers = UseState(std::make_shared<ProvidersByDocument>());
  const std::shared_ptr<DemoCompletionProvider> completion = UseState(std::make_shared<DemoCompletionProvider>());
  const auto documents = UseState(std::optional<DemoDocuments>{});
  const auto history = UseState(EditHistory{});
  const auto status = search.status;
  const auto cursor = UseState(ce::TextPosition{});
  const auto selection = UseState(std::optional<ce::TextRange>{});
  const auto document_revision = UseState<std::size_t>(0);
  const auto editor = search.editor;

  const auto resources = UseService<Resources>();
  const auto type_icon = UseImage(app::images::ic_gutter_down);
  const auto annotation_icon = UseImage(app::images::ic_gutter_at);
  if (!documents->has_value()) documents = LoadDemoDocuments(*resources);
  const DemoDocument& file = documents->value()[current_file];
  if (!current_document->has_value()) {
    current_document = ce::Document(file.text);
  }
  const FontFamily font_family = settings->maple ? LoadMapleFamily(settings->ligatures) : FontFamily::Monospace();
  auto options = MakeEditorOptions(file, settings, font_family, breakpoints, *providers, completion);
  options.document = current_document->value();
  const auto renderer = settings->custom_completion_rows ? DemoCompletionRows(*options.theme)
                                                         : ce::CompletionItemRenderer{};
  Lifecycle([editor, renderer] { editor.SetCompletionItemRenderer(renderer); },
            settings->custom_completion_rows, settings->dark);
  const bool search_open = search.Mode() != SearchMode::Closed;

  const auto select = [current_file, current_document, documents, status, editor](std::size_t index) {
    if (index == current_file) return;
    ce::Document document(documents->value()[index].text);
    editor.SetDiffOriginal(std::nullopt);
    current_document = std::move(document);
    current_file = index;
    status = "Opened " + std::string(kDemoFiles[index].label);
  };
  const auto text_changed = [editor, history, document_revision] {
    history = EditHistory{editor.CanUndo(), editor.CanRedo()};
    ++document_revision;
  };

  return Column {
    DemoToolbar(search, settings, history, current_file, select),
    Divider(),
    search_open ? Column {SearchPanel(search, document_revision), Divider()}
        .With(CrossAlign(CrossAxisAlignment::Stretch)) : View{},
    ce::CodeEditor(editor, options)
        .On<ce::EditorEvents::DocumentLoaded>([documents, current_file, editor, cursor, selection, text_changed,
                                              type_icon, annotation_icon] {
          editor.SetEditorIconProvider([type_icon, annotation_icon](std::int32_t icon) -> std::optional<ImageAsset> {
            if (icon == demo::kTypeGutterIcon) return type_icon;
            if (icon == demo::kAnnotationGutterIcon) return annotation_icon;
            return std::nullopt;
          });
          editor.SetDiffOriginal(documents->value()[current_file].diff_original);
          cursor = editor.GetCursorPosition().value_or(ce::TextPosition{});
          selection = editor.GetSelection();
          text_changed();
        })
        .On<ce::EditorEvents::TextChanged>([text_changed](const ce::TextChangedEvent&) { text_changed(); })
        .On<ce::EditorEvents::LinkClicked>([status](const ce::LinkClickEvent& event) {
          status = "Link: " + event.target;
        })
        .On<ce::EditorEvents::CodeLensClicked>([status](const ce::CodeLensClickEvent& event) {
          status = "CodeLens command at line " + std::to_string(event.line + 1);
        })
        .On<ce::EditorEvents::GutterIconClicked>([status, breakpoints, editor](const ce::GutterIconClickEvent& event) {
          std::vector<std::size_t> lines = breakpoints;
          const auto found = std::find(lines.begin(), lines.end(), event.line);
          if (found == lines.end()) {
            lines.push_back(event.line);
            status = "Breakpoint set at line " + std::to_string(event.line + 1);
          } else {
            lines.erase(found);
            status = "Breakpoint removed at line " + std::to_string(event.line + 1);
          }
          breakpoints = std::move(lines);
          editor.RequestDecorationRefresh();
        })
        .On<ce::EditorEvents::InlayHintClicked>([status](const ce::InlayHintClickEvent& event) {
          status = "Inlay hint at " + std::to_string(event.line + 1) + ":" + std::to_string(event.column + 1);
        })
        .On<ce::EditorEvents::CursorChanged>([cursor](const ce::CursorChangedEvent& event) {
          cursor = event.cursor_position;
        })
        .On<ce::EditorEvents::SelectionChanged>([selection](const ce::SelectionChangedEvent& event) {
          selection = event.selection;
        })
        .With(Grow()).Key("preview-editor"),
    Divider(),
    EditorStatusBar(kDemoFiles[current_file].language, status, cursor, selection),
  }.With(CrossAlign(CrossAxisAlignment::Stretch));
}

View App() {
  const auto settings = UseState(DemoSettings{});
  const DemoSearch search{
      ce::UseCodeEditor(), UseViewHandle(), UseViewHandle(), UseState(SearchMode::Closed), UseState(TextEditingValue{}),
      UseState(TextEditingValue{}), UseState(SearchFilters{}), UseState(std::optional<ce::SearchState>{}),
      UseState(std::string("Ready")),
  };
  return Stack {
    Theme(PreviewTheme(settings->dark), EditorDemo(settings, search)),
  }.On<ViewEvents::KeyIntercept>([search](const KeyEvent& event) {
    if (event.type != KeyEventType::Down) return false;
    const auto& keys = event.modifiers;
    if ((keys.control || keys.meta) && !keys.shift && !keys.alt && (event.key == Key::F || event.key == Key::H)) {
      search.Open(event.key == Key::H ? SearchMode::Replace : SearchMode::Find);
      return true;
    }
    if (event.key == Key::Escape && search.Mode() != SearchMode::Closed &&
        !search.query->composition && !search.replacement->composition) {
      search.Close();
      return true;
    }
    return false;
  });
}

const Application application{
    App,
    {
        .window = {.title = "CodeEditor Preview", .initial_size = {1080.0F, 720.0F}, .minimum_size = Size{640.0F, 420.0F}},
        .show_debug_overlay = false,
    },
};

}  // namespace
