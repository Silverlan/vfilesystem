/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FILESYSTEM_H__
#define __FILESYSTEM_H__
#include "fsys_shared.h"
#include "fsys_definitions.hpp"
#include <mathutil/umath.h>
#include <memory>

#pragma warning(push)
#pragma warning(disable : 4251)
class DLLFSYSTEM VData
{
private:
	std::string m_name;
public:
	VData(std::string name);
	virtual bool IsFile();
	virtual bool IsDirectory();
	std::string GetName();
};

#ifdef __linux__
	#define FILE_ATTRIBUTE_NORMAL 0x80
	#define FILE_ATTRIBUTE_DIRECTORY 0x10
	#define INVALID_FILE_ATTRIBUTES ((unsigned int)-1)
#endif

#define FVFILE_INVALID 2048
#define FVFILE_PACKAGE 1
#define FVFILE_COMPRESSED 2
#define FVFILE_DIRECTORY 4
#define FVFILE_ENCRYPTED 8
#define FVFILE_VIRTUAL 16
#define FVFILE_READONLY 32

#define VFILE_VIRTUAL 0
#define VFILE_LOCAL 1
#define VFILE_PACKAGE 2

#undef CopyFile
#undef MoveFile

class DLLFSYSTEM VFile
	: public VData
{
private:
	std::shared_ptr<std::vector<uint8_t>> m_data;
public:
	VFile(const std::string &name,const std::shared_ptr<std::vector<uint8_t>> &data);
	bool IsFile();
	unsigned long long GetSize();
	std::shared_ptr<std::vector<uint8_t>> GetData() const;
};

class DLLFSYSTEM VDirectory
	: public VData
{
private:
	std::vector<VData*> m_files;
public:
	VDirectory(std::string name);
	VDirectory();
	~VDirectory();
	std::vector<VData*> &GetFiles();
	VFile *GetFile(std::string path);
	VDirectory *GetDirectory(std::string path);
	bool IsDirectory();
	void Add(VData *file);
	void Remove(VData *file);
	VDirectory *AddDirectory(std::string name);
};

class DLLFSYSTEM FileManager;
class DLLFSYSTEM VFilePtrInternal
{
public:
	friend FileManager;
private:
	struct Comment
	{
		Comment(std::string cmt)
			: Comment(cmt,"\n")
		{}
		Comment(std::string st,std::string en)
			: start(st),end(en)
		{}
		std::string start;
		std::string end;
		bool multiLine;
	};
	std::vector<Comment> m_comments;
protected:
	unsigned char m_type;
	bool m_bRead;
	bool m_bBinary;
	bool ShouldRemoveComments();
	bool RemoveComments(unsigned char &c,bool bRemoveComments);
public:
	VFilePtrInternal();
	virtual ~VFilePtrInternal();
	unsigned char GetType();
	virtual size_t Read(void *ptr,size_t size);
	size_t Read(void *ptr,size_t size,size_t nmemb);
	virtual unsigned long long Tell();
	virtual void Seek(unsigned long long offset);
	void Seek(unsigned long long,int whence);
	virtual int Eof();
	virtual int ReadChar();
	virtual unsigned long long GetSize();
	unsigned int GetFlags();
	template<class T>
		T Read();
	std::string ReadString();
	std::string ReadLine();
	char *ReadString(char *str,int num);
	unsigned long long FindFirstOf(const char *s);
	unsigned long long FindFirstNotOf(const char *s);
	std::string ReadUntil(const char *s);
	unsigned long long FindFirstOf(char c);
	unsigned long long FindFirstNotOf(char c);
	std::string ReadUntil(char c);
	unsigned long long FindFirstOf(std::string s);
	unsigned long long FindFirstNotOf(std::string s);
	std::string ReadUntil(std::string s);
	unsigned long long Find(const char *s,bool bIgnoreCase=false);
	void IgnoreComments(std::string start="//",std::string end="\n");
};

template<class T>
	T VFilePtrInternal::Read()
{
	char c[sizeof(T)];
	Read(c,sizeof(T));
	return (*(T*)&(c[0]));
}

