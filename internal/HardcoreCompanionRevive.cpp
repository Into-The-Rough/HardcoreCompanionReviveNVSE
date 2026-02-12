#include "HardcoreCompanionRevive.hpp"
#include "HCR-DownedMarkers.hpp"
#include "HCR-Settings.hpp"
#include "PatchUtil.hpp"
#include "NVSEInterface.hpp"
#include "TESForm.hpp"
#include "Actor.hpp"
#include "Sound.hpp"
#include "Console.hpp"
#include <cstring>

namespace HardcoreCompanionRevive
{
	static TESForm* g_ScrapMetal = nullptr;
	static char g_sRevivePrompt[] = "Revive";

	static UInt32 s_revivingCompanionRefID = 0;
	static UInt32 s_pendingHealthRestoreRefID = 0;
	static float s_pendingMaxHealth = 0.0f;
	static int s_damageDelayFrames = 0;
	static bool s_hooksInstalled = false;
	static bool s_killHookInstalled = false;
	static bool s_hooksInitPending = false;
	static bool s_jipChecked = false;
	static bool s_scriptCompiled = false;
	static Script* g_f4stDispatchScript = nullptr;

	struct UnconsciousEntry {
		UInt32 refID;
		float gameTimeDays;
	};
	static const int kMaxTrackedCompanions = 16;
	static UnconsciousEntry s_unconsciousCompanions[kMaxTrackedCompanions] = {0};

	static float GetGameDaysPassed() {
		GameTimeGlobals* gtg = GameTimeGlobals::Get();
		if (gtg && gtg->daysPassed) {
			return gtg->daysPassed->data;
		}
		return 0.0f;
	}

	static void TrackUnconsciousCompanion(UInt32 refID) {
		float currentDays = GetGameDaysPassed();
		for (int i = 0; i < kMaxTrackedCompanions; i++) {
			if (s_unconsciousCompanions[i].refID == refID) {
				return;
			}
		}
		for (int i = 0; i < kMaxTrackedCompanions; i++) {
			if (s_unconsciousCompanions[i].refID == 0) {
				s_unconsciousCompanions[i].refID = refID;
				s_unconsciousCompanions[i].gameTimeDays = currentDays;
				return;
			}
		}
	}

	static float GetUnconsciousGameTime(UInt32 refID) {
		for (int i = 0; i < kMaxTrackedCompanions; i++) {
			if (s_unconsciousCompanions[i].refID == refID) {
				return s_unconsciousCompanions[i].gameTimeDays;
			}
		}
		return -1.0f;
	}

	static void UntrackCompanion(UInt32 refID) {
		for (int i = 0; i < kMaxTrackedCompanions; i++) {
			if (s_unconsciousCompanions[i].refID == refID) {
				s_unconsciousCompanions[i].refID = 0;
				s_unconsciousCompanions[i].gameTimeDays = 0.0f;
				return;
			}
		}
	}

	static bool IsHardcoreMode() {
		void* pc = *(void**)kAddr_PlayerCharacter;
		if (!pc) return false;
		return *(bool*)((UInt8*)pc + 0x7BC);
	}

	static const char* GetALCHName(TESForm* form) {
		if (!form || form->typeID != kFormType_ALCH) return nullptr;
		return *(const char**)((UInt8*)form + 0x34);
	}

	static bool StrContainsCI(const char* haystack, const char* needle) {
		if (!haystack || !needle) return false;
		size_t needleLen = strlen(needle);
		for (const char* p = haystack; *p; p++) {
			if (_strnicmp(p, needle, needleLen) == 0) return true;
		}
		return false;
	}

	static SInt32 GetItemCount(void* actor, TESForm* form) {
		void* extraDataList = (void*)((UInt8*)actor + 0x44);
		void* containerChanges = ((void*(__thiscall*)(void*, UInt32))0x410220)(extraDataList, 0x15);

		if (!containerChanges) {
			return 0;
		}

		void* data = *(void**)((UInt8*)containerChanges + 0xC);

		if (!data) {
			return 0;
		}

		return ((SInt32(__thiscall*)(void*, TESForm*))0x4C8F30)(data, form);
	}

