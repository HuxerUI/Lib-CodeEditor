#include <huxerui/codeeditor.h>


#include "decoration_pipeline.h"
#include "text_input_bridge.h"

#include <huxerui/app.h>
#include <huxerui/clipboard.h>
#include <huxerui/environment.h>
#include <huxerui/indication.h>
#include <huxerui/presentation.h>
#include <huxerui/layout.h>
#include <huxerui/modifier.h>
#include <huxerui/paint.h>
#include <huxerui/scroll.h>
#include <huxerui/semantics.h>
#include <huxerui/state.h>
#include <huxerui/text.h>
#include <huxerui/text_input.h>

#include <sweeteditor/decoration.h>
#include <sweeteditor/document.h>
#include <sweeteditor/editor_core.h>
#include <sweeteditor/utility.h>
#include <sweeteditor/visual.h>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace huxerui::codeeditor {
namespace detail {

struct DocumentAccess {
  static const std::shared_ptr<sweeteditor::Document>& Core(const Document& document) { return document.core_; }
};

}  // namespace detail

namespace {

namespace se = sweeteditor;
using namespace huxerui;

#if defined(_WIN32) || (defined(__APPLE__) && TARGET_OS_OSX) || (defined(__linux__) && !defined(__ANDROID__))
constexpr bool kSelectionHandlesEnabled = false;
#else
constexpr bool kSelectionHandlesEnabled = true;
#endif

constexpr float kSelectionMenuHeight = 36.0F;
constexpr float kSelectionMenuHandleClearance = 32.0F;

std::string Utf16ToUtf8(std::u16string_view input) {
  std::string out;
  out.reserve(input.size());
  for (std::size_t i = 0; i < input.size(); ++i) {
    char32_t cp = input[i];
    if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < input.size() && input[i + 1] >= 0xDC00 &&
        input[i + 1] <= 0xDFFF) {
      cp = 0x10000 + ((cp - 0xD800) << 10) + (input[i + 1] - 0xDC00);
      ++i;
    }
    if (cp < 0x80) {
      out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
      out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
      out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
  }
  return out;
}

// SweetEditor's UTF-16 string is std::basic_string<wchar_t> on Windows (where
// wchar_t is a 16-bit UTF-16 code unit) and std::u16string elsewhere; view it
// as UTF-16 code units for the UTF-8 conversion.
std::string Utf16ToUtf8(const se::U16String& input) {
  static_assert(sizeof(se::U16Char) == 2, "SweetEditor UTF-16 code units must be 16-bit");
  return Utf16ToUtf8(std::u16string_view(reinterpret_cast<const char16_t*>(input.data()), input.size()));
}








// SweetEditor style objects carry 0xAARRGGBB integers.
int32_t ToArgb(const Color& color) {
  const auto channel = [](float value) {
    const int scaled = static_cast<int>(value * 255.0F + 0.5F);
    return scaled < 0 ? 0 : (scaled > 255 ? 255 : scaled);
  };
  return (channel(color.alpha) << 24) | (channel(color.red) << 16) | (channel(color.green) << 8) | channel(color.blue);
}

se::TextStyle ToCoreStyle(const EditorTextStyle& style) {
  return {ToArgb(style.foreground), ToArgb(style.background), style.font_style};
}

std::vector<std::pair<std::uint32_t, se::TextStyle>> CoreTextStyles(const EditorTheme& theme) {
  std::vector<std::pair<std::uint32_t, se::TextStyle>> styles;
  styles.reserve(theme.text_styles.size());
  for (const auto& [id, style] : theme.text_styles) {
    styles.emplace_back(id, ToCoreStyle(style));
  }
  return styles;
}

Color Argb(int32_t argb) {
  const int a = (argb >> 24) & 0xFF;
  const int r = (argb >> 16) & 0xFF;
  const int g = (argb >> 8) & 0xFF;
  const int b = argb & 0xFF;
  return Color::Rgb(r, g, b, static_cast<float>(a) / 255.0F);
}

TextStyle MakeTextStyle(int32_t font_style, int32_t argb_color, Font font) {
  if ((font_style & se::FONT_STYLE_BOLD) != 0) {
    font = font.WithWeight(FontWeight::Bold);
  }
  if ((font_style & se::FONT_STYLE_ITALIC) != 0) {
    font = font.WithSlant(FontSlant::Italic);
  }
  TextDecoration decoration = TextDecoration::None;
  if ((font_style & se::FONT_STYLE_STRIKETHROUGH) != 0) {
    decoration = TextDecoration::StrikeThrough;
  }
  return TextStyle{font, Argb(argb_color), decoration};
}

}  // namespace

namespace detail {

class HuxeruiTextMeasurer final : public se::TextMeasurer {
public:
  explicit HuxeruiTextMeasurer(huxerui::TextMeasurer& measurer, float font_size, FontFamily font_family)
      : measurer_(&measurer), font_size_(font_size), font_family_(std::move(font_family)) {}

  [[nodiscard]] huxerui::Font ContentFont() const {
    return Font::FromFamily(font_family_, font_size_);
  }

  [[nodiscard]] huxerui::Font InlayHintFont() const {
    return Font::System(font_size_ * 0.86F);
  }

  void SetFont(FontFamily font_family, float font_size) {
    font_family_ = std::move(font_family);
    font_size_ = font_size;
  }

  float measureWidth(const se::U16String& text, int32_t font_style) override {
    if (!measurer_) {
      return 0.0F;
    }
    return measurer_->MeasureRun(Utf16ToUtf8(text), MakeTextStyle(font_style, 0, ContentFont())).advance;
  }

  float measureInlayHintWidth(const se::U16String& text) override {
    if (!measurer_) {
      return 0.0F;
    }
    return measurer_
        ->MeasureRun(
            Utf16ToUtf8(text), huxerui::TextStyle{InlayHintFont(), huxerui::Color::Black(), huxerui::TextDecoration::None}
        )
        .advance;
  }

  float measureIconWidth(int32_t) override { return font_size_; }

  se::FontMetrics getFontMetrics() override {
    if (!measurer_) {
      return {0.0F, 0.0F};
    }
    const huxerui::FontMetrics metrics = measurer_->Metrics(ContentFont());
    return {-metrics.ascent, metrics.descent};
  }

private:
  FontFamily font_family_;
  huxerui::TextMeasurer* measurer_;
  float font_size_;
};

}  // namespace detail

namespace {

se::KeyModifier ToModifiers(const KeyModifiers& modifiers) {
  int value = 0;
  if (modifiers.shift) {
    value |= static_cast<int>(se::KeyModifier::SHIFT);
  }
  if (modifiers.control) {
    value |= static_cast<int>(se::KeyModifier::CTRL);
  }
  if (modifiers.alt) {
    value |= static_cast<int>(se::KeyModifier::ALT);
  }
  if (modifiers.meta) {
    value |= static_cast<int>(se::KeyModifier::META);
  }
  return static_cast<se::KeyModifier>(value);
}

se::KeyCode ToKeyCode(Key key) {
  if (key >= Key::A && key <= Key::Z) {
    return static_cast<se::KeyCode>(static_cast<int>(se::KeyCode::A) + (static_cast<int>(key) - static_cast<int>(Key::A)));
  }
  switch (key) {
  case Key::Backspace:
    return se::KeyCode::BACKSPACE;
  case Key::Tab:
    return se::KeyCode::TAB;
  case Key::Enter:
    return se::KeyCode::ENTER;
  case Key::Escape:
    return se::KeyCode::ESCAPE;
  case Key::Space:
    return se::KeyCode::SPACE;
  case Key::Delete:
    return se::KeyCode::DELETE_KEY;
  case Key::ArrowLeft:
    return se::KeyCode::LEFT;
  case Key::ArrowRight:
    return se::KeyCode::RIGHT;
  case Key::ArrowUp:
    return se::KeyCode::UP;
  case Key::ArrowDown:
    return se::KeyCode::DOWN;
  case Key::Home:
    return se::KeyCode::HOME;
  case Key::End:
    return se::KeyCode::END;
  case Key::PageUp:
    return se::KeyCode::PAGE_UP;
  case Key::PageDown:
    return se::KeyCode::PAGE_DOWN;
  default:
    return se::KeyCode::NONE;
  }
}

se::KeyEvent ToKeyEvent(const KeyEvent& event) {
  return {ToKeyCode(event.key), event.text, ToModifiers(event.modifiers)};
}

se::GestureEvent ToGestureEvent(const PointerEvent& event) {
  se::GestureEvent out;
  out.points.push_back(se::PointF{event.position.x, event.position.y});
  out.modifiers = ToModifiers(event.modifiers);
  const bool mouse = event.device_kind == PointerDeviceKind::Mouse || event.device_kind == PointerDeviceKind::Pen;
  switch (event.type) {
  case PointerEventType::Down:
    out.type = mouse ? se::EventType::MOUSE_DOWN : se::EventType::TOUCH_DOWN;
    break;
  case PointerEventType::Move:
    out.type = mouse ? se::EventType::MOUSE_MOVE : se::EventType::TOUCH_MOVE;
    break;
  case PointerEventType::Up:
    out.type = mouse ? se::EventType::MOUSE_UP : se::EventType::TOUCH_UP;
    break;
  case PointerEventType::Cancel:
    out.type = se::EventType::TOUCH_CANCEL;
    break;
  }
  return out;
}

se::GestureEvent ToWheelGestureEvent(const ScrollInputEvent& event) {
  se::GestureEvent out;
  out.type = se::EventType::MOUSE_WHEEL;
  out.wheel_delta_x = -event.delta_x;
  out.wheel_delta_y = -event.delta_y;
  return out;
}


const std::vector<std::pair<char32_t, char32_t>> kDefaultAutoClosingPairs = {
    {U'(', U')'},
    {U'{', U'}'},
    {U'[', U']'},
};

// Completion panel dimensions follow SweetEditor's Android layout.
constexpr float kCompletionPanelWidth = 300.0F;
constexpr float kCompletionRowHeight = 32.0F;
constexpr float kCompletionPanelPaddingH = 4.0F;
constexpr float kCompletionPanelPaddingV = 6.0F;
constexpr float kCompletionRowPaddingH = 8.0F;
constexpr float kCompletionBadgeSize = 18.0F;
constexpr float kCompletionColumnGap = 8.0F;
constexpr float kCompletionGap = 4.0F;
constexpr float kCompletionFramePadding = 8.0F;
constexpr float kCompletionScrollbarSpace = 10.0F;
constexpr std::size_t kCompletionMaxVisible = 4;
constexpr float kCompletionLabelSize = 13.0F;
constexpr float kCompletionDetailSize = 11.0F;
constexpr float kCompletionBadgeFontSize = 10.0F;

int32_t CompletionKindColor(CompletionItemKind kind) {
  switch (kind) {
  case CompletionItemKind::Keyword:
    return 0xFFC678DD;
  case CompletionItemKind::Function:
    return 0xFF61AFEF;
  case CompletionItemKind::Variable:
    return 0xFFE5C07B;
  case CompletionItemKind::Class:
    return 0xFFE06C75;
  case CompletionItemKind::Interface:
    return 0xFF56B6C2;
  case CompletionItemKind::Module:
    return 0xFFD19A66;
  case CompletionItemKind::Property:
    return 0xFF98C379;
  case CompletionItemKind::Snippet:
    return 0xFFBE5046;
  case CompletionItemKind::Text:
  default:
    return 0xFF7A8494;
  }
}

const char* CompletionKindLetter(CompletionItemKind kind) {
  switch (kind) {
  case CompletionItemKind::Keyword:
    return "K";
  case CompletionItemKind::Function:
    return "F";
  case CompletionItemKind::Variable:
    return "V";
  case CompletionItemKind::Class:
    return "C";
  case CompletionItemKind::Interface:
    return "I";
  case CompletionItemKind::Module:
    return "M";
  case CompletionItemKind::Property:
    return "P";
  case CompletionItemKind::Snippet:
    return "S";
  case CompletionItemKind::Text:
  default:
    return "T";
  }
}

struct CompletionPopupColors {
  Color background;
  Color border;
  Color selected;
  Color label;
  Color detail;
  Color scrollbar_track;
  Color scrollbar_thumb;
};

struct CompletionText {
  std::string text;
  float width = 0.0F;
};

CompletionText FitCompletionText(TextMeasurer& measurer, std::string_view text, const TextStyle& style, float limit) {
  text = text.substr(0, text.find_first_of("\r\n"));
  if (text.empty() || limit <= 0.0F) return {};
  const auto measure = [&](std::string_view value) { return std::ceil(measurer.MeasureRun(value, style).advance); };
  const float width = measure(text);
  if (width <= limit) return {std::string(text), width};

  constexpr std::string_view ellipsis = "\xE2\x80\xA6";
  const float ellipsis_width = measure(ellipsis);
  if (ellipsis_width > limit) return {};
  CompletionText fitted{std::string(ellipsis), ellipsis_width};

  se::U16String source;
  se::StrUtil::convertUTF8ToUTF16(std::string(text), source);
  std::vector<std::size_t> boundaries;
  for (std::size_t column = 0; column < source.size();) {
    column = se::UnicodeUtil::nextGraphemeBoundaryColumn(source, column);
    boundaries.push_back(column);
  }
  std::size_t first = 0;
  std::size_t last = boundaries.size();
  while (first < last) {
    const std::size_t middle = first + (last - first) / 2;
    auto candidate = Utf16ToUtf8(source.substr(0, boundaries[middle]));
    candidate += ellipsis;
    const float candidate_width = measure(candidate);
    if (candidate_width <= limit) {
      fitted = {std::move(candidate), candidate_width};
      first = middle + 1;
    } else {
      last = middle;
    }
  }
  return fitted;
}

View DefaultCompletionRow(const CompletionItem& item, const CompletionPopupColors& colors,
                          float row_width, TextMeasurer& measurer) {
  constexpr float minimum_label_width = 36.0F;
  const float content_width = row_width - 2.0F * kCompletionRowPaddingH;
  const float badge_extent = kCompletionBadgeSize + kCompletionColumnGap;
  const bool show_badge = content_width >= badge_extent + kCompletionLabelSize;
  const float text_width = content_width - (show_badge ? badge_extent : 0.0F);
  const TextStyle detail_style{Font::System(kCompletionDetailSize), colors.detail};
  auto detail = FitCompletionText(measurer, item.detail, detail_style,
      std::min(text_width * 0.45F, text_width - minimum_label_width - kCompletionColumnGap));
  const float label_width = text_width - (detail.text.empty() ? 0.0F : detail.width + kCompletionColumnGap);
  const TextStyle label_style{Font::Monospace(kCompletionLabelSize), colors.label};
  auto label = FitCompletionText(measurer, item.label, label_style, label_width);
  return Row {
    show_badge ? Text(CompletionKindLetter(item.kind))
        .Style({Font::System(kCompletionBadgeFontSize).WithWeight(FontWeight::Bold), Color::White()})
        .Align(TextAlign::Center)
        .VerticalAlign(TextVerticalAlign::Center)
        .With(Frame{.width = kCompletionBadgeSize, .height = kCompletionBadgeSize},
              Background(Argb(CompletionKindColor(item.kind))), CornerRadius(4.0F)) : View{},
    Text(std::move(label.text)).Style(label_style).With(Grow()),
    detail.text.empty() ? View{} : Text(std::move(detail.text)).Style(detail_style).Align(TextAlign::Trailing)
        .With(Frame{.width = detail.width}),
  }.With(Padding(EdgeInsets::Symmetric(kCompletionRowPaddingH, 0.0F)),
         Spacing(kCompletionColumnGap), CrossAlign(CrossAxisAlignment::Center));
}

struct CompletionPopupContent {
  std::shared_ptr<const std::vector<CompletionItem>> items;
  std::size_t selected;
  CompletionItemRenderer renderer;
  CompletionPopupColors colors;
  Size size;
  std::function<void(std::size_t)> accept;
};

[[huxerui::composable]]
View CompletionPopupView(CompletionPopupContent content) {
  const auto scroll = UseScrollController();
  auto* measurer = &UseTextMeasurer();
  const float list_height = content.size.height - 2.0F * kCompletionPanelPaddingV;
  const float scrollbar_space = static_cast<float>(content.items->size()) * kCompletionRowHeight > list_height
      ? kCompletionScrollbarSpace : 0.0F;
  const float row_width = content.size.width - 2.0F * kCompletionPanelPaddingH - scrollbar_space;

  Lifecycle([scroll, selected = content.selected, list_height] {
    const auto metrics = scroll.Metrics();
    const float top = static_cast<float>(selected) * kCompletionRowHeight;
    const float visible_bottom = metrics.offset + std::min(metrics.viewport_extent, list_height);
    if (top < metrics.offset) {
      scroll.ScrollToItem(selected, ScrollAlignment::Start);
    } else if (top + kCompletionRowHeight > visible_bottom) {
      scroll.ScrollToItem(selected, ScrollAlignment::End);
    }
  }, content.items, content.selected, list_height);

  return Column {
    VirtualList(content.items->size(), [content, measurer, row_width, scrollbar_space](std::size_t index) {
      const auto& item = (*content.items)[index];
      const bool selected = index == content.selected;
      View row = (content.renderer ? content.renderer(item, selected)
                                   : DefaultCompletionRow(item, content.colors, row_width, *measurer)).With(Grow());
      return Row {
        row,
      }.OnClick([accept = content.accept, index] { accept(index); })
       .With(Frame{.width = row_width + scrollbar_space, .height = kCompletionRowHeight},
             Padding(EdgeInsets{.right = scrollbar_space}),
             CrossAlign(CrossAxisAlignment::Center), Focusable(false),
             Background(selected ? content.colors.selected : Color::Transparent()), CornerRadius(6.0F),
             Semantics{.role = SemanticRole::ListItem, .label = item.label, .value = item.detail,
                       .selected = selected, .descendants = SemanticDescendantPolicy::Exclude})
       .Key(index);
    }).ItemExtent(kCompletionRowHeight)
      .Controller(scroll)
      .With(Grow(), ScrollBar{ScrollBarStyle{.thickness = 4.0F, .minimum_thumb_extent = 12.0F,
                                            .margin = 2.0F, .corner_radius = 2.0F,
                                            .track_color = content.colors.scrollbar_track,
                                            .thumb_color = content.colors.scrollbar_thumb}},
            Semantics{.role = SemanticRole::List}),
  }.With(Frame{.width = content.size.width, .height = content.size.height},
         Padding(EdgeInsets::Symmetric(kCompletionPanelPaddingH, kCompletionPanelPaddingV)),
         CrossAlign(CrossAxisAlignment::Stretch), Background(content.colors.background),
         Border{content.colors.border}, CornerRadius(12.0F), ClipChildren(),
         Shadow{.color = Color::Rgb(0, 0, 0, 0.26F), .offset = {0.0F, 4.0F}, .blur_radius = 10.0F});
}

// Converts "(){}[]" style pair strings into the core's bracket pair list.
std::vector<se::BracketPair> MakeBracketPairs(const std::vector<std::pair<char32_t, char32_t>>& pairs) {
  std::vector<se::BracketPair> result;
  result.reserve(pairs.size());
  for (const auto& [open, close] : pairs) {
    result.push_back({open, close});
  }
  return result;
}

se::EditorRenderColors MakeRenderColors(const EditorTheme& theme) {
  se::EditorRenderColors colors;
  colors.text_foreground = ToArgb(theme.text_color);
  colors.link_foreground = ToArgb(theme.link_color);
  colors.active_link_foreground = ToArgb(theme.link_active_color);
  colors.codelens_foreground = ToArgb(theme.code_lens_color);
  colors.active_codelens_foreground = ToArgb(theme.code_lens_active_color);
  colors.diff_added_line_background = ToArgb(theme.diff_added_background);
  colors.diff_removed_line_background = ToArgb(theme.diff_removed_background);
  colors.diff_added_gutter_background = ToArgb(theme.diff_added_gutter);
  colors.diff_removed_gutter_background = ToArgb(theme.diff_removed_gutter);
  return colors;
}

se::EditorRangeEffectStyles MakeRangeEffectStyles(const EditorTheme& theme) {
  se::EditorRangeEffectStyles styles;
  styles.selection.foreground_color = ToArgb(theme.selection_text_color);
  styles.selection.background_color = ToArgb(theme.selection_color);
  styles.search_match.background_color = ToArgb(theme.search_match_bg_color);
  styles.search_current.border_color = ToArgb(theme.search_current_border_color);
  styles.search_current.background_color = ToArgb(theme.search_current_bg_color);
  styles.bracket_match.border_color = ToArgb(theme.bracket_highlight_border_color);
  styles.linked_editing_active.border_color = ToArgb(theme.linked_editing_active_color);
  styles.linked_editing_inactive.border_color = ToArgb(theme.linked_editing_inactive_color);
  styles.bracket_match.background_color = ToArgb(theme.bracket_highlight_bg_color);
  styles.ime_composition.underline_color = ToArgb(theme.composition_underline_color);
  styles.ime_composition.underline_style = se::RangeEffectUnderlineStyle::SOLID;

  styles.document_highlight_text.background_color = ToArgb(theme.document_highlight_text_bg_color);
  styles.document_highlight_read.background_color = ToArgb(theme.document_highlight_read_bg_color);
  styles.document_highlight_write.background_color = ToArgb(theme.document_highlight_write_bg_color);
  styles.diagnostic_error.underline_color = ToArgb(theme.diagnostic_error_color);
  styles.diagnostic_error.underline_style = se::RangeEffectUnderlineStyle::WAVY;
  styles.diagnostic_warning.underline_color = ToArgb(theme.diagnostic_warning_color);
  styles.diagnostic_warning.underline_style = se::RangeEffectUnderlineStyle::WAVY;
  styles.diagnostic_info.underline_color = ToArgb(theme.diagnostic_info_color);
  styles.diagnostic_info.underline_style = se::RangeEffectUnderlineStyle::WAVY;
  styles.diagnostic_hint.underline_color = ToArgb(theme.diagnostic_hint_color);
  styles.diagnostic_hint.underline_style = se::RangeEffectUnderlineStyle::DASHED;
  return styles;
}


}  // namespace

