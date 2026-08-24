/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2024 Xenia Canary. All rights reserved.                          *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */
#include "xenia/app/profile_dialogs.h"
#include <ctime>
#include <filesystem>

#include "third_party/stb/stb_image.h"
#include "xenia/app/emulator_window.h"
#include "xenia/app/ui_text_effect_helpers.h"
#include "xenia/base/png_utils.h"
#include "xenia/base/logging.h"
#include "xenia/base/system.h"
#include "xenia/kernel/util/shim_utils.h"
#include "xenia/kernel/xam/xam_ui.h"
#include "xenia/kernel/xam/user_tracker.h"
#include "xenia/ui/file_picker.h"

#include "xenia/kernel/xam/ui/create_profile_ui.h"
#include "xenia/kernel/xam/ui/gamercard_ui.h"
#include "xenia/kernel/xam/ui/signin_ui.h"
#include "xenia/kernel/xam/ui/game_achievements_ui.h"

namespace xe {
namespace app {

namespace {

std::vector<size_t> build_top_loop_order(size_t count, size_t selected_index) {
  std::vector<size_t> order;
  if (count == 0) {
    return order;
  }
  
  // Add selected index first
  order.push_back(selected_index);
  
  // Add items above selected
  for (size_t i = 1; i < count; ++i) {
    size_t index = (selected_index + i) % count;
    order.push_back(index);
  }
  
  return order;
}

}  // namespace

void NoProfileDialog::OnDraw(ImGuiIO& io) {
  auto profile_manager = emulator_window_->emulator()
                             ->kernel_state()
                             ->xam_state()
                             ->profile_manager();

  if (profile_manager->GetAccountCount()) {
    Close();
    return;
  }

  const std::string message =
      "There is no profile available! You will not be able to save\n"
      "without one.\n\nWould you like to create one?";

  const auto content_files = xe::filesystem::ListDirectories(
      emulator_window_->emulator()->content_root());

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                           ImGuiWindowFlags_NoResize |
                           ImGuiWindowFlags_NoMove |
                           ImGuiWindowFlags_NoSavedSettings |
                           ImGuiWindowFlags_NoScrollbar |
                           ImGuiWindowFlags_NoBackground;
  if (focus_requested_) {
    ImGui::SetNextWindowFocus();
  }

  if (!ImGui::Begin("##no_profile_overlay", nullptr, flags)) {
    ImGui::End();
    return;
  }

  ImDrawList* root_draw_list = ImGui::GetWindowDrawList();
  const ImVec2 screen_min = viewport->Pos;
  const ImVec2 screen_max(
      screen_min.x + viewport->Size.x, screen_min.y + viewport->Size.y);
  root_draw_list->AddRectFilled(
      screen_min, screen_max,
      ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.55f)));

  static std::shared_ptr<xe::ui::ImmediateTexture> guide_bg_tex = nullptr;
  static std::shared_ptr<xe::ui::ImmediateTexture> button_a_tex = nullptr;
  static std::shared_ptr<xe::ui::ImmediateTexture> button_b_tex = nullptr;
  if (!guide_bg_tex) {
    guide_bg_tex = xe::app::LoadConfiguredGuideBackgroundTexture(imgui_drawer());
  }
  if (!button_a_tex) {
    button_a_tex = xe::app::LoadButtonTexture(imgui_drawer(), 'A');
  }
  if (!button_b_tex) {
    button_b_tex = xe::app::LoadButtonTexture(imgui_drawer(), 'B');
  }

  const ImVec2 panel_size =
      xe::app::GetGuidePanelSize(guide_bg_tex, io.DisplaySize.y);
  const ImVec2 panel_padding = xe::app::GetGuidePanelPadding();
  const float ux = io.DisplaySize.x / 1024.0f;
  const float uy = io.DisplaySize.y / 576.0f;
  const ImVec2 overlay_min = viewport->Pos;
  const ImVec2 overlay_max(
      overlay_min.x + panel_size.x, overlay_min.y + panel_size.y);

  xe::app::DrawGuidePanelBackground(root_draw_list, guide_bg_tex, overlay_min,
                                    overlay_max);

