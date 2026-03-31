# OG (1.10.163) Port — Caveats & TODO

Features that are skipped or degraded on OG due to unresolved address IDs or API differences.

## Skipped on OG

### 1. ApplyColorUpdateEvent (subtitle color sync)
**File:** `src/ImGui/FontStyles.cpp` — `FontStyles::Register()`
**Impact:** Subtitle colors won't update when the player changes HUD color in settings. Uses hardcoded default colors instead.
**Root cause:** `ApplyColorUpdateEvent::GetEventSource` (OG ID 860383) resolves but crashes when constructing the event source via `BSTGlobalEvent::GetSingleton()`. The `BSTGlobalEvent` singleton ID (1424022) was added but the event source creation pattern may differ on OG.
**To fix:** Debug at runtime with x64dbg — break at `ApplyColorUpdateEvent::GetEventSource`, step through the `new EventSource_t(...)` call, find why it crashes.

### 2. PlayerCrosshairModeEvent (crosshair mode tracking)
**File:** `src/Manager.cpp` — `Manager::OnDataLoaded()`
**Impact:** Crosshair mode changes aren't tracked. Minor — only affects subtitle alpha when looking directly at an NPC.
**Root cause:** Same `BSTGlobalEvent` event source pattern. OG ID candidates were 1549628 and 1362726 but both crash.
**To fix:** Same approach as #1 — likely the same underlying `BSTGlobalEvent` issue.

### 3. ILStringMap / Localized Subtitles (dual-language)
**File:** `src/Localization.cpp` — `ReadILStringFiles()` via `RE::GetILStringMap()`
**Impact:** Dual-language subtitle display is disabled. Only the game's native language subtitles are shown.
**Root cause:** `GetILStringMap()` uses OG ID 90497 with offset -0x8, resolved by tracing `StringFileInfo::ResolveID` data references. Crashes at runtime — the ID or offset may be wrong, or the `BSTHashMap` layout differs on OG.
**To fix:** Use x64dbg on OG, break in `StringFileInfo::ResolveID` (OG RVA 0x1B7E70), inspect what address is loaded into RCX at +0x59. Compare with what ID 90497 resolves to. The offset -0x8 may need adjustment.

### 4. ViewCaster / IsCrosshairRef
**File:** `src/RE.cpp` — `IsCrosshairRef()`
**Impact:** Always returns false. Crosshair-targeted NPC subtitle highlighting doesn't work.
**Root cause:** `ViewCaster` class doesn't exist in Dear-Modding CommonLibF4. Needs full class RE with `QActivatePickRef()` method.

## Unresolved IDs in CommonLibF4

~65 entries in `commonlibf4/include/RE/IDs.h` remain NG-only (no OG equivalent). These are IDs that:
- Had no MainList (OG→AE address) mapping
- Had no matching byte signature in the decrypted OG binary
- Were not found in the Ryan-rsm-McKenzie dev-1.10.163 branch

Most are for code paths this mod doesn't touch. If one is hit at runtime, the game will show "Failed to find offset for Address Library ID" with the ID number. Resolution approach:
1. Find the ID in `IDs.h`, note the namespace/name
2. Search the OG function name database (`.local/Fallout4_IDA_OG_to_AE1-11-191_NamePort/fallout4_og_funcs.json`) for the function name
3. Look up the OG RVA in the OG address library (`version-1-10-163-0.bin`)
4. Add the OG ID to the entry: `{ og_id, ng_id }`

## CommonLibF4 Patches (in local clone)

These changes were made to `Documents/Projects/commonlibf4` and should be contributed upstream:

### F4SE API.cpp — OG F4SE compatibility
- `GetSaveFolderName()` guarded by F4SE version check (OG F4SE 0.6.x doesn't have it)
- `InitLog()` falls back to "Fallout4" save folder name when empty

### IDs.h — 800+ OG IDs added
Resolved via address library cross-reference, MainList mapping, Ryan-rsm-McKenzie fork harvesting, and byte signature matching.