namespace detail {

namespace {

Document ResolveDocument(const EditorOptions& options) {
  return std::visit([](const auto& value) { return Document(value); }, options.document);
}

}  // namespace

class EditorSession : public std::enable_shared_from_this<EditorSession> {
  enum class SelectionMenuPhase { Hidden, Pending, Visible, Suspended };

 public:
  EditorSession(TextMeasurer& measurer, const EditorOptions& options, TaskScope tasks, MenuHandle menu,
               PopupHandle popup, ViewHandle editor_anchor,
               std::shared_ptr<Clipboard> clipboard, EventEmitter events, EditorController controller,
               std::function<void()> invalidate, std::function<void()> invalidate_semantics,
               std::function<void(PointerCursorKind)> set_cursor, std::function<void()> show_keyboard,
               std::function<Point(Point)> local_to_window, std::function<std::optional<Point>(Point)> window_to_local)
      : text_measurer_(std::make_shared<HuxeruiTextMeasurer>(
            measurer, options.settings.editor_text_size, options.settings.font_family)),
        core_(std::make_shared<se::EditorCore>(text_measurer_, options.core_options)),
        document_handle_(ResolveDocument(options)),
        document_(detail::DocumentAccess::Core(document_handle_)),
        measurer_(measurer), tasks_(std::move(tasks)), menu_(std::move(menu)),
        popup_(std::move(popup)), editor_anchor_(std::move(editor_anchor)),
        clipboard_(std::move(clipboard)), events_(std::move(events)), controller_(std::move(controller)),
        invalidate_(std::move(invalidate)), invalidate_semantics_(std::move(invalidate_semantics)),
        set_cursor_(std::move(set_cursor)), show_keyboard_(std::move(show_keyboard)),
        local_to_window_(std::move(local_to_window)), window_to_local_(std::move(window_to_local)) {}

  void Initialize(const EditorOptions& options) {
    text_input_ = CreateTextInput();
    Dispatch(core_->loadDocument(document_));
    ApplySettings(options.settings, true);
    ApplyTheme(*options.theme);
    SetKeyMap(options.key_map ? *options.key_map : EditorKeyMap::DefaultKeyMap());
    Dispatch(core_->setAutoClosingPairs(MakeBracketPairs(kDefaultAutoClosingPairs)));
    SyncOptions(options, nullptr);
    document_event_pending_ = true;
  }

  void SyncOptions(const EditorOptions& options, const EditorOptions* previous) {
    if (previous) {
      if (options.settings != previous->settings) ApplySettings(options.settings);
      if (options.theme != previous->theme) ApplyTheme(*options.theme);
      if (options.key_map != previous->key_map) {
        SetKeyMap(options.key_map ? *options.key_map : EditorKeyMap::DefaultKeyMap());
      }
    }
    if (!previous || options.language_configuration != previous->language_configuration) {
      SetLanguage(options.language_configuration
          ? std::optional<LanguageConfiguration>(*options.language_configuration) : std::nullopt);
    }
    if (!previous || options.metadata != previous->metadata) {
      SetMetadata(options.metadata ? *options.metadata : EditorMetadata{});
    }
    if (!previous || options.decoration_providers != previous->decoration_providers) {
      if (previous) {
        for (const auto& provider : previous->decoration_providers) {
          if (std::ranges::find(options.decoration_providers, provider) == options.decoration_providers.end()) {
            RemoveDecorationProvider(provider);
          }
        }
      }
      for (const auto& provider : options.decoration_providers) AddDecorationProvider(provider);
    }
    if (!previous || options.completion_providers != previous->completion_providers) {
      if (previous) {
        for (const auto& provider : previous->completion_providers) {
          if (std::ranges::find(options.completion_providers, provider) == options.completion_providers.end()) {
            RemoveCompletionProvider(provider);
          }
        }
      }
      for (const auto& provider : options.completion_providers) AddCompletionProvider(provider);
    }
    if (!previous || options.newline_action_providers != previous->newline_action_providers) {
      if (previous) {
        for (const auto& provider : previous->newline_action_providers) {
          if (std::ranges::find(options.newline_action_providers, provider) == options.newline_action_providers.end()) {
            RemoveProvider(newline_providers_, provider);
          }
        }
      }
      for (const auto& provider : options.newline_action_providers) AddProvider(newline_providers_, provider);
    }
    if (previous && options.document != previous->document) {
      LoadDocument(ResolveDocument(options));
    }
  }

  void Activate() {
    if (active_) return;
    active_ = true;
    PublishPendingEvents();
  }

  void Close() {
    active_ = false;
    text_input_->Detach();
    CancelDecorations();
    DismissCompletion();
    HideMenus();
    invalidate_ = {};
    invalidate_semantics_ = {};
    set_cursor_ = {};
    show_keyboard_ = {};
    local_to_window_ = {};
    window_to_local_ = {};
  }

  se::EditorCore& Core() { return *core_; }
  const Document& CurrentDocument() const { return document_handle_; }
  const EditorSettings& Settings() const { return settings_; }
  const EditorTheme& Theme() const { return theme_; }
  const EditorKeyMap& KeyMap() const { return key_map_; }
  const std::optional<LanguageConfiguration>& Language() const { return language_; }
  const EditorMetadata& Metadata() const { return metadata_; }
  std::shared_ptr<detail::TextInputBridge> TextInput() const { return text_input_; }

  bool LoadDocument(Document document) {
    if (!detail::DocumentAccess::Core(document)) return false;
    text_input_->Detach();
    DismissCompletion();
    DismissInlineSuggestion();
    HideMenus();
    CancelDecorations();
    ++document_generation_;
    previous_requested_revision_ = edit_generation_;
    pending_changes_.clear();
    snapshot_.reset();
    document_handle_ = std::move(document);
    document_ = detail::DocumentAccess::Core(document_handle_);
    // A new client gives pending platform callbacks a stale session ID.
    text_input_ = CreateTextInput();
    Dispatch(core_->loadDocument(document_));
    if (invalidate_semantics_) invalidate_semantics_();
    if (diff_original_) Dispatch(core_->computeDiff(*diff_original_));
    RequestDecorations();
    document_event_pending_ = true;
    if (active_ && !defer_events_) PublishPendingEvents();
    return true;
  }

  bool ApplyTheme(EditorTheme theme) {
    if (!std::isfinite(theme.caret_width) || theme.caret_width <= 0.0F) {
      throw std::invalid_argument("CodeEditor caret width must be positive and finite");
    }
    theme_ = std::move(theme);
    text_input_->SetCaretWidth(theme_.caret_width);
    Dispatch(core_->setEditorRenderColors(MakeRenderColors(theme_)));
    Dispatch(core_->setEditorRangeEffectStyles(MakeRangeEffectStyles(theme_)));
    Dispatch(core_->registerBatchTextStyles(CoreTextStyles(theme_)));
    if (completion_.items) InvalidateCompletionPopup();
    return true;
  }

  bool ApplySettings(const EditorSettings& next, bool initial = false) {
    ValidateSettings(next);
    const auto previous = settings_;
    settings_ = next;
    settings_.scale = core_->getViewState().scale;
    if (initial || next.editor_text_size != previous.editor_text_size || next.font_family != previous.font_family ||
        next.line_number_margin != previous.line_number_margin) {
      UpdateFontMetrics();
    }
    if (initial || next.scale != previous.scale) {
      Dispatch(core_->setScale(next.scale));
      settings_.scale = core_->getViewState().scale;
    }
    if (initial || next.wrap_mode != previous.wrap_mode) Dispatch(core_->setWrapMode(next.wrap_mode));
    if (initial || next.fold_arrow_mode != previous.fold_arrow_mode) Dispatch(core_->setFoldArrowMode(next.fold_arrow_mode));
    if (initial || next.line_spacing_add != previous.line_spacing_add || next.line_spacing_mult != previous.line_spacing_mult) {
      Dispatch(core_->setLineSpacing(next.line_spacing_add, next.line_spacing_mult));
    }
    if (initial || next.content_start_padding != previous.content_start_padding) {
      Dispatch(core_->setContentStartPadding(next.content_start_padding));
    }
    if (initial || next.show_split_line != previous.show_split_line) Dispatch(core_->setShowSplitLine(next.show_split_line));
    if (initial || next.gutter_sticky != previous.gutter_sticky) Dispatch(core_->setGutterSticky(next.gutter_sticky));
    if (initial || next.gutter_visible != previous.gutter_visible) Dispatch(core_->setGutterVisible(next.gutter_visible));
    if (initial || next.current_line_render_mode != previous.current_line_render_mode) {
      Dispatch(core_->setCurrentLineRenderMode(next.current_line_render_mode));
    }
    if (initial || next.render_whitespace != previous.render_whitespace) Dispatch(core_->setRenderWhitespace(next.render_whitespace));
    if (initial || next.render_line_breaks != previous.render_line_breaks) Dispatch(core_->setRenderLineBreaks(next.render_line_breaks));
    if (initial || next.auto_indent_mode != previous.auto_indent_mode) Dispatch(core_->setAutoIndentMode(next.auto_indent_mode));
    if (initial || next.backspace_unindent != previous.backspace_unindent) Dispatch(core_->setBackspaceUnindent(next.backspace_unindent));
    if (initial || next.read_only != previous.read_only) {
      Dispatch(core_->setReadOnly(next.read_only));
      if (next.read_only) {
        DismissCompletion();
        DismissInlineSuggestion();
      }
      if (invalidate_semantics_) invalidate_semantics_();
    }
    if (initial || next.tab_size != previous.tab_size || next.insert_spaces != previous.insert_spaces) ApplyIndentSettings();
    if (initial || next.max_gutter_icons != previous.max_gutter_icons) Dispatch(core_->setMaxGutterIcons(next.max_gutter_icons));
    if (initial || (kSelectionHandlesEnabled && next.handles != previous.handles)) {
      // The core includes both edges in hit tests, so invert the horizontal bounds for an empty hit area.
      constexpr se::HandleHitArea empty_hit_area{.left = 1.0F};
      constexpr HandleConfig disabled_handles{.start_hit_area = empty_hit_area, .end_hit_area = empty_hit_area};
      Dispatch(core_->setHandleConfig(kSelectionHandlesEnabled ? next.handles : disabled_handles));
    }
    if (initial || next.scrollbar != previous.scrollbar) Dispatch(core_->setScrollbarConfig(next.scrollbar));
    if (initial || next.decoration_scroll_refresh_min_interval_ms != previous.decoration_scroll_refresh_min_interval_ms ||
        next.decoration_overscan_viewport_multiplier != previous.decoration_overscan_viewport_multiplier) {
      RequestDecorations();
    }
    return true;
  }

  bool SetKeyMap(EditorKeyMap key_map) {
    key_map_ = std::move(key_map);
    se::KeyMap core_key_map;
    for (const auto& binding : key_map_.Bindings()) core_key_map.addBinding(binding);
    Dispatch(core_->setKeyMap(std::move(core_key_map)));
    return true;
  }

