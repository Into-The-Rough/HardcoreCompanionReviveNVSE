#include "HardcoreCompanionRevive.hpp"
#include "HCR-Settings.hpp"
#include "NVSEInterface.hpp"
#include "Types.hpp"

static PluginHandle g_pluginHandle = kPluginHandle_Invalid;
static NVSEMessagingInterface* g_messagingInterface = nullptr;
NVSEScriptInterface* g_scriptInterface = nullptr;

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
