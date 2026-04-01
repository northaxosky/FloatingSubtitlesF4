#include "RayCaster.h"

#include "ImGui/Util.h"

// Stub definitions for hkBaseObject virtuals (declared in game headers, defined in game binary).
// Safe because RayCollector is stack-allocated and overrides both.
RE::hkBaseObject::~hkBaseObject() = default;
void RE::hkBaseObject::__first_virtual_table_function__() {}

RayCollector::RayCollector(RE::Actor* a_actor, RE::hknpBSWorld* a_physicsWorld) :
	hknpCollisionQueryCollector(),
	actor(a_actor),
	physicsWorld(a_physicsWorld)
{
	Reset();
}

void RayCollector::Reset()
{
	hasValidHit = false;
	earlyOutThreshold.real = _mm_set1_ps(1.0f);
}

bool RayCollector::HasHit() const
{
	return hasValidHit;
}

std::int32_t RayCollector::GetNumHits() const
{
	return hasValidHit ? 1 : 0;
}

const RE::hknpCollisionResult* RayCollector::GetHits() const
{
	return hasValidHit ? &closestHit : nullptr;
}

void RayCollector::AddHit(const RE::hknpCollisionResult& a_result)
{
	static int hitLog = 0;
	if (hitLog < 5) {
		auto layer = a_result.hitBodyInfo.shapeCollisionFilterInfo.storage.GetCollisionLayer();
		spdlog::info("    AddHit called! layer={}", static_cast<int>(layer));
		spdlog::default_logger()->flush();
		hitLog++;
	}
	auto colLayer = a_result.hitBodyInfo.shapeCollisionFilterInfo.storage.GetCollisionLayer();

	switch (colLayer) {
	case RE::COL_LAYER::kStatic:
	case RE::COL_LAYER::kTerrain:
	case RE::COL_LAYER::kGround:
		closestHit = a_result;
		hasValidHit = true;
		earlyOutThreshold.real = _mm_set1_ps(a_result.fraction.storage);
		break;
	case RE::COL_LAYER::kBiped:
	case RE::COL_LAYER::kBipedNoCC:
	case RE::COL_LAYER::kDeadBip:
	case RE::COL_LAYER::kCharController:
		{
			RE::TESObjectREFR* owner = nullptr;
			if (physicsWorld) {
				auto bodyId = a_result.hitBodyInfo.bodyId;
				if (auto body = RE::bhkNPCollisionObject::Getbhk(reinterpret_cast<RE::bhkWorld*>(physicsWorld), bodyId)) {
					owner = RE::TESObjectREFR::FindReferenceFor3D(body->sceneObject);
				}
			}

			if (owner == actor) {
				closestHit = a_result;
				hasValidHit = true;
				earlyOutThreshold.real = _mm_set1_ps(a_result.fraction.storage);
			}
		}
		break;
	default:
		break;
	}
}

void RayCaster::StartPoint::Init()
{
	static int spLog = 0;
	bool doLog = spLog < 3;
	auto L = [&](const char* msg) { if (doLog) { spdlog::info("    SP: {}", msg); spdlog::default_logger()->flush(); } };

	L("1-player");
	auto player = RE::PlayerCharacter::GetSingleton();
	if (!player) {
		if (doLog) spLog++;
		return;
	}

	L("2-pos");
	debug = player->GetPosition();
	L("3-eye");
	debug.z += player->GetCurrentEyeLevel();

	if (auto pcCamera = RE::PlayerCamera::GetSingleton(); pcCamera && pcCamera->cameraRoot) {
		L("6b-pccamera");
		camera = pcCamera->cameraRoot->world.translate;
	} else {
		L("6c-fallback");
		camera = debug;
	}
	L("7-done");
	if (doLog) spLog++;
}

RayCaster::RayCaster(RE::Actor* a_target) :
	actor(a_target)
{
	static int initLog = 0;
	if (initLog < 3) {
		spdlog::info("  RC-init: actor={}", (void*)a_target);
		spdlog::default_logger()->flush();
	}
	startPoint.Init();
	if (initLog < 3) {
		spdlog::info("  RC-init: done");
		spdlog::default_logger()->flush();
		initLog++;
	}
}

