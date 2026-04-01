#include "Manager.h"

#include "ImGui/Util.h"
#include "RayCaster.h"
#include "SettingLoader.h"

bool Manager::GlobalSettings::LoadGlobalSettings()
{
	showSpeakerName.load();
	subtitleSize.load();
	showDualSubs.load();
	subtitleHeadOffset.load();
	requireLOS.load();
	subtitleSpacing.load();
	obscuredSubtitleAlpha.load();
	subtitleAlphaPrimary.load();
	subtitleAlphaSecondary.load();

	return showDualSubs.changed() || subtitleSize.changed();  // rebuild subs
}

void Manager::LoadGlobalSettings()
{
	bool rebuildSubs = false;

	rebuildSubs |= settings.LoadGlobalSettings();
	rebuildSubs |= localizedSubs.LoadGlobalSettings();

	if (rebuildSubs) {
		ImGui::GetStyle().FontSizeBase = settings.subtitleSize.get() * ImGui::GetResolutionScale();
		ClearScaleformSubtitle();
		RebuildProcessedSubtitles();
	}

	localizedSubs.PostSettingsLoad();
}

void Manager::OnDataLoaded()
{
	logger::info("  RegisterSink MenuOpenCloseEvent...");
	RE::UI::GetSingleton()->RegisterSink<RE::MenuOpenCloseEvent>(this);
	if (REL::Module::IsRuntimeNG()) {
		RE::PlayerCrosshairModeEvent::GetEventSource()->RegisterSink(this);
	}
	logger::info("  RegisterSink TESLoadGameEvent...");
	RE::TESLoadGameEvent::GetEventSource()->RegisterSink(this);

	if (REL::Module::IsRuntimeNG()) {
		logger::info("  BuildLocalizedSubtitles...");
		localizedSubs.BuildLocalizedSubtitles();
	} else {
		logger::info("  OG: skipping localized subtitles (ILStringMap not verified)");
	}

	logger::info("  LoadGlobalSettings...");
	LoadGlobalSettings();

	float gameMaxDistance = 3000.0f;
	if (auto setting = RE::GetINISetting("fMaxSubtitleDistance:Interface")) {
		gameMaxDistance = setting->GetFloat();
	}
	maxDistanceStartSq = gameMaxDistance * gameMaxDistance;
	maxDistanceEndSq = (gameMaxDistance * 1.05f) * (gameMaxDistance * 1.05f);

	logger::info("  OnDataLoaded complete. Max distance: {:.2f}", gameMaxDistance);
}

bool Manager::SkipRender() const
{
	return !ShowGeneralSubtitles();
}

bool Manager::ShowGeneralSubtitles() const
{
	if (auto setting = RE::INIPrefSettingCollection::GetSingleton()->GetSetting("bGeneralSubtitles:Interface")) {
		return setting->GetBinary();
	}
	return true;
}

bool Manager::ShowDialogueSubtitles() const
{
	if (auto setting = RE::INIPrefSettingCollection::GetSingleton()->GetSetting("bDialogueSubtitles:Interface")) {
		return setting->GetBinary();
	}
	return true;
}

DualSubtitle Manager::CreateDualSubtitles(const char* subtitle) const
{
	auto primarySub = localizedSubs.GetPrimarySubtitle(subtitle);
	if (settings.showDualSubs.get()) {
		auto secondarySub = localizedSubs.GetSecondarySubtitle(subtitle);
		if (!primarySub.empty() && !secondarySub.empty() && primarySub != secondarySub) {
			return DualSubtitle(primarySub, secondarySub);
		}
	}
	return DualSubtitle(primarySub);
}

void Manager::AddProcessedSubtitle(const char* subtitle)
{
	WriteLocker locker(subtitleLock);
	processedSubtitles.try_emplace(subtitle, CreateDualSubtitles(subtitle));
}

