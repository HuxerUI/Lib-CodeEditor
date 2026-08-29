#include "sweet_editor.h"
#include "sweetline_highlighter.h"

#include <sweeteditor/decoration.h>
#include <sweeteditor/document.h>
#include <sweeteditor/editor_core.h>
#include <sweeteditor/visual.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sweetedit_huxer {
namespace {

namespace se = sweeteditor;
using namespace huxerui;

// ---- UTF-16 code units <-> UTF-8 -------------------------------------------
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

// ---- ARGB color ------------------------------------------------------------
Color Argb(int32_t argb) {
  const int a = (argb >> 24) & 0xFF;
  const int r = (argb >> 16) & 0xFF;
  const int g = (argb >> 8) & 0xFF;
  const int b = argb & 0xFF;
  return Color::Rgb(r, g, b, static_cast<float>(a) / 255.0F);
}

// ---- Font / style mapping (shared by the measurer and the renderer) --------
TextStyle MakeTextStyle(int32_t font_style, int32_t argb_color, float font_size) {
  Font font = Font::Monospace(font_size);
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

// ---- SweetEditor TextMeasurer bridge ---------------------------------------
class HuxeruiTextMeasurer final : public se::TextMeasurer {
public:
  explicit HuxeruiTextMeasurer(huxerui::TextMeasurer& measurer, float font_size)
      : measurer_(&measurer), font_size_(font_size) {}

  float measureWidth(const se::U16String& text, int32_t font_style) override {
    if (!measurer_) {
      return 0.0F;
    }
    return measurer_->MeasureRun(Utf16ToUtf8(text), MakeTextStyle(font_style, 0, font_size_)).advance;
  }

  float measureInlayHintWidth(const se::U16String& text) override {
    if (!measurer_) {
      return 0.0F;
    }
    // SweetEditor's reference renderer draws inlay hints in a sans-serif face at 90% of the text size.
    const huxerui::Font hint_font = huxerui::Font::System(font_size_ * 0.9F);
    return measurer_
        ->MeasureRun(
            Utf16ToUtf8(text), huxerui::TextStyle{hint_font, huxerui::Color::Black(), huxerui::TextDecoration::None}
        )
        .advance;
  }

  float measureIconWidth(int32_t) override {
    return 0.0F;
  }

  se::FontMetrics getFontMetrics() override {
    if (!measurer_) {
      return {0.0F, 0.0F};
    }
    const huxerui::FontMetrics metrics = measurer_->Metrics(huxerui::Font::Monospace(font_size_));
    return {-metrics.ascent, metrics.descent};
  }

private:
  huxerui::TextMeasurer* measurer_;
  float font_size_;
};

// ---- Input mapping ---------------------------------------------------------
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
  case Key::A:
    return se::KeyCode::A;
  case Key::C:
    return se::KeyCode::C;
  case Key::V:
    return se::KeyCode::V;
  case Key::X:
    return se::KeyCode::X;
  case Key::Y:
    return se::KeyCode::Y;
  case Key::Z:
    return se::KeyCode::Z;
  case Key::K:
    return se::KeyCode::K;
  default:
    return se::KeyCode::NONE;
  }
}

se::KeyEvent ToKeyEvent(const KeyEvent& event) {
  se::KeyEvent out;
  out.modifiers = ToModifiers(event.modifiers);
  // The web adapter only fills `text` for unmodified single-character input,
  // so a non-empty `text` always means a printable character.
  if (!event.text.empty()) {
    out.key_code = se::KeyCode::NONE;
    out.text = event.text;
  } else {
    out.key_code = ToKeyCode(event.key);
  }
  return out;
}