	static TESForm* FindStimpakInInventory(void* player) {
		void* extraDataList = (void*)((UInt8*)player + 0x44);
		void* xChanges = ((void*(__thiscall*)(void*, UInt32))0x410220)(extraDataList, 0x15);
		if (!xChanges) return nullptr;

		void* data = *(void**)((UInt8*)xChanges + 0x0C);
		if (!data) return nullptr;

		struct EntryData {
			void* extendData;
			SInt32 countDelta;
			TESForm* type;
		};

		struct Node {
			EntryData* item;
			Node* next;
		};

		void* objList = *(void**)data;
		if (!objList) return nullptr;

		Node* node = (Node*)objList;
		while (node) {
			EntryData* entry = node->item;
			if (entry && entry->type && entry->countDelta > 0 && entry->type->typeID == kFormType_ALCH) {
				const char* name = GetALCHName(entry->type);
				if (name && StrContainsCI(name, "stimpak")) {
					return entry->type;
				}
			}
			node = node->next;
		}

		return nullptr;
	}

	static TESForm* GetRevivalItem(void* actor, void* player, bool* outIsRobot) {
		if (!g_ScrapMetal) {
			g_ScrapMetal = LookupFormByID(kFormID_ScrapMetal);
		}
		bool isRobot = IsActorRobot(actor);
		if (outIsRobot) *outIsRobot = isRobot;
		if (isRobot && g_ScrapMetal) {
			return g_ScrapMetal;
		}
		return FindStimpakInInventory(player);
	}

	static void RemoveItem(void* actor, TESForm* form) {
		void* vtbl = *(void**)actor;
		typedef void* (__thiscall* RemoveItemFn)(void*, TESForm*, void*, UInt32, bool, bool, void*, UInt32, UInt32, bool, bool);
		RemoveItemFn fn = (RemoveItemFn)*((UInt32*)vtbl + 0x5F);
		fn(actor, form, nullptr, 1, false, false, nullptr, 0, 0, true, false);
	}

	static void PlayPickupPutdownSound(void* actor, TESForm* item) {
		((void(__thiscall*)(void*, TESForm*, bool, bool))0x8ADED0)(actor, item, false, true);
	}

	static bool __fastcall OnActivateUnconsciousTeammate(void* actorParam) {
		void* actor = actorParam;
		void* pc = *(void**)kAddr_PlayerCharacter;

		if (!Settings::bAllowInCombat && IsPlayerInCombat()) {
			if (Settings::bShowMessage) {
				QueueUIMessage("Cannot revive companions during combat.", 0, (const char*)0x1049638, nullptr, 2.0f, 0);
				PlayGameSound("UIMenuCancel");
			}
			return true;
		}

		BaseProcess* process = GetActorProcess(actor);
		if (!process) {
			return false;
		}

		float timer = process->GetEssentialDownTimer();

		bool isRobot = false;
		TESForm* healingItem = GetRevivalItem(actor, pc, &isRobot);

		if (!IsHardcoreMode() && timer <= 1.0f) {
			return false;
		}

		bool hasItem = false;
		if (isRobot && healingItem) {
			hasItem = GetItemCount(pc, healingItem) > 0;
		} else {
			hasItem = (healingItem != nullptr);
		}

		if (!hasItem) {
			if (Settings::bShowMessage) {
				const char* msg = isRobot ? "You need scrap metal to revive this companion." : "You need a stimpak to revive this companion.";
				QueueUIMessage(msg, 0, (const char*)0x1049638, nullptr, 2.0f, 0);
				PlayGameSound("UIMenuCancel");
			}
			return true;
		}

		RemoveItem(pc, healingItem);

		s_revivingCompanionRefID = ((TESForm*)actor)->refID;

		if (isRobot) {
			PlayGameSound(kUIRepairWeaponSound);
		} else {
			PlayPickupPutdownSound(actor, healingItem);
		}

		if (Settings::bShowMessage) {
			const char* removedMsg = isRobot ? "Scrap metal removed." : "Stimpak removed.";
			QueueUIMessage(removedMsg, 0, nullptr, nullptr, 2.0f, 0);
		}

		UntrackCompanion(((TESForm*)actor)->refID);

		UInt32* lifeStatePtr = (UInt32*)((UInt8*)actor + 0x108);
		*lifeStatePtr = kLifeState_Alive;
		process->SetEssentialDownTimer(0.0f);

		void* avOwner = (void*)((UInt8*)actor + 0xA4);
		void** avOwnerVtbl = *(void***)avOwner;
		typedef float (__thiscall *GetBaseAVFn)(void*, UInt32);
		GetBaseAVFn getBaseAV = (GetBaseAVFn)avOwnerVtbl[1];
		float maxHealth = getBaseAV(avOwner, kAVCode_Health);

		void** vtbl = *(void***)actor;
		typedef void (__thiscall *InitGetUpPackageFn)(void*);
		InitGetUpPackageFn initGetUp = (InitGetUpPackageFn)vtbl[0x106];
		initGetUp(actor);

		s_pendingHealthRestoreRefID = ((TESForm*)actor)->refID;
		s_pendingMaxHealth = maxHealth;
		s_damageDelayFrames = 30;

		return true;
	}

