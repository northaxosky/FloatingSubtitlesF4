#include "ImGui/FontStyles.h"

#include "SettingLoader.h"
#include "ImGui/Util.h"

namespace ImGui
{
	ImVec4 FontStyles::GetGameplayHUDColor()
	{
		if (!hudColorLoaded) {
			if (REL::Module::IsRuntimeNG()) {
				hudGameplayColor = GetColor(RE::HUDMenuUtils::GetGameplayHUDColor());
			} else {
				// OG: read HUD color from INI directly
				float r = 0.14f, g = 1.0f, b = 0.14f;
				if (auto s = RE::GetINISetting("iHUDColorR:Interface")) r = static_cast<float>(s->GetInt()) / 255.0f;
				if (auto s = RE::GetINISetting("iHUDColorG:Interface")) g = static_cast<float>(s->GetInt()) / 255.0f;
				if (auto s = RE::GetINISetting("iHUDColorB:Interface")) b = static_cast<float>(s->GetInt()) / 255.0f;
				hudGameplayColor = ImVec4(r, g, b, 1.0f);
			}
			hudColorLoaded = true;
		}
		return hudGameplayColor;
	}

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
		// Subtitle INI colors loaded now; HUD color loaded lazily on first render (HUD not ready at init)
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