  const bool child_dialog_open = imgui_drawer()->GetDialogCount() > 2;
  bool should_close = false;
  bool should_open_create_profile = false;
  bool should_open_profile_menu = false;
  if (!child_dialog_open && ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
    should_close = true;
  }

  const xe::app::OverlayHeaderLayout header_layout =
      xe::app::DrawOverlayHeader(root_draw_list, overlay_min, panel_size,
                                 panel_padding, ux, uy,
                                 "No Profiles Found");

  const float content_start_y =
      header_layout.position.y + header_layout.font_size + 55.0f;
  const float field_width = 320.0f * ux;
  ImGui::SetCursorScreenPos(
      ImVec2(overlay_min.x + panel_padding.x, content_start_y));
  ImGui::PushTextWrapPos(overlay_min.x + panel_padding.x + field_width);
  DrawConfiguredParagraph(message);
  ImGui::PopTextWrapPos();
  ImGui::Dummy(ImVec2(0.0f, 14.0f * uy));

  const ImVec2 button_size(field_width, 0.0f);
  const char* create_label =
      content_files.empty() ? "Create Profile" : "Create profile & migrate data";
  if (focus_requested_) {
    ImGui::SetKeyboardFocusHere();
    focus_requested_ = false;
  }

  ImGui::BeginDisabled(child_dialog_open);
  ImGui::SetCursorScreenPos(
      ImVec2(overlay_min.x + panel_padding.x, ImGui::GetCursorScreenPos().y));
  if (DrawTextEffectButton(create_label, button_size)) {
    should_open_create_profile = true;
  }
  ImGui::Spacing();
  ImGui::SetCursorScreenPos(
      ImVec2(overlay_min.x + panel_padding.x, ImGui::GetCursorScreenPos().y));
  if (DrawTextEffectButton("Open profile menu", button_size)) {
    should_open_profile_menu = true;
  }
  ImGui::Spacing();
  ImGui::SetCursorScreenPos(
      ImVec2(overlay_min.x + panel_padding.x, ImGui::GetCursorScreenPos().y));
  if (DrawTextEffectButton("Close", button_size)) {
    should_close = true;
  }
  ImGui::EndDisabled();

  const float footer_text_size = 13.5f * uy;
  const float footer_icon_size = 15.6f * uy;
  const float footer_spacing_y = 17.0f * uy;
  const float footer_select_y = overlay_max.y - (32.0f * uy);
  const float footer_back_y = footer_select_y - footer_spacing_y;
  const float footer_base_x = overlay_min.x + panel_size.x * 0.9f;
  const float footer_back_text_x = footer_base_x - (70.0f * ux);
  const float footer_back_icon_offset = 35.0f * ux;
  const float footer_select_text_x = footer_base_x - (60.0f * ux);
  const float footer_select_icon_offset = 42.0f * ux;
  xe::app::DrawFooterPrompt(root_draw_list, button_b_tex, footer_text_size,
                            footer_icon_size, "Back", footer_back_y,
                            footer_back_text_x, footer_back_icon_offset);
  xe::app::DrawFooterPrompt(root_draw_list, button_a_tex, footer_text_size,
                            footer_icon_size, "Select", footer_select_y,
                            footer_select_text_x, footer_select_icon_offset);

  ImGui::End();

  if (should_open_create_profile) {
    if (content_files.empty()) {
      new kernel::xam::ui::CreateProfileUI(emulator_window_->imgui_drawer(),
                                           emulator_window_->emulator());
    } else {
      new kernel::xam::ui::CreateProfileUI(emulator_window_->imgui_drawer(),
                                           emulator_window_->emulator(), true);
    }
    return;
  }

  if (should_open_profile_menu) {
    emulator_window_->ToggleProfilesConfigDialog();
    return;
  }

  if (should_close) {
    emulator_window_->SetHotkeysState(true);
    delete this;
    return;
  }
}

void ProfileConfigDialog::LoadProfileIcon() {
  if (!emulator_window_) {
    return;
  }

  for (uint8_t user_index = 0; user_index < XUserMaxUserCount; user_index++) {
    const auto profile = emulator_window_->emulator()
                             ->kernel_state()
                             ->xam_state()
                             ->profile_manager()
                             ->GetProfile(user_index);

    if (!profile) {
      continue;
    }
    LoadProfileIcon(profile->xuid());
  }
}

