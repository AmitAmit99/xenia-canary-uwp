#ifndef XENIA_APP_UI_TEXT_EFFECT_HELPERS_H_
#define XENIA_APP_UI_TEXT_EFFECT_HELPERS_H_

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "third_party/stb/stb_image.h"
#include "xenia/base/cvar.h"
#include "xenia/ui/imgui_drawer.h"

namespace xe {
namespace app {

namespace ui_text_effect_helpers {

enum class ConfiguredTextEffect {
  kNone,
  kShadow,
  kLift,
  kStrokeFill,
  kGlow,
  kOutline,
  kDoubleShadow,
  kBold,
};

enum class ConfiguredTextColorMode {
  kWhite,
  kBlack,
};

struct ConfiguredTextPass {
  ImVec2 offset;
  float alpha_scale;
  bool use_opposite_color;
};

inline std::string GetStringConfigValue(const char* name,
                                        const char* fallback) {
  auto it = cvar::ConfigVars->find(name);
  if (it == cvar::ConfigVars->end()) {
    return fallback;
  }
  auto* value = dynamic_cast<cvar::ConfigVar<std::string>*>(it->second);
  return value ? value->GetTypedConfigValue() : std::string(fallback);
}

inline double GetDoubleConfigValue(const char* name, double fallback) {
  auto it = cvar::ConfigVars->find(name);
  if (it == cvar::ConfigVars->end()) {
    return fallback;
  }
  auto* value = dynamic_cast<cvar::ConfigVar<double>*>(it->second);
  return value ? value->GetTypedConfigValue() : fallback;
}

inline ConfiguredTextEffect GetConfiguredTextEffect() {
  const std::string effect = GetStringConfigValue("ui_text_effect", "stroke_fill");
  if (effect == "shadow") {
    return ConfiguredTextEffect::kShadow;
  }
  if (effect == "lift") {
    return ConfiguredTextEffect::kLift;
  }
  if (effect == "stroke_fill") {
    return ConfiguredTextEffect::kStrokeFill;
  }
  if (effect == "glow") {
    return ConfiguredTextEffect::kGlow;
  }
  if (effect == "outline") {
    return ConfiguredTextEffect::kOutline;
  }
  if (effect == "double_shadow") {
    return ConfiguredTextEffect::kDoubleShadow;
  }
  if (effect == "bold") {
    return ConfiguredTextEffect::kBold;
  }
  return ConfiguredTextEffect::kNone;
}

inline ConfiguredTextColorMode GetConfiguredTextColorMode() {
  return GetStringConfigValue("ui_text_color", "white") == "black"
             ? ConfiguredTextColorMode::kBlack
             : ConfiguredTextColorMode::kWhite;
}

inline uint8_t ClampTextColorChannel(float value) {
  return static_cast<uint8_t>(
      std::clamp<long>(std::lround(value), 0l, 255l));
}

inline bool IsNearlyMonochrome(ImU32 color) {
  const int r = static_cast<int>((color >> IM_COL32_R_SHIFT) & 0xFFu);
  const int g = static_cast<int>((color >> IM_COL32_G_SHIFT) & 0xFFu);
  const int b = static_cast<int>((color >> IM_COL32_B_SHIFT) & 0xFFu);
  return std::max({r, g, b}) - std::min({r, g, b}) <= 24;
}

inline float GetTextColorLuminance(ImU32 color) {
  const float r = float((color >> IM_COL32_R_SHIFT) & 0xFFu) / 255.0f;
  const float g = float((color >> IM_COL32_G_SHIFT) & 0xFFu) / 255.0f;
  const float b = float((color >> IM_COL32_B_SHIFT) & 0xFFu) / 255.0f;
  return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

inline ImU32 ApplyConfiguredBrightnessContrast(ImU32 color) {
  const float brightness =
      std::max(0.0f, static_cast<float>(GetDoubleConfigValue("ui_text_brightness", 1.0)));
  const float contrast =
      std::max(0.0f, static_cast<float>(GetDoubleConfigValue("ui_text_contrast", 1.0)));
  const uint32_t alpha = (color >> IM_COL32_A_SHIFT) & 0xFFu;
  auto transform_channel = [&](uint32_t shift) {
    float channel = float((color >> shift) & 0xFFu) / 255.0f;
    channel = ((channel - 0.5f) * contrast) + 0.5f;
    channel *= brightness;
    return ClampTextColorChannel(channel * 255.0f);
  };
  return IM_COL32(transform_channel(IM_COL32_R_SHIFT),
                  transform_channel(IM_COL32_G_SHIFT),
                  transform_channel(IM_COL32_B_SHIFT), alpha);
}

inline ImU32 GetConfiguredBaseTextColor(ImU32 color) {
  const uint32_t alpha = (color >> IM_COL32_A_SHIFT) & 0xFFu;
  if (IsNearlyMonochrome(color)) {
    const uint8_t channel =
        GetConfiguredTextColorMode() == ConfiguredTextColorMode::kBlack ? 0u : 255u;
    color = IM_COL32(channel, channel, channel, alpha);
  }
  return ApplyConfiguredBrightnessContrast(color);
}

inline ImU32 GetConfiguredPassColor(const ConfiguredTextPass& pass,
                                    ImU32 base_color) {
  const uint32_t base_alpha = (base_color >> IM_COL32_A_SHIFT) & 0xFFu;
  const uint32_t alpha = static_cast<uint32_t>(std::clamp<int>(
      static_cast<int>(std::lround(float(base_alpha) * pass.alpha_scale)), 0,
      255));
  if (!pass.use_opposite_color) {
    return (base_color & 0x00FFFFFFu) | (alpha << IM_COL32_A_SHIFT);
  }
  const uint8_t channel = GetTextColorLuminance(base_color) >= 0.5f ? 0u : 255u;
  return IM_COL32(channel, channel, channel, alpha);
}

inline std::vector<ConfiguredTextPass> GetConfiguredTextPasses(
    ConfiguredTextEffect effect) {
  switch (effect) {
    case ConfiguredTextEffect::kShadow:
      return {{ImVec2(1.5f, 1.5f), 180.0f / 255.0f, true}};
    case ConfiguredTextEffect::kLift:
      return {{ImVec2(-1.0f, -1.0f), 150.0f / 255.0f, true}};
    case ConfiguredTextEffect::kStrokeFill:
      return {{ImVec2(-1.0f, 0.0f), 0.95f, true},
              {ImVec2(1.0f, 0.0f), 0.95f, true},
              {ImVec2(0.0f, -1.0f), 0.95f, true},
              {ImVec2(0.0f, 1.0f), 0.95f, true},
              {ImVec2(-1.0f, -1.0f), 0.75f, true},
              {ImVec2(1.0f, -1.0f), 0.75f, true},
              {ImVec2(-1.0f, 1.0f), 0.75f, true},
              {ImVec2(1.0f, 1.0f), 0.75f, true},
              {ImVec2(0.7f, 0.0f), 1.0f, false}};
    case ConfiguredTextEffect::kGlow:
      return {{ImVec2(-1.5f, 0.0f), 0.30f, true},
              {ImVec2(1.5f, 0.0f), 0.30f, true},
              {ImVec2(0.0f, -1.5f), 0.30f, true},
              {ImVec2(0.0f, 1.5f), 0.30f, true},
              {ImVec2(-1.5f, -1.5f), 0.24f, true},
              {ImVec2(1.5f, -1.5f), 0.24f, true},
              {ImVec2(-1.5f, 1.5f), 0.24f, true},
              {ImVec2(1.5f, 1.5f), 0.24f, true},
              {ImVec2(-3.0f, 0.0f), 0.14f, true},
              {ImVec2(3.0f, 0.0f), 0.14f, true},
              {ImVec2(0.0f, -3.0f), 0.14f, true},
              {ImVec2(0.0f, 3.0f), 0.14f, true}};
    case ConfiguredTextEffect::kOutline:
      return {{ImVec2(-1.0f, 0.0f), 0.90f, true},
              {ImVec2(1.0f, 0.0f), 0.90f, true},
              {ImVec2(0.0f, -1.0f), 0.90f, true},
              {ImVec2(0.0f, 1.0f), 0.90f, true},
              {ImVec2(-1.0f, -1.0f), 0.70f, true},
              {ImVec2(1.0f, -1.0f), 0.70f, true},
              {ImVec2(-1.0f, 1.0f), 0.70f, true},
              {ImVec2(1.0f, 1.0f), 0.70f, true}};
    case ConfiguredTextEffect::kDoubleShadow:
      return {{ImVec2(1.5f, 1.5f), 0.55f, true},
              {ImVec2(3.0f, 3.0f), 0.30f, true}};
    case ConfiguredTextEffect::kBold:
      return {{ImVec2(0.7f, 0.0f), 1.0f, false},
              {ImVec2(0.0f, 0.45f), 0.75f, false}};
    case ConfiguredTextEffect::kNone:
    default:
      return {};
  }
}

inline void ConfiguredTextMarkerCallback(const ImDrawList*, const ImDrawCmd*) {}

inline void MarkConfiguredTextBegin(ImDrawList* draw_list) {
  draw_list->AddCallback(ConfiguredTextMarkerCallback,
                         reinterpret_cast<void*>(1));
}

inline void MarkConfiguredTextEnd(ImDrawList* draw_list) {
  draw_list->AddCallback(ConfiguredTextMarkerCallback,
                         reinterpret_cast<void*>(2));
}

}  // namespace ui_text_effect_helpers

inline ImVec4 GetConfiguredUIAccentColor(float alpha = 1.0f) {
  const std::string accent =
      ui_text_effect_helpers::GetStringConfigValue("ui_border_color", "green");
  if (accent == "purple") {
    return ImVec4(168.0f / 255.0f, 78.0f / 255.0f, 211.0f / 255.0f, alpha);
  }
  if (accent == "red") {
    return ImVec4(206.0f / 255.0f, 46.0f / 255.0f, 60.0f / 255.0f, alpha);
  }
  if (accent == "orange") {
    return ImVec4(224.0f / 255.0f, 119.0f / 255.0f, 47.0f / 255.0f, alpha);
  }
  if (accent == "blue") {
    return ImVec4(104.0f / 255.0f, 157.0f / 255.0f, 203.0f / 255.0f, alpha);
  }
  if (accent == "yellow") {
    return ImVec4(214.0f / 255.0f, 195.0f / 255.0f, 82.0f / 255.0f, alpha);
  }
  if (accent == "grey") {
    return ImVec4(215.0f / 255.0f, 217.0f / 255.0f, 219.0f / 255.0f, alpha);
  }
  if (accent == "black") {
    return ImVec4(0.0f, 0.0f, 0.0f, alpha);
  }
  if (accent == "white") {
    return ImVec4(1.0f, 1.0f, 1.0f, alpha);
  }
  return ImVec4(97.0f / 255.0f, 192.0f / 255.0f, 50.0f / 255.0f, alpha);
}

inline bool UseDarkAccentComboText() {
  return ui_text_effect_helpers::GetStringConfigValue("ui_border_color", "green") ==
         "white";
}

struct ScopedAccentComboStyle {
  explicit ScopedAccentComboStyle() : pushed_text_(UseDarkAccentComboText()) {
    const ImVec4 accent = GetConfiguredUIAccentColor();
    ImGui::PushStyleColor(ImGuiCol_Border,
                          ImVec4(accent.x, accent.y, accent.z, 0.75f));
    ImGui::PushStyleColor(ImGuiCol_Button,
                          ImVec4(accent.x, accent.y, accent.z, 0.75f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(accent.x, accent.y, accent.z, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          ImVec4(accent.x, accent.y, accent.z, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Header,
                          ImVec4(accent.x, accent.y, accent.z, 0.80f));
    if (pushed_text_) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    }
  }

  ~ScopedAccentComboStyle() { ImGui::PopStyleColor(pushed_text_ ? 6 : 5); }

 private:
  bool pushed_text_;
};

inline void MarkConfiguredTextBegin(ImDrawList* draw_list) {
  ui_text_effect_helpers::MarkConfiguredTextBegin(draw_list);
}

inline void MarkConfiguredTextEnd(ImDrawList* draw_list) {
  ui_text_effect_helpers::MarkConfiguredTextEnd(draw_list);
}

inline void DrawTextWithConfiguredEffect(ImDrawList* draw_list, ImFont* font,
                                         float font_size, ImVec2 pos,
                                         ImU32 color, const char* text) {
  if (!text || !text[0]) {
    return;
  }
  const ImU32 base_color =
      ui_text_effect_helpers::GetConfiguredBaseTextColor(color);
  ui_text_effect_helpers::MarkConfiguredTextBegin(draw_list);
  for (const ui_text_effect_helpers::ConfiguredTextPass& pass :
       ui_text_effect_helpers::GetConfiguredTextPasses(
           ui_text_effect_helpers::GetConfiguredTextEffect())) {
    draw_list->AddText(font, font_size,
                       ImVec2(pos.x + pass.offset.x, pos.y + pass.offset.y),
                       ui_text_effect_helpers::GetConfiguredPassColor(pass,
                                                                      base_color),
                       text);
  }
  draw_list->AddText(font, font_size, pos, base_color, text);
  ui_text_effect_helpers::MarkConfiguredTextEnd(draw_list);
}

inline void DrawTextWithConfiguredEffect(ImDrawList* draw_list, ImFont* font,
                                         float font_size, ImVec2 pos,
                                         ImU32 color, const char* text,
                                         float wrap_width) {
  if (!text || !text[0]) {
    return;
  }
  const ImU32 base_color =
      ui_text_effect_helpers::GetConfiguredBaseTextColor(color);
  ui_text_effect_helpers::MarkConfiguredTextBegin(draw_list);
  for (const ui_text_effect_helpers::ConfiguredTextPass& pass :
       ui_text_effect_helpers::GetConfiguredTextPasses(
           ui_text_effect_helpers::GetConfiguredTextEffect())) {
    draw_list->AddText(font, font_size,
                       ImVec2(pos.x + pass.offset.x, pos.y + pass.offset.y),
                       ui_text_effect_helpers::GetConfiguredPassColor(pass,
                                                                      base_color),
                       text, nullptr, wrap_width);
  }
  draw_list->AddText(font, font_size, pos, base_color, text, nullptr,
                     wrap_width);
  ui_text_effect_helpers::MarkConfiguredTextEnd(draw_list);
}

inline void DrawConfiguredLabel(const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  ImGui::TextUnformatted(text);
  ImVec2 label_min = ImGui::GetItemRectMin();
  ImGui::PopStyleColor();
  DrawTextWithConfiguredEffect(ImGui::GetWindowDrawList(), ImGui::GetFont(),
                               ImGui::GetFontSize(), label_min,
                               ImGui::GetColorU32(ImGuiCol_Text), text);
}

inline bool DrawConfiguredInputText(const char* id, char* buffer,
                                    size_t buffer_size,
                                    ImGuiInputTextFlags flags = 0) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,
                        ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive,
                        ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  bool changed = ImGui::InputText(id, buffer, buffer_size, flags);
  const ImVec2 input_min = ImGui::GetItemRectMin();
  const ImVec2 input_max = ImGui::GetItemRectMax();
  ImGui::PopStyleColor(5);
  const ImGuiStyle& style = ImGui::GetStyle();
  const float line_height = ImGui::GetTextLineHeight();
  const ImVec2 text_pos(input_min.x + style.FramePadding.x,
                        input_min.y +
                            (input_max.y - input_min.y - line_height) * 0.5f);
  DrawTextWithConfiguredEffect(ImGui::GetWindowDrawList(), ImGui::GetFont(),
                               ImGui::GetFontSize(), text_pos,
                               ImGui::GetColorU32(ImGuiCol_Text), buffer);
  return changed;
}

inline bool DrawTextEffectButton(const char* label,
                                 const ImVec2& size = ImVec2(0.0f, 0.0f)) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  bool pressed = ImGui::Button(label, size);
  ImVec2 button_min = ImGui::GetItemRectMin();
  ImVec2 button_max = ImGui::GetItemRectMax();
  ImGui::PopStyleColor();
  const ImVec2 label_size = ImGui::CalcTextSize(label);
  const ImVec2 label_pos(
      button_min.x + (button_max.x - button_min.x - label_size.x) * 0.5f,
      button_min.y + (button_max.y - button_min.y - label_size.y) * 0.5f);
  DrawTextWithConfiguredEffect(ImGui::GetWindowDrawList(), ImGui::GetFont(),
                               ImGui::GetFontSize(), label_pos,
                               ImGui::GetColorU32(ImGuiCol_Text), label);
  return pressed;
}

inline void DrawConfiguredParagraph(const std::string& text) {
  const ImVec2 text_pos = ImGui::GetCursorScreenPos();
  DrawTextWithConfiguredEffect(ImGui::GetWindowDrawList(), ImGui::GetFont(),
                               ImGui::GetFontSize(), text_pos,
                               ImGui::GetColorU32(ImGuiCol_Text),
                               text.c_str());
  const ImVec2 size = ImGui::CalcTextSize(text.c_str(), nullptr, false,
                                          ImGui::GetContentRegionAvail().x);
  ImGui::Dummy(size);
}

inline void DrawConfiguredComboText(const ImVec2& min, const ImVec2& max,
                                    const char* text) {
  const ImGuiStyle& style = ImGui::GetStyle();
  const float line_height = ImGui::GetTextLineHeight();
  const ImVec2 text_pos(min.x + style.FramePadding.x,
                        min.y + (max.y - min.y - line_height) * 0.5f);
  DrawTextWithConfiguredEffect(ImGui::GetWindowDrawList(), ImGui::GetFont(),
                               ImGui::GetFontSize(), text_pos,
                               ImGui::GetColorU32(ImGuiCol_Text), text);
}

inline void DrawConfiguredComboHighlight(const ImVec2& min, const ImVec2& max,
                                         float thickness_scale = 2.0f,
                                         float rounding = 0.0f) {
  const ImVec4 border_color = GetConfiguredUIAccentColor(0.95f);
  ImGui::GetWindowDrawList()->AddRect(
      min, max, ImGui::GetColorU32(border_color),
      rounding, 0,
      std::max(1.0f, thickness_scale * (ImGui::GetIO().DisplayFramebufferScale.x > 0
                                            ? ImGui::GetIO().DisplayFramebufferScale.x
                                            : 1.0f)));
}

inline std::string GetClockString() {
  std::time_t now = std::time(nullptr);
  if (now == -1) {
    return "";
  }
  std::tm time_info = {};
#if XE_PLATFORM_WIN32
  localtime_s(&time_info, &now);
#else
  localtime_r(&now, &time_info);
#endif
  char buffer[16] = {};
  if (std::strftime(buffer, sizeof(buffer), "%I:%M %p", &time_info) == 0) {
    return "";
  }
  return buffer;
}

inline std::shared_ptr<ui::ImmediateTexture> LoadOverlayTexture(
    ui::ImGuiDrawer* imgui_drawer, const char* asset_path, bool transparent) {
  if (!imgui_drawer || !asset_path || !std::filesystem::exists(asset_path)) {
    return nullptr;
  }
  auto* drawer = imgui_drawer->GetImmediateDrawer();
  if (!drawer) {
    return nullptr;
  }

  int width = 0;
  int height = 0;
  int channels = 0;
  unsigned char* image_data =
      stbi_load(asset_path, &width, &height, &channels, STBI_rgb_alpha);
  if (!image_data || width <= 0 || height <= 0) {
    if (image_data) {
      stbi_image_free(image_data);
    }
    return nullptr;
  }

  auto texture = drawer->CreateTexture(
      static_cast<uint32_t>(width), static_cast<uint32_t>(height),
      ui::ImmediateTextureFilter::kLinear, transparent,
      reinterpret_cast<uint8_t*>(image_data));
  stbi_image_free(image_data);
  if (!texture) {
    return nullptr;
  }
  return std::shared_ptr<ui::ImmediateTexture>(texture.release());
}

inline std::string ResolveConfiguredGuideBackgroundAssetPath(
    bool prefer_wide = false) {
  std::string guide_name =
      ui_text_effect_helpers::GetStringConfigValue("ui_guide_image", "blades.png");
  if (guide_name.empty() || guide_name.find('.') == std::string::npos) {
    guide_name = "blades.png";
  }

  const auto has_suffix = [](const std::string& value,
                             const std::string& suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) ==
               0;
  };

