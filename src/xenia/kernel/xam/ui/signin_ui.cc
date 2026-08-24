/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2025 Xenia Canary. All rights reserved.                          *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/kernel/xam/ui/signin_ui.h"
#include <algorithm>
#include "xenia/app/ui_text_effect_helpers.h"
#include "xenia/base/logging.h"
#include "xenia/emulator.h"
#include "xenia/ui/window.h"
#if XE_PLATFORM_WINRT
#include "xenia-canary-uwp/WinRTKeyboard.h"
#endif

namespace xe {
namespace kernel {
namespace xam {
namespace ui {

void SigninUI::OnClose() {
  auto pending_login_profiles = std::move(pending_login_profiles_);
  XamDialog::OnClose();
  DispatchPendingLogins(std::move(pending_login_profiles));
}

SigninUI::SigninUI(xe::ui::ImGuiDrawer* imgui_drawer,
                   ProfileManager* profile_manager, uint32_t last_used_slot,
                   uint32_t users_needed)
    : XamDialog(imgui_drawer),
      profile_manager_(profile_manager),
      last_user_(last_used_slot),
      users_needed_(users_needed),
      title_("Sign In") {}

void SigninUI::OnDraw(ImGuiIO& io) {
#if XE_PLATFORM_WINRT
  constexpr const char* kSigninPopupName = "##signin_popup";
  constexpr const char* kCreateProfileWindowName = "##signin_create_profile";

  if (!has_opened_) {
    has_opened_ = true;
    ReloadProfiles(true);
    ImGui::OpenPopup(kSigninPopupName);
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  ImGuiWindowFlags popup_flags = ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoScrollbar |
                                 ImGuiWindowFlags_NoBackground;
  if (focus_requested_) {
    ImGui::SetNextWindowFocus();
    focus_requested_ = false;
  }

  if (!ImGui::BeginPopupModal(kSigninPopupName, nullptr, popup_flags)) {
    Close();
    return;
  }

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
                                 panel_padding, ux, uy, title_.c_str());

  const float content_start_y =
      header_layout.position.y + header_layout.font_size + 65.0f;
  const float field_width = 320.0f * ux;
  ImGui::SetCursorScreenPos(
      ImVec2(overlay_min.x + panel_padding.x, content_start_y));
  ImGui::PushItemWidth(field_width);

  auto draw_signin_combo = [&](const char* id,
                               std::vector<const char*>& items,
                               int& current_index) {
    if (items.empty()) {
      items.push_back("---");
      current_index = 0;
    }
    current_index = std::clamp(current_index, 0, (int)items.size() - 1);
    xe::app::ScopedAccentComboStyle combo_style;
    const char* preview = items[current_index];
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    bool open = ImGui::BeginCombo(id, preview);
    ImVec2 preview_min = ImGui::GetItemRectMin();
    ImVec2 preview_max = ImGui::GetItemRectMax();
    ImGui::PopStyleColor();
    xe::app::DrawConfiguredComboText(preview_min, preview_max, preview);
    bool changed = false;
    if (open) {
      for (int idx = 0; idx < static_cast<int>(items.size()); ++idx) {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        bool is_selected = current_index == idx;
        if (ImGui::Selectable(items[idx], is_selected)) {
          current_index = idx;
          changed = true;
        }
        ImVec2 item_min = ImGui::GetItemRectMin();
        ImVec2 item_max = ImGui::GetItemRectMax();
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
          xe::app::DrawConfiguredComboHighlight(item_min, item_max);
        }
        xe::app::DrawConfiguredComboText(item_min, item_max, items[idx]);
        if (is_selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
    return changed;
  };

  for (uint32_t i = 0; i < users_needed_; i++) {
    ImGui::BeginGroup();

    std::vector<const char*> combo_items;
    int items_count = 0;
    int current_item = 0;

    std::vector<uint8_t> slots;
    slots.push_back(XUserIndexAny);
    combo_items.push_back("---");
    for (auto& elem : slot_data_) {
      bool already_taken = false;
      for (uint32_t j = 0; j < users_needed_; j++) {
        if (chosen_slots_[j] == elem.first) {
          if (i == j) {
            current_item = static_cast<int>(combo_items.size());
          } else {
            already_taken = true;
          }
          break;
        }
      }

      if (already_taken) {
        continue;
      }

      slots.push_back(elem.first);
      combo_items.push_back(elem.second.c_str());
    }
    items_count = static_cast<int>(combo_items.size());

    ImGui::BeginDisabled(users_needed_ == 1);
    draw_signin_combo(fmt::format("##Slot{:d}", i).c_str(), combo_items,
                      current_item);
    chosen_slots_[i] = slots[current_item];
    ImGui::EndDisabled();
    ImGui::Spacing();

    combo_items.clear();
    current_item = 0;

    std::vector<uint64_t> xuids;
    xuids.push_back(0);
    combo_items.push_back("---");
    if (chosen_slots_[i] != XUserIndexAny) {
      for (auto& elem : profile_data_) {
        bool already_taken = false;
        for (uint32_t j = 0; j < users_needed_; j++) {
          if (chosen_xuids_[j] == elem.first) {
            if (i == j) {
              current_item = static_cast<int>(combo_items.size());
            } else {
              already_taken = true;
            }
            break;
          }
        }

        if (already_taken) {
          continue;
        }

        xuids.push_back(elem.first);
        combo_items.push_back(elem.second.c_str());
      }
    }
    items_count = static_cast<int>(combo_items.size());

    ImGui::BeginDisabled(chosen_slots_[i] == XUserIndexAny);
    draw_signin_combo(fmt::format("##Profile{:d}", i).c_str(), combo_items,
                      current_item);
    chosen_xuids_[i] = xuids[current_item];
    ImGui::EndDisabled();
    ImGui::Spacing();

    uint8_t slot = chosen_slots_[i];
    uint64_t xuid = chosen_xuids_[i];
    const auto account = profile_manager_->GetAccount(xuid);

    if (slot != XUserIndexAny && account) {
      xeDrawProfileContent(imgui_drawer(), xuid, slot, account, nullptr, {},
                           {}, nullptr);
    }

    ImGui::EndGroup();
    if (i != (users_needed_ - 1) && (i == 0 || i == 2)) {
      ImGui::SameLine();
    }
  }
  ImGui::PopItemWidth();

  ImGui::Spacing();

  // Create Profile button with same width as combo boxes
  const ImVec2 create_button_size(field_width, 0.0f);
  ImGui::SetCursorScreenPos(
      ImVec2(overlay_min.x + panel_padding.x,
             ImGui::GetCursorScreenPos().y));
  if (xe::app::DrawTextEffectButton("Create Profile", create_button_size)) {
    creating_profile_ = true;
    creating_profile_focus_requested_ = true;
#if XE_PLATFORM_WINRT
    UWP::ShowKeyboard();
#endif
  }
  ImGui::Spacing();

  // OK and Cancel buttons side-by-side with same total width as combo boxes
  const float button_gap = 8.0f * ux;
  const ImVec2 ok_cancel_button_size((field_width - button_gap) / 2.0f, 0.0f);
  ImGui::SetCursorScreenPos(
      ImVec2(overlay_min.x + panel_padding.x,
             ImGui::GetCursorScreenPos().y));
  bool can_confirm = false;
  for (uint32_t i = 0; i < users_needed_; i++) {
    if (chosen_slots_[i] != XUserIndexAny && chosen_xuids_[i] != 0) {
      can_confirm = true;
      break;
    }
  }
  ImGui::BeginDisabled(!can_confirm);
  if (xe::app::DrawTextEffectButton("OK", ok_cancel_button_size)) {
    std::map<uint8_t, uint64_t> profile_map;
    for (uint32_t i = 0; i < users_needed_; i++) {
      uint8_t slot = chosen_slots_[i];
      uint64_t xuid = chosen_xuids_[i];
      if (slot != XUserIndexAny && xuid != 0) {
        profile_map[slot] = xuid;
      }
    }
    pending_login_profiles_ = std::move(profile_map);
    ImGui::CloseCurrentPopup();
    Close();
  }
  ImGui::EndDisabled();
  ImGui::SameLine(0.0f, button_gap);

  if (xe::app::DrawTextEffectButton("Cancel", ok_cancel_button_size)) {
    pending_login_profiles_.clear();
    ImGui::CloseCurrentPopup();
    Close();
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
  xe::app::DrawFooterPrompt(root_draw_list, GetOrCreateButtonTexture('B'),
                            footer_text_size, footer_icon_size, "Back",
                            footer_back_y, footer_back_text_x,
                            footer_back_icon_offset);
  xe::app::DrawFooterPrompt(root_draw_list, GetOrCreateButtonTexture('A'),
                            footer_text_size, footer_icon_size, "Select",
                            footer_select_y, footer_select_text_x,
                            footer_select_icon_offset);

  // Create Profile popup (modal)
  ImGui::SetNextWindowPos(overlay_min, ImGuiCond_Always);
  ImGui::SetNextWindowSize(panel_size, ImGuiCond_Always);
  ImGuiWindowFlags create_flags = ImGuiWindowFlags_NoTitleBar |
                                  ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoSavedSettings |
                                  ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoBackground;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg,
                        ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  if (creating_profile_) {
    if (creating_profile_focus_requested_) {
      ImGui::SetNextWindowFocus();
    }
    ImGui::Begin(kCreateProfileWindowName, nullptr, create_flags);
    ImDrawList* create_draw_list = ImGui::GetWindowDrawList();
    const ImVec2 create_min = ImGui::GetWindowPos();
    const ImVec2 create_max(create_min.x + ImGui::GetWindowSize().x,
                            create_min.y + ImGui::GetWindowSize().y);
    xe::app::DrawGuidePanelBackground(create_draw_list, guide_bg_tex,
                                      create_min, create_max);

    if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
      std::fill(std::begin(gamertag_), std::end(gamertag_), '\0');
      creating_profile_ = false;
      creating_profile_focus_requested_ = false;
      focus_requested_ = true;
    }

    const xe::app::OverlayHeaderLayout create_header_layout =
        xe::app::DrawOverlayHeader(create_draw_list, create_min, panel_size,
                                   panel_padding, ux, uy, "Create Profile");

    const float create_content_start_y =
        create_header_layout.position.y + create_header_layout.font_size +
        55.0f;
    const float create_button_gap = ImGui::GetStyle().ItemSpacing.x;
    const ImVec2 create_button_size(
        (field_width - create_button_gap) * 0.5f, 0.0f);
    const float create_content_x = create_min.x + panel_padding.x;
    ImGui::SetCursorScreenPos(
        ImVec2(create_content_x, create_content_start_y));

    auto profile_manager =
        kernel_state()->xam_state()->profile_manager();

    xe::app::DrawConfiguredLabel("Gamertag:");
    if (creating_profile_focus_requested_) {
      ImGui::SetKeyboardFocusHere();
      creating_profile_focus_requested_ = false;
#if XE_PLATFORM_WINRT
      UWP::ShowKeyboard();
#endif
    }
    ImGui::SetCursorScreenPos(
        ImVec2(create_content_x, ImGui::GetCursorScreenPos().y));
    ImGui::PushItemWidth(create_button_size.x);
    xe::app::DrawConfiguredInputText("##Gamertag", gamertag_,
                                     sizeof(gamertag_));
    ImGui::PopItemWidth();
#if XE_PLATFORM_WINRT
    if (ImGui::IsItemActivated()) {
      UWP::ShowKeyboard();
    }
#endif
    const std::string gamertag_string = gamertag_;
    bool valid = profile_manager->IsGamertagValid(gamertag_string);

    ImGui::SetCursorScreenPos(
        ImVec2(create_content_x,
               ImGui::GetCursorScreenPos().y + (10.0f * uy)));
    ImGui::BeginDisabled(!valid);
    if (xe::app::DrawTextEffectButton("Create", create_button_size)) {
      profile_manager->CreateProfile(gamertag_string, false);
      std::fill(std::begin(gamertag_), std::end(gamertag_), '\0');
      creating_profile_ = false;
      creating_profile_focus_requested_ = false;
      focus_requested_ = true;
      ReloadProfiles(false);
    }
    ImGui::EndDisabled();
    ImGui::SameLine(0.0f, create_button_gap);

    if (xe::app::DrawTextEffectButton("Cancel", create_button_size)) {
      std::fill(std::begin(gamertag_), std::end(gamertag_), '\0');
      creating_profile_ = false;
      creating_profile_focus_requested_ = false;
      focus_requested_ = true;
    }

    const float create_footer_text_size = 13.5f * uy;
    const float create_footer_icon_size = 15.6f * uy;
    const float create_footer_spacing_y = 17.0f * uy;
    const float create_footer_select_y = create_max.y - (32.0f * uy);
    const float create_footer_back_y =
        create_footer_select_y - create_footer_spacing_y;
    const float create_footer_base_x = create_min.x + panel_size.x * 0.9f;
    const float create_footer_back_text_x =
        create_footer_base_x - (70.0f * ux);
    const float create_footer_back_icon_offset = 35.0f * ux;
    const float create_footer_select_text_x =
        create_footer_base_x - (60.0f * ux);
    const float create_footer_select_icon_offset = 42.0f * ux;
    xe::app::DrawFooterPrompt(create_draw_list, GetOrCreateButtonTexture('B'),
                              create_footer_text_size, create_footer_icon_size,
                              "Back", create_footer_back_y,
                              create_footer_back_text_x,
                              create_footer_back_icon_offset);
    xe::app::DrawFooterPrompt(create_draw_list, GetOrCreateButtonTexture('A'),
                              create_footer_text_size, create_footer_icon_size,
                              "Select", create_footer_select_y,
                              create_footer_select_text_x,
                              create_footer_select_icon_offset);

    ImGui::End();
  }
  ImGui::PopStyleColor(3);
  ImGui::PopStyleVar();

  ImGui::EndPopup();
#else
  constexpr const char* kSigninPopupName = "##signin_popup";
  constexpr const char* kCreateProfilePopupName = "##signin_create_profile";

  if (!has_opened_) {
    has_opened_ = true;
    ReloadProfiles(true);
    ImGui::OpenPopup(kSigninPopupName);
  }

  if (focus_requested_) {
    ImGui::SetNextWindowFocus();
    focus_requested_ = false;
  }

  if (!ImGui::BeginPopupModal(kSigninPopupName, nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    Close();
    return;
  }

  const bool create_popup_open =
      ImGui::IsPopupOpen(kCreateProfilePopupName, ImGuiPopupFlags_AnyPopupLevel);
  if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false) &&
      !create_popup_open) {
    pending_login_profiles_.clear();
    ImGui::CloseCurrentPopup();
    Close();
    ImGui::EndPopup();
    return;
  }

  auto draw_signin_combo = [&](const char* id,
                               std::vector<const char*>& items,
                               int& current_index) {
    if (items.empty()) {
      items.push_back("---");
      current_index = 0;
    }
    current_index = std::clamp(current_index, 0, (int)items.size() - 1);
    ImGui::Combo(id, &current_index, items.data(), static_cast<int>(items.size()));
  };

  for (uint32_t i = 0; i < users_needed_; i++) {
    ImGui::BeginGroup();

    std::vector<const char*> combo_items;
    int current_item = 0;

    std::vector<uint8_t> slots;
    slots.push_back(XUserIndexAny);
    combo_items.push_back("---");
    for (auto& elem : slot_data_) {
      bool already_taken = false;
      for (uint32_t j = 0; j < users_needed_; j++) {
        if (chosen_slots_[j] == elem.first) {
          if (i == j) {
            current_item = static_cast<int>(combo_items.size());
          } else {
            already_taken = true;
          }
          break;
        }
      }

      if (already_taken) {
        continue;
      }

      slots.push_back(elem.first);
      combo_items.push_back(elem.second.c_str());
    }

    ImGui::BeginDisabled(users_needed_ == 1);
    draw_signin_combo(fmt::format("##Slot{:d}", i).c_str(), combo_items,
                      current_item);
    chosen_slots_[i] = slots[current_item];
    ImGui::EndDisabled();
    ImGui::Spacing();

    combo_items.clear();
    current_item = 0;

    std::vector<uint64_t> xuids;
    xuids.push_back(0);
    combo_items.push_back("---");
    if (chosen_slots_[i] != XUserIndexAny) {
      for (auto& elem : profile_data_) {
        bool already_taken = false;
        for (uint32_t j = 0; j < users_needed_; j++) {
          if (chosen_xuids_[j] == elem.first) {
            if (i == j) {
              current_item = static_cast<int>(combo_items.size());
            } else {
              already_taken = true;
            }
            break;
          }
        }

        if (already_taken) {
          continue;
        }

        xuids.push_back(elem.first);
        combo_items.push_back(elem.second.c_str());
      }
    }

    ImGui::BeginDisabled(chosen_slots_[i] == XUserIndexAny);
    draw_signin_combo(fmt::format("##Profile{:d}", i).c_str(), combo_items,
                      current_item);
    chosen_xuids_[i] = xuids[current_item];
    ImGui::EndDisabled();
    ImGui::Spacing();

    uint8_t slot = chosen_slots_[i];
    uint64_t xuid = chosen_xuids_[i];
    const auto account = profile_manager_->GetAccount(xuid);
    if (slot != XUserIndexAny && account) {
      xeDrawProfileContent(imgui_drawer(), xuid, slot, account, nullptr, {},
                           {}, nullptr);
    }

    ImGui::EndGroup();
    if (i != (users_needed_ - 1) && (i == 0 || i == 2)) {
      ImGui::SameLine();
    }
  }

  ImGui::Spacing();

  if (ImGui::Button("Create Profile")) {
    creating_profile_ = true;
    creating_profile_focus_requested_ = true;
    ImGui::OpenPopup(kCreateProfilePopupName);
  }
  ImGui::Spacing();

  bool can_confirm = false;
  for (uint32_t i = 0; i < users_needed_; i++) {
    if (chosen_slots_[i] != XUserIndexAny && chosen_xuids_[i] != 0) {
      can_confirm = true;
      break;
    }
  }

  ImGui::BeginDisabled(!can_confirm);
  if (ImGui::Button("OK")) {
    std::map<uint8_t, uint64_t> profile_map;
    for (uint32_t i = 0; i < users_needed_; i++) {
      uint8_t slot = chosen_slots_[i];
      uint64_t xuid = chosen_xuids_[i];
      if (slot != XUserIndexAny && xuid != 0) {
        profile_map[slot] = xuid;
      }
    }
    pending_login_profiles_ = std::move(profile_map);
    ImGui::CloseCurrentPopup();
    Close();
    ImGui::EndDisabled();
    ImGui::EndPopup();
    return;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();

  if (ImGui::Button("Cancel")) {
    pending_login_profiles_.clear();
    ImGui::CloseCurrentPopup();
    Close();
    ImGui::EndPopup();
    return;
  }

  if (ImGui::BeginPopupModal(kCreateProfilePopupName, nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
      std::ranges::fill(gamertag_, '\0');
      creating_profile_ = false;
      creating_profile_focus_requested_ = false;
      focus_requested_ = true;
      ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    } else {
      ImGui::TextUnformatted("Gamertag:");
      if (creating_profile_focus_requested_) {
        ImGui::SetKeyboardFocusHere();
        creating_profile_focus_requested_ = false;
      }
      if (ImGui::InputText("##Gamertag", gamertag_, sizeof(gamertag_))) {
        valid_gamertag_ =
            profile_manager_->IsGamertagValid(std::string(gamertag_));
      }

      ImGui::BeginDisabled(!valid_gamertag_);
      if (ImGui::Button("Create")) {
        profile_manager_->CreateProfile(std::string(gamertag_), false);
        std::ranges::fill(gamertag_, '\0');
        creating_profile_ = false;
        creating_profile_focus_requested_ = false;
        focus_requested_ = true;
        ReloadProfiles(false);
        ImGui::CloseCurrentPopup();
        ImGui::EndDisabled();
        ImGui::EndPopup();
      } else {
        ImGui::EndDisabled();
        ImGui::SameLine();

        if (ImGui::Button("Cancel")) {
          std::ranges::fill(gamertag_, '\0');
          creating_profile_ = false;
          creating_profile_focus_requested_ = false;
          focus_requested_ = true;
          ImGui::CloseCurrentPopup();
          ImGui::EndPopup();
        } else {
          ImGui::EndPopup();
        }
      }
    }
  }

  ImGui::EndPopup();
#endif
}

void SigninUI::DispatchPendingLogins(
    std::map<uint8_t, uint64_t>&& profiles) {
  if (profiles.empty()) {
    return;
  }

  auto* profile_manager = profile_manager_;
  auto* display_window = kernel_state()->emulator()->display_window();
  if (display_window) {
    display_window->app_context().CallInUIThreadDeferred(
        [profile_manager, profiles = std::move(profiles)]() mutable {
          profile_manager->LoginMultiple(profiles);
        });
  } else {
    profile_manager->LoginMultiple(profiles);
  }
}

std::shared_ptr<xe::ui::ImmediateTexture> SigninUI::GetOrCreateButtonTexture(
    char button) {
  std::shared_ptr<xe::ui::ImmediateTexture>* dst = nullptr;
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

void SigninUI::ReloadProfiles(bool first_draw) {
  auto profile_manager = kernel_state()->xam_state()->profile_manager();
  auto profiles = profile_manager->GetAccounts();

  profile_data_.clear();
  for (auto& [xuid, account] : *profiles) {
    profile_data_.push_back({xuid, account.GetGamertagString()});
  }

  if (first_draw) {
    // If only one user is requested, request last used controller to sign in.
    if (users_needed_ == 1) {
      const auto connected_users = kernel_state()->GetConnectedUsers();
      if (last_user_ < XUserMaxUserCount &&
          (connected_users.none() || connected_users.test(last_user_))) {
        chosen_slots_[0] = static_cast<uint8_t>(last_user_);
      } else {
        chosen_slots_[0] = 0;
        for (uint8_t i = 0; i < XUserMaxUserCount; ++i) {
          if (connected_users.test(i)) {
            chosen_slots_[0] = i;
            break;
          }
        }
      }
    } else {
      for (uint32_t i = 0; i < users_needed_; i++) {
        // TODO: Not sure about this, needs testing on real hardware.
        chosen_slots_[i] = i;
      }
    }

    // Default profile selection to profile that is already signed in.
    for (auto& elem : profile_data_) {
      uint64_t xuid = elem.first;
      uint8_t slot = profile_manager->GetUserIndexAssignedToProfile(xuid);
      for (uint32_t j = 0; j < users_needed_; j++) {
        if (chosen_slots_[j] != XUserIndexAny && slot == chosen_slots_[j]) {
          chosen_xuids_[j] = xuid;
        }
      }
    }

    if (users_needed_ == 1 && chosen_slots_[0] != XUserIndexAny &&
        chosen_xuids_[0] == 0 && !profile_data_.empty()) {
      chosen_xuids_[0] = profile_data_.front().first;
    }
  }
}

}  // namespace ui
}  // namespace xam
}  // namespace kernel
}  // namespace xe
