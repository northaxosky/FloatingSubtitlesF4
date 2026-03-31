#pragma once

// Game version configuration
// Build with -DFALLOUT4_OG=ON to target the original game (1.10.163)
// Default (no flag) targets the Next-Gen/AE version

namespace GameVersion
{
#ifdef FALLOUT4_OG
	// ===== OG (1.10.163) Address Library IDs =====
	// Resolved from OG address library (version-1-10-163-0.bin) via name port cross-reference.

	// SubtitleManager::ShowSubtitle (OG RVA: 0x12B2F70)
	inline constexpr std::uint64_t ShowSubtitle = 875508;

	// SubtitleManager::DisplayNextSubtitle (OG RVA: 0x12B3530)
	// OG hooks this function directly via prologue hook (no call-site offset needed)
	inline constexpr std::uint64_t DisplayNextSubtitle = 102120;

	// BSTValueEventSource<HUDSubtitleDisplayEvent>::BroadcastEvent (OG RVA: 0xD4B150)
	inline constexpr std::uint64_t BroadcastEvent = 328561;

	// IL String map global (OG RVA: 0x58D0680, map pointer at 0x58D0678)
	inline constexpr std::uint64_t ILStringMap = 90497;
	inline constexpr std::ptrdiff_t ILStringMapOffset = -0x8;

#else
	// ===== NG/AE Address Library IDs =====

	// SubtitleManager::ShowSubtitle
	inline constexpr std::uint64_t ShowSubtitle = 2249542;

	// SubtitleManager::UpdateSubtitles (contains the DisplayNextSubtitle call)
	// NG hooks a CALL instruction within this function at the offset below
	inline constexpr std::uint64_t DisplayNextSubtitle = 2249545;
	inline constexpr std::ptrdiff_t DisplayNextSubtitleOffset = 0xA3;

	// BSTValueEventSource<HUDSubtitleDisplayEvent>::BroadcastEvent
	inline constexpr std::uint64_t BroadcastEvent = 2229076;

	// IL String map global
	inline constexpr std::uint64_t ILStringMap = 2661471;
	inline constexpr std::ptrdiff_t ILStringMapOffset = -0x8;

#endif
}
