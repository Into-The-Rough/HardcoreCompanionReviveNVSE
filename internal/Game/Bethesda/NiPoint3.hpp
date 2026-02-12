#pragma once
#include "Types.hpp"
#include <cmath>

struct NiPoint3 {
	float x, y, z;
};

inline bool WorldToScreen(const NiPoint3* worldPos, NiPoint3* screenOut, int offscreenMode) {
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
