#include "Hooks.h"

#include "Manager.h"

namespace Hooks
{
	struct ShowSubtitle
	{
		static void thunk(RE::SubtitleManager* a_manager, RE::TESObjectREFR* speaker, const RE::BSFixedStringCS& subtitleText, RE::TESTopicInfo* topicInfo, bool spokenToPlayer)
		{
			func(a_manager, speaker, subtitleText, topicInfo, spokenToPlayer);

			Manager::GetSingleton()->AddSubtitle(a_manager, subtitleText.c_str());
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct DisplayNextSubtitle
	{
		static bool thunk(RE::SubtitleManager* a_manager)
		{
			return Manager::GetSingleton()->UpdateSubtitleInfo(a_manager);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	void Install()
	{
		REL::Relocation<std::uintptr_t> showSubtitle(REL::ID(GameVersion::ShowSubtitle));
		stl::hook_function_prologue<ShowSubtitle, 5>(showSubtitle.address());

#ifdef FALLOUT4_OG
		// OG: hook DisplayNextSubtitle directly via prologue hook
		REL::Relocation<std::uintptr_t> displayNext(REL::ID(GameVersion::DisplayNextSubtitle));
		stl::hook_function_prologue<DisplayNextSubtitle, 5>(displayNext.address());
#else
		// NG: hook the CALL to DisplayNextSubtitle within UpdateSubtitles
		REL::Relocation<std::uintptr_t> subtitleUpdate(REL::ID(GameVersion::DisplayNextSubtitle));
		stl::write_thunk_call<DisplayNextSubtitle>(subtitleUpdate.address() + GameVersion::DisplayNextSubtitleOffset);
#endif

		logger::info("Installed subtitle hooks");
	}
}
