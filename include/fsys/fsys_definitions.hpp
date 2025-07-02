// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __FSYS_DEFINITIONS_HPP__
#define __FSYS_DEFINITIONS_HPP__

#ifdef VFILESYSTEM_STATIC
#define DLLFSYSTEM
#elif VFILESYSTEM_DLL
#ifdef __linux__
#define DLLFSYSTEM __attribute__((visibility("default")))
#else
#define DLLFSYSTEM __declspec(dllexport) // export DLL information
#endif
#else
#ifdef __linux__
#define DLLFSYSTEM
#else
#define DLLFSYSTEM __declspec(dllimport) // import DLL information
#endif
#endif

#endif
