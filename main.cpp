#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cmath>

typedef unsigned char UInt8;
typedef unsigned short UInt16;
typedef unsigned int UInt32;
typedef signed int SInt32;
typedef UInt32 PluginHandle;

const PluginHandle kPluginHandle_Invalid = 0xFFFFFFFF;
PluginHandle g_pluginHandle = kPluginHandle_Invalid;

#define PLUGIN_VERSION 1

namespace Settings {
	bool bShowMessage = true;
	bool bShowIcon = true;
	bool bAllowInCombat = true;
	bool bAllowDeath = false;
	float fBleedoutTime = 1.0f;

	char g_iniPath[MAX_PATH];

	void LoadINI() {
		GetModuleFileNameA(NULL, g_iniPath, MAX_PATH);
		char* lastSlash = strrchr(g_iniPath, '\\');
		if (lastSlash) {
			strcpy_s(lastSlash + 1, MAX_PATH - (lastSlash + 1 - g_iniPath),
				"Data\\Config\\HardcoreCompanionReviveNVSE\\HardcoreCompanionReviveNVSE.ini");
		}

			bShowMessage = GetPrivateProfileIntA("General", "bShowMessage", 1, g_iniPath) != 0;
			bShowIcon = GetPrivateProfileIntA("General", "bShowIcon", 1, g_iniPath) != 0;
			bAllowInCombat = GetPrivateProfileIntA("General", "bAllowInCombat", 1, g_iniPath) != 0;
			bAllowDeath = GetPrivateProfileIntA("General", "bAllowDeath", 0, g_iniPath) != 0;

		char buf[32];
		GetPrivateProfileStringA("General", "fBleedoutTime", "1.0", buf, sizeof(buf), g_iniPath);
		fBleedoutTime = (float)atof(buf);
		if (fBleedoutTime <= 0.0f) fBleedoutTime = 1.0f;
	}
}

struct TESForm;
struct TESObjectREFR;
struct Actor;
struct PlayerCharacter;
struct BaseProcess;
struct NVSEInterface;
struct PluginInfo;
struct Setting;
struct NVSEMessagingInterface;

NVSEMessagingInterface* g_messagingInterface = nullptr;

struct Script;
struct NVSEScriptInterface {
	enum { kVersion = 1 };
	void* CallFunction;
	void* GetFunctionParams;
	void* ExtractArgsEx;
	void* ExtractFormatStringArgs;
	bool (*CallFunctionAlt)(Script* funcScript, void* callingObj, UInt8 numArgs, ...);
	Script* (*CompileScript)(const char* scriptText);
	Script* (*CompileExpression)(const char* expression);
};
NVSEScriptInterface* g_scriptInterface = nullptr;
Script* g_f4stDispatchScript = nullptr;
const UInt32 kMessage_PostLoad = 0;
const UInt32 kMessage_ExitToMainMenu = 2;
const UInt32 kMessage_PostLoadGame = 9;
const UInt32 kMessage_NewGame = 14;
const UInt32 kMessage_MainGameLoop = 20;
const UInt32 kMessage_ReloadConfig = 25;
static bool s_jipChecked = false;

const UInt32 kAddr_PlayerCharacter = 0x011DEA3C;
const UInt32 kAddr_BGSDefaultObjectManager = 0x11CA80C;
const UInt32 kAddr_QueueUIMessage = 0x7052F0;
const UInt32 kAddr_LookupFormByID = 0x4839C0;
const UInt32 kAddr_BSWin32Audio = 0x11F6D98;
const UInt32 kAddr_GameModActorValueF = 0x881130;

const UInt32 kFormType_TESCreature = 0x2B;
const UInt32 kFormType_ALCH = 0x2F;
const UInt32 kLifeState_EssentialUnconscious = 0x6;
const UInt32 kLifeState_Alive = 0x0;
const UInt32 kAVCode_Health = 16;
const UInt32 kCreatureType_Robot = 6;
const UInt32 kFormID_ScrapMetal = 0x31944;
const char* kUIRepairWeaponSound = (const char*)0x1075D38;