	__declspec(naked) void __fastcall OnActivateNPCHook() {
		__asm {
			mov eax, [ecx + 0x108]
			cmp byte ptr ds:[ecx + 0x18D], 0
			je nonCompanion

			cmp eax, 0x6
			jne done

			push ecx
			call OnActivateUnconsciousTeammate
			pop ecx
			test al, al
			jz letVanillaHandle
			add esp, 4
			mov eax, 0x608D62
			jmp eax

		letVanillaHandle:
			mov eax, [ecx + 0x108]
			cmp eax, 0x4
			sete al
			ret

		done:
		nonCompanion:
			cmp eax, 0x4
			sete al
			ret
		}
	}

	__declspec(naked) void __fastcall OnActivateCreatureHook() {
		__asm {
			mov eax, [ecx + 0x108]
			cmp byte ptr ds:[ecx + 0x18D], 0
			je nonCompanion

			cmp eax, 0x6
			jne done

			push ecx
			call OnActivateUnconsciousTeammate
			pop ecx
			test al, al
			jz letVanillaHandle
			add esp, 4
			mov eax, 0x5FB168
			jmp eax

		letVanillaHandle:
			mov eax, [ecx + 0x108]
			cmp eax, 0x4
			sete al
			ret

		done:
		nonCompanion:
			cmp eax, 0x4
			sete al
			ret
		}
	}

	static const char* __cdecl GetRevivePromptIfApplicable(void* actor, UInt32 promptType) {
		if (!actor) return nullptr;
		if (promptType != 7 && promptType != 0xA) return nullptr;
		UInt32 vtbl = *(UInt32*)actor;
		if (vtbl < 0x1000000 || vtbl > 0x1200000) return nullptr;
		if (!GetActorIsTeammate(actor)) return nullptr;
		if (GetActorLifeState(actor) != kLifeState_EssentialUnconscious) return nullptr;
		return g_sRevivePrompt;
	}

	__declspec(naked) void OnGetPromptHook() {
		__asm {
			push ecx
			mov eax, [ebp + 8]
			push ecx
			push eax
			call GetRevivePromptIfApplicable
			add esp, 8
			pop ecx
			test eax, eax
			jz done

			mov ecx, 0x77789E
			jmp ecx

		done:
			mov ecx, dword ptr ds:[ecx * 4 + 0x11D5160]
			mov eax, 0x777899
			jmp eax
		}
	}

	static int __fastcall GetLifeStateReturnAliveIfCompanion(void* actor) {
		if (IsHardcoreMode() && GetActorIsTeammate(actor)) {
			return kLifeState_Alive;
		}
		return GetActorLifeState(actor);
	}

	static bool s_killingCompanion = false;
	static void* s_forceKillActor = nullptr;

	static bool __fastcall IsEssentialHook_Check(void* actor, void* edx) {
		if (actor == s_forceKillActor && s_forceKillActor != nullptr) {
			return false;
		}
		return ((bool(__thiscall*)(void*))0x87F3D0)(actor);
	}

	__declspec(naked) void IsEssentialHook_ActorKill() {
		__asm {
			push edx
			mov edx, 0
			call IsEssentialHook_Check
			pop edx
			ret
		}
	}

	const UInt8 kJIPFlag_TeammateKillable = 0x04; //jipActorFlags2 bit 2

	static void SetActorKillableViaJIP(void* actor, bool killable) {
		UInt8* jipFlags2 = (UInt8*)((UInt8*)actor + 0x106);
		if (killable) {
			*jipFlags2 |= kJIPFlag_TeammateKillable;
		} else {
			*jipFlags2 &= ~kJIPFlag_TeammateKillable;
		}
	}

	static void ClearRuntimeEssentialFlag(void* actor) {
		UInt32* flags0140 = (UInt32*)((UInt8*)actor + 0x140);
		*flags0140 &= ~0x80000000;
	}

	static void ClearBaseFormEssentialFlag(void* actor) {
		TESForm* baseForm = *(TESForm**)((UInt8*)actor + 0x20);
		if (baseForm) {
			UInt32* actorBaseFlags = (UInt32*)((UInt8*)baseForm + 0x34);
			*actorBaseFlags &= ~0x02;
		}
	}