se::GestureEvent ToGestureEvent(const PointerEvent& event) {
  se::GestureEvent out;
  out.points.push_back(se::PointF{event.position.x, event.position.y});
  out.modifiers = se::KeyModifier::NONE;
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

se::GestureEvent ToWheelGestureEvent(const ScrollEvent& event) {
  se::GestureEvent out;
  out.type = se::EventType::MOUSE_WHEEL;
  out.wheel_delta_x = event.delta_x;
  out.wheel_delta_y = event.delta_y;
  return out;
}

// ---- Theme defaults --------------------------------------------------------
constexpr int32_t kEditorBackground = 0xFFFFFFFF;
constexpr int32_t kGutterBackground = 0xFFF2F3F5;
constexpr int32_t kLineNumberColor = 0xFF9AA0A6;
constexpr int32_t kCursorColor = 0xFF1F1F1F;
constexpr int32_t kSelectionBackground = 0x554A90E2;
constexpr int32_t kCurrentLineBackground = 0x0F000000;
constexpr int32_t kGuideColor = 0xFFC8C8C8;
constexpr int32_t kSeparatorColor = 0xFFB0B7C3;
constexpr int32_t kInlayHintBackground = 0x143B82F6;
constexpr int32_t kInlayHintText = 0xB0344A73;
constexpr int32_t kFoldPlaceholderBackground = 0x2E748DB0;
constexpr int32_t kFoldPlaceholderText = 0xFF284A70;

// Auto-closing bracket pairs enabled by default (reference platform ships
// with these); hosts may override via SweetEditorOptions.auto_closing_pairs.
const std::vector<std::pair<char32_t, char32_t>> kDefaultAutoClosingPairs = {
    {U'(', U')'},
    {U'{', U'}'},
    {U'[', U']'},
};
constexpr int32_t kGutterIconColor = 0xFF267F99;
constexpr float kCursorWidth = 2.0F;

// Completion panel, matching the SweetEditor reference light theme and
// dimensions (Android CompletionPopupController / EditorTheme.java).
constexpr int32_t kCompletionPanelBackground = 0xF0FAFBFD;
constexpr int32_t kCompletionPanelBorder = 0x30A0A8B8;
constexpr int32_t kCompletionSelectedBackground = 0x3D3B82F6;
constexpr int32_t kCompletionLabelColor = 0xFF1F2937;
constexpr int32_t kCompletionDetailColor = 0xFF8A94A6;
constexpr float kCompletionPanelWidth = 300.0F;
constexpr float kCompletionPanelMinWidth = 120.0F;
constexpr float kCompletionRowHeight = 32.0F;
constexpr float kCompletionPanelPaddingH = 4.0F;
constexpr float kCompletionPanelPaddingV = 6.0F;
constexpr float kCompletionRowPaddingH = 8.0F;
constexpr float kCompletionBadgeSize = 18.0F;
constexpr float kCompletionBadgeGap = 8.0F;
constexpr float kCompletionDetailGap = 8.0F;
constexpr float kCompletionGap = 4.0F;
constexpr float kCompletionFramePadding = 8.0F;
constexpr size_t kCompletionMaxVisible = 4;
constexpr float kCompletionLabelSize = 13.0F;
constexpr float kCompletionDetailSize = 11.0F;
constexpr float kCompletionBadgeSizePx = 10.0F;

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

// Baseline that centers text (ascent above, descent below) on a row center.
// DrawTextRun places its origin on the alphabetic baseline.
float CenterBaseline(float center_y, const FontMetrics& metrics) {
  return center_y + (metrics.ascent - metrics.descent) * 0.5F;
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

se::EditorRenderColors MakeRenderColors() {
  se::EditorRenderColors colors;
  colors.text_foreground = 0xFF1F1F1F;
  colors.link_foreground = 0xFF4C9DFF;
  colors.active_link_foreground = 0xFF4C9DFF;
  colors.codelens_foreground = 0xB0344A73;
  colors.active_codelens_foreground = 0xFF3A5FA0;
  colors.diff_added_line_background = 0x1FA6E22E;
  colors.diff_removed_line_background = 0x1FF92672;
  colors.diff_added_gutter_background = 0x2FA6E22E;
  colors.diff_removed_gutter_background = 0x2FF92672;
  return colors;
}

se::EditorRangeEffectStyles MakeRangeEffectStyles() {
  se::EditorRangeEffectStyles styles;
  styles.selection.background_color = kSelectionBackground;
  styles.search_match.background_color = 0x33F59E0B;
  styles.search_current.background_color = 0x55F59E0B;
  styles.bracket_match.background_color = 0x260F766E;
  styles.ime_composition.underline_color = 0xFF2563EB;
  styles.ime_composition.underline_style = se::RangeEffectUnderlineStyle::SOLID;

  // SweetEditor light-theme reference values (platform/Android EditorTheme.java).
  styles.document_highlight_text.background_color = 0x142563EB;
  styles.document_highlight_read.background_color = 0x1C2563EB;
  styles.document_highlight_write.background_color = 0x282563EB;
  styles.diagnostic_error.underline_color = 0xFFDC2626;
  styles.diagnostic_error.underline_style = se::RangeEffectUnderlineStyle::WAVY;
  styles.diagnostic_warning.underline_color = 0xFFD97706;
  styles.diagnostic_warning.underline_style = se::RangeEffectUnderlineStyle::WAVY;
  styles.diagnostic_info.underline_color = 0xFF0EA5E9;
  styles.diagnostic_info.underline_style = se::RangeEffectUnderlineStyle::WAVY;
  styles.diagnostic_hint.underline_color = 0xFF64748B;
  styles.diagnostic_hint.underline_style = se::RangeEffectUnderlineStyle::WAVY;
  return styles;
}


// ---- Text input bridge (HuxerUI IME session -> SweetEditor core) -----------
class SweetEditorTextInputClient final : public TextInputClient, public TextSelectionClient {
public:
  SweetEditorTextInputClient(
      std::shared_ptr<se::EditorCore> core,
      std::shared_ptr<se::Document> document,
      std::shared_ptr<SweetLineHighlighter> highlighter,
      std::function<void()> on_change,
      std::function<void(const std::vector<se::TextChange>&)> on_text_committed,
      std::function<void()> on_edit,
      std::function<bool()> on_tab,
      bool read_only
  )
      : core_(std::move(core)),
        document_(std::move(document)),
        highlighter_(std::move(highlighter)),
        on_change_(std::move(on_change)),
        on_text_committed_(std::move(on_text_committed)),
        on_edit_(std::move(on_edit)),
        on_tab_(std::move(on_tab)),
        read_only_(read_only) {}

  TextInputConfiguration Configuration() const override {
    TextInputConfiguration configuration;
    configuration.type = TextInputType::Text;
    configuration.capitalization = TextCapitalization::None;
    configuration.action = TextInputAction::Newline;
    configuration.multiline = true;
    configuration.secure = false;
    configuration.autocorrect = false;
    configuration.read_only = read_only_;
    return configuration;
  }

  TextInputState State() const override {
    std::optional<TextRange> composition;
    if (composition_start_ >= 0 && composition_end_ >= composition_start_) {
      composition = TextRange{composition_start_, composition_end_};
    }
    return {
        session_id_,
        revision_,
        content_revision_,
        Selection(),
        composition,
    };
  }

  TextInputState BeginTextInput(TextInputSessionId session_id) override {
    session_id_ = session_id;
    return State();
  }

  // The runtime requires revision_ to advance on every observable selection change
  // (for example, a click or drag that moves the caret), not only on text edits.
  void NotifySelectionChanged() {
    ++revision_;
  }

  void NotifyContentChanged(const std::vector<se::TextChange>& changes) {
    ++revision_;
    ++content_revision_;
    if (highlighter_) {
      highlighter_->ApplyChanges(*core_, changes);
      highlighter_->RefreshBracketMatch(*core_);
    }
    // Live diff: re-run the comparison after every edit while a diff original
    // is active so added/removed rows track typing. computeDiff emits no text
    // changes, only a redraw request (the hook invalidates the frame).
    if (on_diff_edit_) {
      on_diff_edit_();
    }
  }

  // Selection/content edits performed outside the normal input pipeline (word
  // selection, drag-handle extension, clipboard actions) must mark the render
  // model dirty and schedule a repaint.
  void NotifyEdit() {
    on_change_();
    if (on_edit_) {
      on_edit_();
    }
  }

  TextInputApplyResult ApplyTextInput(const TextInputCommandBatch& batch) override {
    if (batch.session_id != session_id_) {
      return {TextInputResultCode::SessionMismatch, TextInputSyncAction::None};
    }
    if (read_only_) {
      return {TextInputResultCode::ReadOnly, TextInputSyncAction::None};
    }

    bool changed = false;
    std::vector<se::TextChange> changes;
    for (const TextInputCommand& command : batch.commands) {
      changed = ApplyCommand(command, changes) || changed;
    }
    if (changed) {
      NotifyContentChanged(changes);
      NotifyEdit();
      if (on_text_committed_) {
        on_text_committed_(changes);
      }
    }
    return {TextInputResultCode::Ok, TextInputSyncAction::None};
  }

  TextInputContext QueryTextInputContext(TextInputSessionId session_id, TextOffset, TextOffset) const override {
    TextInputContext result;
    result.session_id = session_id;
    if (session_id != session_id_) {
      result.result_code = TextInputResultCode::SessionMismatch;
      return result;
    }
    const se::U16String text = document_->getU16Text();
    result.result_code = TextInputResultCode::Ok;
    result.slice_start = 0;
    result.total_length = static_cast<TextOffset>(text.size());
    result.text = document_->getU8Text();
    result.selection = Selection();
    if (composition_start_ >= 0 && composition_end_ >= composition_start_) {
      result.composition = TextRange{composition_start_, composition_end_};
    }
    return result;
  }

  TextInputGeometry QueryTextInputGeometry(TextInputSessionId session_id, TextRange) const override {
    TextInputGeometry result;
    result.session_id = session_id;
    if (session_id != session_id_) {
      result.result_code = TextInputResultCode::SessionMismatch;
      return result;
    }
    const se::CursorRect cursor = core_->getCursorScreenRect();
    result.result_code = TextInputResultCode::Ok;
    result.caret = Rect{cursor.x, cursor.y, kCursorWidth, cursor.height};
    return result;
  }

  TextInputPositionResult QueryTextInputPosition(TextInputSessionId session_id, Point point) const override {
    TextInputPositionResult result;
    result.session_id = session_id;
    if (session_id != session_id_ || document_ == nullptr) {
      result.result_code = TextInputResultCode::SessionMismatch;
      return result;
    }
    // Report the caret's document offset: the platform uses this to anchor
    // IME candidate windows. A precise screen->position hit test would need
    // the layout internals; the caret offset is what Android queries between
    // composition updates.
    const se::TextPosition cursor = core_->getCursorPosition();
    result.result_code = TextInputResultCode::Ok;
    result.position = {
        static_cast<TextOffset>(document_->getCharIndexFromPosition(cursor)),
        TextAffinity::Downstream,
    };
    return result;
  }

  TextInputKeyResult HandleTextKey(const KeyEvent& event) override {
    // Tab reaches the editor only through this hook: the runtime normally
    // reserves Tab for focus traversal, but gives a focused text input the
    // first chance. The holder decides between snippet tab-stop jumps,
    // phantom-text accept, and completion navigation; a plain indent Tab is
    // left to the core's key handling (which also runs here via on_tab_).
    if (event.type == KeyEventType::Down && event.key == Key::Tab && !event.repeat) {
      if (on_tab_ && on_tab_()) {
        return TextInputKeyResult::Handled;
      }
    }
    return TextInputKeyResult::Unhandled;
  }

  bool CanPerformTextEditingAction(TextEditingAction action, PlatformClipboard* clipboard) const override {
    switch (action) {
    case TextEditingAction::Cut:
    case TextEditingAction::Copy:
      return clipboard != nullptr && !core_->getSelection().isCollapsed();
    case TextEditingAction::Paste:
      return clipboard != nullptr && !read_only_;
    case TextEditingAction::SelectAll:
      return true;
    }
    return false;
  }

  bool PerformTextEditingAction(TextEditingAction action, PlatformClipboard* clipboard) override {
    if (clipboard != nullptr) {
      clipboard_ = clipboard;
    }
    if (clipboard == nullptr) {
      return false;
    }
    switch (action) {
    case TextEditingAction::Copy:
    case TextEditingAction::Cut: {
      const se::TextRange selection = core_->getSelection().normalized();
      if (selection.isCollapsed()) {
        return false;
      }
      const std::string text = document_->getU8Text(selection);
      if (!clipboard->WriteText(text)) {
        return false;
      }
      if (action == TextEditingAction::Cut) {
        if (read_only_) {
          return false;
        }
        const se::EditorActionResult result = core_->replaceText(selection, "");
        if (!result.text_changes.empty()) {
          NotifyContentChanged(result.text_changes);
          NotifyEdit();
        }
      }
      return true;
    }
    case TextEditingAction::Paste: {
      if (read_only_) {
        return false;
      }
      const std::optional<std::string> text = clipboard->ReadText();
      if (!text.has_value() || text->empty()) {
        return false;
      }
      const se::EditorActionResult result = core_->replaceText(core_->getSelection().normalized(), *text);
      if (!result.text_changes.empty()) {
        NotifyContentChanged(result.text_changes);
        NotifyEdit();
      }
      return true;
    }
    case TextEditingAction::SelectAll: {
      const se::EditorActionResult result = core_->selectAll();
      if (result.selection_changed || result.cursor_changed) {
        NotifySelectionChanged();
        NotifyEdit();
      }
      return true;
    }
    }
    return false;
  }

  // Context-menu clipboard actions; the clipboard pointer is cached from the
  // runtime's editing-action calls (the platform adapter outlives the session).
  bool CopySelection() {
    if (clipboard_ == nullptr || core_->getSelection().isCollapsed()) {
      return false;
    }
    return clipboard_->WriteText(document_->getU8Text(core_->getSelection().normalized()));
  }

  bool PasteFromClipboard() {
    if (clipboard_ == nullptr || read_only_) {
      return false;
    }
    const std::optional<std::string> text = clipboard_->ReadText();
    if (!text.has_value() || text->empty()) {
      return false;
    }
    const se::EditorActionResult result = core_->replaceText(core_->getSelection().normalized(), *text);
    if (!result.text_changes.empty()) {
      NotifyContentChanged(result.text_changes);
      NotifyEdit();
    }
    return true;
  }

  void SetDiffEditHook(std::function<void()> hook) {
    on_diff_edit_ = std::move(hook);
  }

  void EndTextInput(TextInputSessionId, TextInputEndReason) override {
    if (ime_session_open_) {
      core_->endImeSession(ime_session_id_);
      ime_session_open_ = false;
      composition_start_ = -1;
      composition_end_ = -1;
    }
  }

  // TextSelectionClient: the editor core owns pointer gestures, double-tap
  // word selection, and drag-handle extension through its gesture pipeline.
  // These runtime callbacks return false so the HuxerUI selection overlay
  // (handles + toolbar) stays hidden — the editor draws its own handles and
  // would otherwise render duplicates that ghost while dragging.
  bool SelectWord(Point) override {
    return false;
  }

  bool ExtendSelection(Point, bool) override {
    return false;
  }

  bool QuerySelectionGeometry(Rect&, Rect&) const override {
    return false;
  }

  Color SelectionHandleColor() const noexcept override {
    return Argb(0xFF1F6FEB);
  }

private:
  TextSelection Selection() const {
    const se::TextRange selection = core_->getSelection();
    return {
        static_cast<TextOffset>(document_->getCharIndexFromPosition(selection.start)),
        static_cast<TextOffset>(document_->getCharIndexFromPosition(selection.end)),
        TextAffinity::Downstream,
    };
  }

  // Opens the core IME session on demand (one session per HuxerUI session).
  bool EnsureImeSession() {
    if (ime_session_open_) {
      return true;
    }
    const se::ImeState state = core_->beginImeSession(se::ImeMutationModel::COMMAND);
    if (state.result_code != se::ImeResultCode::OK) {
      return false;
    }
    ime_session_id_ = state.session_id;
    ime_session_open_ = true;
    return true;
  }

  // Maps a UTF-16 document range to the core's IME coordinate space.
  se::ImeOffsetRange ToImeRange(TextOffset start, TextOffset end) {
    se::ImeOffsetRange range;
    range.coordinate_space = se::ImeCoordinateSpace::DOCUMENT;
    range.start_utf16 = static_cast<int64_t>(std::max<TextOffset>(0, start));
    range.end_utf16 = static_cast<int64_t>(std::max<TextOffset>(0, end));
    return range;
  }

  // Applies a core IME command batch; captures resulting text changes and the
  // active composition range (rendered with the IME underline style).
  bool SendImeCommands(std::vector<se::ImeCommand> commands, std::vector<se::TextChange>& changes) {
    if (!EnsureImeSession()) {
      return false;
    }
    se::ImeCommandBatch batch;
    batch.session_id = ime_session_id_;
    batch.commands = std::move(commands);
    const se::EditorActionResult result = core_->applyImeCommands(batch);
    if (!result.text_changes.empty()) {
      changes.insert(changes.end(), result.text_changes.begin(), result.text_changes.end());
    }
    TrackComposition(result.ime_state);
    return result.handled;
  }

  // Updates the local UTF-16 composition span from the core's IME state so
  // State() can report it to the platform (inline preview/underline).
  void TrackComposition(const se::ImeState& state) {
    const se::ImeOffsetRange& range = state.composition_range;
    if (range.start_utf16 < 0 || range.end_utf16 < range.start_utf16) {
      composition_start_ = -1;
      composition_end_ = -1;
      return;
    }
    composition_start_ = static_cast<TextOffset>(range.start_utf16);
    composition_end_ = static_cast<TextOffset>(range.end_utf16);
  }

  bool ApplyCommand(const TextInputCommand& command, std::vector<se::TextChange>& changes) {
    const auto capture = [&changes](const se::EditorActionResult& result) {
      changes.insert(changes.end(), result.text_changes.begin(), result.text_changes.end());
    };
    switch (command.kind) {
    case TextInputCommandKind::CommitText:
      if (!command.text.empty()) {
        capture(core_->insertText(command.text));
        return true;
      }
      return false;
    case TextInputCommandKind::SetSelection:
      if (command.selection_after.has_value()) {
        const TextSelection& selection = *command.selection_after;
        const se::TextPosition start =
            document_->getPositionFromCharIndex(static_cast<size_t>(std::max<TextOffset>(0, selection.anchor)));
        const se::TextPosition end =
            document_->getPositionFromCharIndex(static_cast<size_t>(std::max<TextOffset>(0, selection.active)));
        core_->setSelection({start, end});
        return true;
      }
      return false;
    case TextInputCommandKind::DeleteSurrounding: {
      bool changed = false;
      for (TextOffset index = 0; index < command.delete_before; ++index) {
        capture(core_->backspace());
        changed = true;
      }
      for (TextOffset index = 0; index < command.delete_after; ++index) {
        capture(core_->deleteForward());
        changed = true;
      }
      return changed;
    }
    case TextInputCommandKind::BeginComposition: {
      // Replaces the current selection with the composition text and marks the
      // range as an active composition (rendered with the IME underline style).
      if (command.text.empty()) {
        return false;
      }
      const TextSelection selection = Selection();
      const TextRange range = selection.Range();
      std::vector<se::ImeCommand> ime_commands;
      se::ImeCommand begin;
      begin.kind = se::ImeCommandKind::BEGIN_COMPOSITION;
      begin.target_range = ToImeRange(range.start, range.end);
      begin.text = command.text;
      ime_commands.push_back(std::move(begin));
      return SendImeCommands(std::move(ime_commands), changes);
    }
    case TextInputCommandKind::UpdateComposition: {
      if (command.text.empty()) {
        return false;
      }
      // Chinese/Japanese IMEs replace the whole composition string on each
      // step, so no target range is needed: the core treats the update as a
      // full-text replacement of the active composition.
      std::vector<se::ImeCommand> ime_commands;
      se::ImeCommand update;
      update.kind = se::ImeCommandKind::UPDATE_COMPOSITION;
      update.text = command.text;
      ime_commands.push_back(std::move(update));
      return SendImeCommands(std::move(ime_commands), changes);
    }
    case TextInputCommandKind::FinishComposition: {
      std::vector<se::ImeCommand> ime_commands;
      se::ImeCommand finish;
      finish.kind = se::ImeCommandKind::FINISH_COMPOSITION;
      if (!command.text.empty()) {
        finish.text = command.text;
      }
      ime_commands.push_back(std::move(finish));
      return SendImeCommands(std::move(ime_commands), changes);
    }
    case TextInputCommandKind::CancelComposition: {
      std::vector<se::ImeCommand> ime_commands;
      se::ImeCommand cancel;
      cancel.kind = se::ImeCommandKind::CANCEL_COMPOSITION;
      ime_commands.push_back(std::move(cancel));
      return SendImeCommands(std::move(ime_commands), changes);
    }
    }
    return false;
  }

  std::shared_ptr<se::EditorCore> core_;
  std::shared_ptr<se::Document> document_;
  std::shared_ptr<SweetLineHighlighter> highlighter_;
  std::function<void()> on_change_;
  std::function<void(const std::vector<se::TextChange>&)> on_text_committed_;
  std::function<void()> on_edit_;
  std::function<bool()> on_tab_;
  std::function<void()> on_diff_edit_;
  bool read_only_;
  PlatformClipboard* clipboard_{nullptr};
  TextInputSessionId session_id_ = 0;
  std::uint64_t revision_ = 0;
  std::uint64_t content_revision_ = 0;
  // SweetEditor core IME session id (distinct from the HuxerUI session id).
  uint64_t ime_session_id_{0};
  bool ime_session_open_{false};
  // Active composition span in UTF-16 document offsets (-1 when none).
  TextOffset composition_start_{-1};
  TextOffset composition_end_{-1};
};

// ---- Editor holder ---------------------------------------------------------
class EditorHolder {
public:
  EditorHolder(
      TextMeasurer& measurer,
      const SweetEditorOptions& options,
      std::string syntax_json,
      std::function<void()> invalidate
  )
      : font_size_(options.font_size),
        invalidate_(std::move(invalidate)),
        completion_provider_(options.completion_provider),
        completion_trigger_characters_(options.completion_trigger_characters) {
    font_metrics_ = measurer.Metrics(Font::Monospace(font_size_));
    char_width_ = measurer.MeasureRun("0", TextStyle{Font::Monospace(font_size_), Color::Black(), TextDecoration::None})
                      .advance;
    completion_label_metrics_ = measurer.Metrics(Font::System(kCompletionLabelSize));
    completion_detail_metrics_ = measurer.Metrics(Font::System(kCompletionDetailSize));
    const TextStyle badge_style{Font::System(kCompletionBadgeSizePx).WithWeight(FontWeight::Bold), Color::White(),
                                TextDecoration::None};
    completion_badge_metrics_ = measurer.Metrics(Font::System(kCompletionBadgeSizePx));
    completion_badge_char_advance_ = measurer.MeasureRun("W", badge_style).advance;

    se::EditorOptions core_options;
    core_ = std::make_shared<se::EditorCore>(
        std::make_shared<HuxeruiTextMeasurer>(measurer, font_size_), core_options);
    core_->setEditorRenderColors(MakeRenderColors());
    core_->setEditorRangeEffectStyles(MakeRangeEffectStyles());
    core_->setLineSpacing(options.line_spacing_add, options.line_spacing_mult);
    core_->setReadOnly(options.read_only);

    // Editing feel aligned with the reference platform: keep the previous
    // line's indent on newline, backspace unindents to the tab stop, Tab
    // inserts spaces up to the next stop, and typed opening brackets insert
    // their closing partner (core default bracket pairs () {} []).
    core_->setTabSize(options.tab_size > 0 ? options.tab_size : 4);
    core_->setAutoIndentMode(se::AutoIndentMode::KEEP_INDENT);
    core_->setBackspaceUnindent(options.backspace_unindent);
    core_->setInsertSpaces(options.insert_spaces);
    // The core disables auto-closing when the pair list is empty; the reference
    // platform ships with () {} [] enabled, so apply that default unless the
    // host explicitly configured a list.
    const std::vector<std::pair<char32_t, char32_t>>& closing_pairs =
        options.auto_closing_pairs.empty()
        ? kDefaultAutoClosingPairs
        : options.auto_closing_pairs;
    core_->setAutoClosingPairs(MakeBracketPairs(closing_pairs));
    core_->setRenderWhitespace(
        options.render_whitespace ? se::WhitespaceRenderMode::ALL : se::WhitespaceRenderMode::NONE
    );
    core_->setRenderLineBreaks(options.render_line_breaks);
    // Gutter icons share the line-number lane: with max_gutter_icons == 0 the
    // core reports overlay mode (icon replaces the line number); the reference
    // demo sets 1 so the icon renders beside the number.
    core_->setMaxGutterIcons(1);
    // Display options: wrap mode (0=NONE/1=CHAR_BREAK/2=WORD_BREAK) and sticky
    // gutter (line numbers stay fixed during horizontal scroll).
    current_wrap_mode_ = options.wrap_mode;
    current_sticky_gutter_ = options.sticky_gutter;
    if (options.wrap_mode >= 1 && options.wrap_mode <= 2) {
      core_->setWrapMode(
          options.wrap_mode == 1 ? se::WrapMode::CHAR_BREAK : se::WrapMode::WORD_BREAK
      );
    }
    core_->setGutterSticky(options.sticky_gutter);
    if (options.content_start_padding > 0.0F) {
      core_->setContentStartPadding(options.content_start_padding);
    }
    // Scrollbar customization (defaults are sensible; hosts may override).
    if (options.scrollbar_thickness > 0.0F || options.scrollbar_mode != 0) {
      se::ScrollbarConfig scrollbar;
      scrollbar.thickness = options.scrollbar_thickness > 0.0F
          ? options.scrollbar_thickness
          : 12.0F;
      scrollbar.mode = static_cast<se::ScrollbarMode>(std::clamp(options.scrollbar_mode, 0, 2));
      core_->setScrollbarConfig(scrollbar);
    }

    document_ = std::make_shared<se::LineArrayDocument>(se::U8String(options.initial_text));
    core_->loadDocument(document_);
    // Optional diff presentation against a provided original text (the demo
    // feeds each file's pristine content so edits light up added/removed rows).
    // The original is tracked so the component can switch the diff on/off
    // without rebuilding the holder (which would discard the live edits).
    current_diff_original_ = options.original_text;
    if (!current_diff_original_.empty()) {
      core_->computeDiff(current_diff_original_);
    }

    highlighter_ = std::make_shared<SweetLineHighlighter>(
        std::move(syntax_json), options.initial_text, options.document_key);
    highlighter_->RegisterStyles(*core_);
    highlighter_->SetHostDecorationProviders(options.decoration_providers);
    highlighter_->SetGutterIconProvider(options.gutter_icon_provider);
    // Publish fold regions once, right after the document loads, so fold
    // arrows are available across the whole file (not just the first viewport).
    highlighter_->PublishFoldRegions(*core_);

    text_input_client_ = std::make_shared<SweetEditorTextInputClient>(
        core_, document_, highlighter_, invalidate_,
        [this](const std::vector<se::TextChange>& changes) { UpdateCompletion(changes); },
        [this]() {
          model_dirty_ = true;
          if (invalidate_) {
            invalidate_();
          }
        },
        [this]() { return HandleTab(); },
        options.read_only);
    text_input_client_->SetDiffEditHook([this]() {
      if (current_diff_original_.empty()) {
        return;
      }
      core_->computeDiff(current_diff_original_);
      model_dirty_ = true;
      if (invalidate_) {
        invalidate_();
      }
    });

    on_link_click_ = options.on_link_click;
    on_codelens_click_ = options.on_codelens_click;
    on_gutter_icon_click_ = options.on_gutter_icon_click;
    on_inlay_click_ = options.on_inlay_click;
    on_text_changed_ = options.on_text_changed;
    on_cursor_changed_ = options.on_cursor_changed;
    on_selection_changed_ = options.on_selection_changed;
    on_scroll_changed_ = options.on_scroll_changed;
    on_fold_toggle_ = options.on_fold_toggle;
    on_long_press_ = options.on_long_press;
    on_double_tap_ = options.on_double_tap;
    newline_action_ = options.newline_action;
    phantom_text_provider_ = options.phantom_text_provider;
    accept_phantom_on_tab_ = options.accept_phantom_on_tab;
    on_toggle_search_ = options.on_toggle_search;
  }

  // Enables/disables the diff presentation against `original` without touching
  // the live document: recomputes the line-level diff (or clears it) and
  // repaints. Used by the host toggle so edits survive switching the diff.
  const std::string& CurrentDiffOriginal() const {
    return current_diff_original_;
  }

  // Runtime sync for display options changed after construction (wrap mode,
  // sticky gutter): the host toggles them without rebuilding the holder.
  void SyncDisplayOptions(int wrap_mode, bool sticky_gutter) {
    if (wrap_mode != current_wrap_mode_) {
      current_wrap_mode_ = wrap_mode;
      if (wrap_mode >= 1 && wrap_mode <= 2) {
        core_->setWrapMode(
            wrap_mode == 1 ? se::WrapMode::CHAR_BREAK : se::WrapMode::WORD_BREAK
        );
      } else {
        core_->setWrapMode(se::WrapMode::NONE);
      }
      AfterCoreAction({});
    }
    if (sticky_gutter != current_sticky_gutter_) {
      current_sticky_gutter_ = sticky_gutter;
      core_->setGutterSticky(sticky_gutter);
      AfterCoreAction({});
    }
  }

  void SetDiffOriginal(std::string original) {
    if (original == current_diff_original_) {
      return;
    }
    current_diff_original_ = std::move(original);
    se::EditorActionResult result = current_diff_original_.empty()
        ? core_->clearDiff()
        : core_->computeDiff(current_diff_original_);
    AfterCoreAction(result);
  }

  // ---- Find & replace (reference search module) ----------------------------

  // Runs a fresh search; empty pattern clears the search state.
  void RunSearch(const std::string& pattern) {
    se::SearchRequest request;
    request.pattern = pattern;
    if (!pattern.empty()) {
      se::EditorActionResult result = core_->search(request);
      AfterCoreAction(result);
    } else {
      se::EditorActionResult result = core_->clearSearch();
      AfterCoreAction(result);
    }
  }

  void FindNext() {
    se::EditorActionResult result = core_->findNextSearchMatch();
    AfterCoreAction(result);
  }

  void FindPrevious() {
    se::EditorActionResult result = core_->findPreviousSearchMatch();
    AfterCoreAction(result);
  }

  void ReplaceCurrent(const std::string& replacement) {
    if (replacement.empty()) {
      FindNext();
      return;
    }
    se::EditorActionResult result = core_->replaceCurrentSearchMatch(replacement);
    AfterCoreAction(result);
  }

  void ReplaceAll(const std::string& replacement) {
    se::EditorActionResult result = core_->replaceAllSearchMatches(replacement);
    AfterCoreAction(result);
  }

  void CloseSearch() {
    se::EditorActionResult result = core_->clearSearch();
    AfterCoreAction(result);
  }

  // Common post-core-action refresh (text/selection/scroll invalidation).
  void AfterCoreAction(const se::EditorActionResult& result) {
    if (!result.text_changes.empty()) {
      text_input_client_->NotifyContentChanged(result.text_changes);
      highlighter_->RefreshBracketMatch(*core_);
      FireTextChanged();
    } else if (result.selection_changed || result.cursor_changed || result.scroll_changed) {
      text_input_client_->NotifySelectionChanged();
      highlighter_->RefreshBracketMatch(*core_);
    }
    model_dirty_ = true;
    if (invalidate_) {
      invalidate_();
    }
  }

  // ---- Context menu (long-press / double-tap) ------------------------------

  void ShowContextMenu(const se::PointF& point) {
    context_menu_.visible = true;
    context_menu_.anchor = point;
    context_menu_.entries = {
        {"Cut", 0},
        {"Copy", 1},
        {"Paste", 2},
        {"Select All", 3},
        {"Find", 4},
        {"Fold All", 5},
        {"Unfold All", 6},
    };
    // Simple anchored sizing: 150px wide, menu row height, clamped to the
    // viewport so the panel never floats off-screen.
    const float width = 150.0F;
    const float row_height = 30.0F;
    const float height = row_height * static_cast<float>(context_menu_.entries.size()) + 8.0F;
    const float x = std::clamp(point.x, 0.0F, std::max(0.0F, viewport_.width - width));
    const float y = std::clamp(point.y, 0.0F, std::max(0.0F, viewport_.height - height));
    context_menu_.panel_rect = {x, y, width, height};
  }

  void DismissContextMenu() {
    context_menu_.visible = false;
    context_menu_.entries.clear();
  }

  // Returns true when the point hits the open context menu (down swallows it).
  bool HitContextMenu(const se::PointF& point, int& out_command) const {
    if (!context_menu_.visible) {
      return false;
    }
    const se::Rect& rect = context_menu_.panel_rect;
    if (point.x < rect.origin.x || point.x > rect.origin.x + rect.width ||
        point.y < rect.origin.y || point.y > rect.origin.y + rect.height) {
      return false;
    }
    const float row_height = 30.0F;
    const size_t index = static_cast<size_t>((point.y - rect.origin.y - 4.0F) / row_height);
    if (index < context_menu_.entries.size()) {
      out_command = context_menu_.entries[index].command;
      return true;
    }
    return false;
  }

  void RunContextCommand(int command) {
    switch (command) {
    case 0: {
      const se::EditorActionResult result = core_->deleteText(core_->getSelection().normalized());
      AfterCoreAction(result);
      break;
    }
    case 1: {
      // Copy via the runtime clipboard through the text input client.
      text_input_client_->CopySelection();
      break;
    }
    case 2:
      text_input_client_->PasteFromClipboard();
      break;
    case 3: {
      const se::EditorActionResult result = core_->selectAll();
      AfterCoreAction(result);
      break;
    }
    case 4:
      if (on_toggle_search_) {
        on_toggle_search_();
      }
      break;
    case 5:
      AfterCoreAction(core_->foldAll());
      break;
    case 6:
      AfterCoreAction(core_->unfoldAll());
      break;
    default:
      break;
    }
    DismissContextMenu();
  }

  void DrawContextMenu(PaintContext& paint) {
    if (!context_menu_.visible) {
      return;
    }
    const se::Rect& rect = context_menu_.panel_rect;
    paint.DrawRect(ToRect(rect), Argb(0xFFFFFFFF), CornerRadii(6.0F));
    paint.DrawBorder(ToRect(rect), Argb(0xFFDDDDDD), 1.0F, CornerRadii(6.0F));
    const float row_height = 30.0F;
    const TextStyle item_style{Font::System(13.0F), Argb(0xFF1F1F1F), TextDecoration::None};
    float y = rect.origin.y + 4.0F;
    for (const ContextMenuEntry& entry : context_menu_.entries) {
      const float baseline = y + (row_height + font_metrics_.descent - font_metrics_.ascent) * 0.5F;
      paint.DrawTextRun(
          Rect{rect.origin.x + 10.0F, y, rect.width - 20.0F, row_height},
          Point{rect.origin.x + 10.0F, baseline},
          entry.label,
          item_style);
      y += row_height;
    }
  }

  bool HandlePointer(const PointerEvent& event) {
    // A down inside the open context menu executes the tapped entry; a down
    // outside dismisses it (then falls through to the editor).
    if (event.type == PointerEventType::Down && context_menu_.visible) {
      int command = -1;
      if (HitContextMenu(se::PointF{event.position.x, event.position.y}, command)) {
        RunContextCommand(command);
        return true;
      }
      DismissContextMenu();
    }
    // Scrollbar thumb dragging takes priority over the editor gestures.
    if (scrollbar_dragging_) {
      if (event.type == PointerEventType::Move) {
        DragScrollbar(event.position);
        return true;
      }
      if (event.type == PointerEventType::Up || event.type == PointerEventType::Cancel) {
        scrollbar_dragging_ = false;
        return true;
      }
    } else if (event.type == PointerEventType::Down) {
      bool vertical = false;
      if (HitScrollbarThumb(event.position, vertical)) {
        scrollbar_dragging_ = true;
        scrollbar_drag_vertical_ = vertical;
        const Rect thumb = ToRect(vertical ? cached_scrollbar_v_.thumb : cached_scrollbar_h_.thumb);
        scrollbar_drag_offset_ = vertical ? event.position.y - thumb.y : event.position.x - thumb.x;
        return true;
      }
    }
    if (event.type == PointerEventType::Down && completion_.visible && !completion_.items.empty()) {
      size_t row = 0;
      if (HitCompletionPanel(event.position, row)) {
        completion_.selected = row;
        ConfirmCompletion();
        return true;
      }
      // A tap outside the panel dismisses it, then falls through to the editor.
      DismissCompletion();
    }
    try {
      const se::EditorActionResult result = core_->handleGestureEvent(ToGestureEvent(event));
      if (result.selection_changed || result.cursor_changed) {
        text_input_client_->NotifySelectionChanged();
        highlighter_->RefreshBracketMatch(*core_);
        highlighter_->RefreshDocumentHighlights(*core_, nullptr);
        FireCaretEvents();
      }
      // Decoration hits (reference fireGestureEvents): fold toggles are
      // handled inline; the rest dispatch to host callbacks.
      if (result.gesture_type == se::GestureType::TAP && result.hit_target.type != se::HitTargetType::NONE) {
        const se::HitTarget& target = result.hit_target;
        switch (target.type) {
        case se::HitTargetType::FOLD_GUTTER:
        case se::HitTargetType::FOLD_PLACEHOLDER:
          // The core's gesture pipeline already folded the line
          // (intent.toggle_fold -> toggleFoldAtInternal); do NOT toggle again
          // here or the double toggle cancels out. Just refresh and notify.
          highlighter_->RefreshVisible(*core_);
          FireFoldToggle(target.line);
          // Folding changes the visible line set, so the cached render model
          // must be rebuilt on the next frame.
          model_dirty_ = true;
          return true;
        case se::HitTargetType::LINK:
          if (on_link_click_) {
            on_link_click_(LinkTextAt(target.line, target.column));
          }
          return true;
        case se::HitTargetType::CODELENS:
          if (on_codelens_click_) {
            on_codelens_click_(target.icon_id);
          }
          return true;
        case se::HitTargetType::GUTTER_ICON:
          if (on_gutter_icon_click_) {
            on_gutter_icon_click_(
                static_cast<uint32_t>(target.line), target.icon_id
            );
          }
          return true;
        case se::HitTargetType::INLAY_HINT_TEXT:
        case se::HitTargetType::INLAY_HINT_ICON:
        case se::HitTargetType::INLAY_HINT_COLOR:
          if (on_inlay_click_) {
            on_inlay_click_(static_cast<uint32_t>(target.line), static_cast<uint32_t>(target.column));
          }
          return true;
        default:
          break;
        }
      }
      FirePointerEvents(result);
      if (result.needs_redraw) {
        model_dirty_ = true;
      }
      if (result.needsAnimation()) {
        animation_pending_ = true;
        if (invalidate_) {
          invalidate_();
        }
      }
      return result.needs_redraw;
    } catch (const std::exception&) {
      return false;
    }
  }

  bool HandleKey(const KeyEvent& event) {
    if (event.type == KeyEventType::Down) {
      // Line operations (reference keymap): Alt+Up/Down move the line,
      // Shift+Alt+Up/Down copy it, Ctrl+Shift+K deletes it. The huxerui Key
      // enum lacks K, so Ctrl+Shift+K is matched through the text payload.
      if (event.modifiers.alt && !event.modifiers.control && !event.modifiers.meta) {
        if (event.key == Key::ArrowUp || event.key == Key::ArrowDown) {
          const bool copy = event.modifiers.shift;
          const se::EditorActionResult result = event.key == Key::ArrowUp
              ? (copy ? core_->copyLineUp() : core_->moveLineUp())
              : (copy ? core_->copyLineDown() : core_->moveLineDown());
          AfterCoreAction(result);
          return result.handled;
        }
      }
      if (!event.modifiers.alt && event.modifiers.control && event.modifiers.shift && event.key == Key::K) {
        const se::EditorActionResult result = core_->deleteLine();
        AfterCoreAction(result);
        return result.handled;
      }
      // Escape dismisses the context menu first.
      if (event.key == Key::Escape && context_menu_.visible) {
        DismissContextMenu();
        if (invalidate_) {
          invalidate_();
        }
        return true;
      }
      // Tab reaches the editor through the text-input key hook (runtime gives a
      // focused text input first chance before focus traversal); it handles
      // linked-editing jumps, phantom accept, completion navigation, and plain
      // indentation in one place. Keep the direct path for platforms that do
      // deliver Tab to the component (e.g. desktop textareas).
      if (event.key == Key::Tab && !event.repeat && HandleTab()) {
        return true;
      }
      // Panel navigation takes priority over the core (reference
      // handleAndroidKeyCode: Enter confirms, Escape dismisses, arrows move).
      // Enter may arrive as a key or as '\r'/'\n' text depending on platform.
      const bool is_enter = event.key == Key::Enter || event.text == "\r" || event.text == "\n";
      if (completion_.visible && !completion_.items.empty()) {
        if (is_enter) {
          ConfirmCompletion();
          return true;
        }
        switch (event.key) {
        case Key::Escape:
          DismissCompletion();
          return true;
        case Key::ArrowUp:
          MoveCompletionSelection(-1);
          return true;
        case Key::ArrowDown:
          MoveCompletionSelection(1);
          return true;
        default:
          break;
        }
      }
      // Host newline action overrides the core's newline insertion
      // (reference NewLineActionProvider).
      if (is_enter && newline_action_) {
        const se::TextPosition cursor = core_->getCursorPosition();
        const std::string replacement = newline_action_(
            static_cast<uint32_t>(cursor.line), static_cast<uint32_t>(cursor.column)
        );
        if (!replacement.empty()) {
          const se::EditorActionResult result =
              core_->replaceText(core_->getSelection().normalized(), replacement);
          if (!result.text_changes.empty()) {
            text_input_client_->NotifyContentChanged(result.text_changes);
            highlighter_->RefreshBracketMatch(*core_);
            FireTextChanged();
          }
          model_dirty_ = true;
          if (invalidate_) {
            invalidate_();
          }
          return true;
        }
      }
      if (event.modifiers.control && event.key == Key::Space && completion_provider_) {
        TriggerCompletion(CompletionContext::TriggerKind::Invoked, "");
        return true;
      }
      // Ctrl+F (the web adapter delivers printable characters through
      // `text`; the key enum has no letter keys beyond the edit shortcuts).
      if (event.modifiers.control && event.text == "f" && on_toggle_search_) {
        on_toggle_search_();
        return true;
      }
    }

    try {
      const se::EditorActionResult result = core_->handleKeyEvent(ToKeyEvent(event));
      if (!result.text_changes.empty()) {
        text_input_client_->NotifyContentChanged(result.text_changes);
        highlighter_->RefreshBracketMatch(*core_);
        FireTextChanged();
        UpdateCompletion(result.text_changes);
      } else if (result.selection_changed || result.cursor_changed) {
        // Caret moves without text changes dismiss the panel (reference manager
        // dismisses whenever the edit result carries no text changes).
        if (completion_.visible || completion_.request_active) {
          DismissCompletion();
        }
        text_input_client_->NotifySelectionChanged();
        highlighter_->RefreshBracketMatch(*core_);
        highlighter_->RefreshDocumentHighlights(*core_, nullptr);
        FireCaretEvents();
      }
      if (result.needs_redraw) {
        model_dirty_ = true;
      }
      return result.needs_redraw;
    } catch (const std::exception&) {
      return false;
    }
  }

  bool HandleScroll(const ScrollEvent& event) {
    // Ctrl+wheel zooms (desktop stand-in for the core's pinch gesture, which
    // needs multi-point input the platform layer does not deliver yet).
    if (event.modifiers.control) {
      const se::ViewState view = core_->getViewState();
      const float factor = event.delta_y < 0.0F ? 1.1F : 0.9F;
      core_->setScale(view.scale * factor);
      model_dirty_ = true;
      if (invalidate_) {
        invalidate_();
      }
      return true;
    }
    const se::EditorActionResult result = core_->handleGestureEvent(ToWheelGestureEvent(event));
    if (result.selection_changed || result.cursor_changed) {
      text_input_client_->NotifySelectionChanged();
    }
    if (result.scroll_changed && on_scroll_changed_) {
      const se::ViewState view = core_->getViewState();
      on_scroll_changed_(view.scroll_x, view.scroll_y);
    }
    if (result.needs_redraw) {
      model_dirty_ = true;
    }
    if (result.needsAnimation()) {
      animation_pending_ = true;
      if (invalidate_) {
        invalidate_();
      }
    }
    return result.needs_redraw;
  }

  void SetFocused(bool focused) {
    focused_ = focused;
    if (!focused) {
      blink_on_ = true;
    }
  }

  // Toggled by the blink task; schedules a repaint (not a model rebuild).
  void TickBlink() {
    if (!focused_) {
      blink_on_ = true;
      return;
    }
    blink_on_ = !blink_on_;
    if (invalidate_) {
      invalidate_();
    }
  }

  std::shared_ptr<SweetEditorTextInputClient> TextInputClient() const {
    return text_input_client_;
  }

  void SetPaintInvalidation(std::function<void()> invalidate) {
    invalidate_ = std::move(invalidate);
  }

  bool HasPendingAnimation() const noexcept {
    return animation_pending_;
  }

  // ---- Editor event bus (reference EditorEventBus) -------------------------

  void FireCaretEvents() {
    if (on_cursor_changed_ || on_selection_changed_) {
      const se::TextPosition cursor = core_->getCursorPosition();
      if (on_cursor_changed_) {
        on_cursor_changed_(static_cast<uint32_t>(cursor.line), static_cast<uint32_t>(cursor.column));
      }
      if (on_selection_changed_) {
        const se::TextRange selection = core_->getSelection();
        on_selection_changed_(
            static_cast<uint32_t>(selection.start.line),
            static_cast<uint32_t>(selection.start.column),
            static_cast<uint32_t>(selection.end.line),
            static_cast<uint32_t>(selection.end.column)
        );
      }
    }
  }

  void FirePointerEvents(const se::EditorActionResult& result) {
    switch (result.gesture_type) {
    case se::GestureType::LONG_PRESS:
      if (on_long_press_) {
        on_long_press_(static_cast<uint32_t>(result.cursor_after.line), static_cast<uint32_t>(result.cursor_after.column));
      }
      ShowContextMenu(result.tap_point);
      break;
    case se::GestureType::DOUBLE_TAP:
      if (on_double_tap_) {
        on_double_tap_(
            static_cast<uint32_t>(result.cursor_after.line), static_cast<uint32_t>(result.cursor_after.column)
        );
      }
      ShowContextMenu(result.tap_point);
      break;
    default:
      break;
    }
  }

  void FireFoldToggle(size_t line) {
    if (on_fold_toggle_) {
      on_fold_toggle_(line);
    }
  }

  void FireTextChanged() {
    if (on_text_changed_) {
      on_text_changed_();
    }
  }

  // Applies phantom (ghost) text for the visible lines (reference phantom text
  // decoration / copilot inline suggestion preview). Returns true when the
  // published phantom set changed (model rebuild required).
  bool ApplyPhantomTexts(uint32_t start_line, uint32_t end_line) {
    std::map<uint32_t, std::string> next;
    if (phantom_text_provider_) {
      for (uint32_t line = start_line; line <= end_line; ++line) {
        const std::string text = phantom_text_provider_(line);
        if (!text.empty()) {
          next[line] = text;
        }
      }
    }
    if (next == cached_phantom_) {
      return false;
    }
    cached_phantom_ = std::move(next);
    if (cached_phantom_.empty()) {
      core_->clearPhantomTexts();
      return true;
    }
    std::vector<std::pair<size_t, std::vector<se::PhantomText>>> entries;
    for (const auto& [line, text] : cached_phantom_) {
      const uint32_t column = document_->getLineColumns(line);
      entries.push_back({line, {{column, text}}});
    }
    core_->setBatchLinePhantomTexts(std::move(entries));
    return true;
  }

  // Single Tab handler shared by the direct component key path and the
  // text-input key hook: advance a snippet's linked editing, accept the
  // caret-line phantom text, move the completion selection, or insert
  // indentation through the core. Tab is always consumed while the editor is
  // focused so the runtime's focus traversal never steals it.
  bool HandleTab() {
    if (core_->isInLinkedEditing()) {
      const se::EditorActionResult result = core_->linkedEditingNextTabStop();
      if (result.selection_changed || result.cursor_changed) {
        text_input_client_->NotifySelectionChanged();
        model_dirty_ = true;
        if (invalidate_) {
          invalidate_();
        }
        return true;
      }
    }
    if (AcceptPhantomText()) {
      return true;
    }
    if (completion_.visible && !completion_.items.empty()) {
      MoveCompletionSelection(1);
      return true;
    }
    // Plain Tab inserts indentation (spaces up to the tab stop when
    // insert_spaces is on); run the core's own key handling for it.
    se::KeyEvent tab_event;
    tab_event.key_code = se::KeyCode::TAB;
    const se::EditorActionResult result = core_->handleKeyEvent(tab_event);
    if (!result.text_changes.empty()) {
      text_input_client_->NotifyContentChanged(result.text_changes);
      highlighter_->RefreshBracketMatch(*core_);
      FireTextChanged();
      UpdateCompletion(result.text_changes);
    } else if (result.selection_changed || result.cursor_changed) {
      text_input_client_->NotifySelectionChanged();
    }
    model_dirty_ = true;
    if (invalidate_) {
      invalidate_();
    }
    return true;
  }

  // Commits the caret-line phantom text (copilot accept via Tab).
  bool AcceptPhantomText() {
    if (!accept_phantom_on_tab_ || !phantom_text_provider_) {
      return false;
    }
    const se::TextPosition cursor = core_->getCursorPosition();
    const std::string text = phantom_text_provider_(static_cast<uint32_t>(cursor.line));
    if (text.empty()) {
      return false;
    }
    const se::EditorActionResult result = core_->insertText(text);
    if (!result.text_changes.empty()) {
      text_input_client_->NotifyContentChanged(result.text_changes);
      highlighter_->RefreshBracketMatch(*core_);
      FireTextChanged();
    }
    model_dirty_ = true;
    if (invalidate_) {
      invalidate_();
    }
    return true;
  }

  // ---- Scrollbar thumb dragging (P3) ---------------------------------------

  bool HitScrollbarThumb(const Point& position, bool& out_vertical) const {
    const auto hit = [&position](const se::ScrollbarModel& bar) {
      const se::Rect& thumb = bar.thumb;
      return position.x >= thumb.origin.x && position.x <= thumb.origin.x + thumb.width &&
             position.y >= thumb.origin.y && position.y <= thumb.origin.y + thumb.height;
    };
    if (cached_scrollbar_v_.visible && cached_scrollbar_v_.thumb.width > 0.0F && hit(cached_scrollbar_v_)) {
      out_vertical = true;
      return true;
    }
    if (cached_scrollbar_h_.visible && cached_scrollbar_h_.thumb.height > 0.0F && hit(cached_scrollbar_h_)) {
      out_vertical = false;
      return true;
    }
    return false;
  }

  void DragScrollbar(const Point& position) {
    const se::ScrollMetrics metrics = core_->getScrollMetrics();
    const se::ScrollbarModel& bar = scrollbar_drag_vertical_ ? cached_scrollbar_v_ : cached_scrollbar_h_;
    const se::Rect track = bar.track;
    const se::Rect thumb = bar.thumb;
    const float track_extent = scrollbar_drag_vertical_ ? track.height : track.width;
    const float thumb_extent = scrollbar_drag_vertical_ ? thumb.height : thumb.width;
    const float pointer =
        scrollbar_drag_vertical_ ? position.y - track.origin.y : position.x - track.origin.x;
    const float max_scroll = scrollbar_drag_vertical_ ? metrics.max_scroll_y : metrics.max_scroll_x;
    const float movable = std::max(1.0F, track_extent - thumb_extent);
    const float ratio = std::clamp((pointer - scrollbar_drag_offset_) / movable, 0.0F, 1.0F);
    const float scroll_x = scrollbar_drag_vertical_ ? metrics.scroll_x : ratio * max_scroll;
    const float scroll_y = scrollbar_drag_vertical_ ? ratio * max_scroll : metrics.scroll_y;
    const se::EditorActionResult result = core_->setScroll(scroll_x, scroll_y);
    if (result.needs_redraw) {
      model_dirty_ = true;
      if (invalidate_) {
        invalidate_();
      }
    }
  }

  // Reads the link text under a hit column from the document line (the core
  // reports the hit position; the URL continues to the next delimiter).
  std::string LinkTextAt(size_t line, size_t column) const {
    const std::u16string text = document_->getLineU16Text(line);
    if (column >= text.size()) {
      return {};
    }
    size_t end = column;
    while (end < text.size()) {
      const char16_t ch = text[end];
      if (ch == u' ' || ch == u'\t' || ch == u'(' || ch == u')' || ch == u'"' || ch == u'\'' || ch == u';' ||
          ch == u',' || ch == u'<' || ch == u'>' || ch == u'{' || ch == u'}') {
        break;
      }
      ++end;
    }
    return Utf16ToUtf8(text.substr(column, end - column));
  }

  void Render(PaintContext& paint, Size size) {
    const se::Size viewport{size.width, size.height};
    if (viewport_.width != viewport.width || viewport_.height != viewport.height) {
      viewport_ = viewport;
      core_->setViewport(viewport);
      // A viewport change (e.g. IME insets resizing the editor) invalidates the
      // visible range and scrollbar geometry; force a rebuild.
      model_dirty_ = true;
    }

    // Drive core-managed animations (fling momentum, edge scroll): each frame
    // advances the animation clock; while an animation is active the frame is
    // kept dirty and a repaint is scheduled, matching the reference platform's
    // Choreographer-driven tick loop.
    if (animation_pending_) {
      const se::EditorActionResult animation = core_->tickAnimations();
      if (animation.needs_redraw || animation.scroll_changed || animation.animation_flags != 0) {
        model_dirty_ = true;
      }
      if (animation.needsAnimation()) {
        if (invalidate_) {
          invalidate_();
        }
      } else {
        animation_pending_ = false;
      }
    }

    // Rebuild the render model only when the core state changed (input, scroll,
    // decorations). Pure repaints (cursor blink, focus changes) reuse the
    // cached model, matching the reference renderer's onDraw caching.
    if (model_dirty_) {
      // buildRenderModel appends into the model, so a fresh (empty) model is
      // required each rebuild; reusing the cached instance would accumulate
      // stale lines/decorations and ghost on every update.
      cached_model_ = se::EditorRenderModel{};
      core_->buildRenderModel(cached_model_);
      model_dirty_ = false;
    }
    const se::EditorRenderModel& model = cached_model_;

    // Re-publish the viewport highlight slice when the visible range changed (scroll).
    // Syntax highlighting and indent guides are applied immediately;
    // decorations/document highlights are deferred until the viewport settles so
    // fast scrolling does not pay their per-frame cost (reference decoration
    // pipeline throttles scroll refreshes). Publishing new spans/guides changes
    // core decorations, so the next frame must rebuild the model.
    const se::IntRange visible = core_->getVisibleLineRange();
    if (visible.start != last_visible_range_.start || visible.end != last_visible_range_.end) {
      last_visible_range_ = visible;
      if (!visible.isEmpty()) {
        highlighter_->PublishVisible(*core_);
        decorations_pending_ = true;
        model_dirty_ = true;
        if (invalidate_) {
          invalidate_();
        }
      }
    } else if (decorations_pending_) {
      highlighter_->RefreshVisibleDecorations(*core_);
      decorations_pending_ = false;
      model_dirty_ = true;
      if (invalidate_) {
        invalidate_();
      }
    }

    RenderModel(paint, size, model);

    cached_scrollbar_v_ = model.vertical_scrollbar;
    cached_scrollbar_h_ = model.horizontal_scrollbar;

    if (!decorations_pending_ && (phantom_text_provider_ || !cached_phantom_.empty())) {
      if (ApplyPhantomTexts(static_cast<uint32_t>(visible.start), static_cast<uint32_t>(visible.end))) {
        model_dirty_ = true;
      }
    }

    // The completion panel floats above the document, anchored to the caret
    // (reference: cached cursor rect updated every frame in onDraw).
    if (completion_.visible && !completion_.items.empty()) {
      if (model.cursor.visible) {
        completion_.last_cursor = model.cursor.position;
        completion_.last_cursor_height = model.cursor.height;
      }
      const se::Rect panel = ComputeCompletionPanelRect(completion_.last_cursor, completion_.last_cursor_height);
      completion_.panel_rect = panel;
      DrawCompletionPanel(paint, panel);
    }

    // The context menu floats above everything, drawn last.
    DrawContextMenu(paint);
  }

private:
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
    const float line_height = ascent + font_metrics_.descent;

    paint.DrawRect(Rect{0.0F, 0.0F, width, height}, Argb(kEditorBackground));

    // The gutter (line-number column) sits on its own background strip; the
    // core crops text runs with a margin that hides under this strip, so it
    // must be painted or cropped characters bleed into the line-number area.
    if (model.split_x > 0.0F) {
      paint.DrawRect(Rect{0.0F, 0.0F, model.split_x, height}, Argb(kGutterBackground));
      // The split line separates the gutter from the text area (reference
      // renderer draws it when splitLineVisible is set).
      if (model.split_line_visible) {
        paint.DrawRect(Rect{model.split_x - 1.0F, 0.0F, 1.0F, height}, Argb(0xFFE3E5E8));
      }
    }

    if (model.current_line_render_mode == se::CurrentLineRenderMode::BACKGROUND) {
      paint.DrawRect(Rect{0.0F, model.current_line.y, width, LineHeight(model)}, Argb(kCurrentLineBackground));
    }

    for (const se::RangeEffectRenderItem& effect : model.range_effects) {
      if (effect.style.background_color != 0) {
        paint.DrawRect(ToRect(effect.rect), Argb(effect.style.background_color));
      }
    }

    const TextStyle line_number_style{Font::Monospace(font_size_), Argb(kLineNumberColor), TextDecoration::None};
    for (const se::VisualLine& line : model.lines) {
      // line_number_position.y is the text baseline; the row background spans
      // from the line top (baseline - ascent) down the full line height.
      if (line.line_background_color != 0) {
        paint.DrawRect(Rect{model.split_x, line.line_number_position.y - ascent, width - model.split_x, line_height},
                       Argb(line.line_background_color));
      }
      if (line.line_number >= 0) {
        const std::string number = std::to_string(line.line_number);
        const float number_width = char_width_ * static_cast<float>(number.size());
        paint.DrawTextRun(
            Rect{line.line_number_position.x, line.line_number_position.y - ascent, number_width, line_height},
            Point{line.line_number_position.x, line.line_number_position.y},
            number,
            line_number_style);
      }
    }
    // Clip everything that lives in the text area right of the gutter: runs,
    // structure guides (bracket/indent connectors), and range-effect
    // underlines (diagnostics/IME squiggles). Gutter icons and fold markers
    // are painted outside the clip because they belong to the gutter lane.
    if (model.split_x < width) {
      paint.PushClip(Rect{model.split_x, 0.0F, width - model.split_x, height});
      for (const se::VisualLine& line : model.lines) {
        for (const se::VisualRun& run : line.runs) {
          DrawRun(paint, run, line_height);
        }
      }
      for (const se::GuideSegment& segment : model.guide_segments) {
        DrawGuideSegment(paint, segment);
      }
      for (const se::RangeEffectRenderItem& effect : model.range_effects) {
        if (effect.style.underline_color != 0 && effect.style.underline_style != se::RangeEffectUnderlineStyle::NONE) {
          DrawUnderline(paint, effect.rect, effect.style.underline_color, effect.style.underline_style);
        }
      }
      paint.PopClip();
    } else {
      for (const se::VisualLine& line : model.lines) {
        for (const se::VisualRun& run : line.runs) {
          DrawRun(paint, run, line_height);
        }
      }
      for (const se::GuideSegment& segment : model.guide_segments) {
        DrawGuideSegment(paint, segment);
      }
      for (const se::RangeEffectRenderItem& effect : model.range_effects) {
        if (effect.style.underline_color != 0 && effect.style.underline_style != se::RangeEffectUnderlineStyle::NONE) {
          DrawUnderline(paint, effect.rect, effect.style.underline_color, effect.style.underline_style);
        }
      }
    }

    for (const se::GutterIconRenderItem& icon : model.gutter_icons) {
      DrawGutterIcon(paint, icon);
    }
    for (const se::FoldMarkerRenderItem& marker : model.fold_markers) {
      DrawFoldMarker(paint, marker);
    }

    const bool caret_visible = model.cursor.visible && focused_ && blink_on_;
    if (caret_visible) {
      paint.DrawRect(
          Rect{model.cursor.position.x, model.cursor.position.y, kCursorWidth, model.cursor.height},
          Argb(kCursorColor));
    }

    // Handles are drawn here (single owner) because the HuxerUI selection
    // overlay is disabled; the core's gesture pipeline handles dragging.
    if (model.selection_start_handle.visible) {
      DrawSelectionHandle(paint, model.selection_start_handle);
    }
    if (model.selection_end_handle.visible) {
      DrawSelectionHandle(paint, model.selection_end_handle);
    }

    if (model.vertical_scrollbar.visible) {
      DrawScrollbar(paint, model.vertical_scrollbar);
    }
    if (model.horizontal_scrollbar.visible) {
      DrawScrollbar(paint, model.horizontal_scrollbar);
    }
  }

  void DrawSelectionHandle(PaintContext& paint, const se::SelectionHandle& handle) {
    const float radius = 6.0F;
    const float cx = handle.position.x;
    // getPositionScreenCoord resolves the logical line's line_number_position.y,
    // which is the line TOP (screen y, scroll already applied) — not the text
    // baseline. The stem spans the full line height; the teardrop sits below
    // the line bottom (reference drag-handle anchor).
    const float line_top = handle.position.y;
    const float line_bottom = handle.position.y + handle.height;
    paint.DrawRect(Rect{cx - 1.0F, line_top, 2.0F, std::max(1.0F, line_bottom - line_top)}, Argb(0xFF1F6FEB));
    paint.DrawCircle(Point{cx, line_bottom + radius * 0.8F}, radius, Argb(0xFF1F6FEB));
  }

  void DrawScrollbar(PaintContext& paint, const se::ScrollbarModel& bar) {
    const Rect thumb = ToRect(bar.thumb);
    if (thumb.width <= 0.0F || thumb.height <= 0.0F) {
      return;
    }
    paint.DrawRect(thumb, Color{0.0F, 0.0F, 0.0F, std::clamp(bar.alpha, 0.0F, 1.0F) * 0.45F}, CornerRadii(4.0F));
  }

  void DrawRun(PaintContext& paint, const se::VisualRun& run, float line_height) {
    const float ascent = font_metrics_.ascent;
    const float top = run.y - ascent;
    const float height = ascent + font_metrics_.descent;

    if (run.type == se::VisualRunType::INLAY_HINT) {
      DrawInlayHint(paint, run, top, height);
      return;
    }
    if (run.type == se::VisualRunType::FOLD_PLACEHOLDER) {
      DrawFoldPlaceholder(paint, run, top, height);
      return;
    }
    if (run.type == se::VisualRunType::WHITESPACE) {
      // Space marker: a faint dot centered on the glyph cell.
      const float center_y = run.y - ascent * 0.35F;
      const float spacing = std::max(2.0F, char_width_ * 0.5F);
      for (float x = run.x + spacing; x < run.x + run.width; x += char_width_) {
        paint.DrawCircle(Point{x, center_y}, 1.2F, Color{0.0F, 0.0F, 0.0F, 0.25F});
      }
      return;
    }
    if (run.type == se::VisualRunType::TAB) {
      // Tab marker: a right arrow at the glyph start.
      const float mid_y = run.y - ascent * 0.5F;
      const float x = run.x + 2.0F;
      Path path;
      path.MoveTo(Point{x, mid_y});
      path.LineTo(Point{x + char_width_ * 0.6F, mid_y});
      path.LineTo(Point{x + char_width_ * 0.45F, mid_y - 3.0F});
      path.MoveTo(Point{x + char_width_ * 0.6F, mid_y});
      path.LineTo(Point{x + char_width_ * 0.45F, mid_y + 3.0F});
      paint.StrokePath(path, Color{0.0F, 0.0F, 0.0F, 0.3F}, 1.0F);
      return;
    }
    if (run.type == se::VisualRunType::NEWLINE) {
      // Line-break symbol: a left-pointing hook like the reference renderer.
      const float mid_y = run.y - ascent * 0.5F;
      const float x = run.x + 2.0F;
      Path path;
      path.MoveTo(Point{x, mid_y});
      path.LineTo(Point{x + 8.0F, mid_y});
      path.LineTo(Point{x + 8.0F, mid_y + 4.0F});
      path.LineTo(Point{x + 4.0F, mid_y + 4.0F});
      paint.StrokePath(path, Color{0.0F, 0.0F, 0.0F, 0.3F}, 1.0F);
      return;
    }
    if (run.text.empty()) {
      return;
    }

    if (run.style.background_color != 0) {
      paint.DrawRect(Rect{run.x, top, run.width, height}, Argb(run.style.background_color));
    }

    // HuxerUI text runs must not contain line breaks; strip any that slip
    // through (for example multi-line snippet expansion) defensively.
    std::string text = Utf16ToUtf8(run.text);
    const size_t line_break = text.find_first_of("\r\n");
    if (line_break != std::string::npos) {
      text.resize(line_break);
    }

    if (run.type == se::VisualRunType::PHANTOM_TEXT) {
      TextStyle phantom{Font::Monospace(font_size_).WithSlant(FontSlant::Italic), Color{0.6F, 0.6F, 0.6F, 0.6F},
                        TextDecoration::None};
      paint.DrawTextRun(Rect{run.x, top, run.width, height}, Point{run.x, run.y}, text, phantom);
      return;
    }

    const TextStyle style = MakeTextStyle(run.style.font_style, run.style.color, font_size_);
    paint.DrawTextRun(Rect{run.x, top, run.width, height}, Point{run.x, run.y}, text, style);
  }

  void DrawInlayHint(PaintContext& paint, const se::VisualRun& run, float top, float height) {
    const float margin = run.margin;
    if (run.color_value != 0) {
      paint.DrawRect(Rect{run.x + margin, top, height, height}, Argb(run.color_value));
      return;
    }
    const Rect background{run.x + margin, top, run.width - margin * 2.0F, height};
    paint.DrawRect(background, Argb(kInlayHintBackground), CornerRadii(height * 0.2F));
    if (!run.text.empty()) {
      const float text_x = run.x + margin + run.padding;
      const TextStyle style{Font::System(font_size_ * 0.9F), Argb(kInlayHintText), TextDecoration::None};
      paint.DrawTextRun(Rect{text_x, top, run.width, height}, Point{text_x, run.y}, Utf16ToUtf8(run.text), style);
    }
  }

  void DrawFoldPlaceholder(PaintContext& paint, const se::VisualRun& run, float top, float height) {
    const float margin = run.margin;
    const Rect background{run.x + margin, top, run.width - margin * 2.0F, height};
    paint.DrawRect(background, Argb(kFoldPlaceholderBackground), CornerRadii(height * 0.2F));
    if (!run.text.empty()) {
      const float text_x = run.x + margin + run.padding;
      const TextStyle style{Font::Monospace(font_size_), Argb(kFoldPlaceholderText), TextDecoration::None};
      paint.DrawTextRun(Rect{text_x, top, run.width, height}, Point{text_x, run.y}, Utf16ToUtf8(run.text), style);
    }
  }

  void DrawGutterIcon(PaintContext& paint, const se::GutterIconRenderItem& icon) {
    const Rect rect = ToRect(icon.rect);
    const Point center{rect.x + rect.width * 0.5F, rect.y + rect.height * 0.5F};
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
      paint.StrokePath(diamond, Argb(kGutterIconColor), 1.5F);
      return;
    }
    if (icon.icon_id == 2) {
      paint.DrawCircle(center, rect.width * 0.5F, Argb(kGutterIconColor));
      return;
    }
    paint.DrawCircle(center, rect.width * 0.35F, Argb(kGutterIconColor));
  }

  void DrawFoldMarker(PaintContext& paint, const se::FoldMarkerRenderItem& marker) {
    const Rect rect = ToRect(marker.rect);
    Path path;
    if (marker.fold_state == se::FoldState::COLLAPSED) {
      path.MoveTo(Point{rect.x + rect.width * 0.35F, rect.y + rect.height * 0.2F});
      path.LineTo(Point{rect.x + rect.width * 0.75F, rect.y + rect.height * 0.5F});
      path.LineTo(Point{rect.x + rect.width * 0.35F, rect.y + rect.height * 0.8F});
    } else {
      path.MoveTo(Point{rect.x + rect.width * 0.2F, rect.y + rect.height * 0.35F});
      path.LineTo(Point{rect.x + rect.width * 0.5F, rect.y + rect.height * 0.75F});
      path.LineTo(Point{rect.x + rect.width * 0.8F, rect.y + rect.height * 0.35F});
    }
    paint.StrokePath(path, Argb(kLineNumberColor), 1.5F);
  }

  void DrawUnderline(
      PaintContext& paint, const se::Rect& rect, int32_t color, se::RangeEffectUnderlineStyle style
  ) {
    const Rect r = ToRect(rect);
    const float baseline = r.y + r.height - 1.0F;
    if (style == se::RangeEffectUnderlineStyle::WAVY) {
      Path path;
      constexpr float kAmplitude = 1.5F;
      constexpr float kWavelength = 5.0F;
      path.MoveTo(Point{r.x, baseline});
      bool up = true;
      for (float x = r.x + kWavelength; x < r.x + r.width; x += kWavelength) {
        path.LineTo(Point{x, baseline + (up ? -kAmplitude : kAmplitude)});
        up = !up;
      }
      path.LineTo(Point{r.x + r.width, baseline + (up ? -kAmplitude : kAmplitude)});
      paint.StrokePath(path, Argb(color), 1.0F);
      return;
    }
    if (style == se::RangeEffectUnderlineStyle::DASHED) {
      constexpr float kDash = 4.0F;
      constexpr float kGap = 3.0F;
      float x = r.x;
      while (x < r.x + r.width) {
        const float end = std::min(x + kDash, r.x + r.width);
        paint.DrawRect(Rect{x, baseline, end - x, 1.5F}, Argb(color));
        x = end + kGap;
      }
      return;
    }
    paint.DrawRect(Rect{r.x, baseline, r.width, 1.5F}, Argb(color));
  }

  void DrawGuideSegment(PaintContext& paint, const se::GuideSegment& segment) {
    const int32_t color = segment.type == se::GuideType::SEPARATOR ? kSeparatorColor : kGuideColor;
    // Separator and bracket lines are lighter than indent guides (reference
    // renderer uses a separate paint for separators).
    const float width = segment.type == se::GuideType::SEPARATOR ? 1.0F : 1.0F;
    const auto stroke = [&](const Point& a, const Point& b) {
      Path path;
      path.MoveTo(a);
      path.LineTo(b);
      paint.StrokePath(path, Argb(color), width);
    };

    // DOUBLE style draws a pair of parallel lines straddling the segment.
    if (segment.style == se::GuideStyle::DOUBLE) {
      constexpr float kOffset = 1.5F;
      if (segment.direction == se::GuideDirection::HORIZONTAL) {
        stroke({segment.start.x, segment.start.y - kOffset}, {segment.end.x, segment.end.y - kOffset});
        stroke({segment.start.x, segment.start.y + kOffset}, {segment.end.x, segment.end.y + kOffset});
      } else {
        stroke({segment.start.x - kOffset, segment.start.y}, {segment.end.x - kOffset, segment.end.y});
        stroke({segment.start.x + kOffset, segment.start.y}, {segment.end.x + kOffset, segment.end.y});
      }
      return;
    }

    // DASHED style: short dashes along the segment (used by separator guides).
    if (segment.style == se::GuideStyle::DASHED) {
      const float dx = segment.end.x - segment.start.x;
      const float dy = segment.end.y - segment.start.y;
      const float length = std::hypot(dx, dy);
      if (length <= 0.0F) {
        return;
      }
      constexpr float kDash = 6.0F;
      constexpr float kGap = 4.0F;
      const float ux = dx / length;
      const float uy = dy / length;
      float distance = 0.0F;
      while (distance < length) {
        const float end = std::min(distance + kDash, length);
        stroke(
            {segment.start.x + ux * distance, segment.start.y + uy * distance},
            {segment.start.x + ux * end, segment.start.y + uy * end}
        );
        distance = end + kGap;
      }
      return;
    }

    // Solid line; when the segment carries an arrow, draw the shaft plus a
    // small arrowhead at the end (flow/return guides).
    if (segment.arrow_end) {
      constexpr float kArrowLength = 8.0F;
      constexpr float kArrowAngle = 0.49F;  // ~28 degrees
      const float dx = segment.end.x - segment.start.x;
      const float dy = segment.end.y - segment.start.y;
      const float length = std::hypot(dx, dy);
      if (length <= kArrowLength) {
        return;
      }
      const float ratio = (length - kArrowLength) / length;
      const Point shaft_end{
          segment.start.x + dx * ratio,
          segment.start.y + dy * ratio,
      };
      stroke({segment.start.x, segment.start.y}, shaft_end);
      // Arrowhead: two short lines back from the tip at ±angle.
      const float ux = dx / length;
      const float uy = dy / length;
      const float cos_a = std::cos(kArrowAngle);
      const float sin_a = std::sin(kArrowAngle);
      // Left wing: rotate (ux, uy) by -angle.
      const float left_x = segment.end.x - kArrowLength * (ux * cos_a + uy * sin_a);
      const float left_y = segment.end.y - kArrowLength * (-ux * sin_a + uy * cos_a);
      stroke(shaft_end, {left_x, left_y});
      // Right wing: rotate (ux, uy) by +angle.
      const float right_x = segment.end.x - kArrowLength * (ux * cos_a - uy * sin_a);
      const float right_y = segment.end.y - kArrowLength * (ux * sin_a + uy * cos_a);
      stroke(shaft_end, {right_x, right_y});
      return;
    }

    stroke({segment.start.x, segment.start.y}, {segment.end.x, segment.end.y});
  }

  // ---- Code completion ----------------------------------------------------

  static bool IsCompletionWordChar(char16_t ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' ||
           ch > 0x7F;
  }

  bool IsTriggerCharacter(const std::string& ch) const {
    return completion_trigger_characters_ && completion_trigger_characters_(ch);
  }

  void UpdateCompletion(const std::vector<se::TextChange>& changes) {
    FireTextChanged();
    if (!completion_provider_) {
      return;
    }
    const bool completion_active = completion_.request_active;
    // The reference manager dismisses when the result carries no text change
    // or more than one (a single character edit is the only live trigger).
    if (changes.empty() || changes.size() != 1) {
      DismissCompletion();
      return;
    }
    const std::string& new_text = changes.front().new_text;
    if (new_text.size() == 1 && IsTriggerCharacter(new_text)) {
      TriggerCompletion(CompletionContext::TriggerKind::Character, new_text);
    } else if (completion_active || new_text.size() == 1) {
      // Any printable character re-requests while active, and also opens the
      // panel from scratch so typing an identifier (for example `for`) shows
      // matching candidates immediately.
      TriggerCompletion(CompletionContext::TriggerKind::Retrigger, "");
    } else {
      completion_.request_active = false;
      if (completion_.visible) {
        DismissCompletion();
      }
    }
  }

  void TriggerCompletion(CompletionContext::TriggerKind kind, const std::string& trigger_character) {
    completion_.request_active = true;
    completion_.last_trigger_kind = kind;
    completion_.last_trigger_char = trigger_character;
    ExecuteCompletionRefresh();
  }

  void DismissCompletion() {
    if (!completion_.visible && !completion_.request_active) {
      return;
    }
    completion_.visible = false;
    completion_.request_active = false;
    completion_.items.clear();
    if (invalidate_) {
      invalidate_();
    }
  }

  void ExecuteCompletionRefresh() {
    if (!completion_.request_active || !completion_provider_) {
      DismissCompletion();
      return;
    }
    const CompletionContext context = BuildCompletionContext(completion_.last_trigger_kind, completion_.last_trigger_char);
    std::vector<CompletionItem> items = completion_provider_(context);
    std::stable_sort(items.begin(), items.end(), [](const CompletionItem& a, const CompletionItem& b) {
      const std::string& key_a = !a.sort_key.empty() ? a.sort_key : a.label;
      const std::string& key_b = !b.sort_key.empty() ? b.sort_key : b.label;
      return key_a < key_b;
    });
    if (items.empty()) {
      DismissCompletion();
      return;
    }
    completion_.items = std::move(items);
    completion_.selected = 0;
    completion_.visible = true;
    if (invalidate_) {
      invalidate_();
    }
  }

  CompletionContext BuildCompletionContext(CompletionContext::TriggerKind kind, const std::string& trigger_character) {
    CompletionContext context;
    context.trigger_kind = kind;
    context.trigger_character = trigger_character;
    const se::TextPosition cursor = core_->getCursorPosition();
    context.cursor_line = static_cast<uint32_t>(cursor.line);
    context.cursor_column = static_cast<uint32_t>(cursor.column);
    const std::u16string line = document_->getLineU16Text(cursor.line);
    context.line_text = Utf16ToUtf8(line);
    size_t start = cursor.column;
    while (start > 0 && IsCompletionWordChar(line[start - 1])) {
      --start;
    }
    size_t end = cursor.column;
    while (end < line.size() && IsCompletionWordChar(line[end])) {
      ++end;
    }
    context.word_start = static_cast<uint32_t>(start);
    context.word_end = static_cast<uint32_t>(end);
    return context;
  }

  void ConfirmCompletion() {
    if (!completion_.visible || completion_.selected >= completion_.items.size()) {
      return;
    }
    const CompletionItem item = completion_.items[completion_.selected];
    DismissCompletion();

    const std::string text = !item.insert_text.empty() ? item.insert_text : item.label;
    const bool is_snippet = item.insert_text_is_snippet;
    se::EditorActionResult result;
    try {
      if (item.has_text_edit) {
        const se::TextPosition cursor = core_->getCursorPosition();
        const std::string edit_text = !item.text_edit_text.empty() ? item.text_edit_text : text;
        const se::TextRange range{{cursor.line, item.text_edit_start}, {cursor.line, item.text_edit_end}};
        if (is_snippet) {
          // Reference behavior: clear the replacement range first, then expand
          // the snippet template from the caret.
          result = core_->replaceText(range, "");
          if (!result.text_changes.empty() && !edit_text.empty()) {
            result = core_->insertSnippet(edit_text);
          }
        } else {
          result = core_->replaceText(range, edit_text);
        }
      } else if (is_snippet) {
        result = core_->insertSnippet(text);
      } else {
        result = core_->insertText(text);
      }
    } catch (const std::exception&) {
      return;
    }
    if (!result.text_changes.empty()) {
      text_input_client_->NotifyContentChanged(result.text_changes);
      model_dirty_ = true;
    }
  }

  void MoveCompletionSelection(int delta) {
    if (!completion_.visible || completion_.items.empty()) {
      return;
    }
    const int count = static_cast<int>(completion_.items.size());
    const int next = std::max(0, std::min(count - 1, static_cast<int>(completion_.selected) + delta));
    if (static_cast<size_t>(next) != completion_.selected) {
      completion_.selected = static_cast<size_t>(next);
      if (invalidate_) {
        invalidate_();
      }
    }
  }

  bool HitCompletionPanel(const Point& position, size_t& out_row) const {
    if (!completion_.visible || completion_.items.empty()) {
      return false;
    }
    const se::Rect& rect = completion_.panel_rect;
    if (position.x < rect.origin.x || position.x >= rect.origin.x + rect.width || position.y < rect.origin.y ||
        position.y >= rect.origin.y + rect.height) {
      return false;
    }
    const float row = (position.y - rect.origin.y - kCompletionPanelPaddingV) / kCompletionRowHeight;
    if (row < 0.0F) {
      return false;
    }
    const size_t index = static_cast<size_t>(row);
    if (index >= completion_.items.size()) {
      return false;
    }
    out_row = index;
    return true;
  }

  se::Rect ComputeCompletionPanelRect(const se::PointF& cursor, float cursor_height) const {
    const float width = std::min(
        kCompletionPanelWidth,
        std::max(kCompletionPanelMinWidth, viewport_.width - kCompletionFramePadding * 2.0F));
    const size_t rows = std::min(completion_.items.size(), kCompletionMaxVisible);
    const float height = kCompletionRowHeight * static_cast<float>(rows) + kCompletionPanelPaddingV * 2.0F;
    const float x = std::max(
        kCompletionFramePadding,
        std::min(cursor.x, std::max(kCompletionFramePadding, viewport_.width - kCompletionFramePadding - width)));
    float y = cursor.y + cursor_height + kCompletionGap;
    if (y + height > viewport_.height - kCompletionFramePadding) {
      // Not enough room below the caret: place the panel above it.
      y = cursor.y - kCompletionGap - height;
      if (y < kCompletionFramePadding) {
        y = kCompletionFramePadding;
      }
    }
    return se::Rect{{x, y}, width, height};
  }

  void DrawCompletionPanel(PaintContext& paint, const se::Rect& rect) {
    const Rect panel{rect.origin.x, rect.origin.y, rect.width, rect.height};
    paint.DrawRect(panel, Argb(kCompletionPanelBackground), CornerRadii(12.0F));
    paint.DrawBorder(panel, Argb(kCompletionPanelBorder), 1.0F, CornerRadii(12.0F));

    const float content_left = rect.origin.x + kCompletionPanelPaddingH + kCompletionRowPaddingH;
    float row_center_y = rect.origin.y + kCompletionPanelPaddingV + kCompletionRowHeight * 0.5F;
    const size_t rows = std::min(completion_.items.size(), kCompletionMaxVisible);
    for (size_t i = 0; i < rows; ++i) {
      const CompletionItem& item = completion_.items[i];
      const bool selected = (i == completion_.selected);
      if (selected) {
        paint.DrawRect(
            Rect{rect.origin.x + kCompletionPanelPaddingH, row_center_y - kCompletionRowHeight * 0.5F,
                 rect.width - kCompletionPanelPaddingH * 2.0F, kCompletionRowHeight},
            Argb(kCompletionSelectedBackground),
            CornerRadii(6.0F));
      }

      // Kind badge: 18x18 rounded square with a single bold letter.
      const float badge_center_y = row_center_y;
      const float badge_x = content_left;
      paint.DrawRect(
          Rect{badge_x, badge_center_y - kCompletionBadgeSize * 0.5F, kCompletionBadgeSize, kCompletionBadgeSize},
          Argb(CompletionKindColor(item.kind)),
          CornerRadii(4.0F));
      const std::string letter = CompletionKindLetter(item.kind);
      const TextStyle badge_style{Font::System(kCompletionBadgeSizePx).WithWeight(FontWeight::Bold), Argb(0xFFFFFFFF),
                                  TextDecoration::None};
      const float badge_baseline = CenterBaseline(badge_center_y, completion_badge_metrics_);
      const float badge_text_x = badge_x + (kCompletionBadgeSize - completion_badge_char_advance_) * 0.5F;
      paint.DrawTextRun(
          Rect{badge_text_x, badge_center_y - kCompletionBadgeSize * 0.5F, kCompletionBadgeSize, kCompletionBadgeSize},
          Point{badge_text_x, badge_baseline},
          letter,
          badge_style);

      // Label, then a right-aligned detail when space allows.
      const float label_x = content_left + kCompletionBadgeSize + kCompletionBadgeGap;
      const float label_baseline = CenterBaseline(badge_center_y, completion_label_metrics_);
      paint.DrawTextRun(
          Rect{label_x, row_center_y - kCompletionRowHeight * 0.5F,
               rect.width - (label_x - rect.origin.x) - kCompletionRowPaddingH, kCompletionRowHeight},
          Point{label_x, label_baseline},
          item.label,
          TextStyle{Font::System(kCompletionLabelSize), Argb(kCompletionLabelColor), TextDecoration::None});

      if (!item.detail.empty()) {
        // Estimated advance (reference renders an 11sp detail at the row end).
        const float detail_advance = kCompletionDetailSize * 0.55F * static_cast<float>(item.detail.size());
        const float detail_right = rect.origin.x + rect.width - kCompletionPanelPaddingH - kCompletionRowPaddingH;
        const float detail_x = detail_right - detail_advance;
        if (detail_x > label_x + kCompletionDetailGap) {
          const float detail_baseline = CenterBaseline(badge_center_y, completion_detail_metrics_);
          paint.DrawTextRun(
              Rect{detail_x, row_center_y - kCompletionRowHeight * 0.5F, detail_advance, kCompletionRowHeight},
              Point{detail_x, detail_baseline},
              item.detail,
              TextStyle{Font::System(kCompletionDetailSize), Argb(kCompletionDetailColor), TextDecoration::None});
        }
      }

      row_center_y += kCompletionRowHeight;
    }
  }

  std::shared_ptr<se::EditorCore> core_;
  std::shared_ptr<se::Document> document_;
  std::shared_ptr<SweetLineHighlighter> highlighter_;
  std::shared_ptr<SweetEditorTextInputClient> text_input_client_;
  std::function<void()> invalidate_;
  se::Size viewport_;
  se::IntRange last_visible_range_{0, -1};
  FontMetrics font_metrics_;
  float font_size_;
  float char_width_;
  CompletionProvider completion_provider_;
  std::function<bool(const std::string&)> completion_trigger_characters_;
  FontMetrics completion_label_metrics_;
  FontMetrics completion_detail_metrics_;
  FontMetrics completion_badge_metrics_;
  float completion_badge_char_advance_{0.0F};

  std::function<void(const std::string&)> on_link_click_;
  std::function<void(int32_t)> on_codelens_click_;
  std::function<void(uint32_t, int32_t)> on_gutter_icon_click_;
  std::function<void(uint32_t, uint32_t)> on_inlay_click_;
  std::function<void()> on_text_changed_;
  std::function<void(uint32_t, uint32_t)> on_cursor_changed_;
  std::function<void(uint32_t, uint32_t, uint32_t, uint32_t)> on_selection_changed_;
  std::function<void(float, float)> on_scroll_changed_;
  std::function<void(size_t)> on_fold_toggle_;
  std::function<void(uint32_t, uint32_t)> on_long_press_;
  std::function<void(uint32_t, uint32_t)> on_double_tap_;
  std::function<std::string(uint32_t, uint32_t)> newline_action_;
  std::function<std::string(uint32_t)> phantom_text_provider_;
  bool accept_phantom_on_tab_{true};
  std::function<void()> on_toggle_search_;
  std::string current_diff_original_;
  int current_wrap_mode_{0};
  bool current_sticky_gutter_{false};
  std::map<uint32_t, std::string> cached_phantom_;

  struct CompletionState {
    bool visible{false};
    bool request_active{false};
    std::vector<CompletionItem> items;
    size_t selected{0};
    CompletionContext::TriggerKind last_trigger_kind{CompletionContext::TriggerKind::Invoked};
    std::string last_trigger_char;
    se::Rect panel_rect{};
    se::PointF last_cursor{};
    float last_cursor_height{0.0F};
  };
  CompletionState completion_;

  // Context menu (double-tap / long-press): a small floating panel with
  // clipboard and selection actions, drawn on the canvas like the completion
  // panel (no host overlay needed).
  struct ContextMenuEntry {
    const char* label;
    int command;  // 0=cut, 1=copy, 2=paste, 3=select-all, 4=find
  };
  struct ContextMenuState {
    bool visible{false};
    se::PointF anchor{};
    se::Rect panel_rect{};
    std::vector<ContextMenuEntry> entries;
  };
  ContextMenuState context_menu_;

  bool focused_{false};
  bool blink_on_{true};
  // Core-managed animations (fling momentum / edge scroll) are pending.
  bool animation_pending_{false};
  se::EditorRenderModel cached_model_;
  bool model_dirty_{true};
  bool decorations_pending_{false};
  // Scrollbar thumb dragging state.
  bool scrollbar_dragging_{false};
  bool scrollbar_drag_vertical_{true};
  float scrollbar_drag_offset_{0.0F};
  se::ScrollbarModel cached_scrollbar_v_;
  se::ScrollbarModel cached_scrollbar_h_;
};

}  // namespace