void ProfileConfigDialog::LoadProfileIcon(const uint64_t xuid) {
  if (!emulator_window_) {
    return;
  }

  const auto profile_manager = emulator_window_->emulator()
                                   ->kernel_state()
                                   ->xam_state()
                                   ->profile_manager();
  if (!profile_manager) {
    return;
  }

  const auto profile = profile_manager->GetProfile(xuid);

  if (!profile) {
    if (profile_icon_.contains(xuid)) {
      profile_icon_[xuid].release();
    }
    return;
  }

  const auto profile_icon =
      profile->GetProfileIcon(kernel::xam::XTileType::kGamerTile);
  if (profile_icon.empty()) {
    return;
  }

  profile_icon_[xuid].release();
  profile_icon_[xuid] = imgui_drawer()->LoadImGuiIcon(profile_icon);
}

void ProfileConfigDialog::OnClose() {
  XELOGI("ProfileConfigDialog: OnClose invoked");
  if (emulator_window_) {
    emulator_window_->DetachProfileConfigDialog(this);
  }
}

std::shared_ptr<ui::ImmediateTexture> ProfileConfigDialog::GetOrCreateButtonTexture(
    char button) {
  std::shared_ptr<ui::ImmediateTexture>* dst = nullptr;
  switch (button) {
    case 'A':
      dst = &button_a_tex_;
      break;
    case 'B':
      dst = &button_b_tex_;
      break;
    default:
      return nullptr;
  }

  if (*dst) {
    return *dst;
  }

  *dst = xe::app::LoadButtonTexture(imgui_drawer(), button);
  return *dst;
}

