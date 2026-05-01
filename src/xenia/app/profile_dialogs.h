/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Canary. All rights reserved.                          *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_APP_PROFILE_DIALOGS_H_
#define XENIA_APP_PROFILE_DIALOGS_H_

#include "xenia/ui/imgui_dialog.h"
#include "xenia/ui/imgui_drawer.h"
#include "xenia/xbox.h"

namespace xe {
namespace kernel {
namespace xam {
class UserProfile;
class ProfileManager;
struct TitleInfo;
}
}
}

namespace xe {
namespace app {

class EmulatorWindow;

class NoProfileDialog final : public ui::ImGuiDialog {
 public:
  NoProfileDialog(ui::ImGuiDrawer* imgui_drawer,
                  EmulatorWindow* emulator_window)
      : ui::ImGuiDialog(imgui_drawer), emulator_window_(emulator_window) {}

 protected:
  void OnDraw(ImGuiIO& io) override;

  EmulatorWindow* emulator_window_;
  bool focus_requested_ = true;
};

class ProfileConfigDialog final : public ui::ImGuiDialog {
 public:
  ProfileConfigDialog(ui::ImGuiDrawer* imgui_drawer,
                      EmulatorWindow* emulator_window)
      : ui::ImGuiDialog(imgui_drawer), emulator_window_(emulator_window) {
    LoadProfileIcon();
  }

 protected:
  void OnClose() override;
  void OnDraw(ImGuiIO& io) override;

 private:
  enum class FocusTarget {
    kAuto,
    kProfiles,
    kCreateProfile,
    kNone,
  };

  void LoadProfileIcon();
  void LoadProfileIcon(const uint64_t xuid);

  std::map<uint64_t, std::unique_ptr<ui::ImmediateTexture>> profile_icon_;
  std::shared_ptr<ui::ImmediateTexture> button_a_tex_;
  std::shared_ptr<ui::ImmediateTexture> button_b_tex_;

  uint64_t selected_xuid_ = 0;
  EmulatorWindow* emulator_window_;
  FocusTarget focus_target_ = FocusTarget::kAuto;
  bool popup_opened_ = false;
  
  std::shared_ptr<ui::ImmediateTexture> GetOrCreateButtonTexture(char button);
};

class TitleListUI final : public ui::ImGuiDialog {
 public:
  TitleListUI(ui::ImGuiDrawer* imgui_drawer, const ImVec2 drawing_position,
              const kernel::xam::UserProfile* profile, EmulatorWindow* emulator_window);

  ~TitleListUI();

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  void LoadProfileTitleList(ui::ImGuiDrawer* imgui_drawer,
                            const kernel::xam::UserProfile* profile);

  static constexpr uint8_t title_name_filter_size = 15;

  std::string dialog_name_ = "";
  char title_name_filter_[title_name_filter_size] = "";
  uint32_t selected_title_ = 0;
  const ImVec2 drawing_position_ = {};
  bool has_opened_ = false;
  bool focus_requested_ = false;

  const kernel::xam::UserProfile* profile_;
  const kernel::xam::ProfileManager* profile_manager_;
  EmulatorWindow* emulator_window_;

  std::map<uint32_t, std::unique_ptr<ui::ImmediateTexture>> title_icon;
  std::vector<kernel::xam::TitleInfo> info_;
};

}  // namespace app
}  // namespace xe

#endif