class DLLFSYSTEM VFilePtrInternalVirtual
	: public VFilePtrInternal
{
private:
	unsigned long long m_offset;
	VFile *m_file;
public:
	VFilePtrInternalVirtual(VFile *file);
	virtual ~VFilePtrInternalVirtual() override;
	size_t Read(void *ptr,size_t size);
	unsigned long long Tell();
	virtual void Seek(unsigned long long offset) override;
	using VFilePtrInternal::Seek;
	int Eof();
	int ReadChar();
	unsigned long long GetSize();
	std::shared_ptr<std::vector<uint8_t>> GetData() const;
};

class DLLFSYSTEM VFilePtrInternalReal
	: public VFilePtrInternal
{
private:
	FILE *m_file;
	unsigned long long m_size;
	std::string m_path;
public:
	VFilePtrInternalReal();
	virtual ~VFilePtrInternalReal() override;
	bool Construct(const char *path,const char *mode);
	const std::string &GetPath() const;
	size_t Read(void *ptr,size_t size);
	size_t Write(const void *ptr,size_t size);
	unsigned long long Tell();
	virtual void Seek(unsigned long long offset) override;
	using VFilePtrInternal::Seek;
	int Eof();
	int ReadChar();
	unsigned long long GetSize();
	template<class T>
		void Write(T t);
	void WriteString(std::string str);
	int WriteString(const char *str);
	bool ReOpen(const char *mode);
};

#undef CreateDirectory
#undef GetFileAttributes
#undef RemoveDirectory

#include <fsys/vfileptr.h>
#include "fsys_searchflags.hpp"
#include <functional>

