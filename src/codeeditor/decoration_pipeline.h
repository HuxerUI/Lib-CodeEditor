#pragma once

#include <huxerui/codeeditor/providers.h>

namespace huxerui::codeeditor::detail {

struct DecorationReceiverAccess {
  static DecorationReceiver Create(TaskScope scope, std::function<void(DecorationResult)> accept);
  static void Cancel(const DecorationReceiver& receiver);
};

class DecorationBatch {
 public:
  using Dispatch = std::function<void(const sweeteditor::EditorActionResult&)>;

  DecorationBatch(IntRange visible, std::vector<std::set<DecorationType>> capabilities);
  void Accept(std::size_t provider, DecorationResult result);
  void Apply(sweeteditor::EditorCore& core, const sweeteditor::Document& document, const Dispatch& dispatch);

  static void Clear(DecorationType type, sweeteditor::EditorCore& core, const Dispatch& dispatch);

 private:
  IntRange visible_;
  std::vector<std::set<DecorationType>> capabilities_;
  std::vector<DecorationResult> results_;
  std::set<DecorationType> changed_;
  DecorationResult base_;
  DecorationResult published_;
};

}  // namespace huxerui::codeeditor::detail
