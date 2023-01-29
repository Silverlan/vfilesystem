/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