  bool SetIconProvider(EditorIconProvider provider) {
    icon_provider_ = std::move(provider);
    Invalidate();
    return true;
  }

  bool SetLanguage(std::optional<LanguageConfiguration> language) {
    if (language && language->tab_size == 0) throw std::invalid_argument("Language tab size must be positive");
    language_ = std::move(language);
    if (language_) {
      if (language_->brackets) Dispatch(core_->setBracketPairs(std::vector(*language_->brackets)));
      if (language_->auto_closing_pairs) Dispatch(core_->setAutoClosingPairs(std::vector(*language_->auto_closing_pairs)));
    }
    ApplyIndentSettings();
    DismissCompletion();
    return RequestDecorations();
  }

  bool SetMetadata(EditorMetadata metadata) {
    metadata_ = std::move(metadata);
    DismissCompletion();
    return RequestDecorations();
  }

  bool SetDiffOriginal(std::optional<std::string> original) {
    if (original == diff_original_) return true;
    diff_original_ = std::move(original);
    Dispatch(diff_original_ ? core_->computeDiff(*diff_original_) : core_->clearDiff());
    return true;
  }

  bool Dispatch(const se::EditorActionResult& result) {
    text_input_->OnAction(result);
    if (result.needs_redraw) {
      model_dirty_ = true;
      Invalidate();
    }
    const double delay = result.next_animation_delay_ms / 1000.0;
    if (result.needsAnimation() && (!animation_ || delay < animation_delay_)) {
      animation_deadline_ = frame_time_ + delay;
      Invalidate();
    }
    animation_ = result.needsAnimation();
    animation_delay_ = delay;
    const bool moving = result.hasActiveInteraction() || result.needsViewportMotion();
    if (moving != viewport_in_motion_) {
      viewport_in_motion_ = moving;
      if (!moving) decorations_pending_ = true;
      Invalidate();
    }
    if (result.pointer_cursor_changed && set_cursor_) {
      const auto cursor = result.pointer_cursor_after;
      set_cursor_(cursor == se::PointerCursorType::HAND ? PointerCursorKind::Hand :
                  cursor == se::PointerCursorType::TEXT ? PointerCursorKind::Text : PointerCursorKind::Default);
    }
    const bool edited = !result.text_changes.empty();
    if (edited) {
      ++edit_generation_;
      snapshot_.reset();
      pending_changes_.insert(pending_changes_.end(), result.text_changes.begin(), result.text_changes.end());
      if (invalidate_semantics_) invalidate_semantics_();
    }
    if (result.selection_changed || result.cursor_changed || edited) {
      CancelDecorations();
      decorations_pending_ = true;
      blink_on_ = true;
      blink_deadline_ = frame_time_ + 0.5;
      if (!edited && result.cursor_changed) DismissCompletion();
      if (edited || result.cursor_changed) DismissInlineSuggestion();
    }
    if (edited) {
      HideContextMenu();
      if (diff_original_) Dispatch(core_->computeDiff(*diff_original_));
    }
    if (result.scale_changed) {
      settings_.scale = result.scale_after;
      UpdateFontMetrics();
    }
    UpdateSelectionMenu(result);
    if (active_ && edited) {
      if (core_->hasComposition()) DismissCompletion();
      else RefreshCompletion(result);
    }
    if (active_) {
      if (defer_events_) pending_events_.push_back(result);
      else PublishAction(result);
    }
    return result.handled;
  }

  bool ScrollInput(const ScrollInputEvent& event) {
    const auto point = window_to_local_(event.position);
    if (!point) return false;
    auto gesture = ToWheelGestureEvent(event);
    gesture.points.push_back({point->x, point->y});
    gesture.modifiers = ToModifiers(event.modifiers);
    const auto result = core_->handleGestureEvent(gesture);
    Dispatch(result);
    return result.scroll_changed || result.scale_changed;
  }

  void HandleContextMenu(Point window_position) {
    const auto point = window_to_local_(window_position);
    if (!point) return;
    se::GestureEvent event;
    event.type = se::EventType::MOUSE_RIGHT_DOWN;
    event.modifiers = modifiers_;
    event.points.push_back({point->x, point->y});
    Dispatch(core_->handleGestureEvent(event));
  }

  void Hover(const HoverEvent& event) {
    if (mouse_pointer_active_) return;
    se::GestureEvent gesture;
    gesture.type = se::EventType::MOUSE_MOVE;
    gesture.modifiers = modifiers_;
    const Point point = event.type == HoverEventType::Leave ? Point{-1.0F, -1.0F} : event.position;
    gesture.points.push_back({point.x, point.y});
    Dispatch(core_->handleGestureEvent(gesture));
  }

  void SetFocused(bool focused) {
    if (focused_ == focused) return;
    focused_ = focused;
    blink_on_ = focused;
    blink_deadline_ = frame_time_ + 0.5;
    if (!focused) {
      UpdatePointerModifiers(KeyModifier::NONE);
      DismissCompletion();
    }
    Invalidate();
  }

  bool PrepareGeometry(Size size) {
    const se::Size viewport{size.width, size.height};
    const bool resized = viewport_.width != viewport.width || viewport_.height != viewport.height;
    if (resized) {
      viewport_ = viewport;
      defer_events_ = true;
      Dispatch(core_->setViewport(viewport_));
      defer_events_ = false;
      decorations_pending_ = true;
    }
    return RebuildModel();
  }

  NodeExtension::FrameResult Frame(double now) {
    frame_time_ = now;
    PublishPendingEvents();
    if (animation_ && now >= animation_deadline_) {
      Dispatch(core_->tickAnimations());
      animation_deadline_ = now + animation_delay_;
    }
    if (focused_ && now >= blink_deadline_) {
      blink_on_ = !blink_on_;
      blink_deadline_ = now + 0.5;
      Invalidate();
    }
    RebuildModel();
    UpdateCompletionPopup();
    const double decoration_interval = settings_.decoration_scroll_refresh_min_interval_ms / 1000.0;
    if (decorations_pending_ && !core_->getVisibleLineRange().isEmpty() &&
        (!viewport_in_motion_ || now >= last_decoration_time_ + decoration_interval)) {
      RefreshDecorations();
    }
    if (selection_menu_phase_ == SelectionMenuPhase::Pending && now >= selection_menu_deadline_) ShowSelectionMenu();

    double wake = std::numeric_limits<double>::infinity();
    if (animation_) wake = std::min(wake, animation_deadline_ - now);
    if (focused_) wake = std::min(wake, blink_deadline_ - now);
    if (decorations_pending_ && !core_->getVisibleLineRange().isEmpty()) {
      wake = std::min(wake, last_decoration_time_ + decoration_interval - now);
    }
    if (selection_menu_phase_ == SelectionMenuPhase::Pending) wake = std::min(wake, selection_menu_deadline_ - now);
    if (!pending_events_.empty()) wake = 0.0;
    if (!std::isfinite(wake)) return {};
    return wake <= 0.0 ? NodeExtension::FrameResult{.needs_frame = true} :
                         NodeExtension::FrameResult{.wake_after = wake};
  }

  void UpdatePointerModifiers(KeyModifier modifiers) {
    if (modifiers_ == modifiers) return;
    modifiers_ = modifiers;
    Dispatch(core_->updatePointerModifiers(modifiers_));
  }

  bool HandleKey(const KeyEvent& event) {
    UpdatePointerModifiers(ToModifiers(event.modifiers));
    if (event.type != KeyEventType::Down) return false;

    if (event.key == Key::Escape) {
      if (DismissCompletion() || DismissInlineSuggestion()) return true;
    }
    if (completion_.items && modifiers_ == KeyModifier::NONE) {
      switch (event.key) {
      case Key::ArrowUp: MoveCompletionSelection(-1); return true;
      case Key::ArrowDown: MoveCompletionSelection(1); return true;
      case Key::Enter:
      case Key::Tab: return ConfirmCompletion();
      default: break;
      }
    }
    if (event.key == Key::Tab && modifiers_ == KeyModifier::NONE && inline_suggestion_) return AcceptInlineSuggestion();
    if (event.key == Key::Enter && modifiers_ == KeyModifier::NONE && !core_->hasComposition() &&
        !core_->isInLinkedEditing() && ApplyNewLine()) return true;

    const auto key = ToKeyEvent(event);
    auto result = core_->handleKeyEvent(key);
    if (!result.handled && key.key_code != KeyCode::NONE && !event.text.empty() &&
        !event.modifiers.control && !event.modifiers.meta) {
      Dispatch(result);
      result = core_->handleKeyEvent({KeyCode::NONE, key.text, key.modifiers});
    }
    Dispatch(result);
    switch (static_cast<EditorBuiltinCommand>(result.command)) {
    case EditorBuiltinCommand::COPY: return Copy();
    case EditorBuiltinCommand::CUT: return Cut();
    case EditorBuiltinCommand::PASTE: return Paste();
    case EditorBuiltinCommand::TRIGGER_COMPLETION: return TriggerCompletion();
    default: break;
    }
    if (const auto handler = key_map_.GetCommand(result.command)) {
      const auto& bindings = key_map_.Bindings();
      const auto match = std::ranges::find(bindings, result.command, &KeyBinding::command);
      if (match != bindings.end()) handler(*match, controller_);
      return true;
    }
    return result.handled;
  }

  bool HandlePointer(const PointerEvent& event) {
    if (event.device_kind != PointerDeviceKind::Touch) {
      if (event.changed_button == PointerButton::Secondary) return false;
      if (event.type == PointerEventType::Down) mouse_pointer_active_ = true;
      if (event.type == PointerEventType::Up || event.type == PointerEventType::Cancel) mouse_pointer_active_ = false;
    }
    if (event.type == PointerEventType::Down) DismissCompletion();
    HideContextMenuOnPress(event);
    last_pointer_touch_ = event.device_kind == PointerDeviceKind::Touch;
    modifiers_ = ToModifiers(event.modifiers);
    const auto gesture = last_pointer_touch_ ? TouchGesture(event) : ToGestureEvent(event);
    const auto result = core_->handleGestureEvent(gesture);
    Dispatch(result);
    if (last_pointer_touch_ && !settings_.read_only && result.hit_target.type == se::HitTargetType::NONE &&
        (result.gesture_type == se::GestureType::TAP || result.gesture_type == se::GestureType::DOUBLE_TAP)) {
      show_keyboard_();
    }
    return result.handled || result.needs_redraw || result.gesture_type != se::GestureType::UNDEFINED;
  }

  void Render(PaintContext& paint, Size size) {
    RenderModel(paint, size, cached_model_);
    DrawInlineSuggestion(paint);
  }

  std::optional<Rect> PositionRect(TextPosition position) {
    if (position.line >= document_->getLineCount()) return std::nullopt;
    const auto rect = core_->getPositionScreenRect(position);
    return Rect{rect.x, rect.y, theme_.caret_width, rect.height};
  }

  Rect CursorRect() {
    const auto rect = core_->getCursorScreenRect();
    return {rect.x, rect.y, theme_.caret_width, rect.height};
  }

  std::optional<TextRange> Selection() const {
    return core_->hasSelection() ? std::optional<TextRange>(core_->getSelection()) : std::nullopt;
  }

  template <class Provider>
  static bool AddProvider(std::vector<std::shared_ptr<Provider>>& providers, std::shared_ptr<Provider> provider) {
    if (!provider || std::ranges::find(providers, provider) != providers.end()) return false;
    providers.push_back(std::move(provider));
    return true;
  }

  template <class Provider>
  static bool RemoveProvider(std::vector<std::shared_ptr<Provider>>& providers, const std::shared_ptr<Provider>& provider) {
    return std::erase(providers, provider) != 0;
  }

  bool AddNewLineProvider(std::shared_ptr<NewLineActionProvider> provider) {
    return AddProvider(newline_providers_, std::move(provider));
  }

  bool RemoveNewLineProvider(const std::shared_ptr<NewLineActionProvider>& provider) {
    return RemoveProvider(newline_providers_, provider);
  }

  bool AddDecorationProvider(std::shared_ptr<DecorationProvider> provider) {
    if (!provider || std::ranges::find(decoration_providers_, provider) != decoration_providers_.end()) return false;
    std::set<DecorationType> types;
    try {
      types = provider->GetCapabilities();
    } catch (const std::exception& error) {
      std::fprintf(stderr, "CodeEditor decoration provider capabilities: %s\n", error.what());
      return false;
    } catch (...) {
      std::fputs("CodeEditor decoration provider capabilities failed\n", stderr);
      return false;
    }
    decoration_providers_.push_back(std::move(provider));
    decoration_capabilities_.push_back(std::move(types));
    return RequestDecorations();
  }

  bool RemoveDecorationProvider(const std::shared_ptr<DecorationProvider>& provider) {
    const auto found = std::ranges::find(decoration_providers_, provider);
    if (found == decoration_providers_.end()) return false;
    const auto types = decoration_capabilities_.begin() + (found - decoration_providers_.begin());
    for (auto type : *types) {
      detail::DecorationBatch::Clear(type, *core_, [this](const auto& result) { Dispatch(result); });
    }
    decoration_capabilities_.erase(types);
    decoration_providers_.erase(found);
    return RequestDecorations();
  }

  bool RequestDecorations() {
    CancelDecorations();
    decorations_pending_ = true;
    Invalidate();
    return true;
  }

  bool AddCompletionProvider(std::shared_ptr<CompletionProvider> provider) {
    if (!AddProvider(completion_providers_, std::move(provider))) return false;
    DismissCompletion();
    return true;
  }

  bool RemoveCompletionProvider(const std::shared_ptr<CompletionProvider>& provider) {
    if (!RemoveProvider(completion_providers_, provider)) return false;
    DismissCompletion();
    return true;
  }

  bool TriggerCompletion(CompletionTriggerKind kind = CompletionTriggerKind::Invoked, std::string character = {}) {
    if (settings_.read_only || core_->hasComposition() || completion_providers_.empty()) return false;
    if (kind == CompletionTriggerKind::Invoked && !focused_) editor_anchor_.RequestFocus();
    ResetCompletionRequest();
    if (completion_presentation_) Invalidate();
    const auto cursor = core_->getCursorPosition();
    const CompletionContext context{kind, std::move(character), cursor, document_handle_.GetLineText(cursor.line),
                                    core_->getWordRangeAtCursor(), language_, metadata_};
    completion_.requested = true;
    completion_.pending = completion_providers_.size();
    completion_results_.resize(completion_providers_.size());
    for (std::size_t index = 0; index < completion_providers_.size(); ++index) {
      completion_tasks_.push_back(tasks_.Launch(
          [weak = weak_from_this(), provider = completion_providers_[index], context,
           generation = completion_generation_, index]() {
            return CollectCompletion(weak, provider, context, generation, index);
          }));
    }
    return true;
  }

  bool ShowCompletionItems(std::vector<CompletionItem> items) {
    DismissCompletion();
    if (settings_.read_only || items.empty()) return false;
    SetCompletionItems(std::move(items));
    return true;
  }

  bool DismissCompletion() {
    if (!completion_.items && !completion_.requested && !completion_presentation_) return false;
    ResetCompletionRequest();
    DismissCompletionPopup();
    Invalidate();
    return true;
  }

  bool SetCompletionRenderer(CompletionItemRenderer renderer) {
    completion_renderer_ = std::move(renderer);
    if (completion_.items) InvalidateCompletionPopup();
    return true;
  }

  bool SetContextMenuProvider(ContextMenuItemProvider provider) {
    context_menu_provider_ = std::move(provider);
    return true;
  }

  bool SetSelectionMenuProvider(SelectionMenuItemProvider provider) {
    selection_menu_provider_ = std::move(provider);
    return true;
  }

  bool ShowInlineSuggestion(InlineSuggestion suggestion) {
    if (settings_.read_only || suggestion.text.empty() || suggestion.line >= document_->getLineCount() ||
        suggestion.column > document_->getLineU16TextRef(suggestion.line).size()) return false;
    inline_suggestion_ = std::move(suggestion);
    Invalidate();
    return true;
  }

  bool DismissInlineSuggestion() {
    if (!inline_suggestion_) return false;
    auto suggestion = std::exchange(inline_suggestion_, std::nullopt);
    Invalidate();
    if (active_ && !defer_events_) events_.Emit<EditorEvents::InlineSuggestionDismissed>(*suggestion);
    else if (active_) pending_inline_dismissal_ = std::move(suggestion);
    return true;
  }

  bool IsInlineSuggestionShowing() const { return inline_suggestion_.has_value(); }

  bool Copy() {
    return clipboard_ && core_->hasSelection() && clipboard_->WriteText(core_->getSelectedText());
  }

