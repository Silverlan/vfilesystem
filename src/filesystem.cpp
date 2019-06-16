/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "fsys/filesystem.h"
#ifdef __linux__
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <unistd.h>
	#include <dirent.h>
	#define DIR_SEPARATOR '/'
	#define DIR_SEPARATOR_OTHER '\\'
#else
	#include "Shlwapi.h"
	#define DIR_SEPARATOR '\\'
	#define DIR_SEPARATOR_OTHER '/'
#endif

extern "C" {
	#include "bzlib.h"
}

#include <cstring>
#include <sharedutils/util_string.h>
#include <sharedutils/util.h>
#include "fsys/fsys_searchflags.hpp"
#include "fsys/fsys_package.hpp"
#include "impl_fsys_util.hpp"
#include <array>
#include <iostream>

static unsigned long long get_file_flags(const std::string &fpath)
{
#ifdef __linux__
	auto npath = fpath;
	std::replace(npath.begin(),npath.end(),'\\','/');
	class stat st;
	if(stat(npath.c_str(),&st) != -1)
	{
		unsigned int flags = 0;
		const bool isDir = (st.st_mode &S_IFDIR) != 0;
		if(isDir == true)
			flags |= FVFILE_DIRECTORY;
		return flags;
	}
#else
	unsigned long long attr = GetFileAttributesA(fpath.c_str());
	if(attr != INVALID_FILE_ATTRIBUTES)
	{
		unsigned int flags = 0;
		if((attr &FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
			flags |= FVFILE_DIRECTORY;
		return flags;
	}
#endif
	return FVFILE_INVALID;
}

static unsigned long long update_file_insensitive_path_components_and_get_flags(std::string &appPath,std::string &mountPath,std::string &name)
{
	std::replace(appPath.begin(),appPath.end(),'\\','/');
	std::replace(mountPath.begin(),mountPath.end(),'\\','/');
	std::replace(name.begin(),name.end(),'\\','/');
	ustring::replace(appPath,"//","");
	ustring::replace(mountPath,"//","");
	ustring::replace(name,"//","");

#ifdef _WIN32
	if(appPath.empty() == false && appPath.front() == '/')
		appPath = appPath.substr(1);
#endif
	if(appPath.empty() == false && appPath.back() == '/')
		appPath = appPath.substr(0,appPath.length() -1);

	if(mountPath.empty() == false && mountPath.front() == '/')
		mountPath = mountPath.substr(1);
	if(mountPath.empty() == false && mountPath.back() == '/')
		mountPath = mountPath.substr(0,mountPath.length() -1);

	if(name.empty() == false && name.front() == '/')
		name = name.substr(1);
	if(name.empty() == false && name.back() == '/')
		name = name.substr(0,name.length() -1);

	if(appPath.empty() == false)
	{
		auto fullPath = appPath +"/" +mountPath +"/" +name;
		fsys::impl::to_case_sensitive_path(fullPath);
		auto offset = 0ull;
		appPath = fullPath.substr(offset,appPath.length());
		offset += appPath.length() +1;
		mountPath = fullPath.substr(offset,mountPath.length());
		offset += mountPath.length() +1;
		name = fullPath.substr(offset,name.length());
		return get_file_flags(fullPath);
	}
	auto fullPath = mountPath +"\\" +name;
	fsys::impl::to_case_sensitive_path(fullPath);
	auto offset = 0ull;
	mountPath = fullPath.substr(offset,mountPath.length());
	offset += mountPath.length() +1;
	name = fullPath.substr(offset,name.length());
	return get_file_flags(fullPath);
}

template<>
#ifdef _WIN32
	DLLFSYSTEM
#endif
	VFilePtrReal FileManager::OpenFile<VFilePtrReal>(const char *cpath,const char *mode,fsys::SearchFlags includeFlags,fsys::SearchFlags excludeFlags)
{
	return std::static_pointer_cast<VFilePtrInternalReal>(OpenFile(cpath,mode,includeFlags,excludeFlags));
}

template<>
#ifdef _WIN32
	DLLFSYSTEM
#endif
	VFilePtrVirtual FileManager::OpenFile<VFilePtrVirtual>(const char *cpath,const char *mode,fsys::SearchFlags includeFlags,fsys::SearchFlags excludeFlags)
{
	return std::static_pointer_cast<VFilePtrInternalVirtual>(OpenFile(cpath,mode,includeFlags,excludeFlags));
}

decltype(FileManager::m_vroot) FileManager::m_vroot;
decltype(FileManager::m_packages) FileManager::m_packages;
decltype(FileManager::m_customMount) FileManager::m_customMount;
decltype(FileManager::m_rootPath) FileManager::m_rootPath;
decltype(FileManager::m_customFileHandler) FileManager::m_customFileHandler = nullptr;

void FileManager::SetCustomFileHandler(const std::function<VFilePtr(const std::string&,const char *mode)> &fHandler) {m_customFileHandler = fHandler;}

DLLFSYSTEM std::string FileManager::GetNormalizedPath(std::string path)
{
#ifdef _WIN32
	StringToLower(path);
#endif
	path = GetCanonicalizedPath(path);
	return path;
}

DLLFSYSTEM std::string FileManager::GetProgramPath() {return util::get_program_path();}

void FileManager::RemoveCustomMountDirectory(const char *cpath)
{
	auto path = GetCanonicalizedPath(cpath);
	auto it = std::find_if(m_customMount.begin(),m_customMount.end(),[&path](const MountDirectory &mount) {
		return (mount.directory == path) ? true : false;
	});
	if(it == m_customMount.end())
		return;
	m_customMount.erase(it);
}

DLLFSYSTEM void FileManager::AddCustomMountDirectory(const char *cpath,fsys::SearchFlags searchMode)
{
	return AddCustomMountDirectory(cpath,false,searchMode);
}

void FileManager::AddCustomMountDirectory(const char *cpath,bool bAbsolutePath,fsys::SearchFlags searchMode)
{
	searchMode |= fsys::SearchFlags::Local;
	searchMode &= ~fsys::SearchFlags::NoMounts;
	searchMode &= ~fsys::SearchFlags::Virtual;
	searchMode &= ~fsys::SearchFlags::Package;
	auto path = GetCanonicalizedPath(cpath);
	for(auto it=m_customMount.begin();it!=m_customMount.end();++it)
	{
		auto &mount = *it;
		if(mount.directory == path)
		{
			mount.searchMode = searchMode;
			return;
		}
	}
	m_customMount.push_back(MountDirectory(path,bAbsolutePath,searchMode));
}

DLLFSYSTEM void FileManager::ClearCustomMountDirectories()
{
	m_customMount.clear();
}

#define NormalizePath(path) \
	path = FileManager::GetNormalizedPath(path);

DLLFSYSTEM std::pair<VDirectory*,VFile*> FileManager::AddVirtualFile(std::string path,const std::shared_ptr<std::vector<uint8_t>> &data)
{
	std::replace(path.begin(),path.end(),'/','\\');
	NormalizePath(path);
	unsigned int cur = 0;
	std::string sub;
	VDirectory *dir = &m_vroot;
	while(cur < path.size() && path[cur] != '\0')
	{
		if(path[cur] == '/' || path[cur] == '\\')
		{
			StringToLower(sub);
			dir = dir->AddDirectory(sub);
			sub = "";
		}
		else
			sub += path[cur];
		cur++;
	}
	StringToLower(sub);
	VFile *f = new VFile(sub,data);
	dir->Add(f);
	return {dir,f};
}

DLLFSYSTEM VDirectory *FileManager::GetRootDirectory() {return &m_vroot;}
DLLFSYSTEM std::string FileManager::GetRootPath()
{
	if(m_rootPath == nullptr)
		return GetProgramPath();
	return *m_rootPath.get();
}
DLLFSYSTEM void FileManager::SetAbsoluteRootPath(const std::string &path)
{
	m_rootPath = nullptr;
	if(!path.empty())
	{
		auto rootPath = GetCanonicalizedPath(path);
		if(rootPath.back() == '\\')
			rootPath = rootPath.substr(0,rootPath.size() -1);
		m_rootPath = std::unique_ptr<std::string>(new std::string(rootPath));
	}
}
DLLFSYSTEM void FileManager::SetRootPath(const std::string &path)
{
	auto r = GetProgramPath();
	r += '\\';
	r += path;
	SetAbsoluteRootPath(r);
}

DLLFSYSTEM bool FileManager::IsWriteMode(const char *mode)
{
	unsigned int c = 0;
	while(mode[c] != '\0')
	{
		if(mode[c] == 'w' || mode[c] == 'W' || mode[c] == 'a' || mode[c] == 'A')
			return true;
		c++;
	}
	return false;
}

DLLFSYSTEM bool FileManager::IsBinaryMode(const char *mode)
{
	unsigned int c = 0;
	while(mode[c] != '\0')
	{
		if(mode[c] == 'b' || mode[c] == 'B')
			return true;
		c++;
	}
	return false;
}

DLLFSYSTEM VFilePtrReal FileManager::OpenSystemFile(const char *cpath,const char *mode)
{
	std::string path = GetCanonicalizedPath(cpath);
	auto ptrReal = std::make_shared<VFilePtrInternalReal>();
	if(!ptrReal->Construct(path.c_str(),mode))
		return nullptr;
	ptrReal->m_bBinary = FileManager::IsBinaryMode(mode);
	ptrReal->m_bRead = !FileManager::IsWriteMode(mode);
	return ptrReal;
}

VFilePtr FileManager::OpenPackageFile(const char *packageName,const char *cpath,const char *mode,fsys::SearchFlags includeFlags,fsys::SearchFlags excludeFlags)
{
	std::string path = GetCanonicalizedPath(cpath);
	bool bBinary = FileManager::IsBinaryMode(mode);
	bool bWrite = IsWriteMode(mode);
	if(bWrite == true)
		return nullptr;
	auto it = m_packages.find(packageName);
	if(it == m_packages.end())
		return nullptr;
	return it->second->OpenFile(path,bBinary,includeFlags,excludeFlags);
}

fsys::PackageManager *FileManager::GetPackageManager(const std::string &name)
{
	auto it = m_packages.find(name);
	if(it == m_packages.end())
		return nullptr;
	return it->second.get();
}

DLLFSYSTEM VFilePtr FileManager::OpenFile(const char *cpath,const char *mode,fsys::SearchFlags includeFlags,fsys::SearchFlags excludeFlags)
{
	std::string path = GetCanonicalizedPath(cpath);
	if(m_customFileHandler != nullptr)
	{
		auto f = m_customFileHandler(path,mode);
		if(f != nullptr)
			return f;
	}
	VFilePtr pfile;
	bool bBinary = FileManager::IsBinaryMode(mode);
	bool bWrite = IsWriteMode(mode);
	if(bWrite) // Can't write virtual or packed file, so it must be on hardspace
	{
		if((includeFlags &fsys::SearchFlags::Local) == fsys::SearchFlags::None)
			return NULL;
		std::string appPath = GetRootPath() +"\\";
		std::string fpath = appPath +path;
		auto ptrReal = std::make_shared<VFilePtrInternalReal>();
		if(ptrReal->Construct(fpath.c_str(),mode))
		{
			pfile = ptrReal;
			pfile->m_bBinary = bBinary;
			pfile->m_bRead = !bWrite;
			return pfile;
		}
	}
#ifdef _WIN32
	StringToLower(path);
#endif
	if((includeFlags &fsys::SearchFlags::Virtual) == fsys::SearchFlags::Virtual)
	{
		VFile *f = m_vroot.GetFile(path);
		if(f != NULL)
		{
			auto pfile = std::make_shared<VFilePtrInternalVirtual>(f);
			pfile->m_bBinary = bBinary;
			pfile->m_bRead = true;
			return pfile;
		}
	}
	if((includeFlags &fsys::SearchFlags::Package) == fsys::SearchFlags::Package)
	{
		for(auto &pair : m_packages)
		{
			pfile = pair.second->OpenFile(path,bBinary,includeFlags,excludeFlags);
			if(pfile != nullptr)
				return pfile;
		}
	}
	if((includeFlags &fsys::SearchFlags::Local) == fsys::SearchFlags::None)
		return NULL;
	std::string appPath = GetRootPath() +"\\";
	std::string fpath;
	bool bFound = false;
	bool bAbsolute = false;
	if((includeFlags &fsys::SearchFlags::NoMounts) == fsys::SearchFlags::None)
	{
		MountIterator it(m_customMount);
		std::string mountPath;
		while(bFound == false && it.GetNextDirectory(mountPath,includeFlags,excludeFlags,bAbsolute))
		{
			fpath = GetNormalizedPath(mountPath +"\\" +path);
			auto rootPath = (bAbsolute == false) ? appPath : "";
			bFound = ((update_file_insensitive_path_components_and_get_flags(rootPath,mountPath,path) &FVFILE_INVALID) == 0) ? true : false;
		}
	}
	if(bFound == false)
		fpath = path;
	if(bAbsolute == false)
		fpath = appPath +fpath;
	auto ptrReal = std::make_shared<VFilePtrInternalReal>();
	if(!ptrReal->Construct(fpath.c_str(),mode))
		return NULL;
	pfile = ptrReal;
	pfile->m_bBinary = bBinary;
	pfile->m_bRead = true;
	return pfile;
}

fsys::Package *FileManager::LoadPackage(std::string package,fsys::SearchFlags searchMode)
{
	for(auto &pair : m_packages)
	{
		auto *pck = pair.second->LoadPackage(package,searchMode);
		if(pck != nullptr)
			return pck;
	}
	return nullptr;
}

void FileManager::ClearPackages(fsys::SearchFlags searchMode)
{
	for(auto &pair : m_packages)
		pair.second->ClearPackages(searchMode);
}

void FileManager::RegisterPackageManager(const std::string &name,std::unique_ptr<fsys::PackageManager> pm)
{
	m_packages.insert(std::make_pair(name,std::move(pm)));
}

bool FileManager::RemoveSystemFile(const char *file)
{
	std::string path = GetCanonicalizedPath(file);
	std::replace(path.begin(),path.end(),'\\','/');
	if(remove(path.c_str()) == 0)
		return true;
	return false;
}

bool FileManager::RemoveFile(const char *file) {return RemoveSystemFile((GetRootPath() +'\\' +GetCanonicalizedPath(file)).c_str());}

static bool remove_directory(const std::string &rootPath,const char *cdir,const std::function<void(const char*,std::vector<std::string>*,std::vector<std::string>*,bool)> &fFindFiles,const std::function<bool(const char*)> &fRemoveFile)
{
	std::string dir = FileManager::GetCanonicalizedPath(cdir);
	std::string dirSub = dir;
	dirSub += '\\';
	std::string dirSearch = dirSub;
	dirSearch += '*';
	std::vector<std::string> files;
	std::vector<std::string> dirs;
	fFindFiles(dirSearch.c_str(),&files,&dirs,false);
	for(auto it=dirs.begin();it!=dirs.end();it++)
		remove_directory(rootPath,(dirSub +*it).c_str(),fFindFiles,fRemoveFile);
	for(auto it=files.begin();it!=files.end();it++)
		fRemoveFile((dirSub +*it).c_str());
	std::string p = rootPath +dir;
#ifdef _WIN32
	auto r = ::RemoveDirectoryA(p.c_str());
	return (r != 0) ? true : false;
#else
	std::replace(p.begin(),p.end(),'\\','/');
	auto r = rmdir(p.c_str());
	return (r == 0) ? true : false;
#endif
}

bool FileManager::RemoveSystemDirectory(const char *cdir) {return remove_directory("",cdir,FileManager::FindSystemFiles,FileManager::RemoveSystemFile);}
bool FileManager::RemoveDirectory(const char *cdir)
{
	return remove_directory(FileManager::GetRootPath() +'\\',cdir,[](const char* cfind,std::vector<std::string> *resfiles,std::vector<std::string> *resdirs,bool bKeepPath) {
		FileManager::FindFiles(cfind,resfiles,resdirs,bKeepPath);
	},FileManager::RemoveFile);
}

static bool rename_file(const std::string &root,const std::string &file,const std::string &fNewName)
{
	std::string path = root +FileManager::GetCanonicalizedPath(file);
	std::string pathNew = root +FileManager::GetCanonicalizedPath(fNewName);
	std::replace(path.begin(),path.end(),'\\','/');
	std::replace(pathNew.begin(),pathNew.end(),'\\','/');
	fsys::impl::to_case_sensitive_path(path);
	if(rename(path.c_str(),pathNew.c_str()) == 0)
		return true;
	return false;
}

bool FileManager::RenameSystemFile(const char *file,const char *fNewName) {return rename_file("",file,fNewName);}

DLLFSYSTEM bool FileManager::RenameFile(const char *file,const char *fNewName) {return rename_file(GetRootPath() +'\\',file,fNewName);}

bool FileManager::FindAbsolutePath(std::string path,std::string &rpath,fsys::SearchFlags includeFlags,fsys::SearchFlags excludeFlags)
{
	NormalizePath(path);
	MountIterator it(m_customMount);
	std::string mountPath;
	std::string appPath = GetRootPath() +"\\";
	auto bAbsolute = false;
	while(it.GetNextDirectory(mountPath,includeFlags,excludeFlags,bAbsolute))
	{
		auto fpath = GetNormalizedPath(mountPath +"\\" +path);
		fsys::impl::to_case_sensitive_path(fpath);
		auto rootPath = bAbsolute ? "" : appPath;
		if((update_file_insensitive_path_components_and_get_flags(rootPath,mountPath,path) &FVFILE_INVALID) == 0)
		{
			rpath = fpath;
			if(bAbsolute == false)
				rpath = appPath +rpath;
			return true;
		}
	}
	return false;
}

bool FileManager::FindLocalPath(std::string path,std::string &rpath,fsys::SearchFlags includeFlags,fsys::SearchFlags excludeFlags)
{
	NormalizePath(path);
	MountIterator it(m_customMount);
	std::string mountPath;
	std::string appPath = GetRootPath() +"\\";
	auto bAbsolute = false;
	while(it.GetNextDirectory(mountPath,includeFlags,excludeFlags,bAbsolute))
	{
		if(bAbsolute == true)
			continue;
		auto fpath = GetNormalizedPath(mountPath +"\\" +path);
		if((update_file_insensitive_path_components_and_get_flags(appPath,mountPath,path) &FVFILE_INVALID) == 0)
		{
			rpath = fpath;
			return true;
		}
	}
	return false;
}

static void find_files(const std::string &path,const std::string &ctarget,const std::string &localMountPath,std::vector<std::string> *resfiles,std::vector<std::string> *resdirs,bool bKeepPath,size_t szFiles=0,size_t szDirs=0)
{
	auto numFilesSpecial = (resfiles != nullptr) ? (resfiles->size() -szFiles) : 0;
	auto numDirsSpecial = (resdirs != nullptr) ? (resdirs->size() -szDirs) : 0;
#ifdef __linux__
	std::string __path = path;
	std::replace(__path.begin(),__path.end(),'\\','/');
	DIR *dir = opendir(__path.c_str());
	if(dir != nullptr)
	{
		class stat st;
		dirent *ent;
		while((ent = readdir(dir)) != nullptr)
		{
			std::string fileName = ent->d_name;
			const std::string fullFileName = __path +fileName;
			if(stat(fullFileName.c_str(),&st) != -1)
			{
				const bool isDir = (st.st_mode &S_IFDIR) != 0;
				if(resfiles != nullptr && isDir == false)
				{
					if(ustring::match(fileName.c_str(),ctarget,false))
					{
						auto pathName = bKeepPath == false ? fileName : (localMountPath +fileName);
						if(!fsys::impl::has_value(resfiles,szFiles,szFiles +numFilesSpecial,fileName,true))
							resfiles->push_back(pathName);
					}
				}
				if(resdirs != nullptr && isDir == true)
				{
					if(fileName != "." && fileName != ".." && ustring::match(fileName.c_str(),ctarget,false))
					{
						auto pathName = bKeepPath == false ? fileName : (localMountPath +fileName);
						if(!fsys::impl::has_value(resdirs,szDirs,szDirs +numDirsSpecial,fileName,true))
							resdirs->push_back(pathName);
					}
				}
			}
		}
		closedir(dir);
	}
#else
	auto searchPath = path +'*';
	WIN32_FIND_DATA data;
	HANDLE hFind = FindFirstFile(searchPath.c_str(),&data);
	while(hFind != INVALID_HANDLE_VALUE)
	{
		if(resfiles != NULL)
		{
			if((data.dwFileAttributes &FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				if(ustring::match(data.cFileName,ctarget,false))
				{
					auto pathName = bKeepPath == false ? data.cFileName : (localMountPath +data.cFileName);
					if(!fsys::impl::has_value(resfiles,szFiles,szFiles +numFilesSpecial,data.cFileName))
						resfiles->push_back(pathName);
				}
			}
		}
		if(resdirs != NULL)
		{
			if((data.dwFileAttributes &FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
			{
				std::string name = data.cFileName;
				if(name != "." && name != ".." && ustring::match(data.cFileName,ctarget,false))
				{
					auto pathName = bKeepPath == false ? data.cFileName : (localMountPath +data.cFileName);
					if(!fsys::impl::has_value(resdirs,szDirs,szDirs +numDirsSpecial,data.cFileName))
						resdirs->push_back(pathName);
				}
			}
		}
		if(!FindNextFile(hFind,&data))
		{
			FindClose(hFind);
			break;
		}
	}
#endif
}

DLLFSYSTEM void FileManager::FindFiles(const char *cfind,std::vector<std::string> *resfiles,std::vector<std::string> *resdirs,fsys::SearchFlags includeFlags,fsys::SearchFlags excludeFlags)
{FindFiles(cfind,resfiles,resdirs,false,includeFlags,excludeFlags);}

static void get_find_string(const char *cfind,std::string &path,std::string &target,size_t &lbr)
{
	std::string find = cfind;
	NormalizePath(find);
	lbr = find.rfind('\\');
	if(lbr == ustring::NOT_FOUND)
	{
		target = find;
		path = "";
	}
	else
	{
		target = find.substr(lbr +1);
		path = find.substr(0,lbr);
		path += '\\';
	}
}

DLLFSYSTEM void FileManager::FindFiles(const char *cfind,std::vector<std::string> *resfiles,std::vector<std::string> *resdirs,bool bKeepPath,fsys::SearchFlags includeFlags,fsys::SearchFlags excludeFlags)
{
	std::string path;
	std::string target;
	size_t lbr;
	get_find_string(cfind,path,target,lbr);

	auto szFiles = (resfiles != nullptr) ? resfiles->size() : 0;
	auto szDirs = (resdirs != nullptr) ? resdirs->size() : 0;
	if((includeFlags &fsys::SearchFlags::Virtual) == fsys::SearchFlags::Virtual)
	{
		VDirectory *dir = &m_vroot;
		if(lbr != ustring::NOT_FOUND)
			dir = dir->GetDirectory(path);
		if(dir != NULL)
		{
			std::vector<VData*> &files = dir->GetFiles();
			for(unsigned int i=0;i<files.size();i++)
			{
				VData *data = files[i];
				std::string name = data->GetName();
				if(data->IsFile())
				{
					if(resfiles != NULL && ustring::match(name.c_str(),target,false))
					{
						auto pathName = bKeepPath == false ? name : (path +name);
						if(!fsys::impl::has_value(resfiles,0,resfiles->size(),name))
							resfiles->push_back(pathName);
					}
				}
				else
				{
					if(resdirs != NULL && ustring::match(name.c_str(),target,false))
					{
						auto pathName = bKeepPath == false ? name : (path +name);
						if(!fsys::impl::has_value(resdirs,0,resdirs->size(),name))
							resdirs->push_back(pathName);
					}
				}
			}
		}
	}
	if((includeFlags &fsys::SearchFlags::Package) == fsys::SearchFlags::Package)
	{
		for(auto &pair : m_packages)
			pair.second->FindFiles(cfind,path,resfiles,resdirs,bKeepPath,includeFlags);
	}
	if((includeFlags &fsys::SearchFlags::Local) == fsys::SearchFlags::None)
		return;
	std::string localPath = path;
	std::string appPath = GetRootPath();
	MountIterator it(m_customMount);
	std::string mountPath;
	bool bAbsolute = false;
	while(it.GetNextDirectory(mountPath,includeFlags,excludeFlags,bAbsolute))
	{
		std::string localMountPath = mountPath +DIR_SEPARATOR +localPath;
		if(bAbsolute == false)
			path = appPath +DIR_SEPARATOR +localMountPath;
		else
			path = localMountPath;
		find_files(path,target,localMountPath,resfiles,resdirs,bKeepPath,szFiles,szDirs);
	}
}

DLLFSYSTEM  char FileManager::GetDirectorySeparator() {return DIR_SEPARATOR;}

DLLFSYSTEM void FileManager::FindSystemFiles(const char *path,std::vector<std::string> *resfiles,std::vector<std::string> *resdirs,bool bKeepPath)
{
	std::string npath;
	std::string target;
	size_t lbr;
	get_find_string(path,npath,target,lbr);
	find_files(path,target,"",resfiles,resdirs,bKeepPath);
}

#ifdef __linux__
static void explodeString(std::string str,const char *sep,std::vector<std::string> *subStrings)
{
	size_t st = str.find_first_of(sep);
	while(st != std::string::npos)
	{
		std::string sub = str.substr(0,st);
		str = str.substr(st +1);
		subStrings->push_back(sub);
		st = str.find_first_of(sep);
	}
	subStrings->push_back(str);
}

static std::string normalizePath(std::string &path)
{
	std::string::iterator it;
	for(size_t i=0;i<path.length();i++)
	{
		if(
				path[i] == ' ' || 
				path[i] == '\t' || 
				path[i] == '\f' || 
				path[i] == '\v' ||
				path[i] == '\n' ||
				path[i] == '\r'
			)
		{
			path.erase(path.begin() +i);
			i--;
		}
	}

	std::vector<std::string> sub;
	explodeString(path,"\\",&sub);
	size_t l = sub.size();
	for(size_t i=0;i<l;)
	{
		if(sub[i].empty() || sub[i] == ".")
		{
			sub.erase(sub.begin() +i);
			l--;
		}
		else if(sub[i] == "..")
		{
			sub.erase(sub.begin() +i);
			if(i > 0)
			{
				sub.erase(sub.begin() +(i -1));
				i--;
				l--;
			}
			l--;
		}
		else
			i++;
	}
	std::string outPath = "";
	l = sub.size();
	for(size_t i=0;i<l;i++)
	{
		outPath += sub[i];
		if(i < (l -1))
			outPath += '\\';
	}
	return outPath;
}
#endif

DLLFSYSTEM std::string FileManager::GetCanonicalizedPath(std::string path)
{
	if(path.empty())
		return path;
	std::replace(path.begin(),path.end(),'/','\\');
	ustring::replace(path,"\\\\","");
#ifdef __linux__
	std::string spathReal = normalizePath(path);
#else
	const char *buf = path.c_str();
	char pathReal[MAX_PATH];
	PathCanonicalize(pathReal,buf);
	std::string spathReal = pathReal;
	if(!spathReal.empty() && spathReal[0] == '\\')
		spathReal = spathReal.substr(1,spathReal.size());
#endif
	return spathReal;
}

DLLFSYSTEM std::string FileManager::GetSubPath(const std::string &root,std::string path)
{
	path = GetCanonicalizedPath(path);
	if(path.empty())
		return path;
	if(path[0] != '\\')
		path = '\\' +path;
	return root +path;
}

DLLFSYSTEM std::string FileManager::GetSubPath(std::string path) {return GetSubPath(GetRootPath(),path);}

static bool create_path(const std::string &root,const char *path)
{
	std::string p = path;
	p = FileManager::GetCanonicalizedPath(p);
	size_t pos = 0;
	do
	{
		pos = p.find_first_of('\\',pos +1);
#ifdef __linux__
		struct stat st = {0};
		std::string subPath = FileManager::GetSubPath(p.substr(0,pos));
		std::replace(subPath.begin(),subPath.end(),'\\','/');
		const char *pSub = subPath.c_str();
		if(stat(pSub,&st) == -1)
		{
			if(mkdir(pSub,0777) != 0)
				return false;
		}
#else
		if(CreateDirectoryA(FileManager::GetSubPath(root,p.substr(0,pos)).c_str(),NULL) == 0 && GetLastError() != ERROR_ALREADY_EXISTS)
			return false;
#endif
	}
	while(pos != ustring::NOT_FOUND);
	return true;
}

DLLFSYSTEM bool FileManager::CreateSystemPath(const std::string &root,const char *path) {return create_path(root,path);}

DLLFSYSTEM bool FileManager::CreatePath(const char *path) {return create_path(GetRootPath(),path);}

#undef CreateDirectory
DLLFSYSTEM bool FileManager::CreateSystemDirectory(const char *dir)
{
	std::string p = dir;
	p = GetCanonicalizedPath(p);
#ifdef __linux__
	std::replace(p.begin(),p.end(),'\\','/');
	const char *pSub = p.c_str();
	struct stat st = {0};
	if(stat(pSub,&st) == -1)
	{
		if(mkdir(pSub,777) != 0)
			return false;
	}
#else
	if(CreateDirectoryA(p.c_str(),NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS)
		return true;
#endif
	return false;
}
DLLFSYSTEM bool FileManager::CreateDirectory(const char *dir)
{
	std::string p = dir;
	p = GetCanonicalizedPath(p);
	return CreateSystemDirectory(GetSubPath(p).c_str());
}

DLLFSYSTEM void FileManager::Close()
{
	m_packages.clear();
}

DLLFSYSTEM std::string FileManager::GetPath(std::string &path)
{
	size_t lpos = path.find_last_of("/\\");
	if(lpos == ustring::NOT_FOUND)
		return "";
	return path.substr(0,lpos +1);
}

DLLFSYSTEM std::string FileManager::GetFile(std::string &path)
{
	size_t lpos = path.find_last_of("/\\");
	if(lpos == ustring::NOT_FOUND)
		return path;
	return path.substr(lpos +1);
}

DLLFSYSTEM std::string FileManager::ReadString(FILE *f)
{
	unsigned char c;
	std::string name = "";
	c = Read<char>(f);
	while(c != '\0')
	{
		name += c;
		c = Read<char>(f);
		if(feof(f))
			break;
	}
	return name;
}

DLLFSYSTEM void FileManager::WriteString(FILE *f,std::string str,bool bBinary)
{
	if(str.length() > 0)
		fwrite(&str[0],str.length(),1,f);
	if(bBinary == false)
		return;
	char n[1] = {'\0'};
	fwrite(n,1,1,f);
}

DLLFSYSTEM unsigned long long FileManager::GetFileSize(std::string name,fsys::SearchFlags fsearchmode)
{
	NormalizePath(name);
	if((fsearchmode &fsys::SearchFlags::Virtual) == fsys::SearchFlags::Virtual)
	{
		VData *vdata = GetVirtualData(name);
		if(vdata != NULL)
		{
			if(vdata->IsDirectory())
				return 0;
			VFile *file = static_cast<VFile*>(vdata);
			return file->GetSize();
		}
	}
	if((fsearchmode &fsys::SearchFlags::Package) == fsys::SearchFlags::Package)
	{
		uint64_t size = 0;
		for(auto &pair : m_packages)
		{
			if(pair.second->GetSize(name,size) == true)
				return size;
		}
	}
	if((fsearchmode &fsys::SearchFlags::Local) == fsys::SearchFlags::None)
		return 0;
	auto f = OpenFile(name.c_str(),"rb",fsearchmode);
	if(f == NULL)
		return 0;
	return f->GetSize();
}

DLLFSYSTEM VData *FileManager::GetVirtualData(std::string path)
{
	NormalizePath(path);
	VFile *f = m_vroot.GetFile(path);
	if(f != NULL)
		return f;
	VDirectory *dir = m_vroot.GetDirectory(path);
	if(dir != NULL)
		return dir;
	return NULL;
}

DLLFSYSTEM bool FileManager::Exists(std::string name,fsys::SearchFlags includeFlags,fsys::SearchFlags excludeFlags)
{
	NormalizePath(name);
	if(name.empty())
		return false;
	if((includeFlags &fsys::SearchFlags::Virtual) == fsys::SearchFlags::Virtual && GetVirtualData(name) != NULL)
		return true;
	if((includeFlags &fsys::SearchFlags::Package) == fsys::SearchFlags::Package)
	{
		for(auto &pair : m_packages)
		{
			if(pair.second->Exists(name,includeFlags) == true)
				return true;
		}
	}
	if((includeFlags &fsys::SearchFlags::Local) == fsys::SearchFlags::None)
		return false;
	std::string appPath = GetRootPath();
	MountIterator it(m_customMount);
	std::string mountPath;
	bool bAbsolute = false;
	while(it.GetNextDirectory(mountPath,includeFlags,excludeFlags,bAbsolute))
	{
		auto rootPath = (bAbsolute == false) ? appPath : "";
		if((update_file_insensitive_path_components_and_get_flags(rootPath,mountPath,name) &FVFILE_INVALID) == 0)
			return true;
	}
	return false;
}

#undef GetFileAttributes
DLLFSYSTEM unsigned long long FileManager::GetFileAttributes(std::string name)
{
	NormalizePath(name);
	std::string appPath = GetRootPath() +"\\";
	MountIterator it(m_customMount);
	std::string mountPath;
	bool bAbsolute = false;
	while(it.GetNextDirectory(mountPath,fsys::SearchFlags::Local,fsys::SearchFlags::None,bAbsolute))
	{
		std::string fpath = mountPath +"\\" +name;
		if(bAbsolute == false)
			fpath = appPath +"\\" +fpath;
#ifdef __linux__
		class stat st;
		std::replace(fpath.begin(),fpath.end(),'\\','/');
		if(stat(fpath.c_str(),&st) != -1)
		{
			const bool isDir = (st.st_mode &S_IFDIR) != 0;
			if(isDir == true)
				return FILE_ATTRIBUTE_DIRECTORY;
			return FILE_ATTRIBUTE_NORMAL;
		}
#else
		unsigned int attr = GetFileAttributesA(fpath.c_str());
		if(attr != INVALID_FILE_ATTRIBUTES)
			return attr;
#endif
	}
	return INVALID_FILE_ATTRIBUTES;
}

DLLFSYSTEM unsigned long long FileManager::GetFileFlags(std::string name,fsys::SearchFlags includeFlags,fsys::SearchFlags excludeFlags)
{
	NormalizePath(name);
	if((includeFlags &fsys::SearchFlags::Virtual) == fsys::SearchFlags::Virtual)
	{
		VData *vdata = GetVirtualData(name);
		if(vdata != NULL)
		{
			unsigned long long flags = (FVFILE_READONLY | FVFILE_VIRTUAL);
			if(vdata->IsDirectory())
				flags |= FVFILE_DIRECTORY;
			return flags;
		}
	}
	if((includeFlags &fsys::SearchFlags::Package) == fsys::SearchFlags::Package)
	{
		uint64_t flags = 0;
		for(auto &pair : m_packages)
		{
			if(pair.second->GetFileFlags(name,includeFlags,flags) == true)
				return true;
		}
	}
	if((includeFlags &fsys::SearchFlags::Local) == fsys::SearchFlags::None)
		return FVFILE_INVALID;
	std::string appPath = GetRootPath() +"\\";
	MountIterator it(m_customMount);
	std::string mountPath;
	bool bAbsolute = false;
	while(it.GetNextDirectory(mountPath,includeFlags,excludeFlags,bAbsolute))
	{
		auto rootPath = (bAbsolute == false) ? appPath : "";
		auto flags = update_file_insensitive_path_components_and_get_flags(rootPath,mountPath,name);
		if(flags != FVFILE_INVALID)
			return flags;
	}
	return FVFILE_INVALID;
}

DLLFSYSTEM bool FileManager::IsFile(std::string name,fsys::SearchFlags fsearchmode)
{
	auto flags = GetFileFlags(name,fsearchmode);
	return (flags != FVFILE_INVALID && (flags &FVFILE_DIRECTORY) == 0) ? true : false;
}

DLLFSYSTEM bool FileManager::IsDir(std::string name,fsys::SearchFlags fsearchmode)
{
	return (GetFileFlags(name,fsearchmode) &FVFILE_DIRECTORY) == FVFILE_DIRECTORY;
}

bool FileManager::ExistsSystem(std::string name)
{
	name = GetNormalizedPath(name);
	fsys::impl::to_case_sensitive_path(name);
	return (get_file_flags(name) &FVFILE_INVALID) == 0;
}
bool FileManager::IsSystemFile(std::string name)
{
	name = GetNormalizedPath(name);
	fsys::impl::to_case_sensitive_path(name);
	return (get_file_flags(name) &FVFILE_DIRECTORY) == 0;
}
bool FileManager::IsSystemDir(std::string name)
{
	name = GetNormalizedPath(name);
	fsys::impl::to_case_sensitive_path(name);
	return (get_file_flags(name) &FVFILE_DIRECTORY) == FVFILE_DIRECTORY;
}

bool FileManager::CopySystemFile(const char *cfile,const char *cfNewPath)
{
	std::string file = GetCanonicalizedPath(cfile);
	auto src = OpenSystemFile(file.c_str(),"rb");
	if(src == NULL)
		return false;
	std::string fNewPath = GetCanonicalizedPath(cfNewPath);
	auto tgt = OpenSystemFile(fNewPath.c_str(),"wb");
	if(tgt == NULL)
		return false;
	unsigned long long size = src->GetSize();
	char *data = (size > 32768) ? (new char[32768]) : (new char[size]);
	while(size > 32768)
	{
		src->Read(&data[0],32768);
		tgt->Write(&data[0],32768);
		size -= 32768;
	}
	if(size > 0)
	{
		src->Read(&data[0],size);
		tgt->Write(&data[0],size);
	}
	delete[] data;
	return true;
}

bool FileManager::CopyFile(const char *cfile,const char *cfNewPath)
{
	std::string file = GetCanonicalizedPath(cfile);
	auto src = OpenFile(file.c_str(),"rb");
	if(src == NULL)
		return false;
	std::string fNewPath = GetCanonicalizedPath(cfNewPath);
	auto tgt = OpenFile<VFilePtrReal>(fNewPath.c_str(),"wb");
	if(tgt == NULL)
		return false;
	unsigned long long size = src->GetSize();
	char *data = (size > 32768) ? (new char[32768]) : (new char[size]);
	while(size > 32768)
	{
		src->Read(&data[0],32768);
		tgt->Write(&data[0],32768);
		size -= 32768;
	}
	if(size > 0)
	{
		src->Read(&data[0],size);
		tgt->Write(&data[0],size);
	}
	delete[] data;
	return true;
}
DLLFSYSTEM bool FileManager::MoveFile(const char *cfile,const char *cfNewPath)
{
	if(IsFile(cfile,fsys::SearchFlags::Local) == true) // It's a local file, just use the os' move functions
	{
		std::string root = GetRootPath();
		auto in = root +'\\';
		in += GetNormalizedPath(cfile);

		auto out = root +'\\';
		out += GetNormalizedPath(cfNewPath);
#ifdef _WIN32
		auto r = MoveFileA(in.c_str(),out.c_str());
		return (r != 0) ? true : false;
#else
		std::replace(in.begin(),in.end(),'\\','/');
		std::replace(out.begin(),out.end(),'\\','/');
		auto r = rename(in.c_str(),out.c_str());
		return (r == 0) ? true : false;
#endif
	}
	if(CopyFile(cfile,cfNewPath) == false)
		return false;
	RemoveFile(cfile);
	return true;
}

bool FileManager::ComparePath(const std::string &a,const std::string &b)
{
	auto na = GetCanonicalizedPath(a);
	auto nb = GetCanonicalizedPath(b);
	return ustring::compare(na,nb,false);
}

//////////////////////////

VFile *VDirectory::GetFile(std::string path)
{
	NormalizePath(path);
	size_t sp = path.find_first_of("/\\");
	if(sp != ustring::NOT_FOUND)
	{
		std::string subDir = path.substr(0,sp);
		path = path.substr(sp +1);
		VDirectory *sub = GetDirectory(subDir);
		if(sub == NULL)
			return NULL;
		return sub->GetFile(path);
	}
	for(unsigned int i=0;i<m_files.size();i++)
	{
		if(m_files[i]->IsFile() && m_files[i]->GetName() == path)
			return static_cast<VFile*>(m_files[i]);
	}
	return NULL;
}

VDirectory *VDirectory::GetDirectory(std::string path)
{
	NormalizePath(path);
	if(!path.empty() && path.back() == '\\')
		path = path.substr(0,path.length() -1);
	size_t sp = path.find_first_of("/\\");
	if(sp != ustring::NOT_FOUND)
	{
		std::string subDir = path.substr(0,sp);
		path = path.substr(sp +1);
		VDirectory *sub = GetDirectory(subDir);
		if(sub == NULL)
			return NULL;
		return sub->GetDirectory(path);
	}
	for(unsigned int i=0;i<m_files.size();i++)
	{
		if(m_files[i]->IsDirectory() && m_files[i]->GetName() == path)
			return static_cast<VDirectory*>(m_files[i]);
	}
	return NULL;
}

VDirectory *VDirectory::AddDirectory(std::string name)
{
	size_t sp = name.find_first_of("\\/");
	if(sp != ustring::NOT_FOUND)
	{
		std::string dir = name.substr(0,sp);
		VDirectory *subdir = AddDirectory(dir);
		std::string sub = name.substr(sp +1);
		return subdir->AddDirectory(sub);
	}
	VDirectory *find = GetDirectory(name);
	if(find != NULL)
		return find;
	find = new VDirectory(name);
	m_files.push_back(find);
	return find;
}

//////////////////////////
//////////////////////////

size_t VFilePtrInternalReal::Read(void *ptr,size_t size)
{
	return fread(ptr,1,size,m_file);
}

size_t VFilePtrInternalReal::Write(const void *ptr,size_t size)
{
	return fwrite(ptr,size,1,m_file);
}

unsigned long long VFilePtrInternalReal::Tell()
{
	return ftell(m_file);
}

void VFilePtrInternalReal::Seek(unsigned long long offset)
{
	fseek(m_file,static_cast<long>(offset),SEEK_SET);
}

int VFilePtrInternalReal::Eof()
{
	return feof(m_file);
}

int VFilePtrInternalReal::ReadChar()
{
	return fgetc(m_file);
}

void VFilePtrInternalReal::WriteString(std::string str) {FileManager::WriteString(m_file,str,m_bBinary);}

int VFilePtrInternalReal::WriteString(const char *str)
{
	unsigned int pos = 0;
	while(str[pos] != '\0')
	{
		Write<char>(str[pos]);
		pos++;
	}
	return 0;
}

//////////////////////////

size_t VFilePtrInternalVirtual::Read(void *ptr,size_t size)
{
	if(Eof() == EOF)
		return 0;
	unsigned long long szMin = m_file->GetSize() -m_offset;
	if(size > szMin)
		size = szMin;
	auto data = m_file->GetData();
	memcpy(ptr,data->data() +m_offset,size);
	m_offset += size;
	return size;
}

unsigned long long VFilePtrInternalVirtual::Tell()
{
	return m_offset;
}

void VFilePtrInternalVirtual::Seek(unsigned long long offset)
{
	m_offset = offset;
}

int VFilePtrInternalVirtual::Eof()
{
	return ((m_offset < m_file->GetSize()) ? 0 : EOF);
}

int VFilePtrInternalVirtual::ReadChar()
{
	if(Eof() == EOF)
		return EOF;
	auto data = m_file->GetData();
	char c = data->at(m_offset);
	m_offset++;
	return c;
}
