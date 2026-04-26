# OG (1.10.163) Port — Status & TODO

## Working on OG

### Floating subtitles
Core subtitle rendering works. NPCs display floating text above their heads with proper word-wrapping and font styling.

### Line-of-sight / Raycasting
Subtitles dim when NPC is behind a wall. Uses `bhkPickData` + custom `RayCollector` through `cell->Pick()`.

**Fixes applied:**
- `bhkPickData::SetStartEnd` OG ID was wrong (55502 → 747470)
- `bhkPickData::ctor` and `GetHitFraction` were missing OG IDs (added 526783, 476687)
- Removed `__first_virtual_table_function__` override from `RayCollector` to fix vtable alignment
- Camera start point uses `PlayerCamera` fallback (`Main::WorldRootNode` OG ID points to wrong data)

### Camera state checks
Subtitles properly hide during dialogue and first-person camera via `QCameraEquals`.

**Fix applied:** `PlayerCamera::Singleton` OG ID was 0 (placeholder) — resolved to 1171980.

### HUD / subtitle colors
Colors load from INI at startup (`iHUDColorR/G/B:Interface` for HUD, `uSubtitleR/G/B:Interface` for subtitles). Match game settings on launch.

## Limitations on OG

### 1. No dynamic color updates
**Impact:** HUD/subtitle colors won't update mid-session if changed in settings. Requires game restart.
**Root cause:** `ApplyColorUpdateEvent::GetEventSource` crashes on OG via `BSTGlobalEvent`. `HUDMenuUtils::GetGameplayHUDColor()` also crashes — colors read from INI instead.
**To fix:** Debug `BSTGlobalEvent` event source creation on OG, or find an alternative hook point.

### 2. No crosshair mode tracking
**Impact:** Minor — subtitle alpha doesn't change when looking directly at an NPC.
**Root cause:** `PlayerCrosshairModeEvent` uses same `BSTGlobalEvent` pattern that crashes on OG.

## CommonLibF4 Patches (in local clone)

Will be contributing upstream