  bool Cut() {
    return !settings_.read_only && Copy() && Dispatch(core_->deleteText(core_->getSelection()));
  }

  bool Paste() {
    if (settings_.read_only || !clipboard_) return false;
    const auto text = clipboard_->ReadText();
    return text && Dispatch(core_->insertText(*text));
  }

  static void ValidateSettings(const EditorSettings& settings) {
    const auto positive = [](float value) { return std::isfinite(value) && value > 0.0F; };
    const auto nonnegative = [](float value) { return std::isfinite(value) && value >= 0.0F; };
    if (!positive(settings.editor_text_size) || !positive(settings.scale) || !nonnegative(settings.line_spacing_mult) ||
        !std::isfinite(settings.line_spacing_add) || !nonnegative(settings.content_start_padding) ||
        !nonnegative(settings.line_number_margin) || settings.tab_size == 0 ||
        settings.wrap_mode < WrapMode::NONE || settings.wrap_mode > WrapMode::WORD_BREAK ||
        settings.fold_arrow_mode < FoldArrowMode::AUTO || settings.fold_arrow_mode > FoldArrowMode::HIDDEN ||
        settings.current_line_render_mode < CurrentLineRenderMode::BACKGROUND ||
        settings.current_line_render_mode > CurrentLineRenderMode::NONE ||
        settings.render_whitespace < WhitespaceRenderMode::NONE || settings.render_whitespace > WhitespaceRenderMode::ALL ||
        settings.auto_indent_mode < AutoIndentMode::NONE || settings.auto_indent_mode > AutoIndentMode::KEEP_INDENT ||
        settings.scrollbar.mode > se::ScrollbarMode::NEVER || settings.scrollbar.track_tap_mode > se::ScrollbarTrackTapMode::DISABLED ||
        !nonnegative(settings.scrollbar.thickness) || !nonnegative(settings.scrollbar.min_thumb) ||
        !nonnegative(settings.scrollbar.thumb_hit_padding) ||
        settings.decoration_scroll_refresh_min_interval_ms < 0 ||
        !nonnegative(settings.decoration_overscan_viewport_multiplier)) {
      throw std::invalid_argument("Invalid CodeEditor settings");
    }
  }

  void DeferEvents(bool defer) { defer_events_ = defer; }

 private:
  std::shared_ptr<TextInputBridge> CreateTextInput() {
    const auto weak = weak_from_this();
    auto input = std::make_shared<detail::TextInputBridge>(
        core_, document_,
        [weak](const se::EditorActionResult& result) {
          if (const auto editor = weak.lock()) editor->Dispatch(result);
        },
        [weak](const KeyEvent& event) {
          const auto editor = weak.lock();
          return editor && editor->HandleKey(event);
        },
        [weak](Point point) {
          const auto editor = weak.lock();
          return editor ? editor->HitTextPosition(point) : std::nullopt;
        },
        [weak](const TextRange& range) {
          const auto editor = weak.lock();
          return editor ? editor->RangeRects(range) : std::vector<Rect>{};
        });
    input->SetCaretWidth(theme_.caret_width);
    return input;
  }

  void Invalidate() {
    if (invalidate_) invalidate_();
  }

  void UpdateFontMetrics() {
    const Font font = ContentFont();
    text_measurer_->SetFont(settings_.font_family, EffectiveFontSize());
    font_metrics_ = measurer_.Metrics(font);
    inlay_hint_metrics_ = measurer_.Metrics(text_measurer_->InlayHintFont());
    char_width_ = std::max(1.0F, measurer_.MeasureRun("0", TextStyle{font, Color::Black(), TextDecoration::None}).advance);
    core_->getLayoutMetrics().line_number_margin = settings_.line_number_margin;
    Dispatch(core_->onFontMetricsChanged());
  }

  float EffectiveFontSize() const { return settings_.editor_text_size * settings_.scale; }
  Font ContentFont() const { return Font::FromFamily(settings_.font_family, EffectiveFontSize()); }

  void ApplyIndentSettings() {
    Dispatch(core_->setTabSize(language_ ? language_->tab_size.value_or(settings_.tab_size) : settings_.tab_size));
    Dispatch(core_->setInsertSpaces(language_ ? language_->insert_spaces.value_or(settings_.insert_spaces) : settings_.insert_spaces));
  }

  void PublishPendingEvents() {
    if (!active_) return;
    for (const auto& result : std::exchange(pending_events_, {})) PublishAction(result);
    if (auto suggestion = std::exchange(pending_inline_dismissal_, std::nullopt)) {
      events_.Emit<EditorEvents::InlineSuggestionDismissed>(*suggestion);
    }
    if (std::exchange(document_event_pending_, false)) events_.Emit<EditorEvents::DocumentLoaded>();
  }

  void PublishAction(const se::EditorActionResult& result) {
    if (!result.text_changes.empty()) {
      events_.Emit<EditorEvents::TextChanged>(TextChangedEvent{result.text_changes, result.text_change_kind, result.source});
    }
    if (result.cursor_changed) events_.Emit<EditorEvents::CursorChanged>(CursorChangedEvent{result.cursor_after});
    const auto selection = result.has_selection_after ? std::optional<TextRange>(result.selection_after) : std::nullopt;
    if (result.selection_changed) {
      events_.Emit<EditorEvents::SelectionChanged>(SelectionChangedEvent{result.has_selection_after, selection, result.cursor_after});
    }
    if (result.scroll_changed) {
      events_.Emit<EditorEvents::ScrollChanged>(ScrollChangedEvent{result.scroll_x_after, result.scroll_y_after});
    }
    if (result.scale_changed) events_.Emit<EditorEvents::ScaleChanged>(ScaleChangedEvent{result.scale_after});
    const Point location{result.tap_point.x, result.tap_point.y};
    switch (result.gesture_type) {
    case se::GestureType::TAP:
      PublishTap(result.hit_target, location);
      break;
    case se::GestureType::DOUBLE_TAP:
      events_.Emit<EditorEvents::DoubleTapped>(DoubleTapEvent{result.cursor_after, result.has_selection_after, selection, location});
      break;
    case se::GestureType::LONG_PRESS:
      events_.Emit<EditorEvents::LongPressed>(LongPressEvent{result.cursor_after, location});
      if (context_menu_provider_) ShowContextMenu(result, ContextMenuTriggerKind::LongPress, selection);
      break;
    case se::GestureType::CONTEXT_MENU:
      events_.Emit<EditorEvents::ContextMenu>(ContextMenuEvent{result.cursor_after, location});
      ShowContextMenu(result, ContextMenuTriggerKind::RightClick, selection);
      break;
    default: break;
    }
  }

  void PublishTap(const HitTarget& target, Point location) {
    using Type = se::HitTargetType;
    switch (target.type) {
    case Type::FOLD_GUTTER:
    case Type::FOLD_PLACEHOLDER:
      events_.Emit<EditorEvents::FoldToggled>(FoldToggleEvent{target.line, target.type == Type::FOLD_GUTTER, location});
      break;
    case Type::LINK:
      events_.Emit<EditorEvents::LinkClicked>(
          LinkClickEvent{target.line, target.column, core_->getLinkTargetAt(target.line, target.column), location});
      break;
    case Type::CODELENS:
      events_.Emit<EditorEvents::CodeLensClicked>(CodeLensClickEvent{target.line, target.column, target.icon_id, location});
      break;
    case Type::GUTTER_ICON:
      events_.Emit<EditorEvents::GutterIconClicked>(GutterIconClickEvent{target.line, target.icon_id, location});
      break;
    case Type::INLAY_HINT_TEXT:
      events_.Emit<EditorEvents::InlayHintClicked>(InlayHintClickEvent{target.line, target.column, InlayType::TEXT, 0, location});
      break;
    case Type::INLAY_HINT_ICON:
      events_.Emit<EditorEvents::InlayHintClicked>(InlayHintClickEvent{target.line, target.column, InlayType::ICON, target.icon_id, location});
      break;
    case Type::INLAY_HINT_COLOR:
      events_.Emit<EditorEvents::InlayHintClicked>(InlayHintClickEvent{target.line, target.column, InlayType::COLOR, target.color_value, location});
      break;
    default: break;
    }
  }

  se::GestureEvent TouchGesture(const PointerEvent& event) {
    se::GestureEvent gesture;
    gesture.modifiers = modifiers_;
    const se::PointF point{event.position.x, event.position.y};
    auto found = std::ranges::find(active_pointers_, event.pointer_id, &std::pair<std::int64_t, se::PointF>::first);
    switch (event.type) {
    case PointerEventType::Down:
      gesture.type = active_pointers_.empty() ? se::EventType::TOUCH_DOWN : se::EventType::TOUCH_POINTER_DOWN;
      if (found == active_pointers_.end()) active_pointers_.emplace_back(event.pointer_id, point);
      else found->second = point;
      break;
    case PointerEventType::Move:
      gesture.type = se::EventType::TOUCH_MOVE;
      if (found != active_pointers_.end()) found->second = point;
      break;
    case PointerEventType::Up:
      if (found != active_pointers_.end()) active_pointers_.erase(found);
      gesture.type = active_pointers_.empty() ? se::EventType::TOUCH_UP : se::EventType::TOUCH_POINTER_UP;
      break;
    case PointerEventType::Cancel:
      active_pointers_.clear();
      gesture.type = se::EventType::TOUCH_CANCEL;
      break;
    }
    for (const auto& [id, position] : active_pointers_) gesture.points.push_back(position);
    if (gesture.points.empty()) gesture.points.push_back(point);
    return gesture;
  }

  bool RebuildModel() {
    const bool changed = std::exchange(model_dirty_, false);
    if (changed) {
      cached_model_ = {};
      core_->buildRenderModel(cached_model_);
      const auto visible = core_->getVisibleLineRange();
      if (visible.start != last_visible_range_.start || visible.end != last_visible_range_.end) {
        last_visible_range_ = visible;
        CancelDecorations();
        decorations_pending_ = true;
      }
    }
    if (inline_suggestion_) inline_rect_ = PositionRect({inline_suggestion_->line, inline_suggestion_->column});
    return changed;
  }

  float RunBoundaryX(const se::VisualRun& run, std::size_t line, std::size_t column) {
    if (column == run.column) return run.x;
    if (column >= run.column + run.length) return run.x + run.width;
    return core_->getPositionScreenRect({line, column}).x;
  }

  std::vector<Rect> RangeRects(const TextRange& range) {
    std::vector<Rect> rects;
    if (model_dirty_) return rects;
    const float line_height = LineHeight(cached_model_);
    const float content_x = cached_model_.split_x;
    for (const auto& row : cached_model_.lines) {
      if (row.kind != se::VisualLineKind::CONTENT) continue;
      for (const auto& run : row.runs) {
        if (run.length == 0 || run.source_line == se::kVisualRunNoDocumentLine) continue;
        const auto line = run.source_line == se::kVisualRunOwnerLine ? row.logical_line : run.source_line;
        if (line < range.start.line || line > range.end.line) continue;
        const auto start = line == range.start.line ? std::max(run.column, range.start.column) : run.column;
        const auto run_end = run.column + run.length;
        const auto end = line == range.end.line ? std::min(run_end, range.end.column) : run_end;
        if (start >= end) continue;
        const float a = RunBoundaryX(run, line, start);
        const float b = RunBoundaryX(run, line, end);
        const float left = std::max(content_x, std::min(a, b));
        const float right = std::min(viewport_.width, std::max(a, b));
        if (right > left) {
          rects.push_back({left, row.line_number_position.y - font_metrics_.ascent, right - left, line_height});
        }
      }
    }
    return rects;
  }

  std::optional<std::pair<TextPosition, se::CaretAffinity>> HitTextPosition(Point point) {
    const float height = LineHeight(cached_model_);
    const auto row = std::ranges::find_if(cached_model_.lines, [&](const se::VisualLine& line) {
      const float y = line.line_number_position.y - font_metrics_.ascent;
      return point.y >= y && point.y < y + height;
    });
    if (row == cached_model_.lines.end()) return std::nullopt;
    if (row->kind != se::VisualLineKind::CONTENT) return {{row->interaction_position, se::CaretAffinity::DOWNSTREAM}};
    auto last = TextPosition{row->logical_line, 0};
    for (const auto& run : row->runs) {
      if (run.length == 0 || run.source_line == se::kVisualRunNoDocumentLine) continue;
      const auto line = run.source_line == se::kVisualRunOwnerLine ? row->logical_line : run.source_line;
      if (point.x < run.x) return {{TextPosition{line, run.column}, se::CaretAffinity::DOWNSTREAM}};
      if (point.x <= run.x + run.width) {
        const auto& text = document_->getLineU16TextRef(line);
        std::size_t start = run.column;
        std::size_t end = std::min(text.size(), run.column + run.length);
        while (start < end) {
          const auto mid = start + (end - start) / 2;
          if (RunBoundaryX(run, line, mid) < point.x) start = mid + 1;
          else end = mid;
        }
        if (start > run.column && start <= text.size()) {
          const auto before = RunBoundaryX(run, line, start - 1);
          const auto after = RunBoundaryX(run, line, start);
          if (point.x - before < after - point.x) --start;
        }
        if (start < text.size() && text[start] >= 0xDC00 && text[start] <= 0xDFFF && start != 0) --start;
        const auto affinity = start == run.column + run.length ? se::CaretAffinity::UPSTREAM : se::CaretAffinity::DOWNSTREAM;
        return {{TextPosition{line, start}, affinity}};
      }
      last = {line, run.column + run.length};
    }
    return {{last, se::CaretAffinity::UPSTREAM}};
  }

  void CancelDecorations() {
    for (const auto& receiver : decoration_receivers_) detail::DecorationReceiverAccess::Cancel(receiver);
    decoration_receivers_.clear();
    for (const auto& task : decoration_tasks_) task.Cancel();
    decoration_tasks_.clear();
    decoration_batch_.reset();
    ++decoration_generation_;
    decoration_apply_posted_ = false;
  }

  static Task<void> CollectDecorations(std::shared_ptr<DecorationProvider> provider, DecorationContext context,
                                       DecorationReceiver receiver) {
    try {
      co_await provider->ProvideDecorations(std::move(context), receiver);
    } catch (const std::exception& error) {
      if (!receiver.IsCancelled()) std::fprintf(stderr, "CodeEditor decoration provider: %s\n", error.what());
    } catch (...) {
      if (!receiver.IsCancelled()) std::fputs("CodeEditor decoration provider failed\n", stderr);
    }
  }

  void RefreshDecorations() {
    decorations_pending_ = false;
    last_decoration_time_ = frame_time_;
    CancelDecorations();
    if (decoration_providers_.empty()) {
      pending_changes_.clear();
      return;
    }
    const auto visible = core_->getVisibleLineRange();
    const auto count = document_->getLineCount();
    const auto last = static_cast<std::int32_t>(std::min<std::size_t>(count - 1, INT32_MAX));
    const double overscan = static_cast<double>(visible.length()) * settings_.decoration_overscan_viewport_multiplier;
    const auto margin = static_cast<std::int32_t>(std::min<double>(last, std::ceil(overscan)));
    const IntRange requested{visible.start - std::min(margin, visible.start), visible.end + std::min(margin, last - visible.end)};
    if (!snapshot_) snapshot_ = std::make_shared<const std::string>(document_handle_.GetText());
    const DecorationContext context{visible, requested, count, core_->getCursorPosition(),
                                    std::exchange(pending_changes_, {}), language_, metadata_, snapshot_,
                                    document_generation_, !viewport_in_motion_,
                                    previous_requested_revision_, edit_generation_};
    previous_requested_revision_ = edit_generation_;
    decoration_batch_.emplace(visible, decoration_capabilities_);
    for (std::size_t index = 0; index < decoration_providers_.size(); ++index) {
      auto receiver = detail::DecorationReceiverAccess::Create(tasks_,
          [weak = weak_from_this(), index, generation = decoration_generation_](DecorationResult result) {
            if (const auto editor = weak.lock()) editor->QueueDecorations(generation, index, std::move(result));
          });
      decoration_tasks_.push_back(tasks_.Launch(
          [provider = decoration_providers_[index], context, receiver] {
            return CollectDecorations(provider, context, receiver);
          }));
      decoration_receivers_.push_back(std::move(receiver));
    }
  }

