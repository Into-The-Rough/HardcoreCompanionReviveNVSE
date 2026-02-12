#pragma once
#include "Types.hpp"
#include <cstdarg>

const UInt32 kAddr_QueueUIMessage = 0x7052F0;

inline void Console_Print(const char* fmt, ...) {
	void* consoleManager = ((void* (*)(bool))0x71B160)(true);
	if (!consoleManager) return;
	va_list args;
	va_start(args, fmt);
	((void (__thiscall*)(void*, const char*, va_list))0x71D0A0)(consoleManager, fmt, args);
	va_end(args);
}

typedef bool (*_QueueUIMessage)(const char* msg, UInt32 iconType, const char* iconPath, const char* soundPath, float displayTime, UInt8 unk);
inline _QueueUIMessage QueueUIMessage = (_QueueUIMessage)kAddr_QueueUIMessage;
