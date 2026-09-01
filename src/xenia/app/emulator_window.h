/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_APP_EMULATOR_WINDOW_H_
#define XENIA_APP_EMULATOR_WINDOW_H_

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "xenia/app/profile_dialogs.h"
#include "xenia/emulator.h"
#include "xenia/gpu/command_processor.h"
#include "xenia/ui/imgui_dialog.h"
#include "xenia/ui/imgui_drawer.h"
#include "xenia/ui/immediate_drawer.h"
#include "xenia/ui/menu_item.h"
#include "xenia/ui/presenter.h"
#include "xenia/ui/window.h"
#include "xenia/ui/window_listener.h"
#include "xenia/ui/windowed_app_context.h"
#include "xenia/xbox.h"

namespace xe {
namespace app {

class ConsoleSettingsDialog;
class ContentListDialog;

struct RecentTitleEntry {
  std::string title_name;
  std::filesystem::path path_to_file;
  std::time_t last_run_time;
};

class EmulatorWindow {
 public:
  using steady_clock = std::chrono::steady_clock;  // stdlib steady clock

  enum : size_t {
    // The UI is on top of the game and is open in special cases, so
    // lowest-priority.
    kZOrderHidInput,
    kZOrderImGui,
    kZOrderProfiler,
    // Emulator window controls are expected to be always accessible by the
    // user, so highest-priority.
    kZOrderEmulatorWindowInput,
  };

  virtual ~EmulatorWindow();

  static std::unique_ptr<EmulatorWindow> Create(
      Emulator* emulator, ui::WindowedAppContext& app_context, uint32_t width,
      uint32_t height);

  std::unique_ptr<xe::threading::Thread> Gamepad_HotKeys_Listener;

  int32_t selected_title_index = -1;

  static constexpr int64_t diff_in_ms(
      const steady_clock::time_point t1,
      const steady_clock::time_point t2) noexcept {
    using ms = std::chrono::milliseconds;
    return std::chrono::duration_cast<ms>(t1 - t2).count();
  }

  steady_clock::time_point last_mouse_up = steady_clock::now();
  steady_clock::time_point last_mouse_down = steady_clock::now();

  Emulator* emulator() const { return emulator_; }
  ui::WindowedAppContext& app_context() const { return app_context_; }
  ui::Window* window() const { return window_.get(); }
  ui::ImGuiDrawer* imgui_drawer() const { return imgui_drawer_.get(); }

  ui::Presenter* GetGraphicsSystemPresenter() const;
  void SetupGraphicsSystemPresenterPainting();
  void ShutdownGraphicsSystemPresenterPainting();

  void OnEmulatorInitialized();

  xe::X_STATUS RunTitle(const std::filesystem::path& path_to_file);
  void UpdateTitle();
  void SetFullscreen(bool fullscreen);
  void ToggleFullscreen();
  void SetInitializingShaderStorage(bool initializing);

  void TakeScreenshot();
  void ExportScreenshot(const xe::ui::RawImage& image);
  void SaveImage(const std::filesystem::path& path,
                 const xe::ui::RawImage& image);

  void ToggleProfilesConfigDialog();
  void ToggleXMPConfigDialog();
  void ToggleConsoleSettingsDialog();
  void ToggleContentListDialog();

  void SetHotkeysState(bool enabled) { disable_hotkeys_ = !enabled; }

  void ExtractContent(const std::filesystem::path file = "");

  // Types of button functions for hotkeys.
  enum class ButtonFunctions {
    ToggleFullscreen,
    RunTitle,
    CpuTimeScalarSetHalf,
    CpuTimeScalarSetDouble,
    CpuTimeScalarReset,
    ClearGPUCache,
    ToggleControllerVibration,
    ClearMemoryPageState,
    ReadbackResolve,
    ToggleLogging,
    IncTitleSelect,
    DecTitleSelect,
    Unknown
  };

  class ControllerHotKey {
   public:
    // If true the hotkey can be activated while a title is running, otherwise
    // false.
    bool title_passthru;

    // If true vibrate the controller after activating the hotkey, otherwise
    // false.
    bool rumble;
    std::string pretty;
    ButtonFunctions function;

    ControllerHotKey(ButtonFunctions fn = ButtonFunctions::Unknown,
                     std::string pretty = "", bool rumble = false,
                     bool active = true) {
      function = fn;
      this->pretty = pretty;
      title_passthru = active;
      this->rumble = rumble;
    }
  };

