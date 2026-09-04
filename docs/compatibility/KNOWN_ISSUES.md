# Known Issues (real-device testing)

Findings confirmed through actual testing on Xbox Series X in Developer Mode, as opposed to the bulk-imported data in `xenia-compatibility-ledger.xlsx`. Updated as issues are investigated.

## Blocked — no fix possible on Xbox

### WWE SmackDown vs. Raw 2011 (SVR11)
Hangs the D3D12 backend during gameplay. Root-caused to ~148,000 single-vertex memexport draws issued in a tight loop, overwhelming D3D12's per-draw submission overhead within Windows' TDR window. Draw-batching was ruled out as a fix — most of these draws don't read from a walkable vertex buffer at all (memexport-only), so there's no safe way to merge them.

The only known working fix is switching to the Vulkan graphics backend, which handles this draw pattern without issue on PC. **Vulkan does not exist on Xbox at the driver level at all** (confirmed by actually linking it into the Xbox build and testing — it's not a missing flag, there's no Vulkan driver on Xbox hardware for any app, ever). This title cannot currently be fixed on this port without a much deeper D3D12 backend rewrite.

## Under investigation

### Metal Gear Solid V: The Phantom Pain
Reported to produce an error on install/launch regardless of whether Disc 1 is installed first. Not yet reproduced or diagnosed — exact error text needed.

### Unidentified title — cutscene camera/rendering glitch
Reported via a third-party video showing an extreme, rotated close-up during a dialogue cutscene. Game title not yet identified; could be an existing upstream Xenia rendering quirk rather than something specific to this port, since no rendering/shader code has been touched by this fork. Needs a game name and a reproducible case before it can be diagnosed.

## App-level issues (not game-specific)

### Settings/main-menu blade crash
Multiple users have reported the app crashing when navigating to the Settings blade via LB/RB, sometimes immediately on frontend startup with no deliberate interaction. Two hypotheses have been tested and ruled out (a missing-cvar guard, and dialog registration order) — neither fixed it. Diagnostic logging has been added (as of the version noted in `CHANGELOG.md`) to pinpoint the exact crash location on the next occurrence, since further blind fixes aren't productive without that data.
