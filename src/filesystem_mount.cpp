/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "fsys/filesystem_mount.h"
#include "fsys/filesystem.h"

MountDirectory::MountDirectory(const std::string &dir,bool _absolutePath,fsys::SearchFlags search)
	: directory(dir),searchMode(search),absolutePath(_absolutePath)
{}

////////////////////////////////

MountIterator::MountIterator(std::vector<MountDirectory> &directories)
	: m_directories(&directories),m_index(0)
{}

void MountIterator::operator++()
{
	m_index++;
}

bool MountIterator::IsValid() {return (m_index > m_directories->size()) ? false : true;}

bool MountIterator::GetNextDirectory(std::string &outDir,fsys::SearchFlags includeFlags,fsys::SearchFlags excludeFlags,bool &bAbsolute)
{
	bAbsolute = false;
	if(!IsValid())
		return false;
	if((includeFlags &fsys::SearchFlags::NoMounts) != fsys::SearchFlags::None)
		m_index = m_directories->size();
	if(m_index == m_directories->size())
	{
		m_index++;
		outDir = '.';

		return true;
	}
	MountDirectory &dir = (*m_directories)[m_index];
	if((includeFlags &dir.searchMode) == fsys::SearchFlags::None || (excludeFlags &dir.searchMode) != fsys::SearchFlags::None)
	{
		m_index++;
		return GetNextDirectory(outDir,includeFlags,excludeFlags,bAbsolute);
	}
	outDir = dir.directory;
	bAbsolute = dir.absolutePath;
	m_index++;
	return true;
}