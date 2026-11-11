#include "text_input_bridge.h"

#include <algorithm>
#include <utility>

namespace huxerui::codeeditor::detail {
namespace {

namespace se = sweeteditor;

TextAffinity Affinity(se::CaretAffinity affinity) {
  return affinity == se::CaretAffinity::UPSTREAM ? TextAffinity::Upstream : TextAffinity::Downstream;
}

std::optional<huxerui::TextRange> CompositionRange(const se::ImeOffsetRange& range) {
  if (range.coordinate_space != se::ImeCoordinateSpace::DOCUMENT ||
      range.start_utf16 < 0 || range.end_utf16 < range.start_utf16) {
    return std::nullopt;
  }
  return huxerui::TextRange{range.start_utf16, range.end_utf16};
}

TextInputResultCode ResultCode(se::ImeResultCode code) {
  switch (code) {
  case se::ImeResultCode::OK: return TextInputResultCode::Ok;
  case se::ImeResultCode::SESSION_MISMATCH: return TextInputResultCode::SessionMismatch;
  case se::ImeResultCode::READ_ONLY: return TextInputResultCode::ReadOnly;
  case se::ImeResultCode::REJECTED: return TextInputResultCode::Rejected;
  }
  return TextInputResultCode::Rejected;
}

std::optional<se::ImeCommand> ToCoreCommand(const TextInputCommand& command) {
  se::ImeCommand result;
  switch (command.kind) {
  case TextInputCommandKind::SetSelection:
    result.kind = se::ImeCommandKind::SET_SELECTION;
    break;
  case TextInputCommandKind::BeginComposition:
    result.kind = se::ImeCommandKind::BEGIN_COMPOSITION;
    break;
  case TextInputCommandKind::UpdateComposition:
    result.kind = se::ImeCommandKind::UPDATE_COMPOSITION;
    break;
  case TextInputCommandKind::CommitText:
    result.kind = se::ImeCommandKind::COMMIT_TEXT;
    break;
  case TextInputCommandKind::FinishComposition:
    result.kind = se::ImeCommandKind::FINISH_COMPOSITION;
    break;
  case TextInputCommandKind::CancelComposition:
    result.kind = se::ImeCommandKind::CANCEL_COMPOSITION;
    break;
  case TextInputCommandKind::DeleteSurrounding:
    result.kind = se::ImeCommandKind::DELETE_SURROUNDING;
    break;
  default:
    return std::nullopt;
  }
  if (command.target) {
    if (!command.target->IsValid()) {
      return std::nullopt;
    }
    result.target_range = {
        command.coordinate_space == TextInputCoordinateSpace::Composition
            ? se::ImeCoordinateSpace::COMPOSITION : se::ImeCoordinateSpace::DOCUMENT,
        command.target->start, command.target->end,
    };
  }
  if (command.selection_after) {
    const auto selection = *command.selection_after;
    if (selection.anchor < 0 || selection.active < 0) {
      return std::nullopt;
    }
    result.selection_after = {
        se::ImeCoordinateSpace::DOCUMENT, selection.anchor, selection.active,
        selection.affinity == TextAffinity::Upstream ? se::CaretAffinity::UPSTREAM : se::CaretAffinity::DOWNSTREAM,
    };
  }
  result.text = command.text;
  result.delete_before = command.delete_before;
  result.delete_after = command.delete_after;
  result.text_unit = command.delete_unit == TextInputUnit::UnicodeCodePoint
      ? se::ImeTextUnit::UNICODE_CODE_POINT : se::ImeTextUnit::UTF16_CODE_UNIT;
  return result;
}

}  // namespace

TextInputBridge::TextInputBridge(std::shared_ptr<se::EditorCore> core, std::shared_ptr<se::Document> document,
                                 Dispatch dispatch, TextKey key, HitTest hit_test, RangeRects range_rects)
    : core_(std::move(core)), document_(std::move(document)), dispatch_(std::move(dispatch)),
      key_(std::move(key)), hit_test_(std::move(hit_test)), range_rects_(std::move(range_rects)) {}

void TextInputBridge::Detach() {
  if (!dispatch_) return;
  EndCoreSession();
  // UiWindow can read the outgoing state while it switches input clients.
  retired_state_ = State();
  dispatch_ = {};
  key_ = {};
  hit_test_ = {};
  range_rects_ = {};
  document_.reset();
  core_.reset();
}

void TextInputBridge::OnAction(const se::EditorActionResult& result) {
  const bool content_changed = !result.text_changes.empty();
  if (content_changed) {
    ++content_revision_;
  }
  if (content_changed || result.cursor_changed || result.selection_changed || result.composition_changed ||
      result.scroll_changed || result.scale_changed || result.ime_host_action != se::ImeHostAction::NONE ||
      result.needs_redraw) {
    ++revision_;
  }
  if (result.ime_host_action == se::ImeHostAction::CLOSE_SESSION ||
      result.ime_host_action == se::ImeHostAction::RESTART_SESSION) {
    core_session_id_ = 0;
  }
}

void TextInputBridge::SetCaretWidth(float width) {
  if (caret_width_ != width) {
    caret_width_ = width;
    ++revision_;
  }
}

TextInputConfiguration TextInputBridge::Configuration() const {
  return {
      .action = TextInputAction::Newline,
      .multiline = true,
      .autocorrect = false,
      .read_only = !dispatch_ || core_->isReadOnly(),
      .keyboard_policy = SoftwareKeyboardPolicy::Manual,
  };
}

TextInputState TextInputBridge::State() const {
  if (retired_state_) return *retired_state_;
  TextInputState result{session_id_, revision_, content_revision_};
  if (core_session_id_ != 0) {
    const auto ime = core_->getImeState(core_session_id_);
    if (ime.result_code == se::ImeResultCode::OK) {
      result.selection = {ime.selection.anchor_utf16, ime.selection.active_utf16, Affinity(ime.selection.affinity)};
      result.composition = CompositionRange(ime.composition_range);
      return result;
    }
  }
  if (!document_) return result;
  const auto selection = core_->getSelection();
  result.selection = {
      static_cast<TextOffset>(document_->getCharIndexFromPosition(selection.start)),
      static_cast<TextOffset>(document_->getCharIndexFromPosition(selection.end)),
      Affinity(core_->getCaretAffinity()),
  };
  return result;
}

bool TextInputBridge::Matches(TextInputSessionId session) const noexcept {
  return dispatch_ && document_ && session != 0 && session == session_id_;
}

bool TextInputBridge::BeginCoreSession() {
  if (core_session_id_ != 0) {
    return true;
  }
  const auto state = core_->beginImeSession(se::ImeMutationModel::COMMAND);
  if (state.result_code != se::ImeResultCode::OK) {
    return false;
  }
  core_session_id_ = state.session_id;
  return true;
}

void TextInputBridge::EndCoreSession() {
  const auto session = std::exchange(core_session_id_, 0);
  if (session != 0) {
    const auto result = core_->endImeSession(session);
    if (dispatch_) {
      dispatch_(result);
    }
  }
}

TextInputState TextInputBridge::BeginTextInput(TextInputSessionId session) {
  if (!dispatch_) return TextInputState{session, revision_, content_revision_};
  EndCoreSession();
  session_id_ = session;
  if (dispatch_ && document_ && !core_->isReadOnly()) BeginCoreSession();
  return State();
}

void TextInputBridge::EndTextInput(TextInputSessionId session, TextInputEndReason) {
  if (session == 0 || session != session_id_) return;
  session_id_ = 0;
  retired_state_.reset();
  EndCoreSession();
}

TextInputApplyResult TextInputBridge::ApplyTextInput(const TextInputCommandBatch& batch) {
  if (!Matches(batch.session_id)) {
    return {TextInputResultCode::SessionMismatch};
  }
  if (core_->isReadOnly()) {
    return {TextInputResultCode::ReadOnly};
  }
  if (batch.commands.empty() || !BeginCoreSession()) {
    return {TextInputResultCode::Rejected};
  }
  se::ImeCommandBatch request;
  request.session_id = core_session_id_;
  request.commands.reserve(batch.commands.size());
  for (const auto& command : batch.commands) {
    auto mapped = ToCoreCommand(command);
    if (!mapped) {
      return {TextInputResultCode::Rejected};
    }
    request.commands.push_back(std::move(*mapped));
  }
  const auto result = core_->applyImeCommands(request);
  dispatch_(result);
  const auto code = ResultCode(result.ime_state.result_code);
  if (code != TextInputResultCode::Ok) {
    return {code};
  }
  switch (result.ime_host_action) {
  case se::ImeHostAction::RESTART_SESSION:
    BeginCoreSession();
    return {code, TextInputSyncAction::Restart};
  case se::ImeHostAction::SYNC_EDITING_STATE:
    return {code, TextInputSyncAction::Update};
  default:
    return {code};
  }
}

TextInputContext TextInputBridge::QueryTextInputContext(
    TextInputSessionId session, TextOffset start, TextOffset length
) const {
  if (!Matches(session)) {
    return {TextInputResultCode::SessionMismatch, session};
  }
  if (start < 0 || length < 0 || core_session_id_ == 0) {
    return {TextInputResultCode::Rejected, session};
  }
  const auto context = core_->getImeContext(core_session_id_, se::ImeTextSource::EDITING, start, length);
  const auto state = State();
  return {ResultCode(context.result_code), session, context.slice_start_utf16, context.total_length_utf16,
          context.text, state.selection, state.composition};
}

TextInputGeometry TextInputBridge::QueryTextInputGeometry(TextInputSessionId session, huxerui::TextRange range) const {
  if (!Matches(session)) {
    return {TextInputResultCode::SessionMismatch, session};
  }
  if (!range.IsValid()) {
    return {TextInputResultCode::Rejected, session};
  }
  const auto to_position = [&](TextOffset offset) -> std::optional<se::TextPosition> {
    const auto index = static_cast<std::size_t>(offset);
    const auto position = document_->getPositionFromCharIndex(index);
    if (document_->getCharIndexFromPosition(position) != index) return std::nullopt;
    const auto& text = document_->getLineU16TextRef(position.line);
    if (position.column < text.size() && text[position.column] >= 0xDC00 && text[position.column] <= 0xDFFF) {
      return std::nullopt;
    }
    return position;
  };
  const auto start = to_position(range.start);
  const auto end = to_position(range.end);
  if (!start || !end) return {TextInputResultCode::Rejected, session};
  const auto caret = *end == core_->getCursorPosition() ? core_->getCursorScreenRect() : core_->getPositionScreenRect(*end);
  if (caret.height <= 0) return {TextInputResultCode::Rejected, session};
  TextInputGeometry result{TextInputResultCode::Ok, session, {caret.x, caret.y, caret_width_, caret.height}};
  if (!range.IsCollapsed()) result.range_rects = range_rects_({*start, *end});
  return result;
}

TextInputPositionResult TextInputBridge::QueryTextInputPosition(TextInputSessionId session, Point point) const {
  if (!Matches(session)) {
    return {TextInputResultCode::SessionMismatch, session};
  }
  const auto position = hit_test_(point);
  if (!position) {
    return {TextInputResultCode::Rejected, session};
  }
  return {TextInputResultCode::Ok, session,
          {static_cast<TextOffset>(document_->getCharIndexFromPosition(position->first)), Affinity(position->second)}};
}

TextInputKeyResult TextInputBridge::HandleTextKey(const KeyEvent& event) {
  return key_ && key_(event) ? TextInputKeyResult::Handled : TextInputKeyResult::Unhandled;
}

}  // namespace huxerui::codeeditor::detail