struct MountDirectory;
namespace fsys {class PackageManager; class Package;};
class DLLFSYSTEM FileManager
{
private:
	static VDirectory m_vroot;
	static std::vector<MountDirectory> m_customMount;
	static std::unordered_map<std::string,std::unique_ptr<fsys::PackageManager>> m_packages;
	static std::unique_ptr<std::string> m_rootPath;
	static std::function<VFilePtr(const std::string&,const char *mode)> m_customFileHandler;
	static VData *GetVirtualData(std::string path);
public:
	static bool IsWriteMode(const char *mode);
	static bool IsBinaryMode(const char *mode);
	static VFilePtr OpenFile(const char *cpath,const char *mode,fsys::SearchFlags includeFlags=fsys::SearchFlags::All,fsys::SearchFlags excludeFlags=fsys::SearchFlags::None);
	template<class T>
		static T OpenFile(const char *cpath,const char *mode,fsys::SearchFlags includeFlags=fsys::SearchFlags::All,fsys::SearchFlags excludeFlags=fsys::SearchFlags::None);
	// Opens a file anywhere from the disk. Needs the whole path. (e.g. 'C:\\directory\\file.txt')
	static std::string GetProgramPath();
	static void AddCustomMountDirectory(const char *cpath,fsys::SearchFlags searchMode=fsys::SearchFlags::Local);
	static void AddCustomMountDirectory(const char *cpath,bool bAbsolutePath,fsys::SearchFlags searchMode=fsys::SearchFlags::Local);
	static void RemoveCustomMountDirectory(const char *path);
	static void ClearCustomMountDirectories();
	static VFilePtrReal OpenSystemFile(const char *cpath,const char *mode);
	static bool CreatePath(const char *path);
	static bool CreateDirectory(const char *dir);
	static void SetCustomFileHandler(const std::function<VFilePtr(const std::string&,const char *mode)> &fHandler);
	static std::pair<VDirectory*,VFile*> AddVirtualFile(std::string path,const std::shared_ptr<std::vector<uint8_t>> &data);
	static VDirectory *GetRootDirectory();
	static fsys::Package *LoadPackage(std::string package,fsys::SearchFlags searchMode=fsys::SearchFlags::Local);
	static void ClearPackages(fsys::SearchFlags searchMode);
	static void RegisterPackageManager(const std::string &name,std::unique_ptr<fsys::PackageManager> pm);
	template<class T>
		static T Read(FILE *f);
	static std::string ReadString(FILE *f);
	template<class T>
		static void Write(FILE *f,T t);
	static void WriteString(FILE *f,std::string str,bool bBinary=true);
	static bool RemoveFile(const char *file);
	static bool RemoveDirectory(const char *dir);
	static bool RenameFile(const char *file,const char *fNewName);
	static void Close();
	static std::string GetPath(std::string &path);
	static std::string GetFile(std::string &path);
	static std::string GetCanonicalizedPath(std::string path);
	static std::string GetSubPath(std::string path);
	static unsigned long long GetFileSize(std::string name,fsys::SearchFlags fsearchmode=fsys::SearchFlags::All);
	static bool Exists(std::string name,fsys::SearchFlags includeFlags=fsys::SearchFlags::All,fsys::SearchFlags excludeFlags=fsys::SearchFlags::None);
	static bool IsFile(std::string name,fsys::SearchFlags fsearchmode=fsys::SearchFlags::All);
	static bool IsDir(std::string name,fsys::SearchFlags fsearchmode=fsys::SearchFlags::All);
	static bool ExistsSystem(std::string name);
	static bool IsSystemFile(std::string name);
	static bool IsSystemDir(std::string name);
	static unsigned long long GetFileAttributes(std::string name);
	static unsigned long long GetFileFlags(std::string name,fsys::SearchFlags includeFlags=fsys::SearchFlags::All,fsys::SearchFlags excludeFlags=fsys::SearchFlags::None);
	static std::string GetNormalizedPath(std::string path);
	static void FindFiles(const char *cfind,std::vector<std::string> *resfiles,std::vector<std::string> *resdirs,bool bKeepPath,fsys::SearchFlags includeFlags=fsys::SearchFlags::All,fsys::SearchFlags excludeFlags=fsys::SearchFlags::None);
	static void FindFiles(const char *cfind,std::vector<std::string> *resfiles,std::vector<std::string> *resdirs,fsys::SearchFlags includeFlags=fsys::SearchFlags::All,fsys::SearchFlags excludeFlags=fsys::SearchFlags::None);
	static void FindSystemFiles(const char *path,std::vector<std::string> *resfiles,std::vector<std::string> *resdirs,bool bKeepPath=false);
	static bool CopyFile(const char *cfile,const char *cfNewPath);
	static bool CopySystemFile(const char *cfile,const char *cfNewPath);
	static bool MoveFile(const char *cfile,const char *cfNewPath);
	static void SetAbsoluteRootPath(const std::string &path);
	static void SetRootPath(const std::string &path);
	static void SetRootPath();
	static std::string GetRootPath();
	// Note: This will not include custom mounts that are located outside of the program location
	static bool FindLocalPath(std::string path,std::string &rpath,fsys::SearchFlags includeFlags=fsys::SearchFlags::All,fsys::SearchFlags excludeFlags=fsys::SearchFlags::None);
	static bool FindAbsolutePath(std::string path,std::string &rpath,fsys::SearchFlags includeFlags=fsys::SearchFlags::All,fsys::SearchFlags excludeFlags=fsys::SearchFlags::None);
	static char GetDirectorySeparator();
	static bool RemoveSystemFile(const char *file);
	static bool RemoveSystemDirectory(const char *dir);
	static bool RenameSystemFile(const char *file,const char *fNewName);
	static std::string GetSubPath(const std::string &root,std::string path);
	static bool CreateSystemPath(const std::string &root,const char *path);
	static bool CreateSystemDirectory(const char *dir);

	static VFilePtr OpenPackageFile(const char *packageName,const char *cpath,const char *mode,fsys::SearchFlags includeFlags=fsys::SearchFlags::All,fsys::SearchFlags excludeFlags=fsys::SearchFlags::None);
	static fsys::PackageManager *GetPackageManager(const std::string &name);

	static bool ComparePath(const std::string &a,const std::string &b);
};
#pragma warning(pop)

template<class T>
	T FileManager::Read(FILE *f)
{
	char c[sizeof(T)];
	fread(c,1,sizeof(T),f);
	return (*(T*)&(c[0]));
}

template<class T>
	void FileManager::Write(FILE *f,T t)
{
	const char *c = static_cast<const char*>(static_cast<const void*>(&t));
	fwrite(c,1,sizeof(T),f);
}

template<class T>
	void VFilePtrInternalReal::Write(T t)
{FileManager::Write(m_file,t);}

#include "filesystem_mount.h"

#endif
