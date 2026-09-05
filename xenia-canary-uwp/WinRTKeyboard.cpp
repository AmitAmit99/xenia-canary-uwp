#include "WinRTKeyboard.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Text.Core.h>
#include <winrt/Windows.UI.ViewManagement.Core.h>

using winrt::Windows::Foundation::Rect;
using winrt::Windows::UI::Core::CoreWindow;
using winrt::Windows::UI::Text::Core::CoreTextEditContext;
using winrt::Windows::UI::Text::Core::CoreTextInputPaneDisplayPolicy;
using winrt::Windows::UI::Text::Core::CoreTextRange;
using winrt::Windows::UI::Text::Core::CoreTextServicesManager;
using winrt::Windows::UI::Text::Core::CoreTextTextUpdatingResult;
using winrt::Windows::UI::ViewManagement::Core::CoreInputView;

namespace UWP {
std::vector<uint32_t> g_char_buffer;
std::mutex g_buffer_mutex;

// The on-screen keyboard is an IME-like text service: it delivers typed
// characters through a focused CoreTextEditContext's TextUpdating event, not
// as raw CoreWindow.CharacterReceived events (those only fire for a real
// attached keyboard). Just calling TryShowPrimaryView() with no edit context
// holding focus gives the shell nothing to consider "the focused text
// control", so it flashes the keyboard and immediately hides it again -
// this is that bug. Keeping the keyboard open requires a real
// CoreTextEditContext that calls NotifyFocusEnter() and answers the text
// service's requests; we don't otherwise track a backing buffer here since
// ImGui's InputText already owns the real text, so every request reports an
// empty field with the caret at 0 and each edit arrives as an insert at
// position 0 - Xenia already turns individual characters into a buffer via
// HandleCharacter() exactly like CharacterReceived does for a real keyboard.
static CoreTextEditContext g_edit_context{nullptr};
static bool g_edit_context_initialized = false;

static void EnsureEditContext() {
  if (g_edit_context_initialized) {
    return;
  }
  g_edit_context_initialized = true;

  g_edit_context =
      CoreTextServicesManager::GetForCurrentView().CreateEditContext();
  g_edit_context.InputPaneDisplayPolicy(
      CoreTextInputPaneDisplayPolicy::Manual);

  g_edit_context.TextRequested([](auto&&, auto const& args) {
    args.Request().Text(L"");
  });

  g_edit_context.SelectionRequested([](auto&&, auto const& args) {
    args.Request().Selection(CoreTextRange{0, 0});
  });

  g_edit_context.LayoutRequested([](auto&&, auto const& args) {
    Rect bounds(0.0f, 0.0f, 1.0f, 1.0f);
    try {
      auto window_bounds = CoreWindow::GetForCurrentThread().Bounds();
      bounds = Rect(0.0f, window_bounds.Height - 1.0f, 1.0f, 1.0f);
    } catch (...) {
    }
    args.Request().LayoutBounds().TextBounds(bounds);
    args.Request().LayoutBounds().ControlBounds(bounds);
  });

  g_edit_context.TextUpdating([](auto&&, auto const& args) {
    for (wchar_t c : args.Text()) {
      HandleCharacter(static_cast<uint32_t>(c));
    }
    args.Result(CoreTextTextUpdatingResult::Succeeded);
  });
}

void ShowKeyboard() {
  EnsureEditContext();
  g_edit_context.NotifyFocusEnter();
  CoreInputView::GetForCurrentView().TryShowPrimaryView();
}

void HideKeyboard() {
  if (!g_edit_context_initialized) {
    return;
  }
  g_edit_context.NotifyFocusLeave();
  CoreInputView::GetForCurrentView().TryHidePrimaryView();
}

void HandleCharacter(uint32_t keycode) {
  std::unique_lock lk(g_buffer_mutex);
  g_char_buffer.push_back(keycode);
}
}  // namespace UWP
