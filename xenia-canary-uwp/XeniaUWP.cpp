#include "XeniaUWP.h"

#include "UWPUtil.h"
#include "WinRTKeyboard.h"

#include <fstream>

#include "windowed_app_context_uwp.h"
#include "surface_uwp.h"
#include "window_uwp.h"

#include "third_party/imgui/imgui.h"

#include "xenia/emulator.h"
#include "xenia/base/filesystem.h"
#include "xenia/ui/windowed_app.h"
#include "xenia/base/cvar.h"
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
  if (!driver) return;

  hid::X_INPUT_STATE state;
  if (driver->GetState(0, &state) != X_STATUS_SUCCESS) return;

  const uint16_t buttons = state.gamepad.buttons;

  io.AddKeyEvent(ImGuiKey_GamepadFaceDown,   (buttons & X_INPUT_GAMEPAD_A) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadFaceRight,  (buttons & X_INPUT_GAMEPAD_B) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadFaceLeft,   (buttons & X_INPUT_GAMEPAD_X) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadFaceUp,     (buttons & X_INPUT_GAMEPAD_Y) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadStart,      (buttons & X_INPUT_GAMEPAD_START) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadBack,       (buttons & X_INPUT_GAMEPAD_BACK) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadL1,         (buttons & X_INPUT_GAMEPAD_LEFT_SHOULDER) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadR1,         (buttons & X_INPUT_GAMEPAD_RIGHT_SHOULDER) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadL3,         (buttons & X_INPUT_GAMEPAD_LEFT_THUMB) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadR3,         (buttons & X_INPUT_GAMEPAD_RIGHT_THUMB) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadDpadLeft,   (buttons & X_INPUT_GAMEPAD_DPAD_LEFT) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadDpadRight,  (buttons & X_INPUT_GAMEPAD_DPAD_RIGHT) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadDpadUp,     (buttons & X_INPUT_GAMEPAD_DPAD_UP) != 0);
  io.AddKeyEvent(ImGuiKey_GamepadDpadDown,   (buttons & X_INPUT_GAMEPAD_DPAD_DOWN) != 0);

  // Left stick — threshold at ~25% (8000/32767)
  constexpr float kStickDeadzone = 8000.0f / 32767.0f;
  const float lx = state.gamepad.thumb_lx / 32767.0f;
  const float ly = state.gamepad.thumb_ly / 32767.0f;
  io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickLeft,  lx < -kStickDeadzone, lx < 0 ? -lx : 0.0f);
  io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickRight, lx >  kStickDeadzone, lx > 0 ?  lx : 0.0f);
  io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickUp,    ly >  kStickDeadzone, ly > 0 ?  ly : 0.0f);
  io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickDown,  ly < -kStickDeadzone, ly < 0 ? -ly : 0.0f);

  // Right stick
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
    for (auto file : std::filesystem::directory_iterator(path)) {
      if (file.is_directory() && file.path().string() != path) {
        RecurseFolderForGames(file.path().string());
        continue;
      }

      if (!file.is_regular_file()) continue;

      switch (xe::GetFileSignature(file.path())) {
        case xe::Emulator::FileSignatureType::XEX1:
        case xe::Emulator::FileSignatureType::XEX2: {
          std::string filename = "default";
          if (_stricmp(file.path().filename().string().c_str(),
                       "default.xex") == 0) {
            if (file.path().has_parent_path())
              filename = file.path().parent_path().filename().string();
          } else {
            filename = file.path().stem().string();
          }

          s_games.push_back({file.path().string(), filename});
          break;
        }
        case xe::Emulator::FileSignatureType::CON:
        case xe::Emulator::FileSignatureType::PIRS:
        case xe::Emulator::FileSignatureType::ZAR: {
          std::string filename = file.path().stem().string();
          s_games.push_back({file.path().string(), filename});
          break;
        }
        case xe::Emulator::FileSignatureType::LIVE: {
          std::ifstream in(file.path().string(), std::ios::binary);

          in.seekg(0x412);

          char data[32];
          for (int i = 0; i < 32; i++) {
            char c;
            in.read(&c, 2);
            std::wctomb(&data[i], static_cast<wchar_t>(c));

            if (c == 0) break;
          }

          s_games.push_back({file.path().string(), data});

          in.close();
        }
        default:
          continue;
      }
    }
  } catch (std::exception) {
    // This folder can't be opened.
  }
}

void UWP::RefreshPaths() {
  s_paths.clear();
  s_games.clear();

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