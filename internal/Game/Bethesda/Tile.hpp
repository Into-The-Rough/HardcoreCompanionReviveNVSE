#pragma once
#include "Types.hpp"

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

inline int StrCompareCI(const char* a, const char* b) {
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

inline UInt32 GetTraitID(const char* name) {
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