  void QueueDecorations(std::uint64_t generation, std::size_t index, DecorationResult result) {
    if (!active_ || generation != decoration_generation_ || !decoration_batch_) return;
    decoration_batch_->Accept(index, std::move(result));
    if (std::exchange(decoration_apply_posted_, true)) return;
    tasks_.Post([weak = weak_from_this(), generation] {
      const auto editor = weak.lock();
      if (!editor || !editor->active_ || generation != editor->decoration_generation_) return;
      editor->decoration_apply_posted_ = false;
      // A core result can change viewport geometry. Keep the batch alive until
      // all of this delivery's writes have been dispatched.
      auto batch = std::move(editor->decoration_batch_);
      editor->decoration_batch_.reset();
      batch->Apply(*editor->core_, *editor->document_, [&](const auto& action) { editor->Dispatch(action); });
      if (generation == editor->decoration_generation_) editor->decoration_batch_ = std::move(batch);
    });
  }

  static Task<void> CollectCompletion(std::weak_ptr<EditorSession> weak, std::shared_ptr<CompletionProvider> provider,
                                      CompletionContext context, std::uint64_t generation, std::size_t index) {
    CompletionResult result;
    try {
      result = co_await provider->ProvideCompletions(std::move(context));
    } catch (const std::exception& error) {
      std::fprintf(stderr, "CodeEditor completion provider: %s\n", error.what());
    } catch (...) {
      std::fputs("CodeEditor completion provider failed\n", stderr);
    }
    const auto editor = weak.lock();
    if (editor && editor->active_ && generation == editor->completion_generation_) {
      --editor->completion_.pending;
      editor->completion_results_[index] = std::move(result);
      editor->PublishCompletions();
    }
  }

  void PublishCompletions() {
    std::vector<CompletionItem> items;
    completion_.incomplete = false;
    for (const auto& result : completion_results_) {
      items.insert(items.end(), result.items.begin(), result.items.end());
      completion_.incomplete = completion_.incomplete || result.is_incomplete;
    }
    SetCompletionItems(std::move(items));
  }

  void SetCompletionItems(std::vector<CompletionItem> items) {
    std::stable_sort(items.begin(), items.end(), [](const CompletionItem& lhs, const CompletionItem& rhs) {
      const auto& left = lhs.sort_key.empty() ? lhs.label : lhs.sort_key;
      const auto& right = rhs.sort_key.empty() ? rhs.label : rhs.sort_key;
      return left < right;
    });
    completion_.items = items.empty() ? nullptr : std::make_shared<const std::vector<CompletionItem>>(std::move(items));
    completion_.selected = 0;
    InvalidateCompletionPopup();
  }

  void RefreshCompletion(const se::EditorActionResult& result) {
    if (result.source != EditorActionSource::KEYBOARD && result.source != EditorActionSource::IME) {
      DismissCompletion();
      return;
    }
    const auto& text = result.text_changes.front().new_text;
    if (!text.empty() && std::ranges::any_of(completion_providers_, [&](const auto& provider) {
          try {
            return provider->IsTriggerCharacter(text);
          } catch (const std::exception& error) {
            std::fprintf(stderr, "CodeEditor completion trigger: %s\n", error.what());
          } catch (...) {
            std::fputs("CodeEditor completion trigger failed\n", stderr);
          }
          return false;
        })) {
      TriggerCompletion(CompletionTriggerKind::Character, text);
    } else if (completion_.requested && (completion_.items || completion_.incomplete || completion_.pending != 0)) {
      TriggerCompletion(CompletionTriggerKind::Retrigger);
    } else {
      DismissCompletion();
    }
  }

  bool ConfirmCompletion() {
    if (!completion_.items || completion_.selected >= completion_.items->size()) return false;
    auto item = (*completion_.items)[completion_.selected];
    DismissCompletion();
    const auto caret = core_->getCursorPosition();
    TextEdit primary = item.text_edit ? std::move(*item.text_edit) :
        TextEdit{{caret, caret}, item.insert_text.value_or(item.label)};
    if (item.insert_text_format == InsertTextFormat::Snippet) {
      auto snippet = std::exchange(primary.new_text, {});
      if (!primary.range.isCollapsed() || !item.additional_text_edits.empty()) {
        item.additional_text_edits.insert(item.additional_text_edits.begin(), std::move(primary));
        if (!Dispatch(core_->applyTextEdits(std::move(item.additional_text_edits)))) return false;
      } else if (primary.range.start != caret) {
        Dispatch(core_->setCursorPosition(primary.range.start));
      }
      return Dispatch(core_->insertSnippet(snippet));
    }
    item.additional_text_edits.insert(item.additional_text_edits.begin(), std::move(primary));
    return Dispatch(core_->applyTextEdits(std::move(item.additional_text_edits)));
  }

  bool ApplyNewLine() {
    if (settings_.read_only || newline_providers_.empty()) return false;
    const auto position = core_->getCursorPosition();
    const NewLineContext context{position.line, position.column, document_handle_.GetLineText(position.line), language_, metadata_};
    for (const auto& provider : newline_providers_) {
      try {
        if (auto action = provider->ProvideNewLineAction(context)) {
          if (!action->text.empty()) Dispatch(core_->insertText(action->text));
          return true;
        }
      } catch (const std::exception& error) {
        std::fprintf(stderr, "CodeEditor newline provider: %s\n", error.what());
      } catch (...) {
        std::fputs("CodeEditor newline provider failed\n", stderr);
      }
    }
    return false;
  }

  bool AcceptInlineSuggestion() {
    if (settings_.read_only || !inline_suggestion_) return false;
    auto suggestion = std::exchange(inline_suggestion_, std::nullopt);
    const TextPosition position{suggestion->line, suggestion->column};
    const bool accepted = Dispatch(core_->replaceText({position, position}, suggestion->text));
    Invalidate();
    if (accepted) events_.Emit<EditorEvents::InlineSuggestionAccepted>(*suggestion);
    else events_.Emit<EditorEvents::InlineSuggestionDismissed>(*suggestion);
    return accepted;
  }

  void DrawInlineSuggestion(PaintContext& paint) {
    if (!inline_suggestion_ || !inline_rect_ || inline_rect_->height <= 0.0F ||
        cached_model_.split_x >= viewport_.width) return;
    const auto& rect = *inline_rect_;
    const auto& text = inline_suggestion_->text;
    const TextStyle style{ContentFont().WithSlant(FontSlant::Italic), theme_.phantom_text_color, TextDecoration::None};
    paint.PushClip({cached_model_.split_x, 0.0F, viewport_.width - cached_model_.split_x, viewport_.height});
    float y = rect.y;
    for (std::size_t start = 0; start < text.size() && y < viewport_.height; ) {
      const auto end = text.find_first_of("\r\n", start);
      const auto count = end == std::string::npos ? text.size() - start : end - start;
      const float x = start == 0 ? rect.x : cached_model_.split_x + settings_.content_start_padding;
      if (count > 0 && y + rect.height > 0.0F) {
        paint.DrawTextRun({x, y, std::max(0.0F, viewport_.width - x), rect.height},
                          {x, y + font_metrics_.ascent}, text.substr(start, count), style);
      }
      if (end == std::string::npos) break;
      start = end + (text[end] == '\r' && end + 1 < text.size() && text[end + 1] == '\n' ? 2 : 1);
      y += rect.height;
    }
    paint.PopClip();
  }

  void HideContextMenuOnPress(const PointerEvent& event) {
    if (event.type == PointerEventType::Down) HideContextMenu();
  }

  void HideContextMenu() {
    if (const auto layer = std::exchange(context_menu_layer_, std::nullopt)) menu_.Dismiss(*layer);
  }

  void HideSelectionMenu(SelectionMenuPhase next) {
    selection_menu_phase_ = next;
    if (const auto layer = std::exchange(selection_menu_layer_, std::nullopt)) popup_.Dismiss(*layer);
  }

  void HideMenus() {
    HideContextMenu();
    HideSelectionMenu(SelectionMenuPhase::Hidden);
  }

  void UpdateSelectionMenu(const se::EditorActionResult& result) {
    if (result.selection_changed && !result.has_selection_after) {
      HideSelectionMenu(SelectionMenuPhase::Hidden);
      return;
    }
    if (last_pointer_touch_ && result.selection_changed && result.has_selection_after &&
        result.source == EditorActionSource::GESTURE && !context_menu_layer_) {
      HideSelectionMenu(SelectionMenuPhase::Pending);
      selection_menu_deadline_ = frame_time_ + 0.20;
      Invalidate();
    }
    if (selection_menu_phase_ == SelectionMenuPhase::Hidden) return;
    if (result.hasActiveInteraction() || result.needsViewportMotion()) {
      HideSelectionMenu(SelectionMenuPhase::Suspended);
    } else if (selection_menu_phase_ == SelectionMenuPhase::Suspended ||
               (selection_menu_phase_ == SelectionMenuPhase::Visible && result.scroll_changed)) {
      HideSelectionMenu(SelectionMenuPhase::Pending);
      selection_menu_deadline_ = frame_time_ + 0.20;
      Invalidate();
    }
  }

  std::vector<ContextMenuSection> DefaultContextItems(const ContextMenuRequest& request) const {
    std::vector<ContextMenuSection> sections;
    if (!request.link_target.empty()) sections.push_back({{{"open_link", "Open Link"}, {"copy_link", "Copy Link"}}});
    if (request.has_selection) sections.push_back({{{"cut", "Cut", {}, !settings_.read_only}, {"copy", "Copy"}}});
    sections.push_back({{{"paste", "Paste", {}, !settings_.read_only}, {"select_all", "Select All"}}});
    return sections;
  }

  bool ExecuteMenuCommand(const std::string& id) {
    if (id == "copy") Copy();
    else if (id == "cut") Cut();
    else if (id == "paste") Paste();
    else if (id == "select_all") Dispatch(core_->selectAll());
    else if (id == "delete") {
      if (const auto selection = Selection()) Dispatch(core_->deleteText(*selection));
    } else return false;
    return true;
  }

  void ShowContextMenu(const se::EditorActionResult& result, ContextMenuTriggerKind kind, std::optional<TextRange> selection) {
    HideMenus();
    const auto& hit = result.hit_target;
    const ContextMenuRequest request{kind, result.cursor_after, {result.tap_point.x, result.tap_point.y},
                                     selection.has_value(), selection, hit,
                                     hit.type == se::HitTargetType::LINK ? core_->getLinkTargetAt(hit.line, hit.column) : std::string{}};
    const auto sections = context_menu_provider_ ? context_menu_provider_(request) : DefaultContextItems(request);
    std::vector<MenuEntry> entries;
    for (const auto& section : sections) {
      if (section.items.empty()) continue;
      if (!entries.empty()) entries.emplace_back(MenuSection{});
      for (const auto& item : section.items) {
        auto action = [weak = weak_from_this(), item, request, document = document_generation_] {
          const auto editor = weak.lock();
          if (!editor || !editor->active_ || document != editor->document_generation_) return;
          editor->HideContextMenu();
          if (editor->ExecuteMenuCommand(item.id)) return;
          if (item.id == "open_link") {
            editor->PublishTap(request.hit_target, request.location_in_editor);
          } else if (item.id == "copy_link" && editor->clipboard_ && !request.link_target.empty()) {
            editor->clipboard_->WriteText(request.link_target);
          } else {
            editor->events_.Emit<EditorEvents::ContextMenuItemClicked>(ContextMenuItemClickEvent{item, request});
          }
        };
        const auto label = item.secondary_label.empty() ? item.label : item.label + "  " + item.secondary_label;
        auto row = item.icon ? MenuItem(*item.icon, label, std::move(action)) : MenuItem(label, std::move(action));
        entries.emplace_back(std::move(row).Enabled(item.enabled));
      }
    }
    if (entries.empty()) return;
    MenuOptions options;
    options.on_dismiss_request = [weak = weak_from_this()] {
      if (const auto editor = weak.lock()) editor->HideContextMenu();
    };
    context_menu_layer_ = menu_.ShowAt(local_to_window_(request.location_in_editor), std::move(entries), std::move(options));
  }

  std::optional<Rect> SelectionMenuAnchor(const TextRange& selection) {
    const float left = std::clamp(cached_model_.split_x, 0.0F, viewport_.width);
    const Rect content{left, 0.0F, viewport_.width - left, viewport_.height};
    Rect bounds;
    const auto include = [&](Rect fragment) {
      fragment = fragment.Intersection(content);
      if (fragment.IsEmpty()) return;
      if (bounds.IsEmpty()) {
        bounds = fragment;
        return;
      }
      const float x = std::min(bounds.x, fragment.x);
      const float y = std::min(bounds.y, fragment.y);
      const float right = std::max(bounds.x + bounds.width, fragment.x + fragment.width);
      const float bottom = std::max(bounds.y + bounds.height, fragment.y + fragment.height);
      bounds = {x, y, right - x, bottom - y};
    };
    for (const Rect fragment : RangeRects(selection)) include(fragment);
    if (bounds.IsEmpty()) {
      if (const auto start = PositionRect(selection.start)) include(*start);
      if (const auto end = PositionRect(selection.end)) include(*end);
    }
    if (bounds.IsEmpty()) return std::nullopt;
    if constexpr (kSelectionHandlesEnabled) bounds.height += kSelectionMenuHandleClearance;
    return bounds;
  }

  void ShowSelectionMenu() {
    const auto selection = Selection();
    const auto anchor = selection ? SelectionMenuAnchor(*selection) : std::nullopt;
    if (!anchor) {
      HideSelectionMenu(SelectionMenuPhase::Hidden);
      return;
    }
    const auto items = selection_menu_provider_ ? selection_menu_provider_(*selection) :
        std::vector<SelectionMenuItem>{{"cut", "Cut", !settings_.read_only}, {"copy", "Copy"},
                                       {"paste", "Paste", !settings_.read_only}, {"select_all", "Select All"}};
    if (items.empty()) {
      HideSelectionMenu(SelectionMenuPhase::Hidden);
      return;
    }

    Views entries;
    const Font font = Font::System(12.0F);
    Color pressed = theme_.selection_menu_text_color;
    pressed.alpha *= 0.12F;
    for (const auto& item : items) {
      if (entries.Size() != 0) {
        entries.Add(Spacer().With(Grow(0.0F), huxerui::Frame{.width = 1.0F, .height = 18.0F},
                                  Background(theme_.selection_menu_divider_color)));
      }
      const auto action = [weak = weak_from_this(), item, selection, document = document_generation_] {
        const auto editor = weak.lock();
        if (!editor || !editor->active_ || document != editor->document_generation_ || editor->Selection() != selection) return;
        editor->HideSelectionMenu(SelectionMenuPhase::Hidden);
        if (!editor->ExecuteMenuCommand(item.id)) {
          editor->events_.Emit<EditorEvents::SelectionMenuItemClicked>(SelectionMenuItemClickEvent{item});
        }
      };
      const auto icon = item.icon_id && icon_provider_ ? icon_provider_(*item.icon_id) : std::nullopt;
      Color label_color = theme_.selection_menu_text_color;
      if (!item.enabled) label_color.alpha *= 0.32F;
      entries.Add(Row {
        icon ? Image(*icon).With(huxerui::Frame{.width = 16.0F, .height = 16.0F},
                                 Opacity(item.enabled ? 1.0F : 0.32F)) : View{},
        Text(item.label).Style({font, label_color}),
      }.OnClick(action)
       .With(huxerui::Frame{.height = kSelectionMenuHeight}, Padding(EdgeInsets::Symmetric(12.0F, 0.0F)),
             Spacing(6.0F), CrossAlign(CrossAxisAlignment::Center), Enabled(item.enabled), Focusable(false),
             CornerRadius(4.0F), Indication{.press = IndicationLayer{.fill = pressed}},
             Semantics{.role = SemanticRole::MenuItem, .label = item.label,
                       .descendants = SemanticDescendantPolicy::Exclude}));
    }
    const View bar = Row {std::move(entries)}.With(
        Padding(EdgeInsets::Symmetric(4.0F, 0.0F)), CrossAlign(CrossAxisAlignment::Center));
    const View surface = ScrollView(bar)
        .ScrollAxis(Axis::Horizontal)
        .With(huxerui::Frame{.height = kSelectionMenuHeight, .max_width = std::max(1.0F, viewport_.width - 16.0F)},
              Background(theme_.selection_menu_background_color), CornerRadius(8.0F), ClipChildren(),
              Shadow{.color = Color::Rgb(0, 0, 0, 0.24F), .offset = {0.0F, 2.0F}, .blur_radius = 6.0F},
              Semantics{.role = SemanticRole::Menu});
    ViewFactory factory = [surface] { return surface; };
    selection_menu_layer_ = popup_.ShowAtAnchor(editor_anchor_, *anchor, std::move(factory), {
        .placement = {.side = AnchorSide::Above, .alignment = AnchorAlignment::Center},
        .retain_anchor_focus = true,
        .on_dismiss_request = [weak = weak_from_this()] {
          if (const auto editor = weak.lock()) editor->HideSelectionMenu(SelectionMenuPhase::Hidden);
        },
    });
    selection_menu_phase_ = SelectionMenuPhase::Visible;
  }

