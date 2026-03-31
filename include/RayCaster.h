#pragma once

class RayCollector : public RE::hknpCollisionQueryCollector
{
public:
	RayCollector() { Reset(); }
	RayCollector(RE::Actor* a_actor, RE::hknpBSWorld* a_physicsWorld);
	~RayCollector() override = default;
	void __first_virtual_table_function__() override {}

	// override (hknpCollisionQueryCollector)
	void                       Reset() override;
	void                       AddHit(const RE::hknpCollisionResult& a_result) override;
	bool                       HasHit() const override;
	std::int32_t               GetNumHits() const override;
	const RE::hknpCollisionResult* GetHits() const override;

private:
	// members
	RE::Actor*              actor{ nullptr };
	RE::hknpBSWorld*        physicsWorld{ nullptr };
	RE::hknpCollisionResult closestHit{};
	bool                    hasValidHit{ false };
};

class RayCaster
{
public:
	struct StartPoint
	{
		void Init();

		RE::NiPoint3 camera;
		RE::NiPoint3 debug;
	};

	enum class Result
	{
		kOffscreen = 0,
		kObscured,
		kVisible
	};

	RayCaster() = default;
	RayCaster(RE::Actor* a_target);

	Result GetResult(bool a_debugRay);

private:
	void DebugRay(const RE::bhkPickData& a_pickData, RE::NiAVObject* a_obj, RE::TESObjectREFR* a_owner, const RE::NiPoint3& a_targetPos, ImU32 color) const;

	// members
	StartPoint                  startPoint;
	std::array<RE::NiPoint3, 4> targetPoints;
	std::array<ImU32, 4>        debugColors{ 0xFF2626FF, 0xFF26FFD3, 0xFF7CFF26, 0xFFFF7C2 };
	RE::Actor*                  actor;
};
