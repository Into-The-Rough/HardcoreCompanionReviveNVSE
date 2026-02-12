#pragma once
#include "Types.hpp"
#include <windows.h>
#include <cstring>

inline bool WriteMemoryChecked(UInt32 addr, const void* data, size_t size) {
	DWORD oldProtect = 0;
	if (!VirtualProtect((void*)addr, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
		return false;
	}

	memcpy((void*)addr, data, size);
	FlushInstructionCache(GetCurrentProcess(), (void*)addr, size);

	DWORD restoreProtect = 0;
	return VirtualProtect((void*)addr, size, oldProtect, &restoreProtect) != 0;
}

inline UInt32 DecodeRelTarget(UInt32 addr, const UInt8 bytes[5]) {
	SInt32 rel = 0;
	memcpy(&rel, bytes + 1, sizeof(rel));
	return addr + 5 + rel;
}

inline void BuildRelPatchBytes(UInt8 opcode, UInt32 addr, UInt32 target, UInt8 outBytes[5]) {
	outBytes[0] = opcode;
	const SInt32 rel = (SInt32)(target - addr - 5);
	memcpy(outBytes + 1, &rel, sizeof(rel));
}

inline bool WriteRelBranch(UInt32 addr, UInt8 opcode, UInt32 target) {
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
