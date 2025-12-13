// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include <cstdio>
#include <cerrno>
#ifdef __linux__
#include <sys/stat.h>
#elif _WIN32
#include <Windows.h>
#endif

module pragma.filesystem;

#ifdef __linux__
import :case_open;
#endif
import :file_system;

std::optional<std::wstring> string_to_wstring(const std::string &str);

pragma::filesystem::VData::VData(std::string name) { m_name = name; }
bool pragma::filesystem::VData::IsFile() { return false; }
bool pragma::filesystem::VData::IsDirectory() { return false; }
std::string pragma::filesystem::VData::GetName() { return m_name; }

///////////////////////////

pragma::filesystem::VFile::VFile(const std::string &name, const std::shared_ptr<std::vector<uint8_t>> &data) : VData(name), m_data(data) {}
bool pragma::filesystem::VFile::IsFile() { return true; }
unsigned long long pragma::filesystem::VFile::GetSize() { return m_data->size(); }
std::shared_ptr<std::vector<uint8_t>> pragma::filesystem::VFile::GetData() const { return m_data; }

///////////////////////////

pragma::filesystem::VDirectory::VDirectory(std::string name) : VData(name) {}
pragma::filesystem::VDirectory::VDirectory() : VData("root") {}
pragma::filesystem::VDirectory::~VDirectory()
{
	for(unsigned int i = 0; i < m_files.size(); i++)
		delete m_files[i];
}
std::vector<pragma::filesystem::VData *> &pragma::filesystem::VDirectory::GetFiles() { return m_files; }
bool pragma::filesystem::VDirectory::IsDirectory() { return true; }
void pragma::filesystem::VDirectory::Add(VData *file) { m_files.push_back(file); }
void pragma::filesystem::VDirectory::Remove(VData *file)
{
	auto it = std::find(m_files.begin(), m_files.end(), file);
	if(it == m_files.end())
		return;
	m_files.erase(it);
	delete file;
}

///////////////////////////

pragma::filesystem::VFilePtrInternal::VFilePtrInternal() : m_bRead(false), m_bBinary(false) {}
pragma::filesystem::VFilePtrInternal::~VFilePtrInternal() {}
bool pragma::filesystem::VFilePtrInternal::ShouldRemoveComments() { return (m_bRead == true && m_bBinary == false) ? true : false; }
EVFile pragma::filesystem::VFilePtrInternal::GetType() const { return m_type; }
size_t pragma::filesystem::VFilePtrInternal::Read(void *, size_t) { return 0; }
size_t pragma::filesystem::VFilePtrInternal::Read(void *ptr, size_t size, size_t nmemb) { return Read(ptr, size * nmemb); }
unsigned long long pragma::filesystem::VFilePtrInternal::Tell() { return 0; }
void pragma::filesystem::VFilePtrInternal::Seek(unsigned long long) {}
void pragma::filesystem::VFilePtrInternal::Seek(unsigned long long offset, int whence)
{
	switch(whence) {
	case SEEK_CUR:
		return Seek(Tell() + offset);
	case SEEK_END:
		return Seek(GetSize() + offset);
	}
	return Seek(offset);
}
int pragma::filesystem::VFilePtrInternal::Eof() { return 0; }
int pragma::filesystem::VFilePtrInternal::ReadChar() { return 0; }
unsigned long long pragma::filesystem::VFilePtrInternal::GetSize() { return 0; }

