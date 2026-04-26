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

### 2. ~~No dual-language subtitles~~ — FIXED in 1.1.6
**Resolved:** OG `ILStringMap` now uses Address Library ID `1497866` (RVA `0x036D9598`) without the `-0x8` indirection used on AE/NG (where the symbol points 8 bytes after a runtime-populated handle slot). RE'd via `StringFileInfo::ResolveID` (RVA `0x001B7E70`) callers of `StringFileInfo::ctor`. See `include/RE.h::GetILStringMap`.

### 3. No crosshair mode tracking
**Impact:** Minor — subtitle alpha doesn't change when looking directly at an NPC.
**Root cause:** `PlayerCrosshairModeEvent` uses same `BSTGlobalEvent` pattern that crashes on OG.

### 4. ~~No crosshair ref highlighting~~ — FIXED in 1.1.6
**Resolved:** `IsCrosshairRef` now resolves the `ViewCaster` singleton + `ViewCasterBase::QActivatePickRef` directly via per-runtime RVAs (no dependency on a `ViewCaster` class declaration). Singleton-pointer RVAs: OG `0x058E2B30`, NG `0x02E77AB8`, AE `0x030EEEF8`. QActivatePickRef RVAs: OG `0x009DDDF0`, NG `0x00940810`, AE `0x00993F60`. See `src/RE.cpp::IsCrosshairRef`.

## CommonLibF4 Patches (in local clone)

Will be contributing upstream