View SweetEditor(SweetEditorOptions options) {
  auto holder_state = UseState<std::shared_ptr<EditorHolder>>(nullptr);
  auto revision = UseState(0);
  auto loaded_key = UseState<std::string>(std::string(options.document_key));
  // Hooks must be called unconditionally on every recomposition with a stable
  // order; calling UseTextMeasurer/UseRawResource only inside the rebuild
  // branch shifts state slots when the document key changes and crashes.
  TextMeasurer& measurer = UseTextMeasurer();
  const RawAsset cpp_syntax = UseRawResource(RawResource("app", "raw/syntaxes/cpp.json"));
  // Find/replace bar state (hooks must stay unconditional across recomposes).
  auto search_visible = UseState(false);
  auto search_text = UseState(std::string());
  auto replace_text = UseState(std::string());

  if (!holder_state.Get() || loaded_key.Get() != options.document_key) {
    loaded_key = options.document_key;
    const std::string syntax_json =
        !options.syntax_json.empty() ? options.syntax_json : cpp_syntax.ToString();
    if (!options.on_toggle_search) {
      options.on_toggle_search = [search_visible] { search_visible = !search_visible.Get(); };
    }
    holder_state = std::make_shared<EditorHolder>(
        measurer, options, syntax_json, [revision] { revision += 1; });
  }

  const std::shared_ptr<EditorHolder> holder = holder_state.Get();
  // Read the revision so this scope recomposes (and repaints) after every input.
  static_cast<void>(revision.Get());
  // Diff toggle: the host changes options.original_text without a new
  // document_key, so sync it into the holder (keeps live edits intact).
  if (options.original_text != holder->CurrentDiffOriginal()) {
    holder->SetDiffOriginal(options.original_text);
  }
  // Display toggles (wrap / sticky gutter) change without a rebuild.
  holder->SyncDisplayOptions(options.wrap_mode, options.sticky_gutter);

  // Cursor blink driven by a recurring delayed task: repaint-only frames have
  // no next tick, so a phase check inside Render would never blink. The task
  // toggles visibility every 500 ms; failures are swallowed so a broken task
  // cannot blank the app.
  auto blink_launched = UseState(false);
  if (!blink_launched.Get()) {
    blink_launched = true;
    try {
      TaskScope tasks = UseTaskScope();
      // Retain the handle through state so the recurring task stays alive.
      static_cast<void>(UseState<huxerui::TaskHandle>(tasks.Launch([holder]() -> Task<void> {
        while (true) {
          co_await Delay(500ms);
          holder->TickBlink();
        }
      })));
    } catch (const std::exception&) {
      // No task dispatcher (e.g. headless platform): fall back to a static caret.
    }
  }

  View editor = Canvas([holder](PaintContext& paint, Size size) { holder->Render(paint, size); })
      .On<ViewEvents::PointerDown>([holder, revision](const PointerEvent& event) {
        if (holder->HandlePointer(event)) {
          revision += 1;
        }
      })
      .On<ViewEvents::PointerMove>([holder, revision](const PointerEvent& event) {
        if (holder->HandlePointer(event)) {
          revision += 1;
        }
      })
      .On<ViewEvents::PointerUp>([holder, revision](const PointerEvent& event) {
        if (holder->HandlePointer(event)) {
          revision += 1;
        }
      })
      .On<ViewEvents::PointerCancel>([holder, revision](const PointerEvent& event) {
        if (holder->HandlePointer(event)) {
          revision += 1;
        }
      })
      .On<ViewEvents::KeyDown>([holder, revision](const KeyEvent& event) {
        if (holder->HandleKey(event)) {
          revision += 1;
        }
      })
      .On<ViewEvents::Scroll>([holder, revision](const ScrollEvent& event) {
        if (holder->HandleScroll(event)) {
          revision += 1;
        }
      })
      .On<ViewEvents::FocusChanged>([holder](bool focused) { holder->SetFocused(focused); })
      .With(SweetEditorInputModifier{holder->TextInputClient()}, Focusable{}, Grow{});

  if (!search_visible.Get()) {
    return editor;
  }

  return Column {
    Row {
      TextField(TextEditingValue::FromText(search_text.Get()))
          .Placeholder("Find")
          .OnChanged([search_text, holder](const TextEditingValue& value) {
            search_text = value.text;
            holder->RunSearch(value.text);
          })
          .OnSubmitted([holder] { holder->FindNext(); })
          .With(Grow{}),
      Button("Prev").On<ViewEvents::Click>([holder] { holder->FindPrevious(); }),
      Button("Next").On<ViewEvents::Click>([holder] { holder->FindNext(); }),
    }.With(Spacing(4.0F), Padding(4.0F)),
    Row {
      TextField(TextEditingValue::FromText(replace_text.Get()))
          .Placeholder("Replace")
          .OnChanged([replace_text](const TextEditingValue& value) { replace_text = value.text; })
          .With(Grow{}),
      Button("Replace").On<ViewEvents::Click>([holder, replace_text] {
        holder->ReplaceCurrent(replace_text.Get());
      }),
      Button("All").On<ViewEvents::Click>([holder, replace_text] { holder->ReplaceAll(replace_text.Get()); }),
      Button("Close").On<ViewEvents::Click>([search_visible, holder] {
        holder->CloseSearch();
        search_visible = false;
      }),
    }.With(Spacing(4.0F), Padding(4.0F)),
    editor,
  }.With(CrossAlign(CrossAxisAlignment::Stretch));
}

}  // namespace sweetedit_huxer
