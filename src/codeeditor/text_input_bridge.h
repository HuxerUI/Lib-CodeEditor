#pragma once

#include <huxerui/text_input.h>
#include <sweeteditor/editor_core.h>

namespace huxerui::codeeditor::detail {

class TextInputBridge final : public TextInputClient {
 public:
  using Dispatch = std::function<void(const sweeteditor::EditorActionResult&)>;
  using TextKey = std::function<bool(const KeyEvent&)>;
  using Hit = std::pair<sweeteditor::TextPosition, sweeteditor::CaretAffinity>;
  using HitTest = std::function<std::optional<Hit>(Point)>;
  using RangeRects = std::function<std::vector<Rect>(const sweeteditor::TextRange&)>;

  TextInputBridge(std::shared_ptr<sweeteditor::EditorCore> core, std::shared_ptr<sweeteditor::Document> document,
                  Dispatch dispatch, TextKey key, HitTest hit_test, RangeRects range_rects);
  void Detach();
  void OnAction(const sweeteditor::EditorActionResult& result);
  void SetCaretWidth(float width);
  [[nodiscard]] bool IsActive() const noexcept { return dispatch_ && session_id_ != 0; }

  TextInputConfiguration Configuration() const override;
  TextInputState State() const override;
  TextInputState BeginTextInput(TextInputSessionId session) override;
  TextInputApplyResult ApplyTextInput(const TextInputCommandBatch& batch) override;
  TextInputContext QueryTextInputContext(TextInputSessionId session, TextOffset start, TextOffset length) const override;
  TextInputGeometry QueryTextInputGeometry(TextInputSessionId session, huxerui::TextRange range) const override;
  TextInputPositionResult QueryTextInputPosition(TextInputSessionId session, Point point) const override;
  TextInputKeyResult HandleTextKey(const KeyEvent& event) override;
  void EndTextInput(TextInputSessionId session, TextInputEndReason reason) override;

 private:
  bool BeginCoreSession();
  void EndCoreSession();
  [[nodiscard]] bool Matches(TextInputSessionId session) const noexcept;

  std::shared_ptr<sweeteditor::EditorCore> core_;
  std::shared_ptr<sweeteditor::Document> document_;
  Dispatch dispatch_;
  TextKey key_;
  HitTest hit_test_;
  RangeRects range_rects_;
  std::optional<TextInputState> retired_state_;
  TextInputSessionId session_id_ = 0;
  std::uint64_t core_session_id_ = 0;
  std::uint64_t revision_ = 0;
  std::uint64_t content_revision_ = 0;
  float caret_width_ = 2.0F;
};

}  // namespace huxerui::codeeditor::detail