struct PluginInfo {
	enum { kInfoVersion = 1 };
	UInt32 infoVersion;
	const char* name;
	UInt32 version;
};

struct NVSEInterface {
	enum {
		kInterface_Serialization = 0,
		kInterface_Console = 1,
		kInterface_Messaging = 2,
		kInterface_CommandTable = 3,
		kInterface_StringVar = 4,
		kInterface_ArrayVar = 5,
		kInterface_Script = 6,
		kInterface_Data = 7,
		kInterface_EventManager = 8
	};
	UInt32 nvseVersion;
	UInt32 runtimeVersion;
	UInt32 editorVersion;
	UInt32 isEditor;
	bool (*RegisterCommand)(void* info);
	void (*SetOpcodeBase)(UInt32 base);
	void* (*QueryInterface)(UInt32 id);
	PluginHandle (*GetPluginHandle)(void);
	UInt32 isNogore;
};

struct NVSEMessagingInterface {
	struct Message {
		const char* sender;
		UInt32 type;
		UInt32 dataLen;
		void* data;
	};
	typedef void (__cdecl *EventCallback)(Message* msg);
	UInt32 version;
	bool (*RegisterListener)(PluginHandle listener, const char* sender, EventCallback handler);
	bool (*Dispatch)(PluginHandle sender, UInt32 messageType, void* data, UInt32 dataLen, const char* receiver);
};

struct TESForm {
	void* vtbl;
	UInt8 typeID;
	UInt8 pad05[3];
	UInt32 flags;
	UInt32 refID;
	UInt8 pad10[8];
};

struct TESGlobal : TESForm {
	UInt8 pad18[0xC];
	float data;
};

struct GameTimeGlobals {
	TESGlobal* year;
	TESGlobal* month;
	TESGlobal* day;
	TESGlobal* hour;
	TESGlobal* daysPassed;
	TESGlobal* timeScale;

	static GameTimeGlobals* Get() { return (GameTimeGlobals*)0x11DE7B8; }
};

struct Setting {
	void* vtbl;
	union {
		UInt32 uint;
		int i;
		float f;
		char* str;
	} data;
	const char* name;
};

struct BaseProcess {
	void* vtbl;
	UInt8 pad04[0x04];

	float GetEssentialDownTimer() { return ((float(__thiscall*)(BaseProcess*))*(UInt32*)((*(UInt32*)this) + 0xE4))(this); }
	void SetEssentialDownTimer(float value) { ((void(__thiscall*)(BaseProcess*, float))*(UInt32*)((*(UInt32*)this) + 0xE8))(this, value); }
};

struct ExtraDataList {
	void* vtbl;
	void* data;
	UInt8 presenceBitfield[0x15];
};

struct TESObjectREFR : TESForm {
	UInt8 pad18[0x8];
	TESForm* baseForm;
	UInt8 pad24[0x40];
	ExtraDataList extraDataList;
};

struct TESCreature : TESForm {
	UInt8 pad18[0x114];
	UInt8 type;
};

struct BGSDefaultObjectManager : TESForm {
	TESForm* forms[34];

	enum DefaultObjects {
		kStimpak = 0
	};

	static BGSDefaultObjectManager* GetSingleton() { return *(BGSDefaultObjectManager**)kAddr_BGSDefaultObjectManager; }
};

template <typename T>
struct tList {
	struct Node {
		T* data;
		Node* next;
	};
	Node node;

	Node* Head() { return &node; }
};


struct Sound {
	UInt32 soundKey;
	UInt8  byte04;
	UInt8  pad05[3];
	UInt32 unk08;

	Sound() : soundKey(0xFFFFFFFF), byte04(0), unk08(0) {}

	void Init(const char* soundPath, UInt32 flags) {
		void* audioManager = *(void**)kAddr_BSWin32Audio;
		((void(__thiscall*)(void*, Sound*, const char*, UInt32))0xAD7550)(audioManager, this, soundPath, flags);
	}

	void Play() {
		if (soundKey != 0xFFFFFFFF) {
			((void(__thiscall*)(Sound*, UInt32))0xAD8830)(this, 0);
		}
	}
};