const DualSubtitle& Manager::GetProcessedSubtitle(const RE::BSFixedStringCS& a_subtitle)
{
	{
		ReadLocker readLock(subtitleLock);
		if (auto it = processedSubtitles.find(a_subtitle.c_str()); it != processedSubtitles.end()) {
			return it->second;
		}
	}

	{
		WriteLocker writeLock(subtitleLock);
		auto [it, inserted] = processedSubtitles.try_emplace(a_subtitle.c_str(), CreateDualSubtitles(a_subtitle.c_str()));
		return it->second;
	}
}

void Manager::AddSubtitle(RE::SubtitleManager* a_manager, const char* a_subtitle)
{
	if (!string::is_empty(a_subtitle) && !string::is_only_space(a_subtitle)) {
		AddProcessedSubtitle(a_subtitle);

		RE::BSAutoWriteLock gameLocker(a_manager->GetRWLock());
		{
			auto& subtitleArray = reinterpret_cast<RE::BSTArray<RE::SubtitleInfoEx>&>(a_manager->subtitlePriorityArray);
			if (!subtitleArray.empty()) {
				auto& subInfo = subtitleArray.back();
				subInfo.flagsRaw() = 0;  // reset any junk values
				subInfo.setAlphaModifier(0.0f);
				subInfo.setFlag(SubtitleFlag::kInitialized, true);
			}
		}
	}
}

void Manager::RebuildProcessedSubtitles()
{
	WriteLocker locker(subtitleLock);
	for (auto& [text, subs] : processedSubtitles) {
		subs = CreateDualSubtitles(text.c_str());
	}
}

void Manager::CalculateAlphaModifier(RE::SubtitleInfoEx& a_subInfo) const
{
	const auto ref = a_subInfo.speaker.get();
	const auto actor = ref->As<RE::Actor>();

	float alpha = 1.0f;

	if (a_subInfo.isFlagSet(SubtitleFlag::kObscured)) {
		alpha *= settings.obscuredSubtitleAlpha.get();
	}

	if (a_subInfo.distFromPlayer > maxDistanceStartSq) {
		const float t = (a_subInfo.distFromPlayer - maxDistanceStartSq) / (maxDistanceEndSq - maxDistanceStartSq);

		constexpr auto cubicEaseOut = [](float t) -> float {
			return 1.0f - (t * t * t);
		};

		alpha *= 1.0f - cubicEaseOut(t);
	} else if (auto high = actor->currentProcess ? actor->currentProcess->high : nullptr; high && high->fadeAlpha < 1.0f) {
		alpha *= high->fadeAlpha;
	} else if (actor->IsDead(false) && actor->voiceTimer < 1.0f) {
		alpha *= actor->voiceTimer;
	}

	a_subInfo.setAlphaModifier(alpha);
}

