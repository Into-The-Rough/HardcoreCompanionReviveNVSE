#include "HCR-DownedMarkers.hpp"
#include "HCR-Settings.hpp"
#include "TESForm.hpp"
#include "Actor.hpp"
#include "Tile.hpp"
#include "NiPoint3.hpp"

namespace HardcoreCompanionRevive
{
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

	void ResetDownedMarkerTiles() {
		for (int i = 0; i < 16; i++) {
			s_downedMarkerTiles[i] = nullptr;
		}
	}

	static void InitDownedMarkerTraits() {
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

	static Tile* GetOrCreateDownedMarker(int index, HUDMainMenu* hud, Tile* jvoTile) {
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

	static void HideDownedMarker(Tile* tile) {
		if (!tile) {
			return;
		}
		tile->SetFloat(s_traitMarkerVisible, 0.0f);
	}

	static bool TryGetActorWorldPos(void* actor, NiPoint3* outWorldPos) {
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

	static const char* GetCompanionName(void* actor) {
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
}