const UInt32 kAudioFlags_2D = 1;
const UInt32 kAudioFlags_100 = 0x100;
const UInt32 kAudioFlags_SystemSound = 0x200000;

TESForm* LookupFormByID(UInt32 id) {
	return ((TESForm*(*)(UInt32))kAddr_LookupFormByID)(id);
}

void PlayGameSound(const char* soundEditorID) {
	Sound sound;
	sound.Init(soundEditorID, kAudioFlags_100 | kAudioFlags_SystemSound | kAudioFlags_2D);
	sound.Play();
}

void Console_Print(const char* fmt, ...) {
	void* consoleManager = ((void* (*)(bool))0x71B160)(true);
	if (!consoleManager) return;
	va_list args;
	va_start(args, fmt);
	((void (__thiscall*)(void*, const char*, va_list))0x71D0A0)(consoleManager, fmt, args);
	va_end(args);
}

typedef bool (*_QueueUIMessage)(const char* msg, UInt32 iconType, const char* iconPath, const char* soundPath, float displayTime, UInt8 unk);
_QueueUIMessage QueueUIMessage = (_QueueUIMessage)kAddr_QueueUIMessage;

inline BaseProcess* GetActorProcess(void* actor) {
	return *(BaseProcess**)((UInt8*)actor + 0x68);
}

inline UInt32 GetActorLifeState(void* actor) {
	return *(UInt32*)((UInt8*)actor + 0x108);
}

inline bool GetActorIsTeammate(void* actor) {
	return *(bool*)((UInt8*)actor + 0x18D);
}

inline bool IsPlayerInCombat() {
	void* pc = *(void**)kAddr_PlayerCharacter;
	if (!pc) return false;
	return *(bool*)((UInt8*)pc + 0xDF0);
}

bool IsActorRobot(void* actor) {
	TESForm* base = *(TESForm**)((UInt8*)actor + 0x20);
	if (base && base->typeID == kFormType_TESCreature) {
		TESCreature* creature = (TESCreature*)base;
		return creature->type == kCreatureType_Robot;
	}
	return false;
}

