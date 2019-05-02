/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "fsys/filesystem.h"
#include "fcaseopen.h"
#include <cstring>

VData::VData(std::string name)
{
	m_name = name;
}
bool VData::IsFile() {return false;}
bool VData::IsDirectory() {return false;}
std::string VData::GetName() {return m_name;}

///////////////////////////

VFile::VFile(const std::string &name,const std::shared_ptr<std::vector<uint8_t>> &data)
	: VData(name),m_data(data)
{}
bool VFile::IsFile() {return true;}
unsigned long long VFile::GetSize() {return m_data->size();}
std::shared_ptr<std::vector<uint8_t>> VFile::GetData() const {return m_data;}

///////////////////////////

VDirectory::VDirectory(std::string name)
	: VData(name)
{}
VDirectory::VDirectory()
	: VData("root")
{}
VDirectory::~VDirectory()
{
	for(unsigned int i=0;i<m_files.size();i++)
		delete m_files[i];
}
std::vector<VData*> &VDirectory::GetFiles() {return m_files;}
bool VDirectory::IsDirectory() {return true;}
void VDirectory::Add(VData *file) {m_files.push_back(file);}
void VDirectory::Remove(VData *file)
{
	auto it = std::find(m_files.begin(),m_files.end(),file);
	if(it == m_files.end())
		return;
	m_files.erase(it);
	delete file;
}

///////////////////////////

#define IsEOF(c) (c == EOF || Eof())

