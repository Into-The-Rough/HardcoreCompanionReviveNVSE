#pragma once
#include "Types.hpp"

struct TESForm;
struct TESObjectREFR;
struct Actor;
struct PlayerCharacter;
struct BaseProcess;
struct NVSEInterface;
struct PluginInfo;
struct Setting;
struct NVSEMessagingInterface;

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

extern NVSEScriptInterface* g_scriptInterface;

const UInt32 kMessage_PostLoad = 0;
const UInt32 kMessage_ExitToMainMenu = 2;
const UInt32 kMessage_PostLoadGame = 9;
const UInt32 kMessage_NewGame = 14;
const UInt32 kMessage_MainGameLoop = 20;
const UInt32 kMessage_ReloadConfig = 25;

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