void ProfileConfigDialog::OnDraw(ImGuiIO& io) {
  if (!emulator_window_->emulator() ||
      !emulator_window_->emulator()->kernel_state() ||
      !emulator_window_->emulator()->kernel_state()->xam_state()) {
    return;
  }

  auto profile_manager = emulator_window_->emulator()
                             ->kernel_state()
                             ->xam_state()
                             ->profile_manager();
  if (!profile_manager) {
    return;
  }

  auto profiles = profile_manager->GetAccounts();
  if (profiles->empty()) {
    selected_xuid_ = 0;
  } else if (profiles->find(selected_xuid_) == profiles->end()) {
    selected_xuid_ = profiles->begin()->first;
  }

  // Regular window like other overlay UIs
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar |
                                  ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoSavedSettings |
                                  ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoBackground;
  
  if (!popup_opened_) {
    ImGui::SetNextWindowFocus();
    popup_opened_ = true;
  }
  
  if (!ImGui::Begin("##profiles_window", nullptr, window_flags)) {
    ImGui::End();
    return;
  }

  // Background and styling like sign-in UI
  ImDrawList* root_draw_list = ImGui::GetWindowDrawList();
  const ImVec2 screen_min = viewport->Pos;
  const ImVec2 screen_max =
      ImVec2(screen_min.x + viewport->Size.x, screen_min.y + viewport->Size.y);
  root_draw_list->AddRectFilled(
      screen_min, screen_max,
      ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.55f)));

  // Load guide background texture
  static std::shared_ptr<xe::ui::ImmediateTexture> guide_bg_tex = nullptr;
  if (!guide_bg_tex) {
    guide_bg_tex = xe::app::LoadConfiguredGuideBackgroundTexture(imgui_drawer());
  }

  const ImVec2 panel_size =
      xe::app::GetGuidePanelSize(guide_bg_tex, io.DisplaySize.y);
  const ImVec2 panel_padding = xe::app::GetGuidePanelPadding();
  const float ux = io.DisplaySize.x / 1024.0f;
  const float uy = io.DisplaySize.y / 576.0f;

  const ImVec2 overlay_min = viewport->Pos;
  const ImVec2 overlay_max(
      overlay_min.x + panel_size.x,
      overlay_min.y + panel_size.y);

  xe::app::DrawGuidePanelBackground(root_draw_list, guide_bg_tex, overlay_min,
                                    overlay_max);

  const xe::app::OverlayHeaderLayout header_layout =
      xe::app::DrawOverlayHeader(root_draw_list, overlay_min, panel_size,
                                 panel_padding, ux, uy, "Profiles Menu");

  const float content_start_y =
      header_layout.position.y + header_layout.font_size + 65.0f;
  const float field_width = 320.0f * ux;
  const float shifted_x = overlay_min.x + panel_padding.x + (20.0f * ux);
  ImGui::SetCursorScreenPos(
      ImVec2(shifted_x, content_start_y));
  bool should_open_selected_profile_menu = false;
  ImVec2 selected_profile_popup_anchor = ImVec2(shifted_x, content_start_y);
  const bool should_focus_selected_profile =
      (focus_target_ == FocusTarget::kAuto ||
       focus_target_ == FocusTarget::kProfiles) &&
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      !ImGui::IsAnyItemFocused() && !ImGui::IsAnyItemActive() &&
      !ImGui::IsMouseClicked(0);
  
  // Profile list with proper rendering
  for (auto& [xuid, account] : *profiles) {
    ImGui::PushID(fmt::format("{:016X}", xuid).c_str());

      const uint8_t user_index =
          profile_manager->GetUserIndexAssignedToProfile(xuid);

      const auto profile_icon = profile_icon_.find(xuid) != profile_icon_.cend()
                                    ? profile_icon_[xuid].get()
                                    : nullptr;

      bool selected = (selected_xuid_ == xuid);

    // Start with selectable that spans the full width
    const float item_height = 60.0f * ImGui::GetIO().DisplayFramebufferScale.y;
    // Adjust position to center with buttons (account for reduced width)
    const float profile_shift_x = shifted_x + (4.0f * ux);
    ImGui::SetCursorScreenPos(ImVec2(profile_shift_x, ImGui::GetCursorScreenPos().y));
    // Reduce width to match actual button rendering (account for ImGui button padding)
    const float selectable_width = field_width - (8.0f * ux);

    if (selected && should_focus_selected_profile) {
      ImGui::SetKeyboardFocusHere();
      focus_target_ = FocusTarget::kNone;
    }
    
    // Make selectable transparent
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    
    bool activate_profile =
        ImGui::Selectable("##profile_selectable", selected, 0,
                          ImVec2(selectable_width, item_height));
    if (!activate_profile && ImGui::IsItemFocused() &&
        ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false)) {
      activate_profile = true;
    }
    if (activate_profile) {
      selected_xuid_ = xuid;
      XELOGI("ProfileConfigDialog: activated profile {:016X}, opening profile popup", xuid);
      should_open_selected_profile_menu = true;
    }
    
    ImGui::PopStyleColor(3);

    if (selected) {
      ImGui::SetItemDefaultFocus();
    }

    if (selected_xuid_ == xuid) {
      selected_profile_popup_anchor =
          ImVec2(ImGui::GetItemRectMin().x + selectable_width * 0.5f,
                 ImGui::GetItemRectMax().y);
    }

    // Draw highlight only when focused (not when just selected)
    if (ImGui::IsItemFocused()) {
      const ImVec2 padding(2.0f, 2.0f);
      xe::app::DrawConfiguredComboHighlight(
          ImVec2(ImGui::GetItemRectMin().x - padding.x,
                 ImGui::GetItemRectMin().y - padding.y),
          ImVec2(ImGui::GetItemRectMax().x + padding.x,
                 ImGui::GetItemRectMax().y + padding.y),
          1.6f, 4.0f);
    }

    // Draw profile content on top of the selectable
    ImVec2 selectable_pos = ImGui::GetItemRectMin();
    ImGui::SetCursorScreenPos(ImVec2(selectable_pos.x + 10.0f * ImGui::GetIO().DisplayFramebufferScale.x, selectable_pos.y + 5.0f * ImGui::GetIO().DisplayFramebufferScale.y));
    ImGui::BeginGroup();
    {
      if (profile_icon) {
        ImGui::Image(reinterpret_cast<ImTextureID>(profile_icon),
                     xe::ui::default_image_icon_size);
      } else {
        if (user_index < XUserMaxUserCount) {
          const auto icon = imgui_drawer()->GetNotificationIcon(user_index);
          ImGui::Image(reinterpret_cast<ImTextureID>(icon),
                       xe::ui::default_image_icon_size);
        } else {
          ImGui::Dummy(xe::ui::default_image_icon_size);
        }
      }

      ImGui::SameLine(0.0f, 10.0f * ImGui::GetIO().DisplayFramebufferScale.x);

      ImGui::BeginGroup();
      {
        ImGui::TextUnformatted(
            fmt::format("User: {}", account.GetGamertagString()).c_str());
        ImGui::TextUnformatted(fmt::format("XUID: {:016X}", xuid).c_str());
        if (user_index != XUserIndexAny) {
          ImGui::TextUnformatted(
              fmt::format("Assigned to slot: {}", user_index + 1).c_str());
        } else {
          ImGui::TextUnformatted("Profile is not signed in");
        }
      }
      ImGui::EndGroup();
    }
    ImGui::EndGroup();

    ImGui::PopID();
    if (xuid != profiles->rbegin()->first) {
      ImGui::Separator();
    }
  }

  if (should_open_selected_profile_menu) {
    XELOGI("ProfileConfigDialog: opening popup for {:016X}",
           selected_xuid_);
    ImGui::OpenPopup("Profile Menu");
  }

  if (ImGui::BeginPopupContextItem("Profile Menu")) {
    XELOGI("ProfileConfigDialog: profile menu visible for {:016X}",
           selected_xuid_);
    auto selected_it = profiles->find(selected_xuid_);
    if (selected_it != profiles->end()) {
      const uint8_t selected_user_index =
          profile_manager->GetUserIndexAssignedToProfile(selected_xuid_);
      const bool is_signedin =
          profile_manager->GetProfile(selected_xuid_) != nullptr;
      if (selected_user_index == XUserIndexAny) {
        if (ImGui::MenuItem("Login")) {
          XELOGD("ProfileConfigDialog: selected Login for {:016X}",
                 selected_xuid_);
          profile_manager->Login(selected_xuid_);
          if (!profile_manager->GetProfile(selected_xuid_)
                   ->GetProfileIcon(kernel::xam::XTileType::kGamerTile)
                   .empty()) {
            LoadProfileIcon(selected_xuid_);
          }
        }
        if (ImGui::BeginMenu("Login to slot:")) {
          for (uint8_t i = 1; i <= XUserMaxUserCount; i++) {
            if (ImGui::MenuItem(fmt::format("slot {}", i).c_str())) {
              XELOGD(
                  "ProfileConfigDialog: selected Login to slot {} for {:016X}",
                  i, selected_xuid_);
              profile_manager->Login(selected_xuid_, i - 1);
              ImGui::CloseCurrentPopup();
            }
          }
          ImGui::EndMenu();
        }
      } else {
        if (ImGui::MenuItem("Logout")) {
          XELOGD("ProfileConfigDialog: selected Logout for {:016X}",
                 selected_xuid_);
          profile_manager->Logout(selected_user_index);
          LoadProfileIcon(selected_xuid_);
        }
      }

      if (ImGui::MenuItem("Modify")) {
        XELOGD("ProfileConfigDialog: selected Modify for {:016X}",
               selected_xuid_);
        new kernel::xam::ui::GamercardUI(
            emulator_window_->window(), emulator_window_->imgui_drawer(),
            emulator_window_->emulator()->kernel_state(), selected_xuid_);
      }

      ImGui::BeginDisabled(!is_signedin);
      if (ImGui::MenuItem("Show Played Titles")) {
        XELOGD("ProfileConfigDialog: selected Show Played Titles for {:016X}",
               selected_xuid_);
        if (imgui_drawer()->GetDialogCount() <= 1) {
          const ImVec2 next_window_position(ImVec2(
              overlay_min.x + panel_padding.x + (15.0f * ux),
              overlay_min.y + (72.0f * uy)));
          new TitleListUI(emulator_window_->imgui_drawer(),
                          next_window_position,
                          profile_manager->GetProfile(selected_user_index),
                          emulator_window_);
        }
      }
      ImGui::EndDisabled();

      if (!emulator_window_->emulator()->is_title_open()) {
        ImGui::Separator();
        if (ImGui::BeginMenu("Delete Profile")) {
          ImGui::BeginTooltip();
          ImGui::TextUnformatted(
              fmt::format(
                  "You're about to delete profile: {} (XUID: {:016X}). "
                  "This will remove all data assigned to this profile "
                  "including savefiles. Are you sure?",
                  selected_it->second.GetGamertagString(), selected_xuid_)
                  .c_str());
          ImGui::EndTooltip();

          if (ImGui::MenuItem("Yes, delete it!")) {
            XELOGD("ProfileConfigDialog: confirmed Delete Profile for {:016X}",
                   selected_xuid_);
            profile_manager->DeleteProfile(selected_xuid_);
          }

          ImGui::EndMenu();
        }
      }
    }
    ImGui::EndPopup();
  }
  
  // Add spacing before buttons
  ImGui::Dummy(ImVec2(0.0f, 24.0f * uy));
  
  // Create Profile button
  const ImVec2 button_size(field_width, 0.0f);
  ImGui::SetCursorScreenPos(ImVec2(shifted_x, ImGui::GetCursorScreenPos().y));
  if (DrawTextEffectButton("Create Profile", button_size)) {
    XELOGD("ProfileConfigDialog: Create Profile button activated");
    new kernel::xam::ui::CreateProfileUI(emulator_window_->imgui_drawer(),
                                         emulator_window_->emulator());
    Close();
    return;
  }
  
  // Close button
  ImGui::Dummy(ImVec2(0.0f, 4.0f * uy));
  ImGui::SetCursorScreenPos(ImVec2(shifted_x, ImGui::GetCursorScreenPos().y));
  if (DrawTextEffectButton("Close", button_size)) {
    XELOGD("ProfileConfigDialog: Close button activated");
    Close();
    return;
  }
  
  // Draw footer prompts
  const float footer_text_size = 13.5f * uy;
  const float footer_icon_size = 15.6f * uy;
  const float footer_spacing_y = 17.0f * uy;
  const float footer_select_y = overlay_max.y - (32.0f * uy);
  const float footer_back_y = footer_select_y - footer_spacing_y;
  const float footer_base_x = overlay_min.x + panel_size.x * 0.9f;
  const float footer_back_text_x = footer_base_x - (70.0f * ux);
  const float footer_back_icon_offset = 35.0f * ux;
  const float footer_select_text_x = footer_base_x - (60.0f * ux);
  const float footer_select_icon_offset = 42.0f * ux;
  
  xe::app::DrawFooterPrompt(root_draw_list, GetOrCreateButtonTexture('B'),
                            footer_text_size, footer_icon_size, "Back",
                            footer_back_y, footer_back_text_x,
                            footer_back_icon_offset);
  xe::app::DrawFooterPrompt(root_draw_list, GetOrCreateButtonTexture('A'),
                            footer_text_size, footer_icon_size, "Select",
                            footer_select_y, footer_select_text_x,
                            footer_select_icon_offset);
  
  ImGui::End();
}

