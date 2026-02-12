#pragma once
#include <windows.h>
#include <cstring>
#include <cstdlib>

namespace Settings {
	inline bool bShowMessage = true;
	inline bool bShowIcon = true;
	inline bool bAllowInCombat = true;
	inline bool bAllowDeath = false;
	inline float fBleedoutTime = 1.0f;

	inline char g_iniPath[MAX_PATH];

	inline void LoadINI() {
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
