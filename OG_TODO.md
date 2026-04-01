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

### 2. No dual-language subtitles
**Impact:** Only the game's native language subtitles are shown.
**Root cause:** `GetILStringMap()` (OG ID 90497, offset -0x8) crashes — the ID/offset may be wrong or `BSTHashMap` layout differs.
**To fix:** Use x64dbg on OG, break in `StringFileInfo::ResolveID` (OG RVA 0x1B7E70), inspect RCX at +0x59.

### 3. No crosshair mode tracking
**Impact:** Minor — subtitle alpha doesn't change when looking directly at an NPC.
**Root cause:** `PlayerCrosshairModeEvent` uses same `BSTGlobalEvent` pattern that crashes on OG.

### 4. No crosshair ref highlighting
**Impact:** Crosshair-targeted NPC subtitle highlighting doesn't work (`IsCrosshairRef` always returns false).
**Root cause:** `ViewCaster` class doesn't exist in Dear-Modding CommonLibF4.

## CommonLibF4 Patches (in local clone)

These changes were made to `Documents/Projects/commonlibf4` and should be contributed upstream:

### F4SE API.cpp — OG F4SE compatibility
- `GetSaveFolderName()` guarded by F4SE version check (OG F4SE 0.6.x doesn't have it)
- `InitLog()` falls back to "Fallout4" save folder name when empty

### IDs.h — 800+ OG IDs added
Resolved via address library cross-reference, MainList mapping, Ryan-rsm-McKenzie fork harvesting, and byte signature matching.

Key fixes during this port:
- `PlayerCamera::Singleton` — was 0, corrected to 1171980
- `bhkPickData::SetStartEnd` — was 55502 (wrong function!), corrected to 747470
- `bhkPickData::ctor` — was NG-only, added OG ID 526783
- `bhkPickData::GetHitFraction` — was NG-only, added OG ID 476687
