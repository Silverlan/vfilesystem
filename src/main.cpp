// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "fsys/filesystem.h"
#include <iostream>
#ifndef __linux__
#include <Windows.h>
#endif

#define COMPILE_EXECUTABLE 0

#if COMPILE_EXECUTABLE == 0
#ifdef __linux__
class InitAndRelease {
  public:
	InitAndRelease() {}
	~InitAndRelease() { FileManager::Close(); }
} initAndRelease __attribute__((visibility("default")));
#else
BOOLEAN WINAPI DllMain(IN HINSTANCE, IN DWORD nReason, IN LPVOID)
{
	switch(nReason) {
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		FileManager::Close();
		break;
	}
	return TRUE;
}
#endif
#endif
