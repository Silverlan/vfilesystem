// SPDX-FileCopyrightText: (c) 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include <cstdio>

module pragma.filesystem;

import :file_interface;

pragma::filesystem::File::File(const VFilePtr &f) : m_file {f} {}
size_t pragma::filesystem::File::Read(void *data, size_t size) { return m_file->Read(data, size); }
size_t pragma::filesystem::File::Write(const void *data, size_t size)
{
	auto type = m_file->GetType();
	if(type != EVFile::Local)
		return 0;
	return static_cast<fs::VFilePtrInternalReal *>(m_file.get())->Write(data, size);
}
bool pragma::filesystem::File::Eof() { return m_file->Eof(); }
size_t pragma::filesystem::File::GetSize() { return m_file->GetSize(); }
size_t pragma::filesystem::File::Tell() { return m_file->Tell(); }
void pragma::filesystem::File::Seek(size_t offset, Whence whence)
{
	switch(whence) {
	case Whence::Cur:
		return m_file->Seek(offset, SEEK_CUR);
	case Whence::End:
		return m_file->Seek(offset, SEEK_END);
	default:
		break;
	}
	return m_file->Seek(offset, SEEK_SET);
}
int32_t pragma::filesystem::File::ReadChar() { return m_file->ReadChar(); }
std::optional<std::string> pragma::filesystem::File::GetFileName() const
{
	auto *f = dynamic_cast<fs::VFilePtrInternalReal *>(m_file.get());
	if(!f)
		return {};
	return f->GetPath();
}
