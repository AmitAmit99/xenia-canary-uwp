# Known Issues (real-device testing)

Findings confirmed through actual testing on Xbox Series X in Developer Mode, as opposed to the bulk-imported data in `xenia-compatibility-ledger-v2.xlsx` and the generated `compatibility_list_v*.xlsx`/`.html`. Updated as issues are investigated.

**Read this before trusting a status from the compatibility list:** every Status/Config value in that ledger (Playable/Gameplay/Reaches UI only/Broken/Untriaged) is bulk-imported from the **desktop** `xenia-canary/game-compatibility` tracker — some rows even further back, from an older pre-Canary tracker whose own entries are explicitly marked "unverified on Canary." None of it has been re-tested on this specific Xbox UWP port. Concrete example: the ledger lists WWE 2K16 as "Reaches UI only" (from the legacy tracker), but it's been reported as playing with no issues on this port — the ledger simply hasn't caught up, in either direction. Treat the ledger as a starting reference, not a verdict on this port; the only statuses this project has actually confirmed itself are what's documented below, plus the "Unplayable" titles blocked in-app (see the next section) — everything else in the spreadsheet could be stale, PC-specific, or simply wrong for Xbox. If you hit a mismatch, that's the ledger being wrong, not you.

## Compatibility has also shifted relative to the original xenia-canary-uwp fork

Reported pattern: some titles that ran on the original `danprice142/xenia-canary-uwp` no longer run on this fork, and some titles that didn't run there now do. This is expected, not a regression introduced by this fork's own changes: this fork rebased onto current Xenia Canary (~440 commits ahead of where the original was forked — see `CHANGELOG.md`), and upstream Xenia Canary's own compatibility isn't monotonic. A PR that fixes one title's rendering can regress a different title's edge case, add a stricter check that a previously-working title happened to depend on being loose, or change timing enough to expose a race that was previously masked. None of that is specific to this being a UWP port — desktop Xenia Canary goes through the exact same churn between its own versions.

Practical effect: **neither the original fork's old behavior nor the PC compatibility ledger is a reliable predictor for this specific build.** The only trustworthy source for "does title X run on this fork" is testing title X on this fork, on real Xbox hardware — which is exactly what `uwp_compatibility_list.xlsx` exists to accumulate. If you find a title that changed status in either direction, it's worth a report (ideally with an `xenia.log`) so the actual cause - an upstream regression, a config difference, or something this fork's own code changed - can be identified rather than guessed at.

## Blocked — needs Vulkan, which doesn't exist on Xbox

**Vulkan does not exist on Xbox at the driver level at all** (confirmed by actually linking it into the Xbox build and testing — it's not a missing flag, there's no Vulkan driver on Xbox hardware for any app, ever). D3D12 is the only backend this port can ever use. `EmulatorWindow::RunTitle`'s `kVulkanRequiredTitleIds` set (in `emulator_window.cc`) blocks launching every title below with an explanatory message instead of letting it hang or render incorrectly with no explanation.

### WWE SmackDown vs. Raw 2011 (SVR11), title ID `5451085D` — verified on this port
Hangs the D3D12 backend during gameplay. Root-caused to ~148,000 single-vertex memexport draws issued in a tight loop, overwhelming D3D12's per-draw submission overhead within Windows' TDR window. Draw-batching was ruled out as a fix — most of these draws don't read from a walkable vertex buffer at all (memexport-only), so there's no safe way to merge them. The only known working fix is the Vulkan backend, which handles this draw pattern without issue on PC.

### 13 more titles — community-reported, not independently re-verified on this port
The compatibility ledger's "Fix Details" column is best-effort text pulled from the upstream `xenia-canary/game-compatibility` tracker (not a manual re-verification of every thread — see the ledger's own Legend tab). Any title whose extracted fix notes set `gpu="vulkan"` explicitly (a required backend switch, not just a `render_target_path_vulkan=...` tuning value that has a working `render_target_path_d3d12=...` counterpart alongside it) was added to the block list on the same reasoning as SVR11: the community found no other way to fix whatever D3D12 issue it has, and Vulkan isn't available here either way. These are **not confirmed to hang** the way SVR11 does — the underlying D3D12 problem could be a hang, a crash, or just incorrect rendering — only that Vulkan is the community's only known fix for it:

| Title ID | Game |
| --- | --- |
| `545407F2` | Grand Theft Auto IV |
| `534307E7` | Just Cause 2 |
| `545407E6` | Mafia II |
| `4B4E085C` | Metal Gear Solid V: Ground Zeroes |
| `58411202` | Sonic Adventure 2 |
| `58410B5D` | Burnout CRASH! |
| `45410891` | EA SPORTS Grand Slam Tennis 2 |
| `535107E4` | Final Fantasy XIII |
| `4D5307FA` | Lost Odyssey |
| `584108B3` | Rez HD |
| `45410852` | The Lord of the Rings: Conquest |
| `45410811` | UEFA Champions League 2006–2007 |
| `5451080B` | WWE SmackDown vs. Raw 2008 |

If any of these turn out to actually run fine on D3D12 (just with a visual quirk the community fixed a different way on PC), pull its title ID back out of `kVulkanRequiredTitleIds` — this list errs toward blocking rather than letting someone hit an unexplained issue.

## Under investigation

### Metal Gear Solid V: The Phantom Pain
Reported to produce an error on install/launch regardless of whether Disc 1 is installed first. Not yet reproduced or diagnosed — exact error text needed.

### Unidentified title — cutscene camera/rendering glitch
Reported via a third-party video showing an extreme, rotated close-up during a dialogue cutscene. Game title not yet identified; could be an existing upstream Xenia rendering quirk rather than something specific to this port, since no rendering/shader code has been touched by this fork. Needs a game name and a reproducible case before it can be diagnosed.

## Performance

### Preload Shader Cache (new option, Settings > GPU)
Requested: a way to prevent lag during gameplay. Xenia already persists a per-title shader/pipeline cache to disk (`store_shaders`, on by default) and reloads it on the next launch, compiling it in the background while the game's own loading screens play - `async_shader_compilation` skips a draw rather than stalling if its pipeline isn't ready yet, so this doesn't freeze the game, but it can still cause momentary pop-in/missing draws right as gameplay starts if the game's own loading screen finishes before the background compilation does.

The new "Preload Shader Cache" toggle makes that step blocking instead: the previously-stored cache finishes warming up (using all available cores, same as before) *before* the title starts, eliminating that pop-in on a return playthrough. First-time launches with nothing cached yet aren't affected either way. **Trade-off, and why it's off by default:** title launch runs synchronously on the same thread as the frontend, and that thread has no path to keep rendering an animated frame while blocked - so turning this on trades a longer, unanimated pause between clicking a game and it starting for a smoother playthrough once it does. Verified to build and link correctly; **not yet confirmed on real Xbox hardware** - specifically whether that pause reads as "loading" or as "frozen" needs an on-device check with a title that already has a warm cache (play it once, exit, relaunch it, toggle the setting).

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
