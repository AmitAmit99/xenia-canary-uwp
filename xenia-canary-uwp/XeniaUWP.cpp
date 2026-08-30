#include "XeniaUWP.h"

#include "UWPUtil.h"
#include "WinRTKeyboard.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>

#include "windowed_app_context_uwp.h"
#include "surface_uwp.h"
#include "window_uwp.h"

#include "third_party/imgui/imgui.h"

#include "xenia/emulator.h"
#include "xenia/base/filesystem.h"
#include "xenia/ui/windowed_app.h"
#include "xenia/base/cvar.h"
#include "xenia/base/clock.h"
#include "xenia/base/logging.h"
#include "xenia/ui/window.h"
#include "xenia/ui/d3d12/d3d12_provider.h"
#include "xenia/gpu/d3d12/d3d12_graphics_system.h"
#include "xenia/hid/xinput/xinput_hid.h"
#include "xenia/hid/nop/nop_hid.h"
#include "xenia/apu/xaudio2/xaudio2_audio_system.h"
#include "xenia/config.h"
#include "xenia/base/main_win.h"
#include "xenia/vfs/devices/disc_zarchive_device.h"

using namespace xe;
using namespace xe::hid;

DECLARE_string(gamepaths);
DEFINE_string(gamepaths, "", "Paths the frontend will search for games.",
              "General");

static std::unique_ptr<ui::WindowedApp> app = nullptr;
static std::unique_ptr<ui::UWPWindowedAppContext> app_context = nullptr;
static ui::Window* s_window;
static Emulator* s_emulator;
static std::vector<std::string> s_paths;
static std::vector<std::tuple<std::string, std::string>> s_games;
static std::vector<std::string> s_scanned_paths;

