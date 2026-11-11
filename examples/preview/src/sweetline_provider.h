#pragma once

#include <cstdint>

#include <huxerui/codeeditor.h>

namespace demo {

inline constexpr std::int32_t kTypeGutterIcon = 1;
inline constexpr std::int32_t kBreakpointGutterIcon = 2;
inline constexpr std::int32_t kAnnotationGutterIcon = 3;

// SweetLine supplies syntax and adornments for the preview.
class SweetLineDecorationProvider final : public huxerui::codeeditor::DecorationProvider {
 public:
  using GutterIconSource =
      std::function<std::vector<std::pair<std::size_t, std::int32_t>>(std::size_t start_line, std::size_t end_line)>;

  SweetLineDecorationProvider(std::string syntax_json, std::string document_key, GutterIconSource gutter_icons);

  std::set<huxerui::codeeditor::DecorationType> GetCapabilities() const override;
  huxerui::Task<void> ProvideDecorations(huxerui::codeeditor::DecorationContext context,
                                        huxerui::codeeditor::DecorationReceiver receiver) override;

 private:
  struct Analysis;
  std::shared_ptr<Analysis> analysis_;
  huxerui::WorkerSequence worker_;
  GutterIconSource gutter_icons_;
};

}  // namespace demo