TitleListUI::TitleListUI(ui::ImGuiDrawer* imgui_drawer,
                         const ImVec2 drawing_position,
                         const kernel::xam::UserProfile* profile,
                         EmulatorWindow* emulator_window)
    : ui::ImGuiDialog(imgui_drawer),
      drawing_position_(drawing_position),
      profile_(profile),
      emulator_window_(emulator_window),
      profile_manager_(emulator_window->emulator()->kernel_state()->xam_state()->profile_manager()),
      dialog_name_(
          fmt::format("{}'s Games List", profile->name())) {
  LoadProfileTitleList(imgui_drawer, profile);
}

TitleListUI::~TitleListUI() {
  for (auto& entry : title_icon) {
    entry.second.release();
  }
}

void TitleListUI::LoadProfileTitleList(ui::ImGuiDrawer* imgui_drawer,
                                       const kernel::xam::UserProfile* profile) {
  info_.clear();

  xe::ui::IconsData data;

  info_ = emulator_window_->emulator()->kernel_state()->xam_state()->user_tracker()->GetPlayedTitles(
      profile->xuid());
  for (const auto& title_info : info_) {
    if (!title_info.icon.empty()) {
      data[title_info.id] = title_info.icon;
    }
  }

  title_icon = imgui_drawer->LoadIcons(data);
}