namespace {
constexpr uint64_t kAnalogNavInitialDelayMs = 275;
constexpr uint64_t kAnalogNavRepeatIntervalMs = 115;

std::string NormalizeScannedPath(const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::path normalized = std::filesystem::weakly_canonical(path, ec);
  if (ec) {
    normalized = path.lexically_normal();
  }

  std::string normalized_string = xe::path_to_utf8(normalized);
  std::replace(normalized_string.begin(), normalized_string.end(), '/', '\\');
  std::transform(normalized_string.begin(), normalized_string.end(),
                 normalized_string.begin(), [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  return normalized_string;
}

bool HasScannedDirectory(const std::string& normalized_path) {
  return std::find(s_scanned_paths.cbegin(), s_scanned_paths.cend(),
                   normalized_path) != s_scanned_paths.cend();
}

bool AddGameEntry(const std::filesystem::path& path, const std::string& name) {
  const std::string normalized_path = NormalizeScannedPath(path);
  auto existing = std::find_if(
      s_games.cbegin(), s_games.cend(), [&](const auto& game) {
        return NormalizeScannedPath(std::get<0>(game)) == normalized_path;
      });
  if (existing != s_games.cend()) {
    return false;
  }

  s_games.push_back({path.string(), name});
  return true;
}

enum class AnalogNavDirection { kLeft = 0, kRight, kUp, kDown };

struct AnalogNavRepeatState {
  bool active = false;
  bool repeating = false;
  uint64_t start_time_ms = 0;
  uint64_t last_emit_time_ms = 0;
};

std::array<AnalogNavRepeatState, 4> g_analog_nav_repeat_states;

bool UpdateAnalogNavRepeatState(AnalogNavDirection direction,
                                bool analog_active,
                                uint64_t now_ms) {
  auto& state = g_analog_nav_repeat_states[static_cast<size_t>(direction)];

  if (!analog_active) {
    state = {};
    return false;
  }

  if (!state.active) {
    state.active = true;
    state.start_time_ms = now_ms;
    state.last_emit_time_ms = now_ms;
    return true;
  }

  if (!state.repeating) {
    if (now_ms - state.start_time_ms >= kAnalogNavInitialDelayMs) {
      state.repeating = true;
      state.last_emit_time_ms = now_ms;
      return true;
    }
    return false;
  }

  if (now_ms - state.last_emit_time_ms >= kAnalogNavRepeatIntervalMs) {
    state.last_emit_time_ms = now_ms;
    return true;
  }

  return false;
}

void ResetAnalogNavRepeatStates() {
  for (auto& state : g_analog_nav_repeat_states) {
    state = {};
  }
}

}  // namespace

void UWP::StartXenia() {
  app_context = std::make_unique<ui::UWPWindowedAppContext>();
  app = xe::ui::GetWindowedAppCreator()(*app_context.get());

  xe::InitializeWin32App(app->GetName());

  if (app->OnInitialize()) {
    RefreshPaths();
    // to-do, remodel this so it doesn't instantly shutdown.
    //app->InvokeOnDestroy();
  }

  //xe::ShutdownWin32App();
}

void UWP::ExecutePendingFunctionsFromUIThread() {
  app_context->ExecutePendingFunctionsFromUIThread();

  if (s_window) {
    app_context->CallInUIThread([=]() { s_window->RequestPaint(); });
  }
}

void UWP::RegisterXeniaWindow(xe::ui::Window* window) { s_window = window; }

void UWP::UpdateImGuiIO() {
  ImGuiIO& io = ImGui::GetIO();
  io.AddKeyEvent(ImGuiKey_Backspace, false);
  io.AddKeyEvent(ImGuiKey_Enter, false);

  {
    std::unique_lock lk(UWP::g_buffer_mutex);
    for (uint32_t c : UWP::g_char_buffer) {
      io.AddInputCharacter(c);

      if (c == '\b') {
        io.AddKeyEvent(ImGuiKey_Backspace, true);
      } else if (c == '\r') {
        io.AddKeyEvent(ImGuiKey_Enter, true);
      }
    }
    UWP::g_char_buffer.clear();
  }

  auto driver = static_cast<xe::ui::UWPWindow*>(s_window)->xinputdriver();
  if (!driver) {
    ResetAnalogNavRepeatStates();
    return;
  }

  hid::X_INPUT_STATE state;
  if (driver->GetState(0, &state) != X_STATUS_SUCCESS) {
    ResetAnalogNavRepeatStates();
    return;
  }

  const uint16_t buttons = state.gamepad.buttons;

  io.AddKeyEvent(ImGuiKey_GamepadFaceDown,   (buttons & X_INPUT_GAMEPAD_A) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadFaceRight,  (buttons & X_INPUT_GAMEPAD_B) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadFaceLeft,   false);
  io.AddKeyEvent(ImGuiKey_F12,               (buttons & X_INPUT_GAMEPAD_X) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadFaceUp,     (buttons & X_INPUT_GAMEPAD_Y) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadStart,      (buttons & X_INPUT_GAMEPAD_START) != 0);

  io.AddKeyEvent(ImGuiKey_GamepadBack,       (buttons & X_INPUT_GAMEPAD_BACK) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadL1,         (buttons & X_INPUT_GAMEPAD_LEFT_SHOULDER) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadR1,         (buttons & X_INPUT_GAMEPAD_RIGHT_SHOULDER) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadL3,         (buttons & X_INPUT_GAMEPAD_LEFT_THUMB) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadR3,         (buttons & X_INPUT_GAMEPAD_RIGHT_THUMB) != 0);

  const int16_t kStickNavDeadzone = X_INPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
  const bool ls_left  = state.gamepad.thumb_lx <= -kStickNavDeadzone;
  const bool ls_right = state.gamepad.thumb_lx >=  kStickNavDeadzone;
  const bool ls_up    = state.gamepad.thumb_ly >=  kStickNavDeadzone;
  const bool ls_down  = state.gamepad.thumb_ly <= -kStickNavDeadzone;
  const uint64_t now_ms = Clock::QueryHostUptimeMillis();

  const bool analog_nav_left =
      UpdateAnalogNavRepeatState(AnalogNavDirection::kLeft, ls_left, now_ms);
  const bool analog_nav_right =
      UpdateAnalogNavRepeatState(AnalogNavDirection::kRight, ls_right, now_ms);
  const bool analog_nav_up =
      UpdateAnalogNavRepeatState(AnalogNavDirection::kUp, ls_up, now_ms);
  const bool analog_nav_down =
      UpdateAnalogNavRepeatState(AnalogNavDirection::kDown, ls_down, now_ms);

  io.AddKeyEvent(ImGuiKey_GamepadDpadLeft,
                 ((buttons & X_INPUT_GAMEPAD_DPAD_LEFT) != 0) ||
                     analog_nav_left);
  io.AddKeyEvent(ImGuiKey_GamepadDpadRight,
                 ((buttons & X_INPUT_GAMEPAD_DPAD_RIGHT) != 0) ||
                     analog_nav_right);
  io.AddKeyEvent(ImGuiKey_GamepadDpadUp,
                 ((buttons & X_INPUT_GAMEPAD_DPAD_UP) != 0) || analog_nav_up);
  io.AddKeyEvent(ImGuiKey_GamepadDpadDown,
                 ((buttons & X_INPUT_GAMEPAD_DPAD_DOWN) != 0) ||
                     analog_nav_down);

  // Right stick still exposed for camera/mouse emulation
  constexpr float kStickDeadzone = 8000.0f / 32767.0f;
  const float rx = state.gamepad.thumb_rx / 32767.0f;
  const float ry = state.gamepad.thumb_ry / 32767.0f;

  io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickLeft,  rx < -kStickDeadzone, rx < 0 ? -rx : 0.0f);
  io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickRight, rx >  kStickDeadzone, rx > 0 ?  rx : 0.0f);
  io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickUp,    ry >  kStickDeadzone, ry > 0 ?  ry : 0.0f);
  io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickDown,  ry < -kStickDeadzone, ry < 0 ? -ry : 0.0f);

  // Triggers as L2/R2
  constexpr float kTriggerDeadzone = 30.0f / 255.0f;
  const float lt = state.gamepad.left_trigger / 255.0f;
  const float rt = state.gamepad.right_trigger / 255.0f;
  io.AddKeyAnalogEvent(ImGuiKey_GamepadL2, lt > kTriggerDeadzone, lt);
  io.AddKeyAnalogEvent(ImGuiKey_GamepadR2, rt > kTriggerDeadzone, rt);
}