	static void KillCompanion(void* actor) {
		s_killingCompanion = true;
		s_forceKillActor = actor;

		UInt32* lifeStatePtr = (UInt32*)((UInt8*)actor + 0x108);

		SetActorKillableViaJIP(actor, true);

		ClearRuntimeEssentialFlag(actor);
		ClearBaseFormEssentialFlag(actor);

		((void(__thiscall*)(void*, bool))0x8BCA90)(actor, false);

		*lifeStatePtr = 0;

		((void(__thiscall*)(void*, void*, float))0x89D900)(actor, nullptr, 0.0f);

		SetActorKillableViaJIP(actor, false);

		s_forceKillActor = nullptr;
		s_killingCompanion = false;
	}

	static void CheckCompanionBleedout() {
		if (!Settings::bAllowDeath || !IsHardcoreMode() || !s_killHookInstalled) {
			return;
		}

		void* pc = *(void**)kAddr_PlayerCharacter;
		if (!pc) return;

		struct TeammateNode {
			void* actor;
			TeammateNode* next;
		};
		TeammateNode* teammates = (TeammateNode*)((UInt8*)pc + 0x5FC);

		float bleedoutDays = Settings::fBleedoutTime / 24.0f;
		float currentDays = GetGameDaysPassed();

		void* actorsToKill[16] = {0};
		int killCount = 0;

		while (teammates) {
			void* actor = teammates->actor;

			if (actor) {
				UInt32 refID = ((TESForm*)actor)->refID;

				if (refID == s_revivingCompanionRefID) {
					int lifeState = GetActorLifeState(actor);
					if (lifeState != kLifeState_EssentialUnconscious) {
						s_revivingCompanionRefID = 0;
						UntrackCompanion(refID);
					}
					teammates = teammates->next;
					continue;
				}

				int lifeState = GetActorLifeState(actor);

				if (lifeState == kLifeState_EssentialUnconscious) {
					float unconsciousTime = GetUnconsciousGameTime(refID);

					if (unconsciousTime < 0.0f) {
						TrackUnconsciousCompanion(refID);
						unconsciousTime = GetUnconsciousGameTime(refID);
					}

					float daysUnconscious = currentDays - unconsciousTime;

					if (daysUnconscious >= bleedoutDays && killCount < 16) {
						actorsToKill[killCount++] = actor;
					}
				} else {
					UntrackCompanion(refID);
				}
			}
			teammates = teammates->next;
		}

		for (int i = 0; i < killCount; i++) {
			void* actor = actorsToKill[i];
			UInt32 refID = ((TESForm*)actor)->refID;
			KillCompanion(actor);
			UntrackCompanion(refID);
			if (Settings::bShowMessage) {
				QueueUIMessage("Your companion has died.", 0, (const char*)0x1049638, nullptr, 2.0f, 0);
			}
		}
	}

	static void ApplyPendingHealthRestore() {
		if (s_pendingHealthRestoreRefID && s_damageDelayFrames > 0) {
			s_damageDelayFrames--;
			if (s_damageDelayFrames == 0) {
				UInt32 refID = s_pendingHealthRestoreRefID;
				float maxHealth = s_pendingMaxHealth;
				s_pendingHealthRestoreRefID = 0;
				s_pendingMaxHealth = 0.0f;

				TESForm* form = LookupFormByID(refID);
				if (form) {
					void* actor = (void*)form;
					typedef void (__thiscall *GameModActorValueFn)(void*, UInt32, float, void*);
					GameModActorValueFn modAV = (GameModActorValueFn)kAddr_GameModActorValueF;
					float damageAmount = -(maxHealth * 0.75f);
					modAV(actor, kAVCode_Health, damageAmount, nullptr);

					if (g_scriptInterface && g_f4stDispatchScript) {
						g_scriptInterface->CallFunctionAlt(g_f4stDispatchScript, nullptr, 0);
					}
				}
			}
		}
	}

	static bool IsJIPInstalled() {
		return *(UInt8*)0x87F427 == 0xE9; //JIP hooks IsEssential here
	}

	static void CheckJIPDependency() {
		if (s_jipChecked) return;
		s_jipChecked = true;

		if (!IsJIPInstalled()) {
			QueueUIMessage(
				"HardcoreCompanionRevive: JIP LN NVSE is required but not detected! The mod will not function correctly.",
				0, (const char*)0x1049638, nullptr, 5.0f, 0
			);
		}
	}

