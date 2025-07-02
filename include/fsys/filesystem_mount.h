// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __FILESYSTEM_MOUNT_H__
#define __FILESYSTEM_MOUNT_H__

#include <string>
#include <vector>
#include <fsys/filesystem.h>

struct MountDirectory {
	MountDirectory(const std::string &dir, bool absolutePath, fsys::SearchFlags search);
	std::string directory;
	fsys::SearchFlags searchMode;
	bool absolutePath;
};

class FileManager;
class MountIterator {
  public:
	friend FileManager;
  private:
	std::vector<MountDirectory> *m_directories;
	size_t m_index;
  protected:
	MountIterator(std::vector<MountDirectory> &directories);
	void operator++();
	bool IsValid();
	bool GetNextDirectory(std::string &outDir, fsys::SearchFlags includeFlags, fsys::SearchFlags excludeFlags, bool &bAbsolute);
};

#endif