pragma::filesystem::FVFile pragma::filesystem::VFilePtrInternal::GetFlags() const
{
	FVFile flags = FVFile::None;
	switch(m_type) {
	case EVFile::Virtual:
		flags |= (FVFile::ReadOnly | FVFile::Virtual);
		break;
	case EVFile::Local:
		break;
	case EVFile::Package:
		flags |= (FVFile::ReadOnly | FVFile::Package);
		break;
	}
	return flags;
}
bool pragma::filesystem::VFilePtrInternal::RemoveComments(unsigned char &c, bool bRemoveComments)
{
	if(bRemoveComments == true) {
		unsigned long long p = Tell();
		for(unsigned int i = 0; i < m_comments.size(); i++) {
			Comment &comment = m_comments[i];
			if(c == comment.start[0]) {
				Seek(p - 1);
				auto lenStart = comment.start.length();
				char *in = new char[lenStart + 1];
				Read(&in[0], comment.start.length());
				in[lenStart] = '\0';
				if(strcmp(in, comment.start.c_str()) == 0) {
					delete[] in;
					char c2;
					while(!Eof()) {
						Read(&c2, 1);
						if(c2 == comment.end[0]) {
							Seek(Tell() - 1);
							auto lenEnd = comment.end.length();
							char *in = new char[lenEnd + 1];
							Read(&in[0], lenEnd);
							in[lenEnd] = '\0';
							if(strcmp(in, comment.end.c_str()) == 0) {
								c = Read<char>();
								RemoveComments(c, bRemoveComments);
								return true;
							}
							delete[] in;
						}
					}
					return true;
				}
				else {
					delete[] in;
					Seek(p);
				}
			}
		}
	}
	return false;
}
std::string pragma::filesystem::VFilePtrInternal::ReadString()
{
	unsigned char c;
	std::string name = "";
	c = Read<char>();
	bool bRemoveComments = ShouldRemoveComments();
	while(c != '\0' && !Eof()) {
		if(RemoveComments(c, bRemoveComments) && !Eof())
			break;
		name += c;
		c = Read<char>();
	}
	return name;
}
std::string pragma::filesystem::VFilePtrInternal::ReadLine()
{
	unsigned char c;
	std::string name = "";
	c = Read<char>();
	if(c == '\n')
		return name;
	bool bRemoveComments = ShouldRemoveComments();
	while(c != '\0' && !Eof()) {
		if(RemoveComments(c, bRemoveComments) && Eof())
			break;
		name += c;
		c = Read<char>();
		if(Eof() || c == '\n')
			break;
	}
	return name;
}
char *pragma::filesystem::VFilePtrInternal::ReadString(char *str, int num)
{
	if(Eof())
		return nullptr;
	int i = 0;
	for(i = 0; i < num; i++) {
		char c = Read<char>();
		if(Eof())
			return nullptr;
		str[i] = c;
		if(str[i] == '\n') {
			if(i + 1 < num)
				str[i + 1] = '\0';
			else
				return nullptr;
			return str;
		}
	}
	return nullptr;
}
unsigned long long pragma::filesystem::VFilePtrInternal::FindFirstOf(const char *s)
{
	if(Eof())
		return static_cast<unsigned long long>(EOF);
	unsigned char c;
	int cur = 0;
	bool bRemoveComments = ShouldRemoveComments();
	do {
		c = static_cast<unsigned char>(ReadChar());
		if(!Eof() && (!RemoveComments(c, bRemoveComments) || !Eof())) {
			while(s[cur] != '\0') {
				if(c == s[cur])
					return c;
				cur++;
			}
		}
	} while(!Eof());
	return static_cast<unsigned long long>(EOF);
}
unsigned long long pragma::filesystem::VFilePtrInternal::FindFirstNotOf(const char *s)
{
	if(Eof())
		return static_cast<unsigned long long>(EOF);
	unsigned char c;
	int cur = 0;
	bool bRemoveComments = ShouldRemoveComments();
	do {
		c = static_cast<unsigned char>(ReadChar());
		if(!Eof() && (!RemoveComments(c, bRemoveComments) || !Eof())) {
			bool ret = true;
			while(s[cur] != '\0') {
				if(c == s[cur]) {
					ret = false;
					break;
				}
				cur++;
			}
			if(ret)
				return c;
			cur = 0;
		}
	} while(!Eof());
	return static_cast<unsigned long long>(EOF);
}
std::string pragma::filesystem::VFilePtrInternal::ReadUntil(const char *s)
{
	std::string ret = "";
	if(Eof())
		return ret;
	unsigned char c;
	int cur = 0;
	bool bRemoveComments = ShouldRemoveComments();
	do {
		c = static_cast<unsigned char>(ReadChar());
		if(!Eof() && (!RemoveComments(c, bRemoveComments) || !Eof())) {
			while(s[cur] != '\0') {
				if(c == s[cur]) {
					Seek(Tell() - 1);
					return ret;
				}
				cur++;
			}
			cur = 0;
			ret += char(c);
		}
	} while(!Eof());
	return ret;
}

unsigned long long pragma::filesystem::VFilePtrInternal::FindFirstOf(char c)
{
	char s[2] = {c, '\0'};
	return FindFirstOf(s);
}
unsigned long long pragma::filesystem::VFilePtrInternal::FindFirstNotOf(char c)
{
	char s[2] = {c, '\0'};
	return FindFirstNotOf(s);
}
std::string pragma::filesystem::VFilePtrInternal::ReadUntil(char c)
{
	char s[2] = {c, '\0'};
	return ReadUntil(s);
}
unsigned long long pragma::filesystem::VFilePtrInternal::FindFirstOf(std::string s) { return FindFirstOf(s.c_str()); }
unsigned long long pragma::filesystem::VFilePtrInternal::FindFirstNotOf(std::string s) { return FindFirstNotOf(s.c_str()); }
std::string pragma::filesystem::VFilePtrInternal::ReadUntil(std::string s) { return ReadUntil(s.c_str()); }

