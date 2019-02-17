/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FSYS_DEFINITIONS_HPP__
#define __FSYS_DEFINITIONS_HPP__

#ifdef VFILESYSTEM_STATIC
	#define DLLFSYSTEM
#elif VFILESYSTEM_DLL
	#ifdef __linux__
		#define DLLFSYSTEM __attribute__((visibility("default")))
	#else
		#define DLLFSYSTEM  __declspec(dllexport)   // export DLL information
	#endif
#else
	#ifdef __linux__
		#define DLLFSYSTEM
	#else
		#define DLLFSYSTEM  __declspec(dllimport)   // import DLL information
	#endif
#endif

#endif
