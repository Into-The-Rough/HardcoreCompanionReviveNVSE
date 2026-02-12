#pragma once
#include "Types.hpp"

const UInt32 kAddr_BSWin32Audio = 0x11F6D98;
const UInt32 kAudioFlags_2D = 1;
const UInt32 kAudioFlags_100 = 0x100;
const UInt32 kAudioFlags_SystemSound = 0x200000;

inline const char* kUIRepairWeaponSound = (const char*)0x1075D38;

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

inline void PlayGameSound(const char* soundEditorID) {
	Sound sound;
	sound.Init(soundEditorID, kAudioFlags_100 | kAudioFlags_SystemSound | kAudioFlags_2D);
	sound.Play();
}