  static Rect ToRect(const se::Rect& rect) {
    return Rect{rect.origin.x, rect.origin.y, rect.width, rect.height};
  }

  float LineHeight(const se::EditorRenderModel& model) const {
    return model.cursor.height > 0.0F ? model.cursor.height : font_metrics_.LineHeight();
  }

  void RenderModel(PaintContext& paint, Size size, const se::EditorRenderModel& model) {
    const float width = size.width;
    const float height = size.height;
    const float ascent = font_metrics_.ascent;
    const float font_height = ascent + font_metrics_.descent;
    const float row_height = LineHeight(model);
    const float row_baseline = ascent + (row_height - font_height) * 0.5F;
    const float split_x = std::clamp(model.split_x, 0.0F, width);

    paint.DrawRect(Rect{0.0F, 0.0F, width, height}, theme_.background_color);
    if (split_x > 0.0F) {
      paint.DrawRect(Rect{0.0F, 0.0F, split_x, height}, theme_.gutter_background);
    }
    for (const auto& line : model.lines) {
      const float row_top = line.line_number_position.y - row_baseline;
      if (line.line_background_color != 0 && split_x < width) {
        paint.DrawRect(Rect{split_x, row_top, width - split_x, row_height}, Argb(line.line_background_color));
      }
      if (line.gutter_background_color != 0 && split_x > 0.0F) {
        paint.DrawRect(Rect{0.0F, row_top, split_x, row_height}, Argb(line.gutter_background_color));
      }
    }

    const Rect current_line{0.0F, model.current_line.y, width, row_height};
    if (model.current_line_render_mode == se::CurrentLineRenderMode::BACKGROUND) {
      paint.DrawRect(current_line, theme_.current_line_color);
    } else if (model.current_line_render_mode == se::CurrentLineRenderMode::BORDER) {
      Color color = theme_.current_line_color == Color::Transparent() ? theme_.line_number_color : theme_.current_line_color;
      color.alpha = std::max(color.alpha, 160.0F / 255.0F);
      paint.DrawBorder(current_line, color, StrokeStyle{1.0F});
    }
    if (model.split_line_visible && split_x > 0.0F && model.split_x <= width) {
      paint.DrawLine({split_x, 0.0F}, {split_x, height}, theme_.split_line_color, StrokeStyle{1.0F});
    }

    const Font content_font = ContentFont();
    for (const se::VisualLine& line : model.lines) {
      if (line.owns_gutter_semantics && line.line_number >= 0) {
        const std::string number = std::to_string(line.line_number);
        const float number_width = char_width_ * static_cast<float>(number.size());
        const Color color = line.logical_line == model.cursor.text_position.line
            ? theme_.current_line_number_color : theme_.line_number_color;
        paint.DrawTextRun(
            Rect{line.line_number_position.x, line.line_number_position.y - ascent, number_width, font_height},
            Point{line.line_number_position.x, line.line_number_position.y},
            number,
            TextStyle{content_font, color, TextDecoration::None});
      }
    }
    if (split_x < width) {
      paint.PushClip(Rect{split_x, 0.0F, width - split_x, height});
      for (const auto& effect : model.range_effects) {
        if (effect.style.background_color != 0) paint.DrawRect(ToRect(effect.rect), Argb(effect.style.background_color));
      }
      for (const auto& line : model.lines) {
        for (const auto& run : line.runs) DrawRun(paint, run);
      }
      for (const auto& segment : model.guide_segments) DrawGuideSegment(paint, segment);
      for (const auto& effect : model.range_effects) {
        if (effect.style.border_color != 0) {
          const float stroke_width = effect.kind == se::RangeEffectKind::LINKED_EDITING_ACTIVE ? 2.0F : 1.5F;
          paint.DrawBorder(ToRect(effect.rect), Argb(effect.style.border_color), StrokeStyle{stroke_width});
        }
        if (effect.style.underline_color != 0 && effect.style.underline_style != se::RangeEffectUnderlineStyle::NONE) {
          DrawUnderline(paint, effect.rect, effect.style.underline_color, effect.style.underline_style);
        }
      }
      paint.PopClip();
    }

    for (const se::GutterIconRenderItem& icon : model.gutter_icons) {
      DrawGutterIcon(paint, icon);
    }
    for (const se::FoldMarkerRenderItem& marker : model.fold_markers) {
      const Color color = marker.logical_line == model.cursor.text_position.line
          ? theme_.current_line_number_color : theme_.line_number_color;
      DrawFoldMarker(paint, marker, color);
    }

    const bool caret_visible = model.cursor.visible && focused_ && blink_on_;
    if (caret_visible) {
      paint.DrawRect(
          Rect{model.cursor.position.x, model.cursor.position.y, theme_.caret_width, model.cursor.height},
          theme_.cursor_color);
    }

    if constexpr (kSelectionHandlesEnabled) {
      if (model.selection_start_handle.visible) {
        DrawSelectionHandle(paint, model.selection_start_handle);
      }
      if (model.selection_end_handle.visible) {
        DrawSelectionHandle(paint, model.selection_end_handle);
      }
    }

    if (model.vertical_scrollbar.visible) {
      DrawScrollbar(paint, model.vertical_scrollbar);
    }
    if (model.horizontal_scrollbar.visible) {
      DrawScrollbar(paint, model.horizontal_scrollbar);
    }
  }

  void DrawSelectionHandle(PaintContext& paint, const se::SelectionHandle& handle) {
    // Solid teardrop handle: a triangle tip touching the line bottom flares
    // into a circular bulb below. Use a solid fill.
    const float radius = 10.0F;
    const float cx = handle.position.x;
    const float line_top = handle.position.y;
    const float line_bottom = handle.position.y + handle.height;
    const Color fill = theme_.cursor_color;
    paint.DrawRect(Rect{cx - 1.5F, line_top, 3.0F, std::max(1.0F, line_bottom - line_top)}, fill);
    const float tip_y = line_bottom + 0.5F;
    const float bulb_y = tip_y + 16.0F;
    Path tip;
    tip.MoveTo(Point{cx, tip_y});
    tip.LineTo(Point{cx - radius * 0.85F, tip_y + 12.0F});
    tip.LineTo(Point{cx + radius * 0.85F, tip_y + 12.0F});
    tip.Close();
    paint.FillPath(std::move(tip), fill);
    paint.DrawCircle(Point{cx, bulb_y}, radius, fill);
  }

  void DrawScrollbar(PaintContext& paint, const se::ScrollbarModel& bar) {
    const Rect thumb = ToRect(bar.thumb);
    if (thumb.width <= 0.0F || thumb.height <= 0.0F) {
      return;
    }
    const auto fade = [alpha = std::clamp(bar.alpha, 0.0F, 1.0F)](Color color) {
      color.alpha *= alpha;
      return color;
    };
    paint.DrawRect(ToRect(bar.track), fade(theme_.scrollbar_track_color));
    paint.DrawRect(thumb, fade(bar.thumb_active ? theme_.scrollbar_thumb_active_color : theme_.scrollbar_thumb_color),
                   CornerRadii(4.0F));
  }

  void DrawRun(PaintContext& paint, const se::VisualRun& run) {
    const float ascent = font_metrics_.ascent;
    const float top = run.y - ascent;
    const float height = ascent + font_metrics_.descent;

    if (run.type == se::VisualRunType::INLAY_HINT) {
      DrawInlayHint(paint, run);
      return;
    }
    if (run.type == se::VisualRunType::FOLD_PLACEHOLDER) {
      DrawFoldPlaceholder(paint, run, top, height);
      return;
    }
    if (run.style.background_color != 0) {
      paint.DrawRect(Rect{run.x, top, run.width, height}, Argb(run.style.background_color));
    }
    if (run.text.empty()) return;

    if (run.type == se::VisualRunType::WHITESPACE) {
      const float cell_width = run.width / static_cast<float>(run.text.size());
      const float center_y = top + height * 0.5F;
      const float radius = std::max(1.0F, std::min(cell_width, EffectiveFontSize()) * 0.08F);
      for (std::size_t index = 0; index < run.text.size(); ++index) {
        const float center_x = run.x + cell_width * (static_cast<float>(index) + 0.5F);
        paint.DrawCircle({center_x, center_y}, radius, theme_.invisible_character_color);
      }
      return;
    }
    if (run.type == se::VisualRunType::TAB) {
      const float center_y = top + height * 0.5F;
      const float padding = std::min(run.width * 0.25F, 8.0F);
      const float left = run.x + padding;
      const float right = run.x + run.width - padding;
      const float arrow = std::clamp((right - left) * 0.35F, 2.0F, 5.0F);
      Path path;
      path.MoveTo({left, center_y}).LineTo({right, center_y});
      path.MoveTo({right - arrow, center_y - arrow}).LineTo({right, center_y}).LineTo({right - arrow, center_y + arrow});
      paint.StrokePath(path, theme_.invisible_character_color,
                       StrokeStyle{.width = std::max(1.0F, EffectiveFontSize() * 0.06F),
                                   .cap = StrokeCap::Round, .join = StrokeJoin::Round});
      return;
    }
    if (run.type == se::VisualRunType::NEWLINE) {
      const TextStyle style{ContentFont(), theme_.invisible_character_color, TextDecoration::None};
      paint.DrawTextRun({run.x, top, run.width, height}, {run.x, run.y}, Utf16ToUtf8(run.text), style);
      return;
    }

    // HuxerUI text runs must not contain line breaks; strip any that slip
    // through (for example multi-line snippet expansion) defensively.
    std::string text = Utf16ToUtf8(run.text);
    const size_t line_break = text.find_first_of("\r\n");
    if (line_break != std::string::npos) {
      text.resize(line_break);
    }

    if (run.type == se::VisualRunType::PHANTOM_TEXT) {
      TextStyle phantom{ContentFont().WithSlant(FontSlant::Italic), theme_.phantom_text_color,
                        TextDecoration::None};
      paint.DrawTextRun(Rect{run.x, top, run.width, height}, Point{run.x, run.y}, text, phantom);
      return;
    }

    const auto style = MakeTextStyle(run.style.font_style, run.style.color, ContentFont());
    paint.DrawTextRun(Rect{run.x, top, run.width, height}, Point{run.x, run.y}, text, style);
    if (run.active && (run.type == se::VisualRunType::CODELENS || run.type == se::VisualRunType::LINK)) {
      const float underline_y = run.y + 1.0F;
      paint.DrawLine({run.x, underline_y}, {run.x + run.width, underline_y}, Argb(run.style.color), StrokeStyle{1.0F});
    }
  }

  void DrawInlayHint(PaintContext& paint, const se::VisualRun& run) {
    const float top = run.y - inlay_hint_metrics_.ascent;
    const float height = inlay_hint_metrics_.ascent + inlay_hint_metrics_.descent;
    const float margin = run.margin;
    const Rect background{run.x + margin, top, run.width - margin * 2.0F, height};
    if (background.width <= 0.0F || height <= 0.0F) return;
    if (run.color_value != 0) {
      const float side = std::min(background.width, height);
      paint.DrawRect(Rect{background.x, top, side, side}, Argb(run.color_value));
      return;
    }
    paint.DrawRect(background, theme_.inlay_hint_bg_color, CornerRadii(height * 0.2F));
    if (run.icon_id != 0) {
      const float side = std::min(height, std::max(0.0F, background.width - run.padding * 2.0F));
      const Rect icon{background.x + (background.width - side) * 0.5F, top + (height - side) * 0.5F, side, side};
      if (const auto asset = icon_provider_ ? icon_provider_(run.icon_id) : std::nullopt) paint.DrawImage(*asset, icon);
      else paint.DrawCircle({icon.x + side * 0.5F, icon.y + side * 0.5F}, side * 0.25F, theme_.inlay_hint_icon_color);
    } else if (!run.text.empty()) {
      const float text_x = background.x + run.padding;
      const TextStyle style{text_measurer_->InlayHintFont(), theme_.inlay_hint_text_color, TextDecoration::None};
      paint.DrawTextRun(Rect{text_x, top, background.width - run.padding * 2.0F, height},
                        Point{text_x, run.y}, Utf16ToUtf8(run.text), style);
    }
  }

  void DrawFoldPlaceholder(PaintContext& paint, const se::VisualRun& run, float top, float height) {
    const float margin = run.margin;
    const Rect background{run.x + margin, top, run.width - margin * 2.0F, height};
    paint.DrawRect(background, theme_.fold_placeholder_bg_color, CornerRadii(height * 0.2F));
    if (!run.text.empty()) {
      const float text_x = run.x + margin + run.padding;
      const TextStyle style{ContentFont(), theme_.fold_placeholder_text_color, TextDecoration::None};
      paint.DrawTextRun(Rect{text_x, top, run.width, height}, Point{text_x, run.y}, Utf16ToUtf8(run.text), style);
    }
  }

  void DrawGutterIcon(PaintContext& paint, const se::GutterIconRenderItem& icon) {
    const Rect rect = ToRect(icon.rect);
    const Point center{rect.x + rect.width * 0.5F, rect.y + rect.height * 0.5F};
    if (icon_provider_) {
      if (const auto asset = icon_provider_(icon.icon_id)) {
        paint.DrawImage(*asset, rect);
        return;
      }
    }
    // icon_id drives the shape: 1 = type marker (diamond), 2 = execution point
    // (filled circle), anything else = plain dot. The demo emits 1 for
    // class/struct rows; hosts may reuse the ids for breakpoints/errors.
    if (icon.icon_id == 1) {
      Path diamond;
      diamond.MoveTo(Point{center.x, rect.y});
      diamond.LineTo(Point{rect.x + rect.width, center.y});
      diamond.LineTo(Point{center.x, rect.y + rect.height});
      diamond.LineTo(Point{rect.x, center.y});
      diamond.Close();
      paint.StrokePath(diamond, theme_.gutter_icon_color, StrokeStyle{1.5F});
      return;
    }
    if (icon.icon_id == 2) {
      paint.DrawCircle(center, rect.width * 0.5F, theme_.gutter_icon_color);
      return;
    }
    paint.DrawCircle(center, rect.width * 0.35F, theme_.gutter_icon_color);
  }

  void DrawFoldMarker(PaintContext& paint, const se::FoldMarkerRenderItem& marker, Color color) {
    const Rect rect = ToRect(marker.rect);
    if (marker.fold_state == se::FoldState::NONE || rect.width <= 0.0F || rect.height <= 0.0F) return;

    const Point center{rect.x + rect.width * 0.5F, rect.y + rect.height * 0.5F};
    const float half_size = std::min(rect.width, rect.height) * 0.28F;
    Path path;
    if (marker.fold_state == se::FoldState::COLLAPSED) {
      path.MoveTo({center.x - half_size * 0.5F, center.y - half_size})
          .LineTo({center.x + half_size * 0.5F, center.y})
          .LineTo({center.x - half_size * 0.5F, center.y + half_size});
    } else {
      path.MoveTo({center.x - half_size, center.y - half_size * 0.5F})
          .LineTo({center.x, center.y + half_size * 0.5F})
          .LineTo({center.x + half_size, center.y - half_size * 0.5F});
    }
    paint.StrokePath(path, color, StrokeStyle{.width = std::max(1.0F, rect.height * 0.1F),
                                             .cap = StrokeCap::Round, .join = StrokeJoin::Round});
  }

  void DrawUnderline(
      PaintContext& paint, const se::Rect& rect, int32_t color, se::RangeEffectUnderlineStyle style
  ) {
    const float start = rect.origin.x;
    const float end = start + rect.width;
    const float y = rect.origin.y + rect.height - 1.0F;
    if (start >= end) return;

    StrokeStyle stroke{.width = 2.0F};
    if (style == se::RangeEffectUnderlineStyle::WAVY) {
      constexpr float kHalfWave = 7.0F;
      constexpr float kControlOffset = 3.5F;
      Path path;
      path.MoveTo({start, y});
      float control_offset = -kControlOffset;
      for (float x = start; x < end; x += kHalfWave) {
        const float next = std::min(x + kHalfWave, end);
        path.QuadraticTo({(x + next) * 0.5F, y + control_offset}, {next, y});
        control_offset = -control_offset;
      }
      paint.StrokePath(path, Argb(color), stroke);
      return;
    }
    if (style == se::RangeEffectUnderlineStyle::DASHED) {
      stroke.dash_pattern = {3.0F, 2.0F};
    }
    paint.DrawLine({start, y}, {end, y}, Argb(color), stroke);
  }