	enum class PatchCheckResult {
		kVanilla = 0,
		kAlreadyOurs,
		kConflict
	};

	static void ReportPatchConflict(const char* siteName, UInt32 address, const UInt8* foundBytes, size_t byteCount) {
		UInt32 target = 0;
		if (byteCount >= 5 && (foundBytes[0] == 0xE8 || foundBytes[0] == 0xE9)) {
			target = DecodeRelTarget(address, foundBytes);
		}

		if (target) {
			Console_Print("HardcoreCompanionRevive conflict at %s (0x%08X): opcode %02X target 0x%08X",
				siteName, address, foundBytes[0], target);
		} else {
			Console_Print("HardcoreCompanionRevive conflict at %s (0x%08X): bytes %02X %02X %02X %02X %02X",
				siteName, address,
				byteCount > 0 ? foundBytes[0] : 0,
				byteCount > 1 ? foundBytes[1] : 0,
				byteCount > 2 ? foundBytes[2] : 0,
				byteCount > 3 ? foundBytes[3] : 0,
				byteCount > 4 ? foundBytes[4] : 0);
		}
	}

	static void ReportPatchWriteFailure(const char* siteName, UInt32 address) {
		Console_Print("HardcoreCompanionRevive failed to patch %s at 0x%08X", siteName, address);
	}

	static PatchCheckResult CheckRelSite(const RelPatchSite& site, UInt8 current[5]) {
		memcpy(current, (const void*)site.address, 5);

		UInt8 ours[5] = {0};
		BuildRelPatchBytes(site.opcode, site.address, site.target, ours);

		if (memcmp(current, ours, 5) == 0) {
			return PatchCheckResult::kAlreadyOurs;
		}
		if (memcmp(current, site.vanillaBytes, 5) == 0) {
			return PatchCheckResult::kVanilla;
		}
		return PatchCheckResult::kConflict;
	}

	static PatchCheckResult CheckWrite16Site(const Write16PatchSite& site, UInt8 current[2]) {
		memcpy(current, (const void*)site.address, 2);

		const UInt8 desiredBytes[2] = {
			(UInt8)(site.desiredValue & 0xFF),
			(UInt8)((site.desiredValue >> 8) & 0xFF)
		};

		if (memcmp(current, desiredBytes, 2) == 0) {
			return PatchCheckResult::kAlreadyOurs;
		}
		if (memcmp(current, site.vanillaBytes, 2) == 0) {
			return PatchCheckResult::kVanilla;
		}
		return PatchCheckResult::kConflict;
	}

	static bool PrecheckRelSites(const RelPatchSite* sites, size_t count) {
		bool ok = true;
		for (size_t i = 0; i < count; i++) {
			UInt8 current[5] = {0};
			PatchCheckResult result = CheckRelSite(sites[i], current);
			if (result == PatchCheckResult::kConflict) {
				ReportPatchConflict(sites[i].name, sites[i].address, current, 5);
				ok = false;
			}
		}
		return ok;
	}

	static bool ApplyRelSites(const RelPatchSite* sites, size_t count) {
		for (size_t i = 0; i < count; i++) {
			UInt8 current[5] = {0};
			PatchCheckResult result = CheckRelSite(sites[i], current);
			if (result == PatchCheckResult::kAlreadyOurs) {
				continue;
			}
			if (result != PatchCheckResult::kVanilla) {
				ReportPatchConflict(sites[i].name, sites[i].address, current, 5);
				return false;
			}

			UInt8 patchBytes[5] = {0};
			BuildRelPatchBytes(sites[i].opcode, sites[i].address, sites[i].target, patchBytes);
			if (!WriteMemoryChecked(sites[i].address, patchBytes, sizeof(patchBytes))) {
				ReportPatchWriteFailure(sites[i].name, sites[i].address);
				return false;
			}
		}
		return true;
	}

	static bool ApplyWrite16SiteIfVanilla(const Write16PatchSite& site) {
		UInt8 current[2] = {0};
		PatchCheckResult result = CheckWrite16Site(site, current);
		if (result == PatchCheckResult::kAlreadyOurs) {
			return true;
		}
		if (result == PatchCheckResult::kConflict) {
			ReportPatchConflict(site.name, site.address, current, 2);
			return false;
		}

		const UInt8 desiredBytes[2] = {
			(UInt8)(site.desiredValue & 0xFF),
			(UInt8)((site.desiredValue >> 8) & 0xFF)
		};
		if (!WriteMemoryChecked(site.address, desiredBytes, sizeof(desiredBytes))) {
			ReportPatchWriteFailure(site.name, site.address);
			return false;
		}
		return true;
	}

