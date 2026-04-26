#include "Hooks.h"

#include "Manager.h"

namespace Hooks
{
	struct ShowSubtitle
	{
		static void thunk(RE::SubtitleManager* a_manager, RE::TESObjectREFR* speaker, const RE::BSFixedStringCS& subtitleText, RE::TESTopicInfo* topicInfo, bool spokenToPlayer)
		{
			static bool logged = false;
			if (!logged) {
				logger::info("ShowSubtitle hook fired: manager={}, speaker={}, text={}",
					(void*)a_manager, (void*)speaker, subtitleText.c_str() ? subtitleText.c_str() : "null");
				logged = true;
			}

			// Capture array size BEFORE vanilla. When subtitles are disabled in
			// the INI (bGeneralSubtitles=0 / bDialogueSubtitles=0), vanilla
			// returns early without pushing — so back() would refer to a stale,
			// unrelated entry. AddSubtitle uses preSize to detect that.
			auto& subtitleArray = reinterpret_cast<RE::BSTArray<RE::SubtitleInfoEx>&>(a_manager->subtitlePriorityArray);
			const auto preSize = subtitleArray.size();

			func(a_manager, speaker, subtitleText, topicInfo, spokenToPlayer);
			Manager::GetSingleton()->AddSubtitle(a_manager, subtitleText.c_str(), preSize);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct DisplayNextSubtitle
	{
		static bool thunk(RE::SubtitleManager* a_manager)
		{
			static bool logged = false;
			if (!logged) {
				logger::info("DisplayNextSubtitle hook fired: manager={}", (void*)a_manager);
				logged = true;
			}

			return Manager::GetSingleton()->UpdateSubtitleInfo(a_manager);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	void Install()
	{
		REL::Relocation<std::uintptr_t> showSubtitle(REL::ID{ 875508, 2249542 });
		stl::hook_function_prologue<ShowSubtitle, 5>(showSubtitle.address());

		REL::Relocation<std::uintptr_t> subtitleUpdate(REL::ID{ 381778, 2249545 });
		stl::write_thunk_call<DisplayNextSubtitle>(subtitleUpdate.address() + (REL::Module::IsRuntimeNG() ? 0xA3 : 0x3A));

		logger::info("Installed subtitle hooks");
	}
}