  void DrawGuideSegment(PaintContext& paint, const se::GuideSegment& segment) {
    const Color color = segment.type == se::GuideType::SEPARATOR ? theme_.separator_line_color : theme_.guide_color;
    StrokeStyle stroke{.width = segment.type == se::GuideType::INDENT ? 1.0F : 1.2F};
    const Point start{segment.start.x, segment.start.y};
    const Point end{segment.end.x, segment.end.y};

    if (segment.style == se::GuideStyle::DOUBLE) {
      constexpr float kOffset = 1.5F;
      const Point offset = segment.direction == se::GuideDirection::HORIZONTAL ? Point{0.0F, kOffset} : Point{kOffset, 0.0F};
      paint.DrawLine(start - offset, end - offset, color, stroke);
      paint.DrawLine(start + offset, end + offset, color, stroke);
      return;
    }
    if (segment.style == se::GuideStyle::DASHED) {
      stroke.dash_pattern = {6.0F, 4.0F};
    }
    if (!segment.arrow_end) {
      paint.DrawLine(start, end, color, stroke);
      return;
    }

    const Point delta = end - start;
    const float length = std::hypot(delta.x, delta.y);
    if (length < 1.0F) return;

    constexpr float kArrowLength = 9.0F;
    constexpr float kArrowAngle = 28.0F * 3.14159265F / 180.0F;
    const float arrow_length = std::min(kArrowLength, length);
    const Point direction = delta * (1.0F / length);
    const float depth = arrow_length * std::cos(kArrowAngle);
    const Point base = end - direction * depth;
    const Point wing = Point{-direction.y, direction.x} * (arrow_length * std::sin(kArrowAngle));
    if (length > depth) paint.DrawLine(start, base, color, stroke);
    Path arrow;
    arrow.MoveTo(end).LineTo(base + wing).LineTo(base - wing).Close();
    paint.FillPath(arrow, color);
  }

  // ---- Code completion ----------------------------------------------------

  void MoveCompletionSelection(int delta) {
    if (!completion_.items) return;
    const auto last = completion_.items->size() - 1;
    const auto current = completion_.selected;
    if ((delta < 0 && current == 0) || (delta > 0 && current == last)) return;
    completion_.selected = delta < 0 ? current - 1 : current + 1;
    InvalidateCompletionPopup();
  }

  void ResetCompletionRequest() {
    ++completion_generation_;
    for (const auto& task : completion_tasks_) task.Cancel();
    completion_tasks_.clear();
    completion_results_.clear();
    completion_ = {};
  }

  void InvalidateCompletionPopup() {
    completion_popup_dirty_ = true;
    Invalidate();
  }

  void DismissCompletionPopup() {
    if (const auto shown = std::exchange(completion_presentation_, std::nullopt)) {
      popup_.Dismiss(shown->layer);
    }
  }

  void UpdateCompletionPopup() {
    if (!completion_.items) {
      DismissCompletionPopup();
      return;
    }
    if (viewport_.width <= 0.0F || viewport_.height <= 0.0F) return;
    const Rect viewport{0.0F, 0.0F, viewport_.width, viewport_.height};
    const Rect anchor = CursorRect().Intersection(viewport);
    const float vertical_padding = 2.0F * kCompletionPanelPaddingV;
    const float below = viewport.height - anchor.y - anchor.height;
    const float available = std::max(anchor.y, below) - kCompletionGap - kCompletionFramePadding - vertical_padding;
    const float rows = std::min(static_cast<float>(std::min(completion_.items->size(), kCompletionMaxVisible)),
                                std::max(1.0F, std::floor(available / kCompletionRowHeight)));
    const Size size{
        std::min(kCompletionPanelWidth, viewport.width - 2.0F * kCompletionFramePadding),
        std::min(rows * kCompletionRowHeight + vertical_padding, viewport.height - 2.0F * kCompletionFramePadding),
    };
    if (anchor.IsEmpty() || size.width <= 2.0F * kCompletionPanelPaddingH + kCompletionScrollbarSpace ||
        size.height <= vertical_padding) {
      DismissCompletion();
      return;
    }

    auto& shown = completion_presentation_;
    if (shown && shown->anchor != anchor) {
      if (popup_.UpdateAnchor(shown->layer, anchor)) {
        shown->anchor = anchor;
      } else {
        shown.reset();
      }
    }
    if (shown && !completion_popup_dirty_ && shown->size == size) return;

    const auto weak = weak_from_this();
    const auto items = completion_.items;
    const CompletionPopupContent content{
        .items = items,
        .selected = completion_.selected,
        .renderer = completion_renderer_,
        .colors = {theme_.completion_bg_color, theme_.completion_border_color, theme_.completion_selected_bg_color,
                   theme_.completion_label_color, theme_.completion_detail_color, theme_.scrollbar_track_color,
                   theme_.scrollbar_thumb_color},
        .size = size,
        .accept = [weak, items](std::size_t index) {
          const auto editor = weak.lock();
          if (editor && editor->active_ && editor->completion_.items == items) {
            editor->completion_.selected = index;
            editor->ConfirmCompletion();
          }
        },
    };
    ViewFactory factory = [content] { return CompletionPopupView(content); };
    if (shown && popup_.Update(shown->layer, factory)) {
      shown->size = size;
    } else {
      const auto layer = popup_.ShowAtAnchor(editor_anchor_, anchor, std::move(factory), {
          .gap = kCompletionGap,
          .viewport_margin = kCompletionFramePadding,
          .retain_anchor_focus = true,
          .on_dismiss_request = [weak] {
            if (const auto editor = weak.lock()) editor->DismissCompletion();
          },
      });
      shown = CompletionPresentation{layer, anchor, size};
    }
    completion_popup_dirty_ = false;
  }

  std::shared_ptr<HuxeruiTextMeasurer> text_measurer_;
  std::shared_ptr<se::EditorCore> core_;
  Document document_handle_;
  std::shared_ptr<se::Document> document_;
  TextMeasurer& measurer_;
  TaskScope tasks_;
  MenuHandle menu_;
  PopupHandle popup_;
  ViewHandle editor_anchor_;
  std::shared_ptr<Clipboard> clipboard_;
  EventEmitter events_;
  EditorController controller_;
  std::function<void()> invalidate_;
  std::function<void()> invalidate_semantics_;
  std::function<void(PointerCursorKind)> set_cursor_;
  std::function<void()> show_keyboard_;
  std::function<Point(Point)> local_to_window_;
  std::function<std::optional<Point>(Point)> window_to_local_;
  std::shared_ptr<detail::TextInputBridge> text_input_;

  EditorSettings settings_;
  EditorTheme theme_;
  EditorKeyMap key_map_;
  std::optional<LanguageConfiguration> language_;
  EditorMetadata metadata_;
  EditorIconProvider icon_provider_;
  std::optional<std::string> diff_original_;
  std::uint64_t document_generation_ = 1;
  std::uint64_t edit_generation_ = 0;
  std::uint64_t previous_requested_revision_ = 0;
  bool active_ = false;
  bool defer_events_ = false;
  bool document_event_pending_ = false;
  std::vector<se::EditorActionResult> pending_events_;

  std::vector<std::shared_ptr<DecorationProvider>> decoration_providers_;
  std::vector<std::set<DecorationType>> decoration_capabilities_;
  std::vector<TaskHandle> decoration_tasks_;
  std::vector<DecorationReceiver> decoration_receivers_;
  std::optional<detail::DecorationBatch> decoration_batch_;
  std::uint64_t decoration_generation_ = 0;
  bool decoration_apply_posted_ = false;
  bool decorations_pending_ = false;
  std::shared_ptr<const std::string> snapshot_;
  std::vector<TextChange> pending_changes_;
  IntRange last_visible_range_;
  double last_decoration_time_ = -1.0;

  struct CompletionState {
    bool requested = false;
    bool incomplete = false;
    std::size_t pending = 0;
    std::shared_ptr<const std::vector<CompletionItem>> items;
    std::size_t selected = 0;
  };
  struct CompletionPresentation {
    LayerId layer;
    Rect anchor;
    Size size;
  };
  CompletionState completion_;
  std::optional<CompletionPresentation> completion_presentation_;
  bool completion_popup_dirty_ = false;
  std::vector<std::shared_ptr<CompletionProvider>> completion_providers_;
  std::vector<CompletionResult> completion_results_;
  std::vector<TaskHandle> completion_tasks_;
  CompletionItemRenderer completion_renderer_;
  std::uint64_t completion_generation_ = 0;
  std::vector<std::shared_ptr<NewLineActionProvider>> newline_providers_;

  std::optional<InlineSuggestion> inline_suggestion_;
  std::optional<InlineSuggestion> pending_inline_dismissal_;
  std::optional<Rect> inline_rect_;
  std::optional<LayerId> context_menu_layer_;
  ContextMenuItemProvider context_menu_provider_;
  std::optional<LayerId> selection_menu_layer_;
  SelectionMenuItemProvider selection_menu_provider_;
  SelectionMenuPhase selection_menu_phase_ = SelectionMenuPhase::Hidden;
  double selection_menu_deadline_ = 0.0;

  std::vector<std::pair<std::int64_t, se::PointF>> active_pointers_;
  KeyModifier modifiers_ = KeyModifier::NONE;
  bool last_pointer_touch_ = false;
  bool mouse_pointer_active_ = false;
  bool viewport_in_motion_ = false;
  se::Size viewport_;
  se::EditorRenderModel cached_model_;
  bool model_dirty_ = true;
  FontMetrics font_metrics_;
  FontMetrics inlay_hint_metrics_;
  float char_width_ = 1.0F;
  bool focused_ = false;
  bool blink_on_ = true;
  double frame_time_ = 0.0;
  double blink_deadline_ = 0.0;
  bool animation_ = false;
  double animation_delay_ = 0.0;
  double animation_deadline_ = 0.0;
};


struct EditorControllerState {
  enum class Phase { Unbound, Attached, Active, Detached };
  Phase phase = Phase::Unbound;
  std::weak_ptr<EditorSession> editor;

  template <class F>
  auto Invoke(F&& action) const -> std::invoke_result_t<F, EditorSession&> {
    if (phase == Phase::Active) {
      if (const auto mounted = editor.lock()) return std::invoke(std::forward<F>(action), *mounted);
    }
    return {};
  }
};

struct EditorControllerAccess {
  static const std::shared_ptr<EditorControllerState>& State(const EditorController& controller) {
    return controller.state_;
  }

  static void Attach(const EditorController& controller, const std::shared_ptr<EditorSession>& editor) {
    auto& state = *State(controller);
    if (state.phase != EditorControllerState::Phase::Unbound) {
      throw std::logic_error("A CodeEditor controller can only be attached once");
    }
    state.editor = editor;
    state.phase = EditorControllerState::Phase::Attached;
  }

  static void Activate(const EditorController& controller, EditorSession& editor) {
    auto& state = *State(controller);
    if (state.phase != EditorControllerState::Phase::Attached) return;
    state.phase = EditorControllerState::Phase::Active;
    editor.Activate();
  }

  static void Detach(const EditorController& controller) {
    auto& state = *State(controller);
    state.phase = EditorControllerState::Phase::Detached;
    state.editor.reset();
  }
};

}  // namespace detail

using detail::EditorSession;

EditorController::EditorController() : state_(std::make_shared<detail::EditorControllerState>()) {}

EditorController UseCodeEditor(const std::source_location& location) {
  return UseState(EditorController{}, location).Get();
}

EditorSettingsHandle::EditorSettingsHandle(std::weak_ptr<detail::EditorControllerState> state)
    : state_(std::move(state)) {}

std::optional<EditorSettings> EditorSettingsHandle::Get() const {
  const auto state = state_.lock();
  return state ? state->Invoke([](EditorSession& editor) { return std::optional(editor.Settings()); }) : std::nullopt;
}

bool EditorSettingsHandle::Apply(EditorSettings settings) const {
  const auto state = state_.lock();
  return state && state->Invoke([&](EditorSession& editor) { return editor.ApplySettings(settings); });
}

std::optional<EditorSettingsHandle> EditorController::GetSettings() const {
  return state_->Invoke([this](EditorSession&) { return std::optional(EditorSettingsHandle(state_)); });
}

namespace {

template <class T>
auto LineEntries(LineDecorations<T> values) {
  std::vector<std::pair<std::size_t, std::vector<T>>> entries;
  entries.reserve(values.size());
  for (auto& [line, items] : values) entries.emplace_back(line, std::move(items));
  return entries;
}

}  // namespace

bool EditorController::LoadDocument(Document document) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.LoadDocument(std::move(document)); });
}

bool EditorController::ApplyTheme(EditorTheme theme) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.ApplyTheme(std::move(theme)); });
}

bool EditorController::SetKeyMap(EditorKeyMap key_map) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.SetKeyMap(std::move(key_map)); });
}

bool EditorController::SetEditorIconProvider(EditorIconProvider provider) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.SetIconProvider(std::move(provider)); });
}

bool EditorController::CopyToClipboard() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Copy(); });
}

bool EditorController::PasteFromClipboard() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Paste(); });
}

bool EditorController::CutToClipboard() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Cut(); });
}

bool EditorController::SetLanguageConfiguration(std::optional<LanguageConfiguration> configuration) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.SetLanguage(std::move(configuration)); });
}

bool EditorController::SetMetadata(EditorMetadata metadata) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.SetMetadata(std::move(metadata)); });
}

bool EditorController::AddDecorationProvider(std::shared_ptr<DecorationProvider> provider) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.AddDecorationProvider(std::move(provider)); });
}

bool EditorController::RemoveDecorationProvider(const std::shared_ptr<DecorationProvider>& provider) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.RemoveDecorationProvider(provider); });
}

bool EditorController::RequestDecorationRefresh() const {
  return state_->Invoke([](EditorSession& editor) { return editor.RequestDecorations(); });
}

bool EditorController::AddCompletionProvider(std::shared_ptr<CompletionProvider> provider) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.AddCompletionProvider(std::move(provider)); });
}

bool EditorController::RemoveCompletionProvider(const std::shared_ptr<CompletionProvider>& provider) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.RemoveCompletionProvider(provider); });
}

bool EditorController::AddNewLineActionProvider(std::shared_ptr<NewLineActionProvider> provider) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.AddNewLineProvider(std::move(provider)); });
}

bool EditorController::RemoveNewLineActionProvider(const std::shared_ptr<NewLineActionProvider>& provider) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.RemoveNewLineProvider(provider); });
}

bool EditorController::TriggerCompletion() const {
  return state_->Invoke([](EditorSession& editor) { return editor.TriggerCompletion(); });
}

bool EditorController::ShowCompletionItems(std::vector<CompletionItem> items) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.ShowCompletionItems(std::move(items)); });
}

bool EditorController::DismissCompletion() const {
  return state_->Invoke([](EditorSession& editor) { return editor.DismissCompletion(); });
}

bool EditorController::SetCompletionItemRenderer(CompletionItemRenderer renderer) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.SetCompletionRenderer(std::move(renderer)); });
}

bool EditorController::SetContextMenuItemProvider(ContextMenuItemProvider provider) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.SetContextMenuProvider(std::move(provider)); });
}

bool EditorController::SetSelectionMenuItemProvider(SelectionMenuItemProvider provider) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.SetSelectionMenuProvider(std::move(provider)); });
}

bool EditorController::ShowInlineSuggestion(InlineSuggestion suggestion) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.ShowInlineSuggestion(std::move(suggestion)); });
}

bool EditorController::DismissInlineSuggestion() const {
  return state_->Invoke([](EditorSession& editor) { return editor.DismissInlineSuggestion(); });
}

bool EditorController::IsInlineSuggestionShowing() const {
  return state_->Invoke([](EditorSession& editor) { return editor.IsInlineSuggestionShowing(); });
}

bool EditorController::SetDiffOriginal(std::optional<std::string> original) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.SetDiffOriginal(std::move(original)); });
}

bool EditorController::InsertText(std::string text) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().insertText(text)); });
}

bool EditorController::InsertTextAt(TextPosition position, std::string text) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().replaceText({position, position}, text)); });
}

bool EditorController::ReplaceText(TextRange range, std::string text) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().replaceText(range, text)); });
}

bool EditorController::DeleteText(TextRange range) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().deleteText(range)); });
}

bool EditorController::ApplyTextEdits(std::vector<TextEdit> edits) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().applyTextEdits(std::move(edits))); });
}