VFilePtrInternal::VFilePtrInternal()
	: m_bRead(false),m_bBinary(false)
{}
VFilePtrInternal::~VFilePtrInternal()
{}
bool VFilePtrInternal::ShouldRemoveComments() {return (m_bRead == true && m_bBinary == false) ? true : false;}
unsigned char VFilePtrInternal::GetType() {return m_type;}
size_t VFilePtrInternal::Read(void*,size_t) {return 0;}
size_t VFilePtrInternal::Read(void *ptr,size_t size,size_t nmemb) {return Read(ptr,size *nmemb);}
unsigned long long VFilePtrInternal::Tell() {return 0;}
void VFilePtrInternal::Seek(unsigned long long) {}
void VFilePtrInternal::Seek(unsigned long long offset,int whence)
{
	switch(whence)
	{
	case SEEK_CUR:
		return Seek(Tell() +offset);
	case SEEK_END:
		return Seek(GetSize() +offset);
	}
	return Seek(offset);
}
int VFilePtrInternal::Eof() {return 0;}
int VFilePtrInternal::ReadChar() {return 0;}
unsigned long long VFilePtrInternal::GetSize() {return 0;}
unsigned int VFilePtrInternal::GetFlags()
{
	unsigned int flags = 0;
	switch(m_type)
	{
	case VFILE_VIRTUAL:
		flags |= (FVFILE_READONLY | FVFILE_VIRTUAL);
		break;
	case VFILE_LOCAL:
		break;
	case VFILE_PACKAGE:
		flags |= (FVFILE_READONLY | FVFILE_PACKAGE);
		break;
	}
	return flags;
}
bool VFilePtrInternal::RemoveComments(unsigned char &c,bool bRemoveComments)
{
	if(bRemoveComments == true)
	{
		unsigned long long p = Tell();
		for(unsigned int i=0;i<m_comments.size();i++)
		{
			Comment &comment = m_comments[i];
			if(c == comment.start[0])
			{
				Seek(p -1);
				auto lenStart = comment.start.length();
				char *in = new char[lenStart +1];
				Read(&in[0],comment.start.length());
				in[lenStart] = '\0';
				if(strcmp(in,comment.start.c_str()) == 0)
				{
					delete[] in;
					char c2;
					while(!Eof())
					{
						Read(&c2,1);
						if(c2 == comment.end[0])
						{
							Seek(Tell() -1);
							auto lenEnd = comment.end.length();
							char *in = new char[lenEnd +1];
							Read(&in[0],lenEnd);
							in[lenEnd] = '\0';
							if(strcmp(in,comment.end.c_str()) == 0)
							{
								c = Read<char>();
								RemoveComments(c,bRemoveComments);
								return true;
							}
							delete[] in;
						}
					}
					return true;
				}
				else
				{
					delete[] in;
					Seek(p);
				}
			}
		}
	}
	return false;
}
std::string VFilePtrInternal::ReadString()
{
	unsigned char c;
	std::string name = "";
	c = Read<char>();
	bool bRemoveComments = ShouldRemoveComments();
	while(c != '\0' && !IsEOF(c) && !Eof())
	{
		if(RemoveComments(c,bRemoveComments) && IsEOF(c) && !Eof())
			break;
		name += c;
		c = Read<char>();
	}
	return name;
}
std::string VFilePtrInternal::ReadLine()
{
	unsigned char c;
	std::string name = "";
	c = Read<char>();
	if(c == '\n')
	{
		name += c;
		return name;
	}
	bool bRemoveComments = ShouldRemoveComments();
	while(c != '\0' && !IsEOF(c))
	{
		if(RemoveComments(c,bRemoveComments) && IsEOF(c))
			break;
		name += c;
		c = Read<char>();
		if(Eof() || c == '\n')
			break;
	}
	return name;
}
char *VFilePtrInternal::ReadString(char *str,int num)
{
	if(Eof())
		return NULL;
	int i=0;
	for(i=0;i<num;i++)
	{
		char c = Read<char>();
		if(IsEOF(c))
			return NULL;
		str[i] = c;
		if(str[i] == '\n')
		{
			if(i +1 < num)
				str[i +1] = '\0';
			else
				return NULL;
			return str;
		}
	}
	return NULL;
}
unsigned long long VFilePtrInternal::FindFirstOf(const char *s)
{
	if(Eof())
		return static_cast<unsigned long long>(EOF);
	unsigned char c;
	int cur = 0;
	bool bRemoveComments = ShouldRemoveComments();
	do
	{
		c = static_cast<unsigned char>(ReadChar());
		if(!IsEOF(c) && (!RemoveComments(c,bRemoveComments) || !IsEOF(c)))
		{
			while(s[cur] != '\0')
			{
				if(c == s[cur])
					return c;
				cur++;
			}
		}
	}
	while(!IsEOF(c));
	return static_cast<unsigned long long>(EOF);
}
unsigned long long VFilePtrInternal::FindFirstNotOf(const char *s)
{
	if(Eof())
		return static_cast<unsigned long long>(EOF);
	unsigned char c;
	int cur = 0;
	bool bRemoveComments = ShouldRemoveComments();
	do
	{
		c = static_cast<unsigned char>(ReadChar());
		if(!IsEOF(c) && (!RemoveComments(c,bRemoveComments) || !IsEOF(c)))
		{
			bool ret = true;
			while(s[cur] != '\0')
			{
				if(c == s[cur])
				{
					ret = false;
					break;
				}
				cur++;
			}
			if(ret)
				return c;
			cur = 0;
		}
	}
	while(!IsEOF(c));
	return static_cast<unsigned long long>(EOF);
}
std::string VFilePtrInternal::ReadUntil(const char *s)
{
	std::string ret = "";
	if(Eof())
		return ret;
	unsigned char c;
	int cur = 0;
	bool bRemoveComments = ShouldRemoveComments();
	do
	{
		c = static_cast<unsigned char>(ReadChar());
		if(!IsEOF(c) && (!RemoveComments(c,bRemoveComments) || !IsEOF(c)))
		{
			while(s[cur] != '\0')
			{
				if(c == s[cur])
				{
					Seek(Tell() -1);
					return ret;
				}
				cur++;
			}
			cur = 0;
			ret += char(c);
		}
	}
	while(!IsEOF(c));
	return ret;
}

unsigned long long VFilePtrInternal::FindFirstOf(char c)
{
	char s[2] = {
		c,
		'\0'
	};
	return FindFirstOf(s);
}
unsigned long long VFilePtrInternal::FindFirstNotOf(char c)
{
	char s[2] = {
		c,
		'\0'
	};
	return FindFirstNotOf(s);
}
std::string VFilePtrInternal::ReadUntil(char c)
{
	char s[2] = {
		c,
		'\0'
	};
	return ReadUntil(s);
}
unsigned long long VFilePtrInternal::FindFirstOf(std::string s) {return FindFirstOf(s.c_str());}
unsigned long long VFilePtrInternal::FindFirstNotOf(std::string s) {return FindFirstNotOf(s.c_str());}
std::string VFilePtrInternal::ReadUntil(std::string s) {return ReadUntil(s.c_str());}