RayCaster::Result RayCaster::GetResult(bool a_debugRay)
{
	static int rcLog = 0;
	bool doLog = rcLog < 3;
	auto L = [&](const char* msg) { if (doLog) { spdlog::info("  RC: {}", msg); spdlog::default_logger()->flush(); } };

	L("1-frustum");
	if (auto root = actor->Get3D()) {
		if (!RE::Main::WorldRootCamera()->PointInFrustum(root->worldBound.center, root->worldBound.fRadius)) {
			if (doLog) rcLog++;
			return Result::kOffscreen;
		}
	}

	L("2-cell");
	auto cell = actor->GetParentCell();
	if (!cell || cell->cellState != RE::TESObjectCELL::CELL_STATE::kAttached || !cell->loadedData) {
		if (doLog) rcLog++;
		return Result::kOffscreen;
	}

	L("3-bhkworld");
	auto bhkWorld = cell->GetbhkWorld();
	if (!bhkWorld || !bhkWorld->worldNP.ptr) {
		if (doLog) rcLog++;
		return Result::kOffscreen;
	}

	L("4-los-locations");
	targetPoints[0] = actor->CalculateLOSLocation(RE::ACTOR_LOS_LOCATION::kEye);
	targetPoints[1] = actor->CalculateLOSLocation(RE::ACTOR_LOS_LOCATION::kHead);
	targetPoints[2] = actor->CalculateLOSLocation(RE::ACTOR_LOS_LOCATION::kTorse);
	targetPoints[3] = actor->CalculateLOSLocation(RE::ACTOR_LOS_LOCATION::kFeet);

	L("5-pickdata-ctor");
	RE::bhkPickData pickData{};

	L("6-collector");
	RayCollector collector(actor, bhkWorld->worldNP.ptr);

	if (doLog) {
		auto* raw = reinterpret_cast<const std::uint8_t*>(&pickData);
		spdlog::info("    pickData pre-set: +C0={:X} +C8={:X} +D0={:X} +D8={:X}",
			*reinterpret_cast<const std::uint64_t*>(raw + 0xC0),
			*reinterpret_cast<const std::uint64_t*>(raw + 0xC8),
			*reinterpret_cast<const std::uint64_t*>(raw + 0xD0),
			*reinterpret_cast<const std::uint64_t*>(raw + 0xD8));
		spdlog::default_logger()->flush();
	}

	pickData.collector = &collector;
	pickData.collectorType = static_cast<RE::bhkPickData::COLLECTOR_TYPE>(1);
	RE::CFilter losFilter{};
	losFilter.SetCollisionLayer(RE::COL_LAYER::kLOS);
	pickData.castQuery.filterData.collisionFilterInfo.storage = losFilter.filter;

	L("7-loop-start");
	bool result = false;

	for (std::uint32_t i = 0; i < targetPoints.size(); ++i) {
		if (result) {
			break;
		}

		pickData.SetStartEnd(startPoint.camera, targetPoints[i]);

		if (doLog) {
			spdlog::info("    ray {}: start=({:.0f},{:.0f},{:.0f}) end=({:.0f},{:.0f},{:.0f})", i,
				startPoint.camera.x, startPoint.camera.y, startPoint.camera.z,
				targetPoints[i].x, targetPoints[i].y, targetPoints[i].z);
			spdlog::info("    pickData: collector={}, collectorType={}, filter=0x{:X}",
				(void*)pickData.collector, (int)pickData.collectorType,
				pickData.castQuery.filterData.collisionFilterInfo.storage);
			spdlog::default_logger()->flush();
		}
		L("8-pick");
		auto object = cell->Pick(pickData);
		if (doLog) { spdlog::info("    pick result: obj={}, hasHit={}, collector.hasHit={}", (void*)object, pickData.HasHit(), collector.HasHit()); spdlog::default_logger()->flush(); }
		L("9-find-ref");
		auto owner = object ? RE::TESObjectREFR::FindReferenceFor3D(object) : nullptr;
		if (owner == actor) {
			result = true;
		}
		if (a_debugRay) {
			DebugRay(pickData, object, owner, targetPoints[i], debugColors[i]);
		}
	}
	L("10-done");
	if (doLog) rcLog++;

	return result ? Result::kVisible : Result::kObscured;
}

void RayCaster::DebugRay(const RE::bhkPickData& a_pickData, RE::NiAVObject* a_obj, [[maybe_unused]] RE::TESObjectREFR* a_owner, const RE::NiPoint3& a_targetPos, ImU32 color) const
{
	const auto hitFrac = a_pickData.GetHitFraction();
	const auto hitPos = (a_targetPos - startPoint.debug) * hitFrac + startPoint.debug;

	ImGui::DrawLine(startPoint.debug, hitPos, a_pickData.HasHit() ? color : IM_COL32_BLACK);

	if (a_obj) {
		auto layer = a_pickData.result.hitBodyInfo.shapeCollisionFilterInfo.storage.GetCollisionLayer();
		auto text = std::format("[{}]", static_cast<std::uint32_t>(a_pickData.HasHit() ? layer : RE::COL_LAYER::kUnidentified));
		ImGui::DrawTextAtPoint(hitPos, text.c_str(), color);
	}
}