 private:
  class EmulatorWindowListener final : public ui::WindowListener,
                                       public ui::WindowInputListener {
   public:
    explicit EmulatorWindowListener(EmulatorWindow& emulator_window)
        : emulator_window_(emulator_window) {}

    void OnClosing(ui::UIEvent& e) override;
    void OnFileDrop(ui::FileDropEvent& e) override;

    void OnKeyDown(ui::KeyEvent& e) override;

    void OnMouseDown(ui::MouseEvent& e) override;
    void OnMouseUp(ui::MouseEvent& e) override;

    void OnUsbDeviceChanged(bool is_arrival) override;

   private:
    EmulatorWindow& emulator_window_;
  };

  class DisplayConfigGameConfigLoadCallback
      : public Emulator::GameConfigLoadCallback {
   public:
    DisplayConfigGameConfigLoadCallback(Emulator& emulator,
                                        EmulatorWindow& emulator_window)
        : Emulator::GameConfigLoadCallback(emulator),
          emulator_window_(emulator_window) {}

    void PostGameConfigLoad() override;

   private:
    EmulatorWindow& emulator_window_;
  };

  class ContentInstallDialog final : public ui::ImGuiDialog {
   public:
    ContentInstallDialog(
        ui::ImGuiDrawer* imgui_drawer, EmulatorWindow& emulator_window,
        std::shared_ptr<std::vector<Emulator::ContentInstallEntry>> entries)
        : ui::ImGuiDialog(imgui_drawer),
          emulator_window_(emulator_window),
          installation_entries_(entries) {
      window_id_ = GetWindowId();
    }

    ~ContentInstallDialog() {
      for (auto& entry : *installation_entries_) {
        entry.icon_.release();
      }
    }

   protected:
    void OnDraw(ImGuiIO& io) override;

   private:
    uint64_t window_id_;

    EmulatorWindow& emulator_window_;
    std::shared_ptr<std::vector<Emulator::ContentInstallEntry>>
        installation_entries_;
  };

  class DisplayConfigDialog final : public ui::ImGuiDialog {
   public:
    DisplayConfigDialog(ui::ImGuiDrawer* imgui_drawer,
                        EmulatorWindow& emulator_window)
        : ui::ImGuiDialog(imgui_drawer), emulator_window_(emulator_window) {}

   protected:
    void OnDraw(ImGuiIO& io) override;

   private:
    EmulatorWindow& emulator_window_;
  };

  class XMPConfigDialog final : public ui::ImGuiDialog {
   public:
    XMPConfigDialog(ui::ImGuiDrawer* imgui_drawer,
                    EmulatorWindow& emulator_window)
        : ui::ImGuiDialog(imgui_drawer), emulator_window_(emulator_window) {
      if (emulator_window_.emulator_->audio_media_player()) {
        volume_ = emulator_window_.emulator_->audio_media_player()
                      ->GetVolume()
                      ->load();
      }
    }

   protected:
    void OnDraw(ImGuiIO& io) override;

   private:
    EmulatorWindow& emulator_window_;
    float volume_ = 0.0f;
  };

  // Persists for the app's lifetime (never calls Close() on itself) so its
  // View+Menu chord check in OnDraw runs every frame regardless of whether
  // the frontend dialog or a game is currently active - see the comment on
  // pause_menu_dialog_ below for why this can't just live inside
  // WinRTFrontendDialog, which is destroyed once a title launches.
  class PauseMenuDialog final : public ui::ImGuiDialog {
   public:
    PauseMenuDialog(ui::ImGuiDrawer* imgui_drawer,
                    EmulatorWindow& emulator_window)
        : ui::ImGuiDialog(imgui_drawer), emulator_window_(emulator_window) {}

   protected:
    void OnDraw(ImGuiIO& io) override;

   private:
    EmulatorWindow& emulator_window_;
    bool show_menu_ = false;
  };

#if XE_PLATFORM_WINRT
  class WinRTFrontendDialog final : public ui::ImGuiDialog {
   public:
    WinRTFrontendDialog(ui::ImGuiDrawer* imgui_drawer,
                        EmulatorWindow& emulator_window)
        : ui::ImGuiDialog(imgui_drawer), emulator_window_(emulator_window) {}

   protected:
    void OnDraw(ImGuiIO& io) override;

   private:
    enum class FrontendPage {
      kGameList = 0,
      kSettings,
      kPaths,
      kAbout,
      kCount,
    };

    enum class ActionPopupMode {
      kInfo,
      kManualPatchPrompt,
      kManualPluginPrompt,
      kManualConfigPrompt,
      kExistingDownloadPrompt,
      kPerGameConfigPrompt,
    };

    enum class ManualInstallKind { kNone, kPatch, kPlugin, kConfig };

    enum class PerGameConfigValueType {
      kUnknown,
      kBool,
      kInt,
      kUInt,
      kUInt64,
      kDouble,
      kString,
      kPath,
    };