void RecurseFolderForGames(std::string path) {
  try {
    const std::string normalized_directory = NormalizeScannedPath(path);

    if (HasScannedDirectory(normalized_directory)) {
      return;
    }
    s_scanned_paths.push_back(normalized_directory);

    std::filesystem::path loose_xex_path;
    std::string loose_xex_name;
    bool has_loose_xex = false;

    // CON/PIRS/ZAR/LIVE-signed files sitting alongside a default.xex are
    // usually companion resources (e.g. a separately-packaged icon/art
    // asset, sometimes named things like "<TitleID>.nxeart"), not a second
    // real game -- STFS/XCONTENT packages can legitimately have those
    // signatures too. Deferred and only added if this folder turns out to
    // have no default.xex, so the game list shows one entry per folder
    // instead of the xex plus a bogus "game" for its own art asset.
    std::vector<std::pair<std::filesystem::path, std::string>>
        deferred_content_entries;

    for (auto file : std::filesystem::directory_iterator(path)) {
      if (file.is_directory() && file.path().string() != path) {
        RecurseFolderForGames(file.path().string());
        continue;
      }

      if (!file.is_regular_file()) continue;

      switch (xe::GetFileSignature(file.path())) {
        case xe::Emulator::FileSignatureType::XEX1:
        case xe::Emulator::FileSignatureType::XEX2: {
          const bool is_default_xex =
              _stricmp(file.path().filename().string().c_str(),
                       "default.xex") == 0;
          if (!is_default_xex) {
            break;
          }

          loose_xex_path = file.path();
          if (file.path().has_parent_path()) {
            loose_xex_name = file.path().parent_path().filename().string();
          } else {
            loose_xex_name = file.path().stem().string();
          }
          has_loose_xex = true;
          break;
        }
        case xe::Emulator::FileSignatureType::CON:
        case xe::Emulator::FileSignatureType::PIRS:
        case xe::Emulator::FileSignatureType::ZAR: {
          std::string filename = file.path().stem().string();
          deferred_content_entries.emplace_back(file.path(), filename);
          break;
        }

        case xe::Emulator::FileSignatureType::LIVE: {
          std::ifstream in(file.path().string(), std::ios::binary);

          in.seekg(0x412);

          // Zero-initialized, and the loop stops one short of the end, so
          // data[] is always null-terminated regardless of how the loop
          // below exits -- AddGameEntry() reads this as a C string.
          char data[32] = {};
          for (int i = 0; i < 31; i++) {
            // Title strings in STFS/XCONTENT headers are big-endian UTF-16;
            // read a full 2-byte code unit rather than 2 bytes into a
            // 1-byte char (which corrupted the adjacent stack byte).
            char bytes[2] = {0, 0};
            in.read(bytes, 2);
            if (!in) break;
            char16_t c = (static_cast<char16_t>(
                              static_cast<unsigned char>(bytes[0]))
                          << 8) |
                         static_cast<unsigned char>(bytes[1]);
            if (c == 0) break;

            if (wctomb_s(nullptr, &data[i], 1, static_cast<wchar_t>(c)) !=
                0) {
              // Can't be represented in a single byte (e.g. non-Latin
              // titles); wctomb_s leaves data[i] untouched on failure, so
              // without this data[i] would stay uninitialized.
              data[i] = '?';
            }
          }

          deferred_content_entries.emplace_back(file.path(), std::string(data));

          in.close();
        }
        default:
          continue;
      }
    }

    if (has_loose_xex) {
      AddGameEntry(loose_xex_path, loose_xex_name);
    } else {
      for (const auto& entry : deferred_content_entries) {
        AddGameEntry(entry.first, entry.second);
      }
    }
  } catch (std::exception) {
    // This folder can't be opened.
  }
}

void UWP::RefreshPaths() {
  s_paths.clear();
  s_games.clear();
  s_scanned_paths.clear();

  RecurseFolderForGames(UWP::GetLocalCache());

  if (!cvars::gamepaths.empty()) {
    std::stringstream ss (cvars::gamepaths);
    std::string item;
    while (std::getline(ss, item, ';')) {
      if (item.empty()) continue;

      RecurseFolderForGames(item);
      s_paths.push_back(item);
    }
  }

  std::sort(s_games.begin(), s_games.end(), [](auto& first, auto& second) {
    return std::get<1>(first) < std::get<1>(second);
  });
}

std::vector<std::tuple<std::string, std::string>> UWP::GetGames() {
  return s_games;
}

void UWP::SetGamePaths(std::vector<std::string> paths) {
  s_paths.clear();
  s_paths.insert(s_paths.end(), paths.begin(), paths.end());
  std::stringstream ss;
  for (auto p : s_paths) {
    ss << p << ";";
  }

  auto cpaths = dynamic_cast<cvar::ConfigVar<std::string>*>(
      cvar::ConfigVars->find("gamepaths")->second);
    cpaths->SetConfigValue(ss.str());
  config::SaveConfig();
  RefreshPaths();
}

std::vector<std::string> UWP::GetPaths() { 
  return s_paths;
}