  std::filesystem::path guide_path(guide_name);
  std::string guide_stem = guide_path.stem().string();
  std::string guide_ext = guide_path.extension().string();
  if (guide_ext.empty()) {
    guide_ext = ".png";
  }

  std::vector<std::string> guide_asset_candidates;
  if (prefer_wide) {
    if (has_suffix(guide_stem, "_wide") || has_suffix(guide_stem, "-wide")) {
      guide_asset_candidates.push_back(guide_name);
    } else {
      guide_asset_candidates.push_back(guide_stem + "_wide" + guide_ext);
      guide_asset_candidates.push_back(guide_stem + "-wide" + guide_ext);
    }
    guide_asset_candidates.push_back("blades_wide.png");
    guide_asset_candidates.push_back("guide-wide.png");
  }

  guide_asset_candidates.push_back(guide_name);
  guide_asset_candidates.push_back("blades.png");

  for (const auto& candidate : guide_asset_candidates) {
    std::string candidate_path = "Assets/guide_images/" + candidate;
    if (std::filesystem::exists(candidate_path)) {
      return candidate_path;
    }
  }

  return "Assets/search.png";
}

inline std::shared_ptr<ui::ImmediateTexture> LoadConfiguredGuideBackgroundTexture(
    ui::ImGuiDrawer* imgui_drawer, bool prefer_wide = false) {
  const std::string guide_asset_path =
      ResolveConfiguredGuideBackgroundAssetPath(prefer_wide);
  return LoadOverlayTexture(imgui_drawer, guide_asset_path.c_str(), true);
}

inline std::shared_ptr<ui::ImmediateTexture> LoadButtonTexture(
    ui::ImGuiDrawer* imgui_drawer, char button) {
  const char* asset_path = nullptr;
  switch (button) {
    case 'A':
      asset_path = "Assets/A.png";
      break;
    case 'B':
      asset_path = "Assets/B.png";
      break;
    default:
      return nullptr;
  }
  return LoadOverlayTexture(imgui_drawer, asset_path, false);
}

inline ImVec2 GetGuidePanelSize(
    const std::shared_ptr<ui::ImmediateTexture>& guide_texture,
    float display_height) {
  const float fallback_aspect = 520.0f / 640.0f;
  float texture_aspect = fallback_aspect;
  if (guide_texture && guide_texture->height > 0) {
    texture_aspect = static_cast<float>(guide_texture->width) /
                     static_cast<float>(guide_texture->height);
  }
  return ImVec2(texture_aspect * display_height, display_height);
}

inline ImVec2 GetGuidePanelPadding() { return ImVec2(32.0f, 34.0f); }

inline void DrawGuidePanelBackground(
    ImDrawList* draw_list,
    const std::shared_ptr<ui::ImmediateTexture>& guide_texture,
    const ImVec2& overlay_min, const ImVec2& overlay_max) {
  if (guide_texture) {
    draw_list->AddImage(reinterpret_cast<ImTextureID>(guide_texture.get()),
                        overlay_min, overlay_max);
  } else {
    draw_list->AddRectFilled(
        overlay_min, overlay_max,
        ImGui::GetColorU32(ImVec4(0.10f, 0.12f, 0.14f, 0.92f)));
  }
}

struct OverlayHeaderLayout {
  float font_size;
  ImVec2 position;
};

inline OverlayHeaderLayout DrawOverlayHeader(
    ImDrawList* draw_list, const ImVec2& overlay_min, const ImVec2& panel_size,
    const ImVec2& panel_padding, float ux, float uy, const char* title,
    bool draw_clock = true) {
  OverlayHeaderLayout layout = {26.0f * uy,
                                ImVec2(overlay_min.x + 30.0f * ux,
                                       overlay_min.y + 18.0f * uy)};
  ImFont* header_font = ImGui::GetFont();
  const ImU32 header_color = IM_COL32(228, 228, 228, 255);
  DrawTextWithConfiguredEffect(draw_list, header_font, layout.font_size,
                               layout.position, header_color, title);

  if (!draw_clock) {
    return layout;
  }

  const std::string clock_text = GetClockString();
  if (clock_text.empty()) {
    return layout;
  }

  const ImVec2 clock_size = header_font->CalcTextSizeA(
      layout.font_size, std::numeric_limits<float>::max(), 0.0f,
      clock_text.c_str(), nullptr, nullptr);
  const float clock_x = overlay_min.x + panel_size.x - panel_padding.x -
                        clock_size.x - (40.0f * ux);
  DrawTextWithConfiguredEffect(draw_list, header_font, layout.font_size,
                               ImVec2(clock_x, layout.position.y),
                               header_color, clock_text.c_str());
  return layout;
}

inline void DrawFooterPrompt(ImDrawList* draw_list,
                             const std::shared_ptr<ui::ImmediateTexture>& tex,
                             float text_size, float icon_size,
                             const char* text, float text_y, float text_x,
                             float icon_offset) {
  DrawTextWithConfiguredEffect(draw_list, ImGui::GetFont(), text_size,
                               ImVec2(text_x, text_y),
                               IM_COL32(205, 205, 205, 255), text);
  if (!tex) {
    return;
  }
  const ImVec2 icon_center(text_x + icon_offset, text_y + text_size * 0.5f);
  const ImVec2 icon_half(icon_size * 0.5f, icon_size * 0.5f);
  draw_list->AddImage(reinterpret_cast<ImTextureID>(tex.get()),
                      ImVec2(icon_center.x - icon_half.x,
                             icon_center.y - icon_half.y),
                      ImVec2(icon_center.x + icon_half.x,
                             icon_center.y + icon_half.y));
}

inline void RotateTextVertices(ImDrawList* draw_list, int vertex_start,
                               int vertex_end, ImVec2 anchor,
                               float rotation_radians) {
  const float cos_angle = std::cos(rotation_radians);
  const float sin_angle = std::sin(rotation_radians);
  for (int i = vertex_start; i < vertex_end; ++i) {
    const ImVec2 pos = draw_list->VtxBuffer[i].pos;
    const float dx = pos.x - anchor.x;
    const float dy = pos.y - anchor.y;
    draw_list->VtxBuffer[i].pos =
        ImVec2(anchor.x + dx * cos_angle - dy * sin_angle,
               anchor.y + dx * sin_angle + dy * cos_angle);
  }
}

inline void DrawRotatedTextWithConfiguredEffect(ImDrawList* draw_list,
                                                ImFont* font,
                                                float font_size,
                                                ImVec2 anchor, ImU32 color,
                                                const char* text,
                                                float rotation_radians) {
  if (!text || !text[0]) {
    return;
  }
  const ImU32 base_color =
      ui_text_effect_helpers::GetConfiguredBaseTextColor(color);
  ui_text_effect_helpers::MarkConfiguredTextBegin(draw_list);
  for (const ui_text_effect_helpers::ConfiguredTextPass& pass :
       ui_text_effect_helpers::GetConfiguredTextPasses(
           ui_text_effect_helpers::GetConfiguredTextEffect())) {
    ImVec2 effect_anchor(anchor.x + pass.offset.x, anchor.y + pass.offset.y);
    const int effect_vertex_start = draw_list->VtxBuffer.Size;
    draw_list->AddText(font, font_size, effect_anchor,
                       ui_text_effect_helpers::GetConfiguredPassColor(pass,
                                                                      base_color),
                       text);
    const int effect_vertex_end = draw_list->VtxBuffer.Size;
    RotateTextVertices(draw_list, effect_vertex_start, effect_vertex_end,
                       effect_anchor, rotation_radians);
  }
  const int vertex_start = draw_list->VtxBuffer.Size;
  draw_list->AddText(font, font_size, anchor, base_color, text);
  const int vertex_end = draw_list->VtxBuffer.Size;
  RotateTextVertices(draw_list, vertex_start, vertex_end, anchor,
                     rotation_radians);
  ui_text_effect_helpers::MarkConfiguredTextEnd(draw_list);
}

}  // namespace app
}  // namespace xe

#endif