    enum class PerGameConfigFocusSide {
      kNone,
      kLeft,
      kRight,
    };

    struct PerGameConfigOption {
      std::string name;
      std::string category;
      std::string description;
      std::string default_value;
      PerGameConfigValueType value_type = PerGameConfigValueType::kUnknown;
      std::vector<std::string> enum_values;
    };

    struct PerGameConfigEntry {
      std::string category;
      std::string value;
    };

    bool EnablePatchesForTitle(const std::filesystem::path& patches_dir,
                               const std::string& title_id);
    void DrawNoProfilePrompt(ImGuiIO& io);

    std::shared_ptr<ui::ImmediateTexture> GetOrCreateBackground();
    std::shared_ptr<ui::ImmediateTexture> GetOrCreateBackgroundFallback();
    std::shared_ptr<ui::ImmediateTexture> GetOrCreateImageTexture(
        const std::string& image_path);
    std::shared_ptr<ui::ImmediateTexture> GetOrCreateXeniaLogo();
    std::shared_ptr<ui::ImmediateTexture> GetOrCreateButtonTexture(char button);

    EmulatorWindow& emulator_window_;
    std::shared_ptr<ui::ImmediateTexture> background_tex_;
    std::shared_ptr<ui::ImmediateTexture> background_fallback_tex_;
    std::shared_ptr<ui::ImmediateTexture> xenia_logo_tex_;
    std::shared_ptr<ui::ImmediateTexture> button_a_tex_;
    std::shared_ptr<ui::ImmediateTexture> button_b_tex_;
    std::shared_ptr<ui::ImmediateTexture> button_x_tex_;
    std::shared_ptr<ui::ImmediateTexture> button_y_tex_;
    std::string selected_path_;
    std::string selected_game_path_;
    std::string selected_game_name_;
    bool show_path_warning_ = false;
    FrontendPage active_frontend_page_ = FrontendPage::kGameList;
    bool show_action_status_ = false;
    std::string action_status_;
    ActionPopupMode action_popup_mode_ = ActionPopupMode::kInfo;
    ManualInstallKind pending_manual_kind_ = ManualInstallKind::kNone;
    std::filesystem::path pending_manual_destination_;
    std::string pending_manual_title_id_;
    ManualInstallKind pending_existing_download_kind_ = ManualInstallKind::kNone;
    std::filesystem::path pending_existing_download_destination_;
    std::string pending_existing_download_title_id_;
    bool action_popup_should_close_ = false;
    bool action_popup_focus_requested_ = false;
    bool show_per_game_config_editor_ = false;
    bool per_game_config_popup_focus_requested_ = false;
    std::filesystem::path pending_per_game_config_path_;
    std::string pending_per_game_config_title_id_;
    bool per_game_config_remove_mode_ = false;
    std::string per_game_config_add_candidate_;
    std::string per_game_config_add_preview_option_;
    std::string selected_per_game_config_option_;
    std::string pending_per_game_config_title_display_;
    PerGameConfigFocusSide per_game_config_focus_jump_request_ =
        PerGameConfigFocusSide::kNone;
    std::string per_game_config_last_focused_control_id_;
    std::string per_game_config_control_jump_request_;
    std::vector<PerGameConfigOption> per_game_config_options_;
    std::map<std::string, PerGameConfigEntry> per_game_config_entries_;
    char per_game_config_buffer_[65536] = {};
    char cl_buffer_[128] = {};
    std::unordered_map<std::string, std::string> cached_game_title_ids_;
    std::unordered_map<std::string, std::shared_ptr<ui::ImmediateTexture>>
        game_image_textures_;
    bool show_search_panel_ = false;
    bool search_panel_focus_input_requested_ = false;
    bool search_panel_list_focus_requested_ = false;
    bool show_game_context_menu_ = false;
    bool game_context_menu_focus_requested_ = false;
    bool search_gamepad_y_was_down_ = false;
    std::string search_selected_game_path_;
    char search_input_buffer_[128] = {};
    bool no_profile_prompt_dismissed_ = false;
  };

#endif  // XE_PLATFORM_WINRT

  explicit EmulatorWindow(Emulator* emulator,
                          ui::WindowedAppContext& app_context, uint32_t width,
                          uint32_t height);
  bool Initialize();

