#pragma once
#include "TESForm.hpp"

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

inline bool IsActorRobot(void* actor) {
	TESForm* base = *(TESForm**)((UInt8*)actor + 0x20);
	if (base && base->typeID == kFormType_TESCreature) {
		TESCreature* creature = (TESCreature*)base;
		return creature->type == kCreatureType_Robot;
	}
	return false;
}
