#pragma once
#include "Types.hpp"

const UInt32 kAddr_PlayerCharacter = 0x011DEA3C;
const UInt32 kAddr_BGSDefaultObjectManager = 0x11CA80C;
const UInt32 kAddr_LookupFormByID = 0x4839C0;
const UInt32 kAddr_GameModActorValueF = 0x881130;

const UInt32 kFormType_TESCreature = 0x2B;
const UInt32 kFormType_ALCH = 0x2F;
const UInt32 kLifeState_EssentialUnconscious = 0x6;
const UInt32 kLifeState_Alive = 0x0;
const UInt32 kAVCode_Health = 16;
const UInt32 kCreatureType_Robot = 6;
const UInt32 kFormID_ScrapMetal = 0x31944;

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

inline TESForm* LookupFormByID(UInt32 id) {
	return ((TESForm*(*)(UInt32))kAddr_LookupFormByID)(id);
}