  // For comparisons, use GetSwapPostEffectForCvarValue instead as the default
  // fallback may be used for multiple values.
  static const char* GetCvarValueForSwapPostEffect(
      gpu::CommandProcessor::SwapPostEffect effect);
  static gpu::CommandProcessor::SwapPostEffect GetSwapPostEffectForCvarValue(
      const std::string& cvar_value);
  // For comparisons, use GetGuestOutputPaintEffectForCvarValue instead as the
  // default fallback may be used for multiple values.
  static const char* GetCvarValueForGuestOutputPaintEffect(
      ui::Presenter::GuestOutputPaintConfig::Effect effect);
  static ui::Presenter::GuestOutputPaintConfig::Effect
  GetGuestOutputPaintEffectForCvarValue(const std::string& cvar_value);
  static ui::Presenter::GuestOutputPaintConfig
  GetGuestOutputPaintConfigForCvars();
  void ApplyDisplayConfigForCvars();

  void OnKeyDown(ui::KeyEvent& e);
  void OnMouseDown(const ui::MouseEvent& e);
  void ToggleFullscreenOnDoubleClick();
  void FileDrop(const std::filesystem::path& filename);
  void OnMouseUp(const ui::MouseEvent& e);
  void FileOpen();
  void FileClose();
  struct DlcInstallContext {
    std::filesystem::path source_path;
    Emulator::ContentInstallEntry entry;
    std::string description;
  };

  enum class DlcInstallMode { kManual, kAutoDownload };

  void InstallContent();
  void InstallContentForTitle(const std::string& title_id,
                              const std::filesystem::path& storage_root,
                              DlcInstallMode mode);
  void ExtractZarchive();
  void CreateZarchive();
  void ShowContentDirectory();
  void CpuTimeScalarReset();
  void CpuTimeScalarSetHalf();
  void CpuTimeScalarSetDouble();
  void CpuBreakIntoDebugger();
  void CpuBreakIntoHostDebugger();
  void GpuTraceFrame();
  void GpuClearCaches();
  void ToggleDisplayConfigDialog();
  void ToggleControllerVibration();
  void ShowCompatibility();
  void ShowFAQ();
  void ShowBuildCommit();

  EmulatorWindow::ControllerHotKey ProcessControllerHotkey(int buttons);
  void VibrateController(xe::hid::InputSystem* input_sys, uint32_t user_index,
                         bool vibrate = true);
  void GamepadHotKeys();
  void ToggleGPUSetting(gpu::GPUSetting setting);
  void CycleReadbackResolve();
  void DisplayHotKeysConfig();

  static std::string CanonicalizeFileExtension(
      const std::filesystem::path& path);
  static bool IsUseNexusForGameBarEnabled();

  void RunPreviouslyPlayedTitle();
  void FillRecentlyLaunchedTitlesMenu(xe::ui::MenuItem* recent_menu);
  void LoadRecentlyLaunchedTitles();
  void AddRecentlyLaunchedTitle(std::filesystem::path path_to_file,
                                std::string title_name);

  void ClearDialogs();

  friend class ProfileConfigDialog;

  Emulator* emulator_;
  ui::WindowedAppContext& app_context_;
  EmulatorWindowListener window_listener_;
  std::unique_ptr<ui::Window> window_;
  std::unique_ptr<ui::ImGuiDrawer> imgui_drawer_;
  std::unique_ptr<DisplayConfigGameConfigLoadCallback>
      display_config_game_config_load_callback_;
  // Creation may fail, in this case immediate drawer UI must not be drawn.
  std::unique_ptr<ui::ImmediateDrawer> immediate_drawer_;

  bool emulator_initialized_ = false;
  std::atomic<bool> disable_hotkeys_ = false;

  std::string base_title_;
  bool initializing_shader_storage_ = false;

  std::unique_ptr<DisplayConfigDialog> display_config_dialog_;
  // Created once in the constructor alongside the other dialogs below and
  // kept alive for the app's lifetime - unlike gamelist_, which is destroyed
  // when a title launches (see WinRTFrontendDialog::OnDraw's Close() call),
  // this needs to keep checking its View+Menu chord during gameplay too.
  std::unique_ptr<EmulatorWindow::PauseMenuDialog> pause_menu_dialog_;
#if XE_PLATFORM_WINRT
  std::unique_ptr<EmulatorWindow::WinRTFrontendDialog> gamelist_;
#endif  // XE_PLATFORM_WINRT

  std::unique_ptr<ConsoleSettingsDialog> console_settings_dialog_;
  std::unique_ptr<ContentListDialog> content_list_dialog_;
  // Storing pointers and toggling dialog state is useful for broadcasting
  // messages back to guest.
  std::unique_ptr<ProfileConfigDialog> profile_config_dialog_;

  std::unique_ptr<XMPConfigDialog> xmp_config_dialog_;

  std::vector<RecentTitleEntry> recently_launched_titles_;

  void DetachProfileConfigDialog(ProfileConfigDialog* dialog);
};

}  // namespace app
}  // namespace xe

#endif  // XENIA_APP_EMULATOR_WINDOW_H_