RE::BSEventNotifyControl Manager::ProcessEvent(const RE::MenuOpenCloseEvent& a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
{
	if (a_event.menuName == "PauseMenu") {
		if (!a_event.opening) {
			LoadGlobalSettings();
		}
	}

	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESLoadGameEvent& a_event, RE::BSTEventSource<RE::TESLoadGameEvent>*)
{
	LoadGlobalSettings();
	
	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl Manager::ProcessEvent(const RE::PlayerCrosshairModeEvent& a_event, RE::BSTEventSource<RE::PlayerCrosshairModeEvent>*)
{
	crosshairMode = std::to_underlying(a_event.optionalValue.value_or((RE::CrosshairMode)- 1));

	return RE::BSEventNotifyControl::kContinue;
}

void Manager::CalculateVisibility(RE::SubtitleInfoEx& a_subInfo)
{
	const auto ref = a_subInfo.speaker.get();
	const auto actor = ref->As<RE::Actor>();

	a_subInfo.setFlag(SubtitleFlag::kOffscreen, false);
	a_subInfo.setFlag(SubtitleFlag::kObscured, false);

	if (actor->IsPlayerRef()) {
		return;
	}

	switch (RayCaster(actor).GetResult(false)) {
	case RayCaster::Result::kOffscreen:
		a_subInfo.setFlag(SubtitleFlag::kOffscreen, true);
		break;
	case RayCaster::Result::kObscured:
		a_subInfo.setFlag(SubtitleFlag::kObscured, true);
		break;
	case RayCaster::Result::kVisible:
		break;
	default:
		std::unreachable();
	}
}

std::string Manager::GetScaleformSubtitle(const RE::BSFixedStringCS& a_subtitle)
{
	auto subtitle = GetProcessedSubtitle(a_subtitle).GetScaleformCompatibleSubtitle(settings.showDualSubs.get());
	return subtitle.empty() ? a_subtitle.c_str() : subtitle;
}

void Manager::ClearScaleformSubtitle(RE::BSTValueEventSource<RE::HUDSubtitleDisplayEvent>& a_event, std::string_view a_subtitle)
{
	RE::BSAutoLock l(a_event.dataLock);
	if (!a_subtitle.empty() && !(a_event.optionalValue.has_value() && a_event.optionalValue->subtitleText == a_subtitle)) {
		return;
	}
	a_event.optionalValue.reset();
	RE::BroadcastEvent(&a_event);
}

void Manager::ClearScaleformSubtitle()
{
	auto                manager = RE::SubtitleManager::GetSingleton();
	RE::BSAutoWriteLock l(manager->GetRWLock());
	ClearScaleformSubtitle(manager->subtitleDisplayData, ""sv);
}

bool Manager::UpdateSubtitleInfo(RE::SubtitleManager* a_manager)
{
	bool gameSubtitleFound = false;

	static int logCount = 0;
	bool doLog = logCount < 3;
	auto L = [&](const char* msg) { if (doLog) { logger::info("  USI: {}", msg); spdlog::default_logger()->flush(); } };

	L("1-start");
	L("2-array-size");

	{
		auto& subtitleArray = reinterpret_cast<RE::BSTArray<RE::SubtitleInfoEx>&>(a_manager->subtitlePriorityArray);
		L("3-cast-ok");

		for (auto& subInfo : subtitleArray) {
			L("4-loop-entry");
			L("5-speaker-get");
			if (const auto& ref = subInfo.speaker.get()) {
				L("6-ref-ok");

				if (!subInfo.isFlagSet(SubtitleFlag::kInitialized)) {
					subInfo.flagsRaw() = 0;
					subInfo.setAlphaModifier(1.0f);
					subInfo.setFlag(SubtitleFlag::kInitialized, true);
				}
				L("7-init-done");

				subInfo.setFlag(SubtitleFlag::kSkip, false);

				if ((subInfo.priority != RE::SUBTITLE_PRIORITY::kForce && subInfo.distFromPlayer > maxDistanceEndSq)) {
					subInfo.setFlag(SubtitleFlag::kSkip, true);
					continue;
				}
				L("8-dist-check-done");

				auto pcCamera = RE::PlayerCamera::GetSingleton();
				L("9-camera-ok");
				bool isActor = ref->IsActor();
				bool isPlayer = ref->IsPlayerRef();

				bool skipSubtitle = false;
				if (!isActor) {
					skipSubtitle = true;
				} else if (pcCamera) {
					bool fp = pcCamera->QCameraEquals(RE::CameraState::kFirstPerson);
					bool dlg = pcCamera->QCameraEquals(RE::CameraState::kDialogue);
					skipSubtitle = (isPlayer && fp) || dlg;
				}
				L("9e-skip-eval");

				if (skipSubtitle) {
					subInfo.setFlag(SubtitleFlag::kSkip, true);
				} else {
					L("10-calc-vis");
					CalculateVisibility(subInfo);
				}
				L("11-vis-done");

				if (subInfo.isFlagSet(SubtitleFlag::kSkip) || subInfo.isFlagSet(SubtitleFlag::kOffscreen)) {
					L("12-skip/offscreen");
					if (!gameSubtitleFound) {
						bool shouldDisplay = false;

						L("13-menu-topic");
						if (RE::MenuTopicManager::GetSingleton()->menuOpen) {
							shouldDisplay = ShowDialogueSubtitles();
						} else {
							shouldDisplay = ShowGeneralSubtitles();
						}
						L("14-display-check");

						if (shouldDisplay) {
							L("15-get-speaker-name");
							RE::HUDSubtitleDisplayData data(RE::GetSpeakerName(subInfo), GetScaleformSubtitle(subInfo.subtitleText));
							L("16-data-created");
							RE::BSAutoLock<RE::BSSpinLock> l(a_manager->subtitleDisplayData.dataLock);
							L("17-locked");
							{
								if (!a_manager->subtitleDisplayData.optionalValue.has_value() || *a_manager->subtitleDisplayData.optionalValue != data) {
									a_manager->subtitleDisplayData.optionalValue.emplace(data);
									L("18-broadcast");
									RE::BroadcastEvent(&a_manager->subtitleDisplayData);
								}
							}
							L("19-broadcast-done");
						}

						a_manager->currentSpeaker = subInfo.speaker;
						gameSubtitleFound = true;
					}
				} else {
					L("20-alpha");
					CalculateAlphaModifier(subInfo);
					L("21-clear-scaleform");
					ClearScaleformSubtitle(a_manager->subtitleDisplayData, GetScaleformSubtitle(subInfo.subtitleText));
				}
				L("22-entry-done");
			}
		}
		L("23-loop-done");
		logCount++;
	}

	return gameSubtitleFound;
}

RE::NiPoint3 Manager::GetSubtitleAnchorPosImpl(const RE::TESObjectREFRPtr& a_ref, float a_height)
{
	RE::NiPoint3 pos = a_ref->GetPosition();
	if (const auto headNode = RE::GetHeadNode(a_ref)) {
		pos = headNode->world.translate;
	} else {
		pos.z += a_height;
	}
	return pos;
}

RE::NiPoint3 Manager::CalculateSubtitleAnchorPos(const RE::SubtitleInfoEx& a_subInfo) const
{
	const auto ref = a_subInfo.speaker.get();
	const auto height = ref->GetActorHeightOrRefBound();

	auto pos = GetSubtitleAnchorPosImpl(ref, height);
	auto offset = settings.subtitleHeadOffset.get();

	pos.z += offset * (height / 128.0f);

	return pos;
}

void Manager::Draw()
{
	const auto         subtitleManager = RE::SubtitleManager::GetSingleton();
	RE::BSAutoReadLock locker(subtitleManager->GetRWLock());
	{
		auto& subtitleArray = reinterpret_cast<RE::BSTArray<RE::SubtitleInfoEx>&>(subtitleManager->subtitlePriorityArray);
		if (subtitleArray.empty()) {
			return;
		}

		DualSubtitle::ScreenParams params;
		params.spacing = settings.subtitleSpacing.get();

		for (auto& subInfo : subtitleArray | std::views::reverse) {  // reverse order so closer subtitles get rendered on top
			if (const auto& ref = subInfo.speaker.get()) {
				auto actor = ref->As<RE::Actor>();
				if (!actor) {
					continue;
				}
				
				if (subInfo.isFlagSet(SubtitleFlag::kSkip) || subInfo.isFlagSet(SubtitleFlag::kOffscreen) || subInfo.isFlagSet(SubtitleFlag::kObscured) && settings.obscuredSubtitleAlpha.get() == 0.0f) {
					continue;
				}

				auto anchorPos = CalculateSubtitleAnchorPos(subInfo);
				auto zDepth = ImGui::WorldToScreenLoc(anchorPos, params.pos);
				if (zDepth < 0.0f) {
					continue;
				}

				auto alphaMult = subInfo.getAlphaModifier();
				params.alphaPrimary = settings.subtitleAlphaPrimary.get() * alphaMult;
				params.alphaSecondary = settings.subtitleAlphaSecondary.get() * alphaMult;
				if (settings.showSpeakerName.get() && (!RE::IsCrosshairRef(ref) || crosshairMode != 8)) {
					params.speakerName = RE::GetSpeakerName(subInfo);
				}

				auto& processedSub = GetProcessedSubtitle(subInfo.subtitleText);
				processedSub.DrawDualSubtitle(params);
			}
		}
	}
}
