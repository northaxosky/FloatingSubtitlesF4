#include "RE.h"

namespace RE
{
	ILStringTable::ILStringTable(const std::vector<std::byte>& a_buffer)
	{
		std::uint32_t bufferPosition = 0;

		read_uint32(entryCount, a_buffer, bufferPosition);
		read_uint32(dataSize, a_buffer, bufferPosition);

		directory.reserve(entryCount);
		for (uint32_t i = 0; i < entryCount; ++i) {
			DirectoryEntry entry;
			read_uint32(entry.stringID, a_buffer, bufferPosition);
			read_uint32(entry.offset, a_buffer, bufferPosition);
			directory.emplace_back(entry);
		}

		rawData.assign(a_buffer.data() + bufferPosition, a_buffer.data() + bufferPosition + dataSize);
	}

	std::string ILStringTable::GetStringAtOffset(std::uint32_t offset) const
	{
		std::uint32_t length;
		read_uint32(length, rawData, offset);

		const char* strData = reinterpret_cast<const char*>(rawData.data() + offset);
		return std::string(strData, length ? length - 1 : 0);
	}

	void ILStringTable::read_uint32(std::uint32_t& val, const std::vector<std::byte>& a_buffer, std::uint32_t& a_bufferPosition)
	{
		std::memcpy(&val, a_buffer.data() + a_bufferPosition, sizeof(std::uint32_t));
		a_bufferPosition += sizeof(std::uint32_t);
	}

	void SubtitleInfoEx::setFlag(Flag a_flag, bool a_set)
	{
		if (a_set) {
			pad07 |= static_cast<std::uint8_t>(a_flag);
		} else {
			pad07 &= ~(static_cast<std::uint8_t>(a_flag));
		}
	}

	void SubtitleInfoEx::setAlphaModifier(float value)
	{
		std::uint32_t quantized = static_cast<std::uint32_t>(std::round(value * 16777215.0f));  // 2^24 - 1
		pad04 = static_cast<std::uint8_t>((quantized >> 0) & 0xFF);
		pad05 = static_cast<std::uint8_t>((quantized >> 8) & 0xFF);
		pad06 = static_cast<std::uint8_t>((quantized >> 16) & 0xFF);
	}

	float SubtitleInfoEx::getAlphaModifier() const
	{
		std::uint32_t quantized = (static_cast<std::uint32_t>(pad04)) | (static_cast<std::uint32_t>(pad05) << 8) | (static_cast<std::uint32_t>(pad06) << 16);
		return static_cast<float>(quantized) / 16777215.0f;
	}

	bool IsCrosshairRef(const TESObjectREFRPtr& a_ref)
	{
		// ViewCaster is not declared in Dear-Modding CommonLibF4. Resolve the
		// singleton + ViewCasterBase::QActivatePickRef directly via per-runtime RVAs.
		// Singleton is a global ViewCaster* populated by BSTSingletonSDM::InitSDM at startup.
		//   QActivatePickRef:           OG=0x009DDDF0 NG=0x00940810 AE=0x00993F60
		//   ViewCaster* singleton ptr:  OG=0x058E2B30 NG=0x02E77AB8 AE=0x030EEEF8
		// QActivatePickRef is `ObjectRefHandle ViewCasterBase::QActivatePickRef() const`.
		// MSVC __thiscall x64: rcx = this, rdx = hidden out-ptr to a 4-byte ObjectRefHandle;
		// returns rax = rdx. Internally takes a read-lock on `this+0x60`.
		struct RVAs
		{
			std::uintptr_t qActivatePickRef;
			std::uintptr_t singleton;
		};
		const auto rvas = []() -> RVAs {
			if (REL::Module::IsRuntimeOG()) {
				return { 0x009DDDF0, 0x058E2B30 };
			} else if (REL::Module::IsRuntimeNG()) {
				return { 0x00940810, 0x02E77AB8 };
			} else {
				return { 0x00993F60, 0x030EEEF8 };
			}
		}();

		static const REL::Relocation<void**>                                            singleton{ REL::Offset(rvas.singleton) };
		static const REL::Relocation<ObjectRefHandle* (*)(void* a_self, ObjectRefHandle*)> qActivatePickRef{ REL::Offset(rvas.qActivatePickRef) };

		auto* vc = *singleton;
		if (!vc) {
			return false;
		}

		ObjectRefHandle handle;
		qActivatePickRef(vc, &handle);
		return handle.get() == a_ref;
	}

	NiAVObject* GetHeadNode(const TESObjectREFRPtr& a_ref)
	{
		if (auto actor = a_ref->As<Actor>()) {
			if (auto middle = actor->currentProcess ? actor->currentProcess->middleHigh : nullptr) {
				return middle->headNode;
			}
		}
		return nullptr;
	}

	NiAVObject* GetTorsoNode(const Actor* a_actor)
	{
		if (auto middle = a_actor->currentProcess ? a_actor->currentProcess->middleHigh : nullptr) {
			return middle->torsoNode;
		}
		return nullptr;
	}

	void BroadcastEvent(BSTValueEventSource<HUDSubtitleDisplayEvent>* a_this)
	{
		using func_t = decltype(&BroadcastEvent);
		static REL::Relocation<func_t> func{ REL::ID{ 328561, 2229076 } };
		return func(a_this);
	}

	const char* GetSpeakerName(SubtitleInfoEx& a_subInfo)
	{
		if (auto speakerBase = a_subInfo.topicInfo ? a_subInfo.topicInfo->GetSpeaker() : nullptr) {
			return speakerBase->GetFullName();
		}

		// OG: GetDisplayFullName triggers BSResource::EntryDB::Force which can crash
		// Use safe path only: topicInfo speaker (above) or TESNPC::GetShortName
		auto speaker = a_subInfo.speaker.get();
		if (!speaker) {
			return nullptr;
		}

		if (REL::Module::IsRuntimeOG()) {
			if (auto speakerActor = speaker->As<Actor>()) {
				if (auto speakerNPC = static_cast<TESNPC*>(speakerActor->GetObjectReference())) {
					return speakerNPC->GetShortName();
				}
			}
			return nullptr;
		}

		if (!speaker->IsActor() || speaker->extraList && speaker->extraList->HasType<ExtraTextDisplayData>()) {
			return speaker->GetDisplayFullName();
		} else {
			if (auto speakerActor = speaker->As<Actor>()) {
				if (auto speakerNPC = static_cast<TESNPC*>(speakerActor->GetObjectReference())) {
					return speakerNPC->GetShortName();
				}
			}
		}
		return nullptr;
	}
}
