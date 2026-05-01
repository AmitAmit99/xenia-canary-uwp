/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2025 Xenia Canary. All rights reserved.                          *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/kernel/xam/ui/create_profile_ui.h"
#include "xenia/app/ui_text_effect_helpers.h"
#include "xenia/emulator.h"
#if XE_PLATFORM_WINRT
#include "xenia-canary-uwp/WinRTKeyboard.h"
#endif

namespace xe {
namespace kernel {
namespace xam {
namespace ui {

void CreateProfileUI::OnDraw(ImGuiIO& io) {
#if XE_PLATFORM_WINRT
  if (!has_opened_) {
    has_opened_ = true;
    UWP::ShowKeyboard();
  }

  bool should_close = false;

  // Draw dimmed background overlay (same as search panel)
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
  const ImGuiWindowFlags dim_flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs |
      ImGuiWindowFlags_NoCollapse;
  if (ImGui::Begin("##create_profile_dim", nullptr, dim_flags)) {
    ImDrawList* dim_draw_list = ImGui::GetWindowDrawList();
    const ImVec2 dim_min = ImGui::GetWindowPos();
    const ImVec2 dim_max =
        ImVec2(dim_min.x + ImGui::GetWindowSize().x,
               dim_min.y + ImGui::GetWindowSize().y);
    dim_draw_list->AddRectFilled(
        dim_min, dim_max,
        ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.55f)));
  }
  ImGui::End();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar(3);

  // Load guide background texture
  static std::shared_ptr<xe::ui::ImmediateTexture> guide_bg_tex = nullptr;
  if (!guide_bg_tex) {
    guide_bg_tex = xe::app::LoadConfiguredGuideBackgroundTexture(imgui_drawer());
  }

  // Calculate panel dimensions (same as search panel)
  const ImVec2 panel_size =
      xe::app::GetGuidePanelSize(guide_bg_tex, io.DisplaySize.y);
  const ImVec2 panel_padding = xe::app::GetGuidePanelPadding();

  const ImVec2 viewport_pos = ImGui::GetMainViewport()->Pos;
  const ImVec2 overlay_pos(viewport_pos.x, viewport_pos.y);

  // Draw left-side panel with guide background
  ImGui::SetNextWindowPos(overlay_pos, ImGuiCond_Always);
  ImGui::SetNextWindowSize(panel_size, ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, panel_padding);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
  const ImGuiWindowFlags panel_flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoMove;
  
  if (ImGui::Begin("##create_profile_overlay", nullptr, panel_flags)) {
    ImDrawList* overlay_draw_list = ImGui::GetWindowDrawList();
    const ImVec2 overlay_min = ImGui::GetWindowPos();
    const ImVec2 overlay_max =
        ImVec2(overlay_min.x + ImGui::GetWindowSize().x,
               overlay_min.y + ImGui::GetWindowSize().y);

    xe::app::DrawGuidePanelBackground(overlay_draw_list, guide_bg_tex,
                                      overlay_min, overlay_max);

    // Handle B button to close
    if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
      should_close = true;
    }

    // Draw create profile content
    const float ux = io.DisplaySize.x / 1024.0f;
    const float uy = io.DisplaySize.y / 576.0f;
    const xe::app::OverlayHeaderLayout header_layout =
        xe::app::DrawOverlayHeader(overlay_draw_list, overlay_min, panel_size,
                                   panel_padding, ux, uy, "Create Profile");

    const float content_start_y =
        header_layout.position.y + header_layout.font_size + 55.0f;
    const float field_width = 320.0f * ux;
    const float button_gap = ImGui::GetStyle().ItemSpacing.x;

    ImGui::SetCursorScreenPos(
        ImVec2(overlay_min.x + panel_padding.x, content_start_y));

    auto profile_manager =
        emulator_->kernel_state()->xam_state()->profile_manager();

    if (gamertag_focus_requested_ &&
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)) {
      ImGui::SetKeyboardFocusHere(0);
      gamertag_focus_requested_ = false;
    }

    xe::app::DrawConfiguredLabel("Gamertag:");
    ImGui::SetNextItemWidth(field_width);
    xe::app::DrawConfiguredInputText("##Gamertag", gamertag_,
                                     sizeof(gamertag_));

    const std::string gamertag_string = std::string(gamertag_);
    bool valid = profile_manager->IsGamertagValid(gamertag_string);

    ImGui::SetCursorScreenPos(
        ImVec2(overlay_min.x + panel_padding.x,
               ImGui::GetCursorScreenPos().y + (10.0f * uy)));
    const ImVec2 button_size((field_width - button_gap) * 0.5f, 0.0f);
    ImGui::BeginDisabled(!valid);
    if (xe::app::DrawTextEffectButton("Create", button_size)) {
      bool autologin = (profile_manager->GetAccountCount() == 0);
      if (profile_manager->CreateProfile(gamertag_string, autologin,
                                         migration_) &&
          migration_) {
        emulator_->DataMigration(0xB13EBABEBABEBABE);
      }
      std::fill(std::begin(gamertag_), std::end(gamertag_), '\0');
      should_close = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine(0.0f, button_gap);

    if (xe::app::DrawTextEffectButton("Cancel", button_size)) {
      std::fill(std::begin(gamertag_), std::end(gamertag_), '\0');
      should_close = true;
    }
  }
  ImGui::End();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar(3);

  if (should_close) {
    Close();
  }
#else
  constexpr const char* kCreateProfilePopupName = "##create_profile_popup";

  if (!has_opened_) {
    has_opened_ = true;
    ImGui::OpenPopup(kCreateProfilePopupName);
  }

  if (!ImGui::BeginPopupModal(kCreateProfilePopupName, nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }

  if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
    std::fill(std::begin(gamertag_), std::end(gamertag_), '\0');
    ImGui::CloseCurrentPopup();
    Close();
    ImGui::EndPopup();
    return;
  }

  ImGui::TextUnformatted("Create Profile");
  ImGui::Separator();

  auto profile_manager = emulator_->kernel_state()->xam_state()->profile_manager();

  if (gamertag_focus_requested_ &&
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)) {
    ImGui::SetKeyboardFocusHere(0);
    gamertag_focus_requested_ = false;
  }

  ImGui::TextUnformatted("Gamertag:");
  ImGui::InputText("##Gamertag", gamertag_, sizeof(gamertag_));

  const std::string gamertag_string = std::string(gamertag_);
  bool valid = profile_manager->IsGamertagValid(gamertag_string);

  ImGui::BeginDisabled(!valid);
  if (ImGui::Button("Create")) {
    bool autologin = (profile_manager->GetAccountCount() == 0);
    if (profile_manager->CreateProfile(gamertag_string, autologin,
                                       migration_) &&
        migration_) {
      emulator_->DataMigration(0xB13EBABEBABEBABE);
    }
    std::fill(std::begin(gamertag_), std::end(gamertag_), '\0');
    ImGui::CloseCurrentPopup();
    Close();
    ImGui::EndDisabled();
    ImGui::EndPopup();
    return;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();

  if (ImGui::Button("Cancel")) {
    std::fill(std::begin(gamertag_), std::end(gamertag_), '\0');
    ImGui::CloseCurrentPopup();
    Close();
    ImGui::EndPopup();
    return;
  }

  ImGui::EndPopup();
#endif
}

}  // namespace ui
}  // namespace xam
}  // namespace kernel
}  // namespace xe