bool EditorController::MoveLineUp() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().moveLineUp()); });
}

bool EditorController::MoveLineDown() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().moveLineDown()); });
}

bool EditorController::CopyLineUp() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().copyLineUp()); });
}

bool EditorController::CopyLineDown() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().copyLineDown()); });
}

bool EditorController::DeleteLine() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().deleteLine()); });
}

bool EditorController::InsertLineAbove() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().insertLineAbove()); });
}

bool EditorController::InsertLineBelow() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().insertLineBelow()); });
}

bool EditorController::Undo() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().undo()); });
}

bool EditorController::Redo() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().redo()); });
}

bool EditorController::SelectAll() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().selectAll()); });
}

bool EditorController::FoldAll() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().foldAll()); });
}

bool EditorController::UnfoldAll() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().unfoldAll()); });
}

bool EditorController::FindNextSearchMatch() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().findNextSearchMatch()); });
}

bool EditorController::FindPreviousSearchMatch() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().findPreviousSearchMatch()); });
}

bool EditorController::ClearSearch() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().clearSearch()); });
}

bool EditorController::ClearInlayHints() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().clearInlayHints()); });
}

bool EditorController::ClearPhantomTexts() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().clearPhantomTexts()); });
}

bool EditorController::ClearGutterIcons() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().clearGutterIcons()); });
}

bool EditorController::ClearCodeLens() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().clearCodeLens()); });
}

bool EditorController::ClearLinks() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().clearLinks()); });
}

bool EditorController::ClearGuides() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().clearGuides()); });
}

bool EditorController::ClearDiagnostics() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().clearDiagnostics()); });
}

bool EditorController::ClearDocumentHighlights() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().clearDocumentHighlights()); });
}

bool EditorController::ClearAllDecorations() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().clearAllDecorations()); });
}

bool EditorController::CancelLinkedEditing() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().cancelLinkedEditing()); });
}

bool EditorController::SetSelection(TextRange range) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().setSelection(range)); });
}

bool EditorController::SetCursorPosition(TextPosition position) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().setCursorPosition(position)); });
}

bool EditorController::GotoPosition(TextPosition position) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().gotoPosition(position.line, position.column)); });
}

bool EditorController::ScrollToLine(std::size_t line, ScrollBehavior behavior) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().scrollToLine(line, behavior)); });
}

bool EditorController::ToggleFold(std::size_t line) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().toggleFoldAt(line)); });
}

bool EditorController::FoldAt(std::size_t line) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().foldAt(line)); });
}

bool EditorController::UnfoldAt(std::size_t line) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().unfoldAt(line)); });
}

bool EditorController::Search(SearchRequest request) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().search(request)); });
}

bool EditorController::ReplaceCurrentSearchMatch(std::string replacement) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().replaceCurrentSearchMatch(replacement)); });
}

bool EditorController::ReplaceAllSearchMatches(std::string replacement) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().replaceAllSearchMatches(replacement)); });
}

bool EditorController::RegisterTextStyle(std::uint32_t id, EditorTextStyle style) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().registerTextStyle(id, ToCoreStyle(style))); });
}

bool EditorController::SetLineSpans(std::size_t line, SpanLayer layer, std::vector<StyleSpan> spans) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().setLineSpans(line, layer, std::move(spans))); });
}

bool EditorController::SetBatchLineSpans(SpanLayer layer, LineDecorations<StyleSpan> spans) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().setBatchLineSpans(layer, LineEntries(std::move(spans)))); });
}

bool EditorController::SetLineInlayHints(std::size_t line, std::vector<InlayHint> values) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().setLineInlayHints(line, std::move(values))); });
}

bool EditorController::SetBatchLineInlayHints(LineDecorations<InlayHint> values) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().setBatchLineInlayHints(LineEntries(std::move(values)))); });
}

bool EditorController::SetLinePhantomTexts(std::size_t line, std::vector<PhantomText> values) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().setLinePhantomTexts(line, std::move(values))); });
}

bool EditorController::SetBatchLinePhantomTexts(LineDecorations<PhantomText> values) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().setBatchLinePhantomTexts(LineEntries(std::move(values)))); });
}

bool EditorController::SetLineGutterIcons(std::size_t line, std::vector<GutterIcon> values) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().setLineGutterIcons(line, std::move(values))); });
}

bool EditorController::SetBatchLineGutterIcons(LineDecorations<GutterIcon> values) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().setBatchLineGutterIcons(LineEntries(std::move(values)))); });
}

bool EditorController::SetLineCodeLens(std::size_t line, std::vector<CodeLensItem> values) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().setLineCodeLens(line, std::move(values))); });
}

bool EditorController::SetBatchLineCodeLens(LineDecorations<CodeLensItem> values) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().setBatchLineCodeLens(LineEntries(std::move(values)))); });
}

bool EditorController::SetLineLinks(std::size_t line, std::vector<LinkSpan> values) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().setLineLinks(line, std::move(values))); });
}

bool EditorController::SetBatchLineLinks(LineDecorations<LinkSpan> values) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().setBatchLineLinks(LineEntries(std::move(values)))); });
}

bool EditorController::SetLineDiagnostics(std::size_t line, std::vector<Diagnostic> values) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().setLineDiagnostics(line, std::move(values))); });
}

bool EditorController::SetBatchLineDiagnostics(LineDecorations<Diagnostic> values) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().setBatchLineDiagnostics(LineEntries(std::move(values)))); });
}

bool EditorController::SetLineDocumentHighlights(std::size_t line, std::vector<DocumentHighlight> values) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().setLineDocumentHighlights(line, std::move(values))); });
}

bool EditorController::SetBatchLineDocumentHighlights(LineDecorations<DocumentHighlight> values) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().setBatchLineDocumentHighlights(LineEntries(std::move(values)))); });
}

bool EditorController::SetIndentGuides(std::vector<IndentGuide> values) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().setIndentGuides(std::move(values))); });
}

bool EditorController::SetBracketGuides(std::vector<BracketGuide> values) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().setBracketGuides(std::move(values))); });
}

bool EditorController::SetFlowGuides(std::vector<FlowGuide> values) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().setFlowGuides(std::move(values))); });
}

bool EditorController::SetSeparatorGuides(std::vector<SeparatorGuide> values) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().setSeparatorGuides(std::move(values))); });
}

bool EditorController::SetFoldRegions(std::vector<FoldRegion> values) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().setFoldRegions(std::move(values))); });
}

bool EditorController::InsertSnippet(std::string snippet) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().insertSnippet(snippet)); });
}

bool EditorController::StartLinkedEditing(std::vector<TabStopGroup> groups) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Dispatch(editor.Core().startLinkedEditing(std::move(groups))); });
}

bool EditorController::LinkedEditingNext() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().linkedEditingNextTabStop()); });
}

bool EditorController::LinkedEditingPrevious() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Dispatch(editor.Core().linkedEditingPrevTabStop()); });
}

std::optional<Document> EditorController::GetDocument() const {
  return state_->Invoke([](EditorSession& editor) { return std::optional(editor.CurrentDocument()); });
}

std::optional<EditorTheme> EditorController::GetTheme() const {
  return state_->Invoke([](EditorSession& editor) { return std::optional(editor.Theme()); });
}

std::optional<EditorKeyMap> EditorController::GetKeyMap() const {
  return state_->Invoke([](EditorSession& editor) { return std::optional(editor.KeyMap()); });
}

bool EditorController::CanUndo() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Core().canUndo(); });
}

bool EditorController::CanRedo() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Core().canRedo(); });
}

std::string EditorController::GetSelectedText() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Core().getSelectedText(); });
}

std::optional<TextRange> EditorController::GetSelection() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Selection(); });
}

std::optional<TextPosition> EditorController::GetCursorPosition() const {
  return state_->Invoke([](EditorSession& editor) { return std::optional(editor.Core().getCursorPosition()); });
}

std::optional<TextRange> EditorController::GetWordRangeAtCursor() const {
  return state_->Invoke([](EditorSession& editor) { return std::optional(editor.Core().getWordRangeAtCursor()); });
}

std::string EditorController::GetWordAtCursor() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Core().getWordAtCursor(); });
}

std::optional<ScrollMetrics> EditorController::GetScrollMetrics() const {
  return state_->Invoke([](EditorSession& editor) { return std::optional(editor.Core().getScrollMetrics()); });
}

std::optional<Rect> EditorController::GetPositionRect(TextPosition position) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.PositionRect(position); });
}

std::optional<Rect> EditorController::GetCursorRect() const {
  return state_->Invoke([](EditorSession& editor) { return std::optional(editor.CursorRect()); });
}

bool EditorController::IsLineVisible(std::size_t line) const {
  return state_->Invoke([&](EditorSession& editor) { return line < editor.CurrentDocument().GetLineCount() && editor.Core().isLineVisible(line); });
}

std::optional<SearchState> EditorController::GetSearchState() const {
  return state_->Invoke([](EditorSession& editor) { return std::optional(editor.Core().getSearchState()); });
}

std::optional<LanguageConfiguration> EditorController::GetLanguageConfiguration() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Language(); });
}

EditorMetadata EditorController::GetMetadata() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Metadata(); });
}

IntRange EditorController::GetVisibleLineRange() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Core().getVisibleLineRange(); });
}

std::size_t EditorController::GetTotalLineCount() const {
  return state_->Invoke([](EditorSession& editor) { return editor.CurrentDocument().GetLineCount(); });
}

std::string EditorController::GetLinkTargetAt(TextPosition position) const {
  return state_->Invoke([&](EditorSession& editor) { return editor.Core().getLinkTargetAt(position.line, position.column); });
}

bool EditorController::IsInLinkedEditing() const {
  return state_->Invoke([](EditorSession& editor) { return editor.Core().isInLinkedEditing(); });
}

bool EditorController::SetScroll(float x, float y) const {
  return state_->Invoke([&](EditorSession& editor) {
    if (!std::isfinite(x) || !std::isfinite(y)) throw std::invalid_argument("CodeEditor scroll offsets must be finite");
    return editor.Dispatch(editor.Core().setScroll(x, y));
  });
}

bool EditorController::RegisterBatchTextStyles(std::map<std::uint32_t, EditorTextStyle> styles) const {
  return state_->Invoke([&](EditorSession& editor) {
    std::vector<std::pair<std::uint32_t, se::TextStyle>> entries;
    entries.reserve(styles.size());
    for (const auto& [id, style] : styles) entries.emplace_back(id, ToCoreStyle(style));
    return editor.Dispatch(editor.Core().registerBatchTextStyles(std::move(entries)));
  });
}

bool EditorController::ClearHighlights(std::optional<SpanLayer> layer) const {
  return state_->Invoke([&](EditorSession& editor) {
    return editor.Dispatch(layer ? editor.Core().clearHighlights(*layer) : editor.Core().clearHighlights());
  });
}

namespace {

struct EditorBehavior {
  TextMeasurer* measurer;
  EditorOptions options;
  EditorController controller;
  EventEmitter events;
  TaskScope tasks;
  MenuHandle menu;
  PopupHandle popup;
  ViewHandle editor_anchor;
  std::shared_ptr<Clipboard> clipboard;
  State<PointerCursorKind> cursor;

  class Extension final : public NodeExtension {
   public:
    Extension(ViewNode& node, const EditorBehavior& behavior)
        : options_(behavior.options), controller_(behavior.controller),
          holder_(std::make_shared<EditorSession>(*behavior.measurer, options_, behavior.tasks, behavior.menu,
              behavior.popup, behavior.editor_anchor,
              behavior.clipboard, behavior.events, controller_,
              [this] { InvalidatePaint(); }, [this] { InvalidateSemantics(); },
              [cursor = behavior.cursor](PointerCursorKind kind) { if (cursor.Get() != kind) cursor = kind; },
              [this] { ShowKeyboard(); },
              [&node](Point point) { return node.LocalToWindow(point); },
              [&node](Point point) { return node.WindowToLocal(point); })) {
      holder_->Initialize(options_);
      detail::EditorControllerAccess::Attach(controller_, holder_);
    }

    ~Extension() override {
      detail::EditorControllerAccess::Detach(controller_);
      holder_->Close();
    }

    void Update(ViewNode&, const EditorBehavior& behavior) {
      if (controller_ != behavior.controller) throw std::logic_error("A mounted CodeEditor keeps its original controller");
      holder_->DeferEvents(true);
      holder_->SyncOptions(behavior.options, &options_);
      holder_->DeferEvents(false);
      options_ = behavior.options;
    }

    PaintInvalidation PrepareGeometry(ViewNode& node, TextMeasurer&) override {
      return holder_->PrepareGeometry(node.LayoutSize()) ? PaintInvalidation::Foreground : PaintInvalidation::None;
    }

    FrameResult OnFrame(ViewNode&, const FrameInfo& frame) override {
      detail::EditorControllerAccess::Activate(controller_, *holder_);
      return holder_->Frame(frame.timestamp);
    }

    PointerResult OnPointer(ViewNode&, const PointerEvent& event) override {
      const bool handled = holder_->HandlePointer(event);
      if (event.type == PointerEventType::Down &&
          (event.device_kind == PointerDeviceKind::Touch || event.changed_button != PointerButton::Secondary)) {
        return PointerResult::Capture;
      }
      return handled ? PointerResult::Handled : PointerResult::Ignored;
    }

    void OnHover(ViewNode&, const HoverEvent& event) override { holder_->Hover(event); }
    bool OnKey(ViewNode&, const KeyEvent& event) override {
      return !holder_->TextInput()->IsActive() && holder_->HandleKey(event);
    }
    void OnFocusChanged(ViewNode&, bool focused, bool) override { holder_->SetFocused(focused); }

    void PaintAboveContent(const ViewNode& node, PaintContext& paint) const override {
      const Size size = node.LayoutSize();
      paint.PushClip({0.0F, 0.0F, size.width, size.height});
      holder_->Render(paint, size);
      paint.PopClip();
    }

    std::shared_ptr<TextInputClient> GetTextInputClient() noexcept override { return holder_->TextInput(); }

    bool HitTest(ViewNode& node, Point point) const override {
      const auto size = node.LayoutSize();
      return point.x >= 0 && point.x < size.width && point.y >= 0 && point.y < size.height;
    }
    bool HoverHitTest(ViewNode& node, Point point) const override { return HitTest(node, point); }

    void BuildSemantics(SemanticBuilder& builder) const override {
      Semantics semantics;
      semantics.role = SemanticRole::TextField;
      semantics.value = holder_->CurrentDocument().GetText();
      semantics.read_only = holder_->Settings().read_only;
      builder.SetOwner(std::move(semantics));
    }

   private:
    EditorOptions options_;
    EditorController controller_;
    std::shared_ptr<EditorSession> holder_;
  };
};

[[huxerui::composable]]
View EditorView(EditorController controller, EditorOptions options) {
  EditorOptions resolved = options;
  if (!resolved.theme) resolved.theme = EditorTheme::Dark();
  const auto cursor = UseState(PointerCursorKind::Text);
  const auto editor_anchor = UseViewHandle();
  const auto state = detail::EditorControllerAccess::State(controller);
  return Canvas([](PaintContext&, Size) {})
      .With(EditorBehavior{&UseTextMeasurer(), std::move(resolved), controller, UseEvents(), UseTaskScope(), UseMenu(),
                           UsePopup(), editor_anchor, UseApplication().Clipboard(), cursor},
            editor_anchor, Focusable{}, PointerCursor(cursor.Get()))
      .On<ViewEvents::KeyIntercept>([state](const KeyEvent& event) {
        if (event.type == KeyEventType::Down && !event.modifiers.control && !event.modifiers.meta) return false;
        return state->Invoke([&](EditorSession& editor) { return editor.HandleKey(event); });
      })
      .On<ViewEvents::ContextMenuRequested>([state](Point point) {
        state->Invoke([&](EditorSession& editor) {
          editor.HandleContextMenu(point);
          return true;
        });
      })
      .On<ViewEvents::ScrollInput>([state](const ScrollInputEvent& event) {
        return state->Invoke([&](EditorSession& editor) { return editor.ScrollInput(event); });
      });
}

}  // namespace

View CodeEditor(EditorController controller, EditorOptions options) {
  EditorSession::ValidateSettings(options.settings);
  if (const auto* document = std::get_if<Document>(&options.document);
      document && !detail::DocumentAccess::Core(*document)) {
    throw std::invalid_argument("CodeEditor document handle is empty");
  }
  return EditorView(std::move(controller), std::move(options));
}

}  // namespace huxerui::codeeditor
