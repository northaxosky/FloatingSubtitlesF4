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
	auto player = RE::PlayerCharacter::GetSingleton();
	if (!player) {
		return;
	}

	debug = player->GetPosition();
	debug.z += player->GetCurrentEyeLevel();

	if (auto pcCamera = RE::PlayerCamera::GetSingleton(); pcCamera && pcCamera->cameraRoot) {
		camera = pcCamera->cameraRoot->world.translate;
	} else {
		camera = debug;
	}
}

RayCaster::RayCaster(RE::Actor* a_target) :
	actor(a_target)
{
	startPoint.Init();
}

RayCaster::Result RayCaster::GetResult(bool a_debugRay)
{
	if (auto root = actor->Get3D()) {
		if (!RE::Main::WorldRootCamera()->PointInFrustum(root->worldBound.center, root->worldBound.fRadius)) {
			return Result::kOffscreen;
		}
	}

	auto cell = actor->GetParentCell();
	if (!cell || cell->cellState != RE::TESObjectCELL::CELL_STATE::kAttached || !cell->loadedData) {
		return Result::kOffscreen;
	}

	auto bhkWorld = cell->GetbhkWorld();
	if (!bhkWorld || !bhkWorld->worldNP.ptr) {
		return Result::kOffscreen;
	}

	targetPoints[0] = actor->CalculateLOSLocation(RE::ACTOR_LOS_LOCATION::kEye);
	targetPoints[1] = actor->CalculateLOSLocation(RE::ACTOR_LOS_LOCATION::kHead);
	targetPoints[2] = actor->CalculateLOSLocation(RE::ACTOR_LOS_LOCATION::kTorse);
	targetPoints[3] = actor->CalculateLOSLocation(RE::ACTOR_LOS_LOCATION::kFeet);

	RE::bhkPickData pickData{};

	RayCollector collector(actor, bhkWorld->worldNP.ptr);
	pickData.collector = &collector;
	pickData.collectorType = static_cast<RE::bhkPickData::COLLECTOR_TYPE>(1);
	RE::CFilter losFilter{};
	losFilter.SetCollisionLayer(RE::COL_LAYER::kLOS);
	pickData.castQuery.filterData.collisionFilterInfo.storage = losFilter.filter;

	bool result = false;

	for (std::uint32_t i = 0; i < targetPoints.size(); ++i) {
		if (result) {
			break;
		}

		pickData.SetStartEnd(startPoint.camera, targetPoints[i]);

		auto object = cell->Pick(pickData);
		auto owner = object ? RE::TESObjectREFR::FindReferenceFor3D(object) : nullptr;
		if (owner == actor) {
			result = true;
		}
		if (a_debugRay) {
			DebugRay(pickData, object, owner, targetPoints[i], debugColors[i]);
		}
	}

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
