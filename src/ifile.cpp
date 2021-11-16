/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "fsys/ifile.hpp"

fsys::File::File(const ::VFilePtr &f)
	: m_file{f}
{}
size_t fsys::File::Read(void *data,size_t size) {return m_file->Read(data,size);}
size_t fsys::File::Write(const void *data,size_t size)
{
	auto type = m_file->GetType();
	if(type != VFILE_LOCAL)
		return 0;
	return static_cast<VFilePtrInternalReal*>(m_file.get())->Write(data,size);
}
bool fsys::File::Eof() {return m_file->Eof();}
size_t fsys::File::GetSize() {return m_file->GetSize();}
size_t fsys::File::Tell() {return m_file->Tell();}
void fsys::File::Seek(size_t offset,Whence whence)
{
	switch(whence)
	{
	case Whence::Cur:
		return m_file->Seek(offset,SEEK_CUR);
	case Whence::End:
		return m_file->Seek(offset,SEEK_END);
	}
	return m_file->Seek(offset,SEEK_SET);
}
int32_t fsys::File::ReadChar() {return m_file->ReadChar();}
