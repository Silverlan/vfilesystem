/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FILESYSTEM_MOUNT_H__
#define __FILESYSTEM_MOUNT_H__

#include <string>
#include <vector>
#include "filesystem.h"

struct MountDirectory
{
	MountDirectory(const std::string &dir,bool absolutePath,fsys::SearchFlags search);
	std::string directory;
	fsys::SearchFlags searchMode;
	bool absolutePath;
};

class FileManager;
class MountIterator
{
public:
	friend FileManager;
private:
	std::vector<MountDirectory> *m_directories;
	size_t m_index;
protected:
	MountIterator(std::vector<MountDirectory> &directories);
	void operator++();
	bool IsValid();
	bool GetNextDirectory(std::string &outDir,fsys::SearchFlags includeFlags,fsys::SearchFlags excludeFlags,bool &bAbsolute);
};

#endif