void TitleListUI::OnDraw(ImGuiIO& io) {
  constexpr const char* kPlayedTitlesWindowName = "##played_titles_window";
  
  if (!has_opened_) {
    has_opened_ = true;
    focus_requested_ = true;
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                           ImGuiWindowFlags_NoResize |
                           ImGuiWindowFlags_NoMove |
                           ImGuiWindowFlags_NoSavedSettings |
                           ImGuiWindowFlags_NoScrollbar |
                           ImGuiWindowFlags_NoBackground;
  
  if (focus_requested_) {
    ImGui::SetNextWindowFocus();
    focus_requested_ = false;
  }

  if (!ImGui::Begin(kPlayedTitlesWindowName, nullptr, flags)) {
    ImGui::End();
    return;
  }

  ImDrawList* root_draw_list = ImGui::GetWindowDrawList();
  const ImVec2 screen_min = viewport->Pos;
  const ImVec2 screen_max(
      screen_min.x + viewport->Size.x, screen_min.y + viewport->Size.y);
  root_draw_list->AddRectFilled(
      screen_min, screen_max,
      ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.55f)));

  static std::shared_ptr<xe::ui::ImmediateTexture> guide_bg_tex = nullptr;
  static std::shared_ptr<xe::ui::ImmediateTexture> button_a_tex = nullptr;
  static std::shared_ptr<xe::ui::ImmediateTexture> button_b_tex = nullptr;
  if (!guide_bg_tex) {
    guide_bg_tex = xe::app::LoadConfiguredGuideBackgroundTexture(imgui_drawer());
  }
  if (!button_a_tex) {
    button_a_tex = xe::app::LoadButtonTexture(imgui_drawer(), 'A');
  }
  if (!button_b_tex) {
    button_b_tex = xe::app::LoadButtonTexture(imgui_drawer(), 'B');
  }

  const ImVec2 panel_size =
      xe::app::GetGuidePanelSize(guide_bg_tex, io.DisplaySize.y);
  const ImVec2 panel_padding = xe::app::GetGuidePanelPadding();
  const float ux = io.DisplaySize.x / 1024.0f;
  const float uy = io.DisplaySize.y / 576.0f;
  const ImVec2 overlay_min = viewport->Pos;
  const ImVec2 overlay_max(
      overlay_min.x + panel_size.x, overlay_min.y + panel_size.y);

  xe::app::DrawGuidePanelBackground(root_draw_list, guide_bg_tex, overlay_min,
                                    overlay_max);

  bool should_close = false;
  if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
    should_close = true;
  }

  const xe::app::OverlayHeaderLayout header_layout =
      xe::app::DrawOverlayHeader(root_draw_list, overlay_min, panel_size,
                                 panel_padding, ux, uy, "Played Titles");

  const float content_start_y =
      header_layout.position.y + header_layout.font_size + 55.0f;
  const float results_height = overlay_max.y - (102.0f * uy) - content_start_y;
  ImGui::SetCursorScreenPos(
      ImVec2(overlay_min.x + panel_padding.x, content_start_y));

  if (!info_.empty()) {
    if (ImGui::BeginChild("##played_titles_results",
                          ImVec2(-1.0f, results_height), false,
                          ImGuiWindowFlags_NoScrollbar)) {
      const float carousel_outer_padding = 14.0f * ux;
      const float carousel_spacing = 16.0f * ux;
      const float carousel_card_height = 126.0f * uy;
      size_t selected_index = 0;
      for (size_t i = 0; i < info_.size(); ++i) {
        if (info_[i].id == selected_title_) {
          selected_index = i;
          break;
        }
      }
      const std::vector<size_t> ordered_indices =
          build_top_loop_order(info_.size(), selected_index);
      ImGui::Dummy(ImVec2(0.0f, 6.0f * uy));
      for (size_t ordered_index = 0;
           ordered_index < ordered_indices.size(); ++ordered_index) {
        const auto& entry = info_[ordered_indices[ordered_index]];
        ImGui::PushID(entry.id);
        const bool is_selected = selected_title_ == entry.id;
        const float card_width = ImGui::GetContentRegionAvail().x;
        const ImVec2 card_min = ImGui::GetCursorScreenPos();
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f * ux);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        if (ImGui::Selectable("##played_title_card", is_selected,
                              ImGuiSelectableFlags_None,
                              ImVec2(card_width, carousel_card_height))) {
          selected_title_ = entry.id;
          const ImVec2 next_window_position(
              ImVec2(overlay_min.x + panel_padding.x, overlay_min.y + (72.0f * uy)));
          new kernel::xam::ui::GameAchievementsUI(imgui_drawer(), next_window_position, &entry,
                                                 profile_);
        }
        ImGui::PopStyleVar(2);
        const ImVec2 card_max = ImGui::GetItemRectMax();
        if (is_selected) {
          xe::app::DrawConfiguredComboHighlight(
              ImVec2(card_min.x - carousel_outer_padding,
                     card_min.y - carousel_outer_padding),
              ImVec2(card_max.x + carousel_outer_padding,
                     card_max.y + carousel_outer_padding),
              1.6f, 6.0f);
        }
        ImGui::SameLine(0.0f, 12.0f * ux);
        if (title_icon.count(entry.id)) {
          ImGui::Image(reinterpret_cast<ImTextureID>(title_icon.at(entry.id).get()),
                       xe::ui::default_image_icon_size);
        } else {
          ImGui::Dummy(xe::ui::default_image_icon_size);
        }
        ImGui::SameLine(0.0f, 10.0f * ux);
        ImGui::BeginGroup();
        ImGui::PushFont(imgui_drawer()->GetTitleFont());
        ImGui::TextUnformatted(xe::to_utf8(entry.title_name).c_str());
        ImGui::PopFont();
        ImGui::TextUnformatted(
            fmt::format("{}/{} Achievements unlocked ({} Gamerscore)",
                        entry.unlocked_achievements_count, entry.achievements_count,
                        entry.title_earned_gamerscore)
                .c_str());
        if (entry.WasTitlePlayed()) {
          const auto time_date = std::chrono::system_clock::to_time_t(
              std::chrono::system_clock::time_point(
                  entry.last_played.time_since_epoch()));
          ImGui::TextUnformatted(fmt::format("Last played: {:%Y-%m-%d %H:%M}",
                                             *std::localtime(&time_date))
                                 .c_str());
        } else {
          ImGui::TextUnformatted("Last played: Unknown");
        }
        ImGui::EndGroup();
        ImGui::Dummy(ImVec2(0.0f, carousel_spacing));
        ImGui::PopID();
      }
    }
    ImGui::EndChild();
  } else {
    ImGui::SetCursorScreenPos(
        ImVec2(overlay_min.x + panel_padding.x, content_start_y));
    std::string no_entries_message = "There are no titles, so far.";
    ImGui::PushFont(imgui_drawer()->GetTitleFont());
    float windowWidth = ImGui::GetContentRegionAvail().x;
    ImVec2 textSize = ImGui::CalcTextSize(no_entries_message.c_str());
    float textOffsetX = (windowWidth - textSize.x) * 0.5f;
    if (textOffsetX > 0.0f) {
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textOffsetX);
    }
    ImGui::Text("%s", no_entries_message.c_str());
    ImGui::PopFont();
  }

  const float footer_text_size = 13.5f * uy;
  const float footer_icon_size = 15.6f * uy;
  const float footer_spacing_y = 17.0f * uy;
  const float footer_select_y = overlay_max.y - (32.0f * uy);
  const float footer_back_y = footer_select_y - footer_spacing_y;
  const float footer_base_x = overlay_min.x + panel_size.x * 0.9f;
  const float footer_back_text_x = footer_base_x - (70.0f * ux);
  const float footer_back_icon_offset = 35.0f * ux;
  const float footer_select_text_x = footer_base_x - (60.0f * ux);
  const float footer_select_icon_offset = 42.0f * ux;
  xe::app::DrawFooterPrompt(root_draw_list, button_b_tex, footer_text_size,
                           footer_icon_size, "Back", footer_back_y,
                           footer_back_text_x, footer_back_icon_offset);
  xe::app::DrawFooterPrompt(root_draw_list, button_a_tex, footer_text_size,
                           footer_icon_size, "Select", footer_select_y,
                           footer_select_text_x, footer_select_icon_offset);

  ImGui::End();

  if (should_close) {
    delete this;
  }
  return;
}

}  // namespace app
}  // namespace xe