unsigned long long pragma::filesystem::VFilePtrInternal::Find(const char *s, bool bIgnoreCase)
{
	if(Eof() || s[0] == '\0')
		return static_cast<unsigned long long>(EOF);
	unsigned char c;
	int cur = 0;
	char *cpy;
	if(bIgnoreCase) {
		auto sz = strlen(s) + 1;
		cpy = new char[sz];
#ifdef _WIN32
		strcpy_s(cpy, sz, s);
#else
		strcpy(cpy, s);
#endif
		int lw = 0;
		while(cpy[lw] != '\0') {
			cpy[lw] = static_cast<char>(tolower(cpy[lw]));
			lw++;
		}
	}
	else
		cpy = const_cast<char *>(s);
	bool bRemoveComments = ShouldRemoveComments();
	do {
		c = static_cast<char>(ReadChar());
		if(!Eof() && (!RemoveComments(c, bRemoveComments) || !Eof())) {
			while((!bIgnoreCase && c == cpy[cur]) || (bIgnoreCase && tolower(c) == cpy[cur])) {
				cur++;
				if(s[cur] == '\0') {
					Seek(Tell() - cur);
					if(bIgnoreCase)
						delete[] cpy;
					return Tell();
				}
				c = static_cast<char>(ReadChar());
			}
			cur = 0;
		}
	} while(!Eof());
	if(bIgnoreCase)
		delete[] cpy;
	return static_cast<unsigned long long>(EOF);
}
void pragma::filesystem::VFilePtrInternal::IgnoreComments(std::string start, std::string end)
{
	if(start.empty())
		return;
	if(end.empty())
		end = "\n";
	m_comments.push_back(Comment(start, end));
}

///////////////////////////

pragma::filesystem::VFilePtrInternalVirtual::VFilePtrInternalVirtual(VFile *file) : VFilePtrInternal()
{
	m_offset = 0;
	m_type = EVFile::Virtual;
	m_file = file;
}
pragma::filesystem::VFilePtrInternalVirtual::~VFilePtrInternalVirtual() {}
unsigned long long pragma::filesystem::VFilePtrInternalVirtual::GetSize() { return m_file->GetSize(); }
std::shared_ptr<std::vector<uint8_t>> pragma::filesystem::VFilePtrInternalVirtual::GetData() const { return m_file->GetData(); }

///////////////////////////

pragma::filesystem::VFilePtrInternalReal::VFilePtrInternalReal() : VFilePtrInternal()
{
	VFilePtrInternal();
	m_type = EVFile::Local;
	m_file = nullptr;
	m_size = 0;
	m_path = "";
}
pragma::filesystem::VFilePtrInternalReal::~VFilePtrInternalReal()
{
	if(m_file != nullptr)
		fclose(m_file);
}
const std::string &pragma::filesystem::VFilePtrInternalReal::GetPath() const { return m_path; }

#ifdef __linux__
static bool is_directory(FILE *f)
{
	if(!f)
		return false;
	int fd = fileno(f);
	if(fd == -1)
		return false;
	struct stat st;
	if(fstat(fd, &st) != 0)
		return false;
	return S_ISDIR(st.st_mode);
}
#endif

bool pragma::filesystem::VFilePtrInternalReal::Construct(const char *path, const char *mode, int *optOutErrno, std::string *optOutErr)
{
	std::string sPath = path;
	std::replace(sPath.begin(), sPath.end(), '\\', '/');

#ifdef _WIN32
	auto wpath = string_to_wstring(path);
	if(!wpath) {
		if(optOutErrno)
			*optOutErrno = 0;
		if(optOutErr)
			*optOutErr = "failed to convert UTF‑8 path to UTF‑16";
		return false;
	}
	auto wmode = pragma::string::string_to_wstring(mode);
	m_file = nullptr;
	_wfopen_s(&m_file, wpath->data(), wmode.data());
#else
	auto *cpath = sPath.c_str();
	std::string r;
	r.resize(sPath.length() + 3); // same size as before (strlen(cpath) + 3)
	if(casepath(path, r.data())) {
		sPath = r;
		m_file = fopen(r.c_str(), mode);
	}
#endif

	if(m_file == nullptr) {
#ifdef _WIN32
		DWORD lastErr = GetLastError();
		if(optOutErrno)
			*optOutErrno = static_cast<int>(lastErr);
		if(optOutErr) {
			char buf[512] = {0};
			FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastErr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, (DWORD)sizeof(buf), nullptr);
			*optOutErr = buf;
		}
#else
		if(optOutErrno)
			*optOutErrno = errno;
		if(optOutErr)
			*optOutErr = std::strerror(errno);
#endif
		return false;
	}

#ifdef __linux__
	// Linux allows opening directories as files, but we want to
	// disallow that.
	if(is_directory(m_file)) {
		if(optOutErrno)
			*optOutErrno = EISDIR;
		if(optOutErr)
			*optOutErr = std::strerror(EISDIR);
		return false;
	}
#endif

	m_path = std::move(sPath);
	long long cur = ftell(m_file);
	fseek(m_file, 0, SEEK_END);
	m_size = ftell(m_file);
	fseek(m_file, static_cast<long>(cur), SEEK_SET);
	return true;
}

unsigned long long pragma::filesystem::VFilePtrInternalReal::GetSize() { return m_size; }
bool pragma::filesystem::VFilePtrInternalReal::ReOpen(const char *mode)
{
#ifdef _WIN32
	auto wpath = string_to_wstring(m_path);
	if(!wpath)
		return false;
	auto wmode = pragma::string::string_to_wstring(mode);
	_wfreopen_s(&m_file, wpath->data(), wmode.data(), m_file);
#else
	m_file = freopen(m_path.c_str(), mode, m_file);
#endif
	m_bBinary = FileManager::IsBinaryMode(mode);
	m_bRead = !FileManager::IsWriteMode(mode);
	if(m_file == nullptr)
		return false;
	return true;
}
