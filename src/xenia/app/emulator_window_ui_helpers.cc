// Helper functions for WinRT frontend UI elements
#if XE_PLATFORM_WINRT

#include "xenia/app/emulator_window.h"
#include "third_party/stb/stb_image.h"
#include <cmath>
#include <filesystem>

namespace xe {
namespace app {

std::shared_ptr<ui::ImmediateTexture>
EmulatorWindow::WinRTFrontendDialog::GetOrCreateXeniaLogo() {
  if (xenia_logo_tex_ != nullptr) {
    return xenia_logo_tex_;
  }

  std::string path = "Assets/xenia-logo.png";
  if (!std::filesystem::exists(path)) {
    return nullptr;
  }

  if (emulator_window_.immediate_drawer_ == nullptr) {
    return nullptr;
  }

  int width = 0, height = 0, comp = 0;
  auto data = stbi_load(path.c_str(), &width, &height, &comp, 4);
  if (!data || width <= 0 || height <= 0) {
    return nullptr;
  }

  auto tex = emulator_window_.immediate_drawer_->CreateTexture(
      static_cast<uint32_t>(width), static_cast<uint32_t>(height),
      xe::ui::ImmediateTextureFilter::kLinear, false, data);
  stbi_image_free(data);

  xenia_logo_tex_ = std::move(tex);
  return xenia_logo_tex_;
}

std::shared_ptr<ui::ImmediateTexture>
EmulatorWindow::WinRTFrontendDialog::GetOrCreateButtonTexture(char button) {
  std::shared_ptr<ui::ImmediateTexture>* cached_tex = nullptr;
  const char* asset_name = nullptr;
  switch (button) {
    case 'A':
      cached_tex = &button_a_tex_;
      asset_name = "Assets/A.png";
      break;
    case 'B':
      cached_tex = &button_b_tex_;
      asset_name = "Assets/B.png";
      break;
    case 'X':
      cached_tex = &button_x_tex_;
      asset_name = "Assets/X.png";
      break;
    case 'Y':
      cached_tex = &button_y_tex_;
      asset_name = "Assets/Y.png";
      break;
    default:
      return nullptr;
  }

  if (*cached_tex) {
    return *cached_tex;
  }

  if (!asset_name || !std::filesystem::exists(asset_name) ||
      emulator_window_.immediate_drawer_ == nullptr) {
    return nullptr;
  }

  int width = 0, height = 0, comp = 0;
  auto data = stbi_load(asset_name, &width, &height, &comp, 4);
  if (!data || width <= 0 || height <= 0) {
    if (data) {
      stbi_image_free(data);
    }
    return nullptr;
  }

  auto tex = emulator_window_.immediate_drawer_->CreateTexture(
      static_cast<uint32_t>(width), static_cast<uint32_t>(height),
      xe::ui::ImmediateTextureFilter::kLinear, false, data);
  stbi_image_free(data);
  *cached_tex = std::move(tex);
  return *cached_tex;
}

}  // namespace app
}  // namespace xe

#endif  // XE_PLATFORM_WINRT
