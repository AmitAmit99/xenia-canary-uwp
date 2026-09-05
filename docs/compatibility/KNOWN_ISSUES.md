# Known Issues (real-device testing)

Findings confirmed through actual testing on Xbox Series X in Developer Mode, as opposed to the bulk-imported data in `xenia-compatibility-ledger.xlsx`. Updated as issues are investigated.

## Blocked — no fix possible on Xbox

### WWE SmackDown vs. Raw 2011 (SVR11), title ID `5451085D`
Hangs the D3D12 backend during gameplay. Root-caused to ~148,000 single-vertex memexport draws issued in a tight loop, overwhelming D3D12's per-draw submission overhead within Windows' TDR window. Draw-batching was ruled out as a fix — most of these draws don't read from a walkable vertex buffer at all (memexport-only), so there's no safe way to merge them.

The only known working fix is switching to the Vulkan graphics backend, which handles this draw pattern without issue on PC. **Vulkan does not exist on Xbox at the driver level at all** (confirmed by actually linking it into the Xbox build and testing — it's not a missing flag, there's no Vulkan driver on Xbox hardware for any app, ever). This title cannot currently be fixed on this port without a much deeper D3D12 backend rewrite.

As of the version noted in `CHANGELOG.md`, launching this specific title ID is now blocked with an explanatory message instead of being allowed to hang with no explanation. See `EmulatorWindow::RunTitle`'s `kVulkanRequiredTitleIds` set — add a title ID there for any other title confirmed to have the same "only fixable via Vulkan" root cause.

## Under investigation

### Metal Gear Solid V: The Phantom Pain
Reported to produce an error on install/launch regardless of whether Disc 1 is installed first. Not yet reproduced or diagnosed — exact error text needed.

### Unidentified title — cutscene camera/rendering glitch
Reported via a third-party video showing an extreme, rotated close-up during a dialogue cutscene. Game title not yet identified; could be an existing upstream Xenia rendering quirk rather than something specific to this port, since no rendering/shader code has been touched by this fork. Needs a game name and a reproducible case before it can be diagnosed.

## App-level issues (not game-specific)

### Settings/main-menu blade crash — FIXED
Was crashing on every visit to the Settings tab (it always resets to its first section on open). Root cause confirmed via crash-log trace logging: that section's Language dropdown looked up a setting, `user_language`, that was never actually registered anywhere in the code, despite three places in the UI assuming it existed - dereferencing a failed lookup unconditionally is undefined behavior. Fixed by registering the missing cvar. Two earlier guesses (a cvar guard in an unrelated section, dialog registration order) were both red herrings.

A second instance of the same bug class was found and fixed shortly after: scrolling into Settings section 9 (Video) crashed the same way, this time over a resolution setting (`internal_display_resolution`) that also never existed. A systematic sweep of every setting lookup in the Settings UI against the actual registered settings found 5 more UI controls referencing settings that don't exist, but none of them can crash - they're all guarded and just don't render. As of this sweep, no more crash-causing missing-setting lookups are known to remain in Settings.

### Mid-game pause menu closing itself after ~1 second — FIXED
The fix for the pause menu's controller input conflicting with the game's own input (both reacting to the same button presses) initially suppressed the wrong function, which also blinded the pause menu's own gamepad navigation. Fixed by moving the suppression to the same mechanism Xenia's built-in system dialogs already use, which only affects the guest game, not the UI.

### On-screen keyboard flashing open then closing when entering a name — FIXED (needs on-device confirmation)
Reported when entering a gamertag (profile creation, sign-in's Create Profile, and the game-list search box all hit this). Root cause: the Xbox on-screen keyboard is an IME-like text service that delivers typed characters through a focused `CoreTextEditContext`, not as raw key events - the code was only calling `CoreInputView.TryShowPrimaryView()` with no edit context holding focus, so the shell had nothing to consider "the focused text control" and hid the keyboard again almost immediately. Fixed by creating a real `CoreTextEditContext` (`WinRTKeyboard.cpp`) that calls `NotifyFocusEnter()` before showing the keyboard and forwards each `TextUpdating` event's characters into the same buffer physical-keyboard input already uses, plus `NotifyFocusLeave()`/`TryHidePrimaryView()` on close. Verified to build and link correctly; **not yet confirmed on real Xbox hardware** - this needs an on-device retest of entering a gamertag.

## UI polish
- Each blade tab (games/settings/paths/about) now tints the whole panel background with its own color, plus a brief fade-in when switching blades with LB/RB - matching the real Xbox 360 dashboard's per-blade background coloring (the first attempt at this only colored the tab label text, not the background - corrected after a reference screenshot comparison).
