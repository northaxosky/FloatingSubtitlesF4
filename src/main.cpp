#include "Hooks.h"
#include "ImGui/FontStyles.h"
#include "ImGui/Renderer.h"
#include "Manager.h"

void OnInit(F4SE::MessagingInterface::Message* a_msg)
{
	switch (a_msg->type) {
	case F4SE::MessagingInterface::kPostLoad:
		{
			logger::info("{:*^30}", "POST LOAD");
			ImGui::Renderer::Install();
			Hooks::Install();
		}
		break;
	case F4SE::MessagingInterface::kPostPostLoad:
		{
			logger::info("{:*^30}", "POST POST LOAD");
		}
		break;
	case F4SE::MessagingInterface::kGameDataReady:
		{
			logger::info("{:*^30}", "GAME DATA READY");
			ImGui::Renderer::Init();
			logger::info("FontStyles::Register...");
			ImGui::FontStyles::GetSingleton()->Register();
			logger::info("Manager::OnDataLoaded...");
			Manager::GetSingleton()->OnDataLoaded();
			logger::info("GAME DATA READY complete");
		}
		break;
	default:
		break;
	}
}

// OG F4SE uses F4SEPlugin_Query, NG uses F4SEPlugin_Version. Export both.
extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;
	return true;
}

extern "C" DLLEXPORT constinit auto F4SEPlugin_Version = []() noexcept {
	F4SE::PluginVersionData data{};

	data.PluginVersion({ Version::MAJOR, Version::MINOR, Version::PATCH });
	data.PluginName(Version::PROJECT.data());
	data.AuthorName("powerofthree");
	data.UsesAddressLibrary(true);
	data.UsesSigScanning(false);
	data.IsLayoutDependent(true);
	data.HasNoStructUse(false);
	data.CompatibleVersions({ F4SE::RUNTIME_1_10_163, F4SE::RUNTIME_LATEST });

	return data;
}();

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se, { .trampoline = true, .trampolineSize = 1 << 7 });

	logger::info("{} v{} loaded", Version::PROJECT, Version::NAME);
	logger::info("Game version : {}", a_f4se->RuntimeVersion().string());

	const auto messaging = F4SE::GetMessagingInterface();
	messaging->RegisterListener(OnInit);

	return true;
}