	static BaseProcess* __fastcall OnKillEssentialSetDownTimerIfCompanion(void* actor) {
		if (!actor) {
			return nullptr;
		}

		BaseProcess* process = GetActorProcess(actor);

		if (s_killingCompanion) {
			return process;
		}

		if (actor == s_forceKillActor) {
			return process;
		}

		if (!process) {
			return nullptr;
		}

		bool isTeammate = GetActorIsTeammate(actor);

		if (isTeammate && IsHardcoreMode()) {
			process->SetEssentialDownTimer(999999.0f);

			if (Settings::bAllowDeath) {
				UInt32 refID = ((TESForm*)actor)->refID;
				TrackUnconsciousCompanion(refID);
			}
		}
		return process;
	}

	void InitHooks() {
		s_hooksInstalled = false;
		s_killHookInstalled = false;

		const UInt8 essentialPatch[2] = {0x66, 0x90};
		if (!WriteMemoryChecked(0x87F437, essentialPatch, sizeof(essentialPatch))) {
			return;
		}

		if (!WriteRelBranch(0x607B52, 0xE8, (UInt32)OnActivateNPCHook)) return;
		if (!WriteRelBranch(0x5FA563, 0xE8, (UInt32)OnActivateCreatureHook)) return;
		if (!WriteRelBranch(0x777892, 0xE9, (UInt32)OnGetPromptHook)) return;
		if (!WriteRelBranch(0x89DFC6, 0xE8, (UInt32)OnKillEssentialSetDownTimerIfCompanion)) return;
		if (!WriteRelBranch(0x882CFF, 0xE8, (UInt32)GetLifeStateReturnAliveIfCompanion)) return;
		if (!WriteRelBranch(0x8834CD, 0xE8, (UInt32)GetLifeStateReturnAliveIfCompanion)) return;
		if (!WriteRelBranch(0x883A92, 0xE8, (UInt32)GetLifeStateReturnAliveIfCompanion)) return;
		if (!WriteRelBranch(0x883C6E, 0xE8, (UInt32)GetLifeStateReturnAliveIfCompanion)) return;
		if (!WriteRelBranch(0x89DE40, 0xE8, (UInt32)IsEssentialHook_ActorKill)) return;

		s_hooksInstalled = true;
		s_killHookInstalled = true;
	}

	void __cdecl MessageHandler(NVSEMessagingInterface::Message* msg) {
		if (msg->type == kMessage_PostLoad) {
			CheckJIPDependency();
			s_hooksInitPending = true;
		}
		else if (msg->type == kMessage_MainGameLoop) {
			if (s_hooksInitPending) {
				s_hooksInitPending = false;
				InitHooks();
			}

			if (!s_hooksInstalled) {
				return;
			}

			if (!s_scriptCompiled && g_scriptInterface) {
				s_scriptCompiled = true;
				g_f4stDispatchScript = g_scriptInterface->CompileScript(
					"Begin Function{}\n"
					"DispatchEventAlt \"*F4ST_InitMain\"\n"
					"End"
				);
			}

			CheckCompanionBleedout();
			ApplyPendingHealthRestore();
			UpdateDownedCompanionMarkers();
		}
		else if (msg->type == kMessage_PostLoadGame || msg->type == kMessage_NewGame || msg->type == kMessage_ExitToMainMenu) {
			s_revivingCompanionRefID = 0;
			s_pendingHealthRestoreRefID = 0;
			s_pendingMaxHealth = 0.0f;
			s_damageDelayFrames = 0;
			s_forceKillActor = nullptr;
			s_killingCompanion = false;
			memset(s_unconsciousCompanions, 0, sizeof(s_unconsciousCompanions));
			ResetDownedMarkerTiles();
			s_scriptCompiled = false;
			g_f4stDispatchScript = nullptr;
		}
		else if (msg->type == kMessage_ReloadConfig) {
			if (msg->data && msg->dataLen > 0) {
				const char* pluginName = (const char*)msg->data;
				if (_stricmp(pluginName, "HardcoreCompanionReviveNVSE") == 0) {
					Settings::LoadINI();
					Console_Print("HardcoreCompanionReviveNVSE: Config reloaded");
				}
			}
		}
	}
}
