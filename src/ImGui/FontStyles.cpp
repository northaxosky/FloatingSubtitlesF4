#include "ImGui/FontStyles.h"

#include "SettingLoader.h"
#include "ImGui/Util.h"

namespace ImGui
{
	Font::FontParams Font::LoadFontSettings(const CSimpleIniA& a_ini, const char* a_section)
	{
		FontParams params;
		
		params.name = a_ini.GetValue(a_section, "sFont", "");
		params.spacing = static_cast<float>(a_ini.GetDoubleValue(a_section, "fSpacing", 1.0));

		return params;
	}

	void Font::LoadFont(ImFontConfig& config, const Font::FontParams& a_params)
	{
		if (a_params.name.empty() || font) {
			return;
		}

		auto name = R"(Data\Interface\ImGuiFonts\)" + a_params.name;

		std::error_code ec;
		if (!std::filesystem::exists(name, ec)) {
			return;
		}

		const auto& io = ImGui::GetIO();
		config.GlyphExtraAdvanceX = a_params.spacing;
		font = io.Fonts->AddFontFromFileTTF(name.c_str(), 0.0f, &config);

		logger::info("Loaded font {}", a_params.name);
	}

	void FontStyles::LoadFonts(const Font::FontParams& a_primaryFont, const Font::FontParams& a_secondaryFont)
	{
		ImFontConfig config;
		primaryFont.LoadFont(config, a_primaryFont);
		config.MergeMode = true;
		secondaryFont.LoadFont(config, a_secondaryFont);
	}

	RE::BSEventNotifyControl FontStyles::ProcessEvent(const RE::ApplyColorUpdateEvent&, RE::BSTEventSource<RE::ApplyColorUpdateEvent>*)
	{
		hudGameplayColor = GetColor(RE::HUDMenuUtils::GetGameplayHUDColor());

		if (auto setting = RE::GetINISetting("uSubtitleR:Interface")) {
			subtitleColor.x = static_cast<float>(setting->GetUInt()) / 255.0f;
		}
		if (auto setting = RE::GetINISetting("uSubtitleG:Interface")) {
			subtitleColor.y = static_cast<float>(setting->GetUInt()) / 255.0f;
		}
		if (auto setting = RE::GetINISetting("uSubtitleB:Interface")) {
			subtitleColor.z = static_cast<float>(setting->GetUInt()) / 255.0f;
		}
		subtitleColor.w = 1.0f;

		return RE::BSEventNotifyControl::kContinue;
	}

	void FontStyles::Register()
	{
		// Load colors once at init (works on both OG and NG)
		hudGameplayColor = GetColor(RE::HUDMenuUtils::GetGameplayHUDColor());

		if (auto setting = RE::GetINISetting("uSubtitleR:Interface")) {
			subtitleColor.x = static_cast<float>(setting->GetUInt()) / 255.0f;
		}
		if (auto setting = RE::GetINISetting("uSubtitleG:Interface")) {
			subtitleColor.y = static_cast<float>(setting->GetUInt()) / 255.0f;
		}
		if (auto setting = RE::GetINISetting("uSubtitleB:Interface")) {
			subtitleColor.z = static_cast<float>(setting->GetUInt()) / 255.0f;
		}
		subtitleColor.w = 1.0f;

		// NG: also register for dynamic updates when player changes HUD color
		if (REL::Module::IsRuntimeNG()) {
			RE::ApplyColorUpdateEvent::GetEventSource()->RegisterSink(this);
		}
	}

	void FontStyles::LoadFontStyles()
	{
		ImGuiStyle& style = ImGui::GetStyle();
		style.Colors[ImGuiCol_TextShadowDisabled] = ImVec4(0.0, 0.0, 0.0, 0.5);
		style.TextShadowOffset = ImVec2(1.5f, 1.5f);
		style.ScaleAllSizes(ImGui::GetResolutionScale());

		Font::FontParams primaryFontParams;
		Font::FontParams secondaryFontParams;

		// load fonts
		SettingLoader::GetSingleton()->Load(FileType::kFonts, [&](auto& ini) {
			primaryFontParams = primaryFont.LoadFontSettings(ini, "PrimaryFont");
			secondaryFontParams = secondaryFont.LoadFontSettings(ini, "SecondaryFont");
		});

		LoadFonts(primaryFontParams, secondaryFontParams);
	}
}