bool WriteMemoryChecked(UInt32 addr, const void* data, size_t size) {
	DWORD oldProtect = 0;
	if (!VirtualProtect((void*)addr, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
		return false;
	}

	memcpy((void*)addr, data, size);
	FlushInstructionCache(GetCurrentProcess(), (void*)addr, size);

	DWORD restoreProtect = 0;
	return VirtualProtect((void*)addr, size, oldProtect, &restoreProtect) != 0;
}

UInt32 DecodeRelTarget(UInt32 addr, const UInt8 bytes[5]) {
	SInt32 rel = 0;
	memcpy(&rel, bytes + 1, sizeof(rel));
	return addr + 5 + rel;
}

void BuildRelPatchBytes(UInt8 opcode, UInt32 addr, UInt32 target, UInt8 outBytes[5]) {
	outBytes[0] = opcode;
	const SInt32 rel = (SInt32)(target - addr - 5);
	memcpy(outBytes + 1, &rel, sizeof(rel));
}

bool WriteRelBranch(UInt32 addr, UInt8 opcode, UInt32 target) {
	UInt8 patchBytes[5] = {0};
	BuildRelPatchBytes(opcode, addr, target, patchBytes);
	return WriteMemoryChecked(addr, patchBytes, sizeof(patchBytes));
}

struct RelPatchSite {
	const char* name;
	UInt32 address;
	UInt8 opcode;
	UInt8 vanillaBytes[5];
	UInt32 target;
};

struct Write16PatchSite {
	const char* name;
	UInt32 address;
	UInt8 vanillaBytes[2];
	UInt16 desiredValue;
};

struct DListNode {
	DListNode* next;
	DListNode* prev;
	void* data;
};

struct String {
	char* m_data;
	UInt16 m_dataLen;
	UInt16 m_bufLen;
};

int StrCompareCI(const char* a, const char* b) {
	if (!a || !b) return a ? 1 : (b ? -1 : 0);
	while (*a && *b) {
		char ca = (*a >= 'A' && *a <= 'Z') ? (*a + 32) : *a;
		char cb = (*b >= 'A' && *b <= 'Z') ? (*b + 32) : *b;
		if (ca != cb) return ca - cb;
		++a;
		++b;
	}
	return *a - *b;
}

UInt32 GetTraitID(const char* name) {
	return ((UInt32(__cdecl*)(const char*, UInt32))0xA00940)(name, 0xFFFFFFFF);
}

struct Tile {
	void* vtbl;
	DListNode* childFirst;
	DListNode* childLast;
	UInt32 childCount;
	void* valuesData;
	UInt32 valuesSize;
	UInt32 valuesCapacity;
	UInt8 pad1C[4];
	String name;
	Tile* parent;
	void* node;
	UInt32 flags;
	UInt8 unk34;
	UInt8 unk35;
	UInt8 pad36[2];

	Tile* GetChild(const char* childName) {
		for (DListNode* node = childFirst; node; node = node->next) {
			Tile* child = (Tile*)node->data;
			if (child && child->name.m_data && StrCompareCI(child->name.m_data, childName) == 0) {
				return child;
			}
		}
		return nullptr;
	}

	void SetFloat(UInt32 id, float value) {
		((void(__thiscall*)(Tile*, UInt32, float, bool))0xA012D0)(this, id, value, true);
	}

	void SetString(UInt32 id, const char* str) {
		((void(__thiscall*)(Tile*, UInt32, const char*, bool))0xA01350)(this, id, str ? str : "", false);
	}
};

struct TileMenu : Tile {
	UInt32 unk38;
	void* menu;
};

struct Menu {
	void* vtbl;
	TileMenu* tile;

	Tile* AddTileFromTemplate(Tile* destTile, const char* templateName) {
		return ((Tile*(__thiscall*)(Menu*, Tile*, const char*, UInt32))0xA1DDB0)(this, destTile, templateName, 0);
	}
};

struct HUDMainMenu : Menu {
	static HUDMainMenu* Get() { return *(HUDMainMenu**)0x11D96C0; }
};

struct NiPoint3 {
	float x, y, z;
};

bool WorldToScreen(const NiPoint3* worldPos, NiPoint3* screenOut, int offscreenMode) {
	void* sceneGraph = *(void**)0x11DEB7C;
	if (!sceneGraph || !worldPos || !screenOut) return false;
	void* camera = *(void**)((UInt8*)sceneGraph + 0xAC);
	if (!camera) return false;

	float* mat = (float*)((UInt8*)camera + 0x9C);
	float* port = (float*)((UInt8*)camera + 0x100);

	float x = worldPos->x, y = worldPos->y, z = worldPos->z;
	float fW = x * mat[3 * 4 + 0] + y * mat[3 * 4 + 1] + z * mat[3 * 4 + 2] + mat[3 * 4 + 3];
	float fX = x * mat[0 * 4 + 0] + y * mat[0 * 4 + 1] + z * mat[0 * 4 + 2] + mat[0 * 4 + 3];
	float fY = x * mat[1 * 4 + 0] + y * mat[1 * 4 + 1] + z * mat[1 * 4 + 2] + mat[1 * 4 + 3];
	float fZ = x * mat[2 * 4 + 0] + y * mat[2 * 4 + 1] + z * mat[2 * 4 + 2] + mat[2 * 4 + 3];
	if (fW == 0.0f) return false;

	bool behindCamera = (fW <= 0.00001f);
	float invW = 1.0f / fW;
	if (behindCamera) invW = -invW;

	fX *= invW;
	fY *= invW;
	fZ *= invW;

	float halfW = (port[1] - port[0]) * 0.5f;
	float halfH = (port[2] - port[3]) * 0.5f;
	float centerX = (port[1] + port[0]) * 0.5f;
	float centerY = (port[2] + port[3]) * 0.5f;

	float screenX = fX * halfW + centerX;
	float screenY = port[2] - (fY * halfH + centerY);

	bool onScreen = (screenX >= port[0] && screenX <= port[1] &&
	                 screenY >= port[3] && screenY <= port[2] && !behindCamera);

	if (!onScreen && offscreenMode < 2) {
		float x2 = port[1] * 0.5f;
		float y2 = port[2] * 0.5f;
		float d = sqrtf((screenX - x2) * (screenX - x2) + (screenY - y2) * (screenY - y2));
		if (d > 0.0001f) {
			float r = y2 / d;
			screenX = r * screenX + (1.0f - r) * x2;
			screenY = r * screenY + (1.0f - r) * y2;
			if (offscreenMode == 0) {
				float sx = screenX - 0.5f;
				float sy = screenY - 0.5f;
				float divider = (sy * sy > 0.125f) ? fabsf(sy) : fabsf(sx);
				if (divider > 0.0001f) {
					screenX = 0.5f * (sx / divider + 1.0f);
					screenY = 0.5f * (sy / divider + 1.0f);
				}
			}
			screenX = (screenX < 0.0f) ? 0.0f : ((screenX > 1.0f) ? 1.0f : screenX);
			screenY = (screenY < 0.0f) ? 0.0f : ((screenY > 1.0f) ? 1.0f : screenY);
		}
	}

	screenOut->x = screenX;
	screenOut->y = screenY;
	screenOut->z = fZ;
	return onScreen;
}

namespace HardcoreCompanionRevive
{
	void InitHooks();  // Forward declaration

	TESForm* g_ScrapMetal = nullptr;
	char g_sRevivePrompt[] = "Revive";

		static UInt32 s_revivingCompanionRefID = 0;
		static UInt32 s_pendingHealthRestoreRefID = 0;
		static float s_pendingMaxHealth = 0.0f;
		static int s_damageDelayFrames = 0;
		static bool s_hooksInstalled = false;
		static bool s_killHookInstalled = false;
		static bool s_hooksInitPending = false;
		static Tile* s_downedMarkerTiles[16] = {0};
		static Tile* s_lastHUDRootTile = nullptr;
		static bool s_downedMarkerTraitsInit = false;
		static UInt32 s_traitMarkerX = 0;
		static UInt32 s_traitMarkerY = 0;
		static UInt32 s_traitMarkerVisible = 0;
		static UInt32 s_traitMarkerDistance = 0;
		static UInt32 s_traitMarkerText = 0;
		static UInt32 s_traitMarkerIcon = 0;
		static UInt32 s_traitMarkerHasIcon = 0;
		static UInt32 s_traitMarkerWidth = 0;
		static UInt32 s_traitMarkerHeight = 0;
		static UInt32 s_traitMarkerRadiusMax = 0;
		static UInt32 s_traitMarkerTextVisible = 0;
		static UInt32 s_traitMarkerDistanceVisible = 0;
		static const char* kDownedMarkerIconPath = "interface\\HardcoreCompanionRevive\\DownedMarker.dds";
		static const float kDownedMarkerWorldZOffset = 90.0f;
		static const float kDownedMarkerWidth = 58.0f;
		static const float kDownedMarkerHeight = 58.0f;
		static const float kDownedMarkerRadiusMax = 999.0f;

	struct UnconsciousEntry {
		UInt32 refID;
		float gameTimeDays;
	};
	static const int kMaxTrackedCompanions = 16;
	static UnconsciousEntry s_unconsciousCompanions[kMaxTrackedCompanions] = {0};

	float GetGameDaysPassed() {
		GameTimeGlobals* gtg = GameTimeGlobals::Get();
		if (gtg && gtg->daysPassed) {
			return gtg->daysPassed->data;
		}
		return 0.0f;
	}

	void TrackUnconsciousCompanion(UInt32 refID) {
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

	float GetUnconsciousGameTime(UInt32 refID) {
		for (int i = 0; i < kMaxTrackedCompanions; i++) {
			if (s_unconsciousCompanions[i].refID == refID) {
				return s_unconsciousCompanions[i].gameTimeDays;
			}
		}
		return -1.0f;
	}

	void UntrackCompanion(UInt32 refID) {
		for (int i = 0; i < kMaxTrackedCompanions; i++) {
			if (s_unconsciousCompanions[i].refID == refID) {
				s_unconsciousCompanions[i].refID = 0;
				s_unconsciousCompanions[i].gameTimeDays = 0.0f;
				return;
			}
		}
	}

	bool IsHardcoreMode() {
		void* pc = *(void**)kAddr_PlayerCharacter;
		if (!pc) return false;
		return *(bool*)((UInt8*)pc + 0x7BC);
	}

	SInt32 GetItemCount(void* actor, TESForm* form);

	const char* GetALCHName(TESForm* form) {
		if (!form || form->typeID != kFormType_ALCH) return nullptr;
		return *(const char**)((UInt8*)form + 0x34);
	}

	bool StrContainsCI(const char* haystack, const char* needle) {
		if (!haystack || !needle) return false;
		size_t needleLen = strlen(needle);
		for (const char* p = haystack; *p; p++) {
			if (_strnicmp(p, needle, needleLen) == 0) return true;
		}
		return false;
	}

	TESForm* FindStimpakInInventory(void* player) {
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

	TESForm* GetRevivalItem(void* actor, void* player, bool* outIsRobot) {
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

	SInt32 GetItemCount(void* actor, TESForm* form) {
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

	void RemoveItem(void* actor, TESForm* form) {
		void* vtbl = *(void**)actor;
		typedef void* (__thiscall* RemoveItemFn)(void*, TESForm*, void*, UInt32, bool, bool, void*, UInt32, UInt32, bool, bool);
		RemoveItemFn fn = (RemoveItemFn)*((UInt32*)vtbl + 0x5F);
		fn(actor, form, nullptr, 1, false, false, nullptr, 0, 0, true, false);
	}

	void PlayPickupPutdownSound(void* actor, TESForm* item) {
		((void(__thiscall*)(void*, TESForm*, bool, bool))0x8ADED0)(actor, item, false, true);
	}

	bool __fastcall OnActivateUnconsciousTeammate(void* actorParam) {
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

	const char* __cdecl GetRevivePromptIfApplicable(void* actor, UInt32 promptType) {
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
			// ECX is prompt type INDEX, not actor! Actor is at [ebp+8]
			mov eax, [ebp + 8]     // Get actual actor reference
			push ecx              // Push promptType (second arg) - cdecl is right-to-left
			push eax              // Push actor (first arg)
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

	int __fastcall GetLifeStateReturnAliveIfCompanion(void* actor) {
		if (IsHardcoreMode() && GetActorIsTeammate(actor)) {
			return kLifeState_Alive;
		}
		return GetActorLifeState(actor);
	}

	static bool s_killingCompanion = false;
	static void* s_forceKillActor = nullptr;

	bool __fastcall IsEssentialHook_Check(void* actor, void* edx) {
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

	void SetActorKillableViaJIP(void* actor, bool killable) {
		UInt8* jipFlags2 = (UInt8*)((UInt8*)actor + 0x106);
		if (killable) {
			*jipFlags2 |= kJIPFlag_TeammateKillable;
		} else {
			*jipFlags2 &= ~kJIPFlag_TeammateKillable;
		}
	}

	void ClearRuntimeEssentialFlag(void* actor) {
		UInt32* flags0140 = (UInt32*)((UInt8*)actor + 0x140);
		*flags0140 &= ~0x80000000;
	}

	void ClearBaseFormEssentialFlag(void* actor) {
		TESForm* baseForm = *(TESForm**)((UInt8*)actor + 0x20);
		if (baseForm) {
			UInt32* actorBaseFlags = (UInt32*)((UInt8*)baseForm + 0x34);
			*actorBaseFlags &= ~0x02;
		}
	}

	void KillCompanion(void* actor) {
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

		void CheckCompanionBleedout() {
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

		void ApplyPendingHealthRestore() {
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

		void ResetDownedMarkerTiles() {
			for (int i = 0; i < 16; i++) {
				s_downedMarkerTiles[i] = nullptr;
			}
		}

		void InitDownedMarkerTraits() {
			if (s_downedMarkerTraitsInit) {
				return;
			}
			s_traitMarkerX = GetTraitID("_X");
			s_traitMarkerY = GetTraitID("_Y");
			s_traitMarkerVisible = GetTraitID("_JVOInDistance");
			s_traitMarkerDistance = GetTraitID("_JVODistance");
			s_traitMarkerText = GetTraitID("_JVOText");
			s_traitMarkerIcon = GetTraitID("_JVOIcon");
			s_traitMarkerHasIcon = GetTraitID("_JVOHasIcon");
			s_traitMarkerWidth = GetTraitID("_JVOWidth");
			s_traitMarkerHeight = GetTraitID("_JVOHeight");
			s_traitMarkerRadiusMax = GetTraitID("_JVORadiusMax");
			s_traitMarkerTextVisible = GetTraitID("_JVOTextVisible");
			s_traitMarkerDistanceVisible = GetTraitID("_JVODistanceVisible");
			s_downedMarkerTraitsInit = true;
		}

		Tile* GetOrCreateDownedMarker(int index, HUDMainMenu* hud, Tile* jvoTile) {
			if (index < 0 || index >= 16 || !hud || !jvoTile) {
				return nullptr;
			}
			if (!s_downedMarkerTiles[index]) {
				Tile* marker = hud->AddTileFromTemplate(jvoTile, "JVOMarker");
				if (marker) {
					s_downedMarkerTiles[index] = marker;
				}
			}
			return s_downedMarkerTiles[index];
		}

		void HideDownedMarker(Tile* tile) {
			if (!tile) {
				return;
			}
			tile->SetFloat(s_traitMarkerVisible, 0.0f);
		}

		bool TryGetActorWorldPos(void* actor, NiPoint3* outWorldPos) {
			if (!actor || !outWorldPos) {
				return false;
			}

			UInt32 vtbl = *(UInt32*)actor;
			UInt32 refID = ((TESForm*)actor)->refID;
			if (vtbl < 0x1000000 || vtbl > 0x1200000 || refID == 0) {
				return false;
			}

			outWorldPos->x = *(float*)((UInt8*)actor + 0x30);
			outWorldPos->y = *(float*)((UInt8*)actor + 0x34);
			outWorldPos->z = *(float*)((UInt8*)actor + 0x38) + kDownedMarkerWorldZOffset;
			return true;
		}

		const char* GetCompanionName(void* actor) {
			if (!actor) {
				return "Companion";
			}

			TESForm* baseForm = *(TESForm**)((UInt8*)actor + 0x20);
			if (!baseForm) {
				return "Companion";
			}

			const UInt8 typeID = baseForm->typeID;
			//TESNPC/TESCreature/TESLevCreature/TESLevCharacter all carry TESFullName at +0xD0
			if (typeID == 0x2A || typeID == 0x2B || typeID == 0x2C || typeID == 0x2D) {
				const char* name = *(const char**)((UInt8*)baseForm + 0xD4);
				const UInt16 nameLen = *(UInt16*)((UInt8*)baseForm + 0xD8);
				if (name && nameLen > 0 && name[0]) {
					return name;
				}
			}

			return "Companion";
		}

		void UpdateDownedCompanionMarkers() {
			void* pc = *(void**)kAddr_PlayerCharacter;
			if (!pc) {
				return;
			}

			HUDMainMenu* hud = HUDMainMenu::Get();
			if (!hud || !hud->tile) {
				s_lastHUDRootTile = nullptr;
				ResetDownedMarkerTiles();
				return;
			}

			InitDownedMarkerTraits();

			Tile* rootTile = (Tile*)hud->tile;
			if (s_lastHUDRootTile != rootTile) {
				s_lastHUDRootTile = rootTile;
				ResetDownedMarkerTiles();
			}

			Tile* jvoTile = rootTile->GetChild("JVO");
			if (!jvoTile) {
				return;
			}

			if (!Settings::bShowIcon) {
				for (int i = 0; i < 16; i++) {
					HideDownedMarker(s_downedMarkerTiles[i]);
				}
				return;
			}

			struct TeammateNode {
				void* actor;
				TeammateNode* next;
			};
			TeammateNode* teammates = (TeammateNode*)((UInt8*)pc + 0x5FC);
			int markerIndex = 0;

			while (teammates && markerIndex < 16) {
				void* actor = teammates->actor;
				if (actor) {
					if (GetActorIsTeammate(actor) && GetActorLifeState(actor) == kLifeState_EssentialUnconscious) {
						NiPoint3 worldPos = {0.0f, 0.0f, 0.0f};
						if (TryGetActorWorldPos(actor, &worldPos)) {
							NiPoint3 screenPos = {0.0f, 0.0f, 0.0f};
							WorldToScreen(&worldPos, &screenPos, 0);

							Tile* marker = GetOrCreateDownedMarker(markerIndex, hud, jvoTile);
							if (marker) {
								const char* companionName = GetCompanionName(actor);
								marker->SetFloat(s_traitMarkerX, screenPos.x);
								marker->SetFloat(s_traitMarkerY, screenPos.y);
								marker->SetFloat(s_traitMarkerVisible, 1.0f);
								marker->SetFloat(s_traitMarkerWidth, kDownedMarkerWidth);
								marker->SetFloat(s_traitMarkerHeight, kDownedMarkerHeight);
								marker->SetFloat(s_traitMarkerRadiusMax, kDownedMarkerRadiusMax);
								marker->SetFloat(s_traitMarkerTextVisible, 2.0f);
								marker->SetFloat(s_traitMarkerDistanceVisible, 0.0f);
								marker->SetString(s_traitMarkerDistance, "");
								marker->SetString(s_traitMarkerText, companionName);
								marker->SetString(s_traitMarkerIcon, kDownedMarkerIconPath);
								marker->SetFloat(s_traitMarkerHasIcon, 1.0f);
							}
							markerIndex++;
						}
					}
				}
				teammates = teammates->next;
			}

			for (int i = markerIndex; i < 16; i++) {
				HideDownedMarker(s_downedMarkerTiles[i]);
			}
		}
	
		bool IsJIPInstalled() {
		return *(UInt8*)0x87F427 == 0xE9; //JIP hooks IsEssential here
	}

	void CheckJIPDependency() {
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

	void ReportPatchConflict(const char* siteName, UInt32 address, const UInt8* foundBytes, size_t byteCount) {
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

	void ReportPatchWriteFailure(const char* siteName, UInt32 address) {
		Console_Print("HardcoreCompanionRevive failed to patch %s at 0x%08X", siteName, address);
	}

	PatchCheckResult CheckRelSite(const RelPatchSite& site, UInt8 current[5]) {
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

	PatchCheckResult CheckWrite16Site(const Write16PatchSite& site, UInt8 current[2]) {
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

	bool PrecheckRelSites(const RelPatchSite* sites, size_t count) {
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

	bool ApplyRelSites(const RelPatchSite* sites, size_t count) {
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

	bool ApplyWrite16SiteIfVanilla(const Write16PatchSite& site) {
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

	static bool s_scriptCompiled = false;

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
			s_lastHUDRootTile = nullptr;
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

	BaseProcess* __fastcall OnKillEssentialSetDownTimerIfCompanion(void* actor) {
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
}

extern "C" __declspec(dllexport) bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info) {
	info->infoVersion = PluginInfo::kInfoVersion;
	info->name = "HardcoreCompanionReviveNVSE";
	info->version = PLUGIN_VERSION;

	if (nvse->isEditor) {
		return false;
	}

	if (nvse->nvseVersion < 0x06020300) {
		return false;
	}

	if (nvse->runtimeVersion < 0x040020D0) {
		return false;
	}

	return true;
}

extern "C" __declspec(dllexport) bool NVSEPlugin_Load(const NVSEInterface* nvse) {
	g_pluginHandle = nvse->GetPluginHandle();

	Settings::LoadINI();

	g_messagingInterface = (NVSEMessagingInterface*)nvse->QueryInterface(NVSEInterface::kInterface_Messaging);
	if (g_messagingInterface) {
		g_messagingInterface->RegisterListener(g_pluginHandle, "NVSE", HardcoreCompanionRevive::MessageHandler);
	}

	g_scriptInterface = (NVSEScriptInterface*)nvse->QueryInterface(NVSEInterface::kInterface_Script);

	return true;
}