unsigned long long VFilePtrInternal::Find(const char *s,bool bIgnoreCase)
{
	if(Eof() || s[0] == '\0')
		return static_cast<unsigned long long>(EOF);
	unsigned char c;
	int cur = 0;
	char *cpy;
	if(bIgnoreCase)
	{
		auto sz = strlen(s) +1;
		cpy = new char[sz];
#ifdef _WIN32
		strcpy_s(cpy,sz,s);
#else
		strcpy(cpy,s);
#endif
		int lw = 0;
		while(cpy[lw] != '\0')
		{
			cpy[lw] = static_cast<char>(tolower(cpy[lw]));
			lw++;
		}
	}
	else
		cpy = const_cast<char*>(s);
	bool bRemoveComments = ShouldRemoveComments();
	do
	{
		c = static_cast<char>(ReadChar());
		if(!IsEOF(c) && (!RemoveComments(c,bRemoveComments) || !IsEOF(c)))
		{
			while((!bIgnoreCase && c == cpy[cur]) || (bIgnoreCase && tolower(c) == cpy[cur]))
			{
				cur++;
				if(s[cur] == '\0')
				{
					Seek(Tell() -cur);
					if(bIgnoreCase)
						delete[] cpy;
					return Tell();
				}
				c = static_cast<char>(ReadChar());
			}
			cur = 0;
		}
	}
	while(!IsEOF(c));
	if(bIgnoreCase)
		delete[] cpy;
	return static_cast<unsigned long long>(EOF);
}
void VFilePtrInternal::IgnoreComments(std::string start,std::string end) {if(start.empty()) return; if(end.empty()) end = "\n"; m_comments.push_back(Comment(start,end));}

///////////////////////////

VFilePtrInternalVirtual::VFilePtrInternalVirtual(VFile *file)
	: VFilePtrInternal()
{
	m_offset = 0;
	m_type = VFILE_VIRTUAL;
	m_file = file;
}
VFilePtrInternalVirtual::~VFilePtrInternalVirtual() {}
unsigned long long VFilePtrInternalVirtual::GetSize() {return m_file->GetSize();}
std::shared_ptr<std::vector<uint8_t>> VFilePtrInternalVirtual::GetData() const {return m_file->GetData();}

///////////////////////////

VFilePtrInternalReal::VFilePtrInternalReal()
	: VFilePtrInternal()
{
	VFilePtrInternal();
	m_type = VFILE_LOCAL;
	m_file = NULL;
	m_size = 0;
	m_path = "";
}
VFilePtrInternalReal::~VFilePtrInternalReal()
{
	if(m_file != NULL)
		fclose(m_file);
}
const std::string &VFilePtrInternalReal::GetPath() const {return m_path;}
bool VFilePtrInternalReal::Construct(const char *path,const char *mode)
{
	std::string sPath = path;
	std::replace(sPath.begin(),sPath.end(),'\\','/');
	m_file = fcaseopen(sPath.c_str(),mode);
	if(m_file == NULL)
		return false;
	m_path = sPath.c_str();
	long long cur = ftell(m_file);
	fseek(m_file,0,SEEK_END);
	m_size = ftell(m_file);
	fseek(m_file,static_cast<long>(cur),SEEK_SET);
	return true;
}

unsigned long long VFilePtrInternalReal::GetSize() {return m_size;}
bool VFilePtrInternalReal::ReOpen(const char *mode)
{
#ifdef _WIN32
	freopen_s(&m_file,m_path.c_str(),mode,m_file);
#else
	m_file = freopen(m_path.c_str(),mode,m_file);
#endif
	m_bBinary = FileManager::IsBinaryMode(mode);
	m_bRead = !FileManager::IsWriteMode(mode);
	if(m_file == NULL)
		return false;
	return true;
}
