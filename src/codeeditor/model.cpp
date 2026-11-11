#include <huxerui/codeeditor.h>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

#include <sweeteditor/buffer.h>
#include <sweeteditor/utility.h>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace huxerui::codeeditor {
namespace {

#if defined(__ANDROID__) || (defined(__APPLE__) && TARGET_OS_IOS)
constexpr bool kMobilePlatform = true;
#else
constexpr bool kMobilePlatform = false;
#endif

}  // namespace

bool EditorSettings::DefaultGutterSticky() {
  return !kMobilePlatform;
}

ScrollbarConfig EditorSettings::DefaultScrollbarConfig() {
  if constexpr (kMobilePlatform) {
    return {
        .thickness = 5.0F,
        .min_thumb = 40.0F,
        .thumb_hit_padding = 20.0F,
        .mode = sweeteditor::ScrollbarMode::TRANSIENT,
        .thumb_draggable = true,
        .track_tap_mode = sweeteditor::ScrollbarTrackTapMode::DISABLED,
        .fade_delay_ms = 700,
        .fade_duration_ms = 300,
    };
  }
  return {
      .thickness = 6.0F,
      .mode = sweeteditor::ScrollbarMode::ALWAYS,
      .thumb_draggable = true,
      .track_tap_mode = sweeteditor::ScrollbarTrackTapMode::JUMP,
  };
}

sweeteditor::EditorOptions EditorOptions::DefaultCoreOptions() {
  return {.reveal_selection_end_on_select_all = kMobilePlatform};
}

Document::Document(std::string text)
    : Document(std::make_shared<sweeteditor::LineArrayDocument>(std::move(text))) {}

Document::Document(std::shared_ptr<sweeteditor::Document> core) : core_(std::move(core)) {}

Document Document::FromFile(const File& file) {
  auto buffer = std::make_unique<sweeteditor::MappedFileBuffer>(file.Path());
  return Document(std::make_shared<sweeteditor::PieceTableDocument>(std::move(buffer)));
}

std::size_t Document::GetLineCount() const {
  return core_ ? core_->getLineCount() : 0;
}

std::string Document::GetLineText(std::size_t line) const {
  const std::size_t count = GetLineCount();
  if (count == 0) {
    return {};
  }
  std::string text;
  sweeteditor::StrUtil::convertUTF16ToUTF8(core_->getLineU16Text(std::min(line, count - 1)), text);
  return text;
}

std::string Document::GetText() const {
  return core_ ? core_->getU8Text() : std::string{};
}

void EditorKeyMap::AddBinding(KeyBinding binding) {
  if (binding.first.empty()) {
    throw std::invalid_argument("HuxerUI CodeEditor key bindings need a first chord");
  }
  std::erase_if(bindings_, [&](const KeyBinding& existing) {
    return existing.first == binding.first &&
           (existing.second.empty() || binding.second.empty() || existing.second == binding.second);
  });
  bindings_.push_back(binding);
}

EditorCommandId EditorKeyMap::RegisterCommand(KeyBinding binding, CommandHandler handler) {
  if (!handler) {
    throw std::invalid_argument("HuxerUI CodeEditor key bindings need a command handler");
  }
  if (binding.command == static_cast<EditorCommandId>(EditorBuiltinCommand::NONE)) {
    while (commands_.contains(next_custom_command_)) {
      if (next_custom_command_ == std::numeric_limits<EditorCommandId>::max()) {
        throw std::overflow_error("HuxerUI CodeEditor custom command IDs exhausted");
      }
      ++next_custom_command_;
    }
    binding.command = next_custom_command_;
  }
  if (binding.command <= sweeteditor::EDITOR_BUILTIN_COMMAND_MAX) {
    throw std::invalid_argument("HuxerUI CodeEditor custom command IDs must follow the built-in range");
  }
  AddBinding(binding);
  commands_.insert_or_assign(binding.command, std::move(handler));
  return binding.command;
}

EditorKeyMap::CommandHandler EditorKeyMap::GetCommand(EditorCommandId command) const {
  const auto found = commands_.find(command);
  return found != commands_.end() ? found->second : CommandHandler{};
}

EditorKeyMap EditorKeyMap::DefaultKeyMap() {
  return VSCode();
}

EditorKeyMap EditorKeyMap::VSCode() {
  const auto core = sweeteditor::KeyMap::createDefault();
  constexpr KeyCode keys[] = {
      KeyCode::LEFT, KeyCode::RIGHT, KeyCode::UP, KeyCode::DOWN,
      KeyCode::HOME, KeyCode::END, KeyCode::PAGE_UP, KeyCode::PAGE_DOWN,
      KeyCode::BACKSPACE, KeyCode::DELETE_KEY, KeyCode::TAB, KeyCode::ENTER,
      KeyCode::SPACE, KeyCode::A, KeyCode::C, KeyCode::V, KeyCode::X,
      KeyCode::Y, KeyCode::Z, KeyCode::K,
  };
  const KeyModifier modifiers[] = {
      KeyModifier::NONE, KeyModifier::SHIFT, KeyModifier::CTRL, KeyModifier::META, KeyModifier::ALT,
      KeyModifier::SHIFT | KeyModifier::CTRL, KeyModifier::SHIFT | KeyModifier::META,
      KeyModifier::SHIFT | KeyModifier::ALT,
  };
  EditorKeyMap result;
  for (const KeyCode key : keys) {
    for (const KeyModifier modifier : modifiers) {
      const KeyChord chord{modifier, key};
      if (const auto* binding = core.lookup(chord)) {
        if (const auto* command = std::get_if<EditorCommandId>(binding)) {
          result.bindings_.push_back({chord, {}, *command});
        }
      }
    }
  }
  return result;
}

}  // namespace huxerui::codeeditor
