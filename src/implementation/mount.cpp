// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

module pragma.filesystem;

import :mount;

pragma::filesystem::MountDirectory::MountDirectory(const std::string &dir, bool _absolutePath, SearchFlags search) : directory(dir), searchMode(search), absolutePath(_absolutePath) {}

////////////////////////////////

pragma::filesystem::MountIterator::MountIterator(std::vector<MountDirectory> &directories) : m_directories(&directories), m_index(0) {}

void pragma::filesystem::MountIterator::operator++() { m_index++; }

bool pragma::filesystem::MountIterator::IsValid() { return (m_index > m_directories->size()) ? false : true; }

bool pragma::filesystem::MountIterator::GetNextDirectory(std::string &outDir, SearchFlags includeFlags, SearchFlags excludeFlags, bool &bAbsolute)
{
	bAbsolute = false;
	if(!IsValid())
		return false;
	if((includeFlags & SearchFlags::NoMounts) != SearchFlags::None)
		m_index = m_directories->size();
	if(m_index == m_directories->size()) {
		m_index++;
		outDir = '.';

		return true;
	}
	MountDirectory &dir = (*m_directories)[m_index];
	if((includeFlags & dir.searchMode) == SearchFlags::None || (excludeFlags & dir.searchMode) != SearchFlags::None) {
		m_index++;
		return GetNextDirectory(outDir, includeFlags, excludeFlags, bAbsolute);
	}
	outDir = dir.directory;
	bAbsolute = dir.absolutePath;
	m_index++;
	return true;
}
