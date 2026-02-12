#pragma once
#include "NVSEInterface.hpp"

namespace HardcoreCompanionRevive {
	void InitHooks();
	void __cdecl MessageHandler(NVSEMessagingInterface::Message* msg);
}
