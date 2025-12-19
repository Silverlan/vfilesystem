// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "definitions.hpp"

#ifdef __linux__

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#define DIR_SEPARATOR '/'
#define DIR_SEPARATOR_OTHER '\\'

#elif _WIN32

#include "Shlwapi.h"
#define DIR_SEPARATOR '\\'
#define DIR_SEPARATOR_OTHER '/'
#include <wchar.h>

#endif

#include "bzlib_wrapper.hpp"

module pragma.filesystem;

import :file_system;
import :mount;
import :util;

#undef CopyFile
#undef RemoveDirectory
#undef MoveFile

std::optional<std::wstring> string_to_wstring(const std::string &str)
{
	try {
		return pragma::string::string_to_wstring(str);
	}
	catch(const std::runtime_error &err) {
		return {};
	}
	return {};
}

static pragma::filesystem::FVFile get_file_flags(const std::string &fpath)
{
#ifdef __linux__
	auto npath = fpath;
	std::replace(npath.begin(), npath.end(), '\\', '/');
	class stat st;
	if(stat(npath.c_str(), &st) != -1) {
		pragma::filesystem::FVFile flags = pragma::filesystem::FVFile::None;
		const bool isDir = (st.st_mode & S_IFDIR) != 0;
		if(isDir == true)
			flags |= pragma::filesystem::FVFile::Directory;
		return flags;
	}
#else
	auto wstr = string_to_wstring(fpath);
	if(!wstr)
		return pragma::filesystem::FVFile::FileNotFound;
	unsigned long long attr = GetFileAttributesW(wstr->c_str());
	if(attr != INVALID_FILE_ATTRIBUTES) {
		pragma::filesystem::FVFile flags = pragma::filesystem::FVFile::None;
		if((attr & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
			flags |= pragma::filesystem::FVFile::Directory;
		return flags;
	}
#endif
	return pragma::filesystem::FVFile::Invalid;
}

static pragma::filesystem::FVFile update_file_insensitive_path_components_and_get_flags(std::string &appPath, std::string &mountPath, std::string &name)
{
	std::replace(appPath.begin(), appPath.end(), '\\', '/');
	std::replace(mountPath.begin(), mountPath.end(), '\\', '/');
	std::replace(name.begin(), name.end(), '\\', '/');
	pragma::string::replace(appPath, "//", "");
	pragma::string::replace(mountPath, "//", "");
	pragma::string::replace(name, "//", "");

#ifdef _WIN32
	if(appPath.empty() == false && appPath.front() == '/')
		appPath = appPath.substr(1);
#endif
	if(appPath.empty() == false && appPath.back() == '/')
		appPath = appPath.substr(0, appPath.length() - 1);

	if(mountPath.empty() == false && mountPath.front() == '/')
		mountPath = mountPath.substr(1);
	if(mountPath.empty() == false && mountPath.back() == '/')
		mountPath = mountPath.substr(0, mountPath.length() - 1);

	if(name.empty() == false && name.front() == '/')
		name = name.substr(1);
	if(name.empty() == false && name.back() == '/')
		name = name.substr(0, name.length() - 1);

	if(appPath.empty() == false) {
		// The filesystem is case-insensitive, but the operating system may not be, so we
		// convert the path to the actual path on disk
		appPath = pragma::util::DirPath(appPath).GetString();
		if(!mountPath.empty())
			mountPath = pragma::util::DirPath(mountPath).GetString();
		auto fullPath = pragma::util::FilePath(appPath, mountPath, name).GetString();
		pragma::filesystem::impl::to_case_sensitive_path(fullPath);
		auto offset = 0ull;
		appPath = fullPath.substr(offset, appPath.length());
		offset += appPath.length();
		if(offset < fullPath.length() && fullPath[offset] == '/')
			++offset;
		mountPath = fullPath.substr(offset, mountPath.length());
		offset += mountPath.length();
		if(offset < fullPath.length() && fullPath[offset] == '/')
			++offset;
		name = fullPath.substr(offset, name.length());
		return get_file_flags(fullPath);
	}
	auto fullPath = mountPath + DIR_SEPARATOR + name;
	pragma::filesystem::impl::to_case_sensitive_path(fullPath);
	auto offset = 0ull;
	mountPath = fullPath.substr(offset, mountPath.length());
	offset += mountPath.length() + 1;
	name = fullPath.substr(offset, name.length());
	return get_file_flags(fullPath);
}

static pragma::filesystem::FVFile update_file_insensitive_path_components_and_get_flags(std::string &appPath, std::string &mountPath, bool mountPathAbsolute, std::string &path)
{
	if(mountPathAbsolute) {
		std::string mnt {};
		return update_file_insensitive_path_components_and_get_flags(mountPath, mnt, path);
	}
	return update_file_insensitive_path_components_and_get_flags(appPath, mountPath, path);
}

template<>
#ifdef _WIN32
DLLFSYSTEM
#endif
  pragma::filesystem::VFilePtrReal pragma::filesystem::FileManager::OpenFile<pragma::filesystem::VFilePtrReal>(const char *cpath, const char *mode, std::string *optOutErr, SearchFlags includeFlags, SearchFlags excludeFlags)
{
	return std::static_pointer_cast<VFilePtrInternalReal>(OpenFile(cpath, mode, optOutErr, includeFlags, excludeFlags));
}

template<>
#ifdef _WIN32
DLLFSYSTEM
#endif
  pragma::filesystem::VFilePtrVirtual pragma::filesystem::FileManager::OpenFile<pragma::filesystem::VFilePtrVirtual>(const char *cpath, const char *mode, std::string *optOutErr, SearchFlags includeFlags, SearchFlags excludeFlags)
{
	return std::static_pointer_cast<VFilePtrInternalVirtual>(OpenFile(cpath, mode, optOutErr, includeFlags, excludeFlags));
}

decltype(pragma::filesystem::FileManager::m_vroot) pragma::filesystem::FileManager::m_vroot;
decltype(pragma::filesystem::FileManager::m_packages) pragma::filesystem::FileManager::m_packages;
decltype(pragma::filesystem::FileManager::m_customMount) pragma::filesystem::FileManager::m_customMount;
decltype(pragma::filesystem::FileManager::m_rootPath) pragma::filesystem::FileManager::m_rootPath;
decltype(pragma::filesystem::FileManager::m_customFileHandler) pragma::filesystem::FileManager::m_customFileHandler = nullptr;

static std::shared_mutex g_customMountMutex {};
static std::mutex g_packageMutex {};
static std::shared_mutex g_rootPathMutex {};

void pragma::filesystem::FileManager::SetCustomFileHandler(const std::function<VFilePtr(const std::string &, const char *mode)> &fHandler) { m_customFileHandler = fHandler; }

std::string pragma::filesystem::FileManager::GetNormalizedPath(std::string path)
{
#ifdef _WIN32
	pragma::string::to_lower(path);
#endif
	path = GetCanonicalizedPath(path);
	return path;
}

std::string pragma::filesystem::FileManager::GetProgramPath() { return util::get_program_path(); }

void pragma::filesystem::FileManager::RemoveCustomMountDirectory(const char *cpath)
{
	auto path = GetCanonicalizedPath(cpath);
	std::unique_lock lock {g_customMountMutex};
	auto it = std::find_if(m_customMount.begin(), m_customMount.end(), [&path](const MountDirectory &mount) { return (mount.directory == path) ? true : false; });
	if(it == m_customMount.end())
		return;
	m_customMount.erase(it);
}

void pragma::filesystem::FileManager::AddCustomMountDirectory(const char *cpath, SearchFlags searchMode) { return AddCustomMountDirectory(cpath, false, searchMode); }

void pragma::filesystem::FileManager::AddCustomMountDirectory(const char *cpath, bool bAbsolutePath, SearchFlags searchMode)
{
	searchMode |= SearchFlags::Local;
	searchMode &= ~SearchFlags::NoMounts;
	searchMode &= ~SearchFlags::Virtual;
	searchMode &= ~SearchFlags::Package;
	auto path = GetCanonicalizedPath(cpath);
#ifdef __linux__
	std::replace(path.begin(), path.end(), '\\', '/');
#endif
	std::unique_lock lock {g_customMountMutex};
	for(auto it = m_customMount.begin(); it != m_customMount.end(); ++it) {
		auto &mount = *it;
		if(mount.directory == path) {
			mount.searchMode = searchMode;
			return;
		}
	}
	auto *cache = get_root_path_file_cache_manager();
	if(cache) {
		for(auto &[name, fcache] : cache->GetCaches()) {
			//Before we actually request to cache all entries, nativize the file entry.
			auto cachedPath = bAbsolutePath ? "" : util::DirPath(fcache->GetRootPath(), path).GetString();
			//cachedPath.make_preferred();
			std::replace(cachedPath.begin(), cachedPath.end(), DIR_SEPARATOR_OTHER, DIR_SEPARATOR);
			fcache->QueuePath(cachedPath);
		}
	}

	m_customMount.push_back(MountDirectory(path, bAbsolutePath, searchMode));
}

void pragma::filesystem::FileManager::ClearCustomMountDirectories()
{
	std::unique_lock lock {g_customMountMutex};
	m_customMount.clear();
}

#define NormalizePath(path) path = pragma::fs::get_normalized_path(path);

std::pair<pragma::filesystem::VDirectory *, pragma::filesystem::VFile *> pragma::filesystem::FileManager::AddVirtualFile(std::string path, const std::shared_ptr<std::vector<uint8_t>> &data)
{
	std::replace(path.begin(), path.end(), DIR_SEPARATOR_OTHER, DIR_SEPARATOR);

	NormalizePath(path);
	unsigned int cur = 0;
	std::string sub;
	VDirectory *dir = &m_vroot;
	while(cur < path.size() && path[cur] != '\0') {
		if(path[cur] == '/' || path[cur] == '\\') {
			string::to_lower(sub);
			dir = dir->AddDirectory(sub);
			sub = "";
		}
		else
			sub += path[cur];
		cur++;
	}
	string::to_lower(sub);
	VFile *f = new VFile(sub, data);
	dir->Add(f);
	return {dir, f};
}

pragma::filesystem::VDirectory *pragma::filesystem::FileManager::GetRootDirectory() { return &m_vroot; }
std::string pragma::filesystem::FileManager::GetRootPath()
{
	std::shared_lock lock {g_rootPathMutex};
	if(m_rootPath == nullptr)
		return GetProgramPath();
	return *m_rootPath.get();
}
void pragma::filesystem::FileManager::SetAbsoluteRootPath(const std::string &path)
{
	std::unique_lock lock {g_rootPathMutex};
	m_rootPath = nullptr;
	if(!path.empty()) {
		auto rootPath = GetCanonicalizedPath(path);
		if(rootPath.back() == DIR_SEPARATOR)
			rootPath = rootPath.substr(0, rootPath.size() - 1);
		m_rootPath = std::unique_ptr<std::string>(new std::string(rootPath));
	}
}
void pragma::filesystem::FileManager::SetRootPath(const std::string &path)
{
	auto r = GetProgramPath();
	r += DIR_SEPARATOR;
	r += path;
	SetAbsoluteRootPath(r);
}

bool pragma::filesystem::FileManager::IsWriteMode(const char *mode)
{
	unsigned int c = 0;
	while(mode[c] != '\0') {
		if(mode[c] == 'w' || mode[c] == 'W' || mode[c] == 'a' || mode[c] == 'A' || mode[c] == '+')
			return true;
		c++;
	}
	return false;
}

bool pragma::filesystem::FileManager::IsBinaryMode(const char *mode)
{
	unsigned int c = 0;
	while(mode[c] != '\0') {
		if(mode[c] == 'b' || mode[c] == 'B')
			return true;
		c++;
	}
	return false;
}

pragma::filesystem::VFilePtrReal pragma::filesystem::FileManager::OpenSystemFile(const char *cpath, const char *mode, std::string *optOutErr)
{
	std::string path = GetCanonicalizedPath(cpath);
	auto ptrReal = std::make_shared<VFilePtrInternalReal>();
	int err = 0;
	if(!ptrReal->Construct(path.c_str(), mode, &err)) {
		if(optOutErr)
			*optOutErr = std::strerror(err);
		return nullptr;
	}
	ptrReal->m_bBinary = IsBinaryMode(mode);
	ptrReal->m_bRead = !IsWriteMode(mode);
	if(!ptrReal->m_bRead)
		update_file_index_cache(path, true);
	return ptrReal;
}

pragma::filesystem::VFilePtr pragma::filesystem::FileManager::OpenPackageFile(const char *packageName, const char *cpath, const char *mode, SearchFlags includeFlags, SearchFlags excludeFlags)
{
	std::string path = GetCanonicalizedPath(cpath);
	bool bBinary = IsBinaryMode(mode);
	bool bWrite = IsWriteMode(mode);
	if(bWrite == true)
		return nullptr;
	std::unique_lock lock {g_packageMutex};
	auto it = m_packages.find(packageName);
	if(it == m_packages.end())
		return nullptr;
	return it->second->OpenFile(path, bBinary, includeFlags, excludeFlags);
}

pragma::filesystem::PackageManager *pragma::filesystem::FileManager::GetPackageManager(const std::string &name)
{
	std::unique_lock lock {g_packageMutex};
	auto it = m_packages.find(name);
	if(it == m_packages.end())
		return nullptr;
	return it->second.get();
}

pragma::filesystem::VFilePtr pragma::filesystem::FileManager::OpenFile(const char *cpath, const char *mode, std::string *optOutErr, SearchFlags includeFlags, SearchFlags excludeFlags)
{
	std::string path = GetCanonicalizedPath(cpath);
	if(m_customFileHandler != nullptr) {
		auto f = m_customFileHandler(path, mode);
		if(f != nullptr)
			return f;
	}
	VFilePtr pfile;
	bool bBinary = IsBinaryMode(mode);
	bool bWrite = IsWriteMode(mode);
	if(bWrite) // Can't write virtual or packed file, so it must be on hardspace
	{
		if((includeFlags & SearchFlags::Local) == SearchFlags::None)
			return NULL;
		std::string fpath = util::FilePath(get_program_write_path(), path).GetString();
		auto ptrReal = std::make_shared<VFilePtrInternalReal>();
		if(ptrReal->Construct(fpath.c_str(), mode)) {
			pfile = ptrReal;
			pfile->m_bBinary = bBinary;
			pfile->m_bRead = !bWrite;
			if(bWrite)
				update_file_index_cache(fpath, true);
			return pfile;
		}
	}
#ifdef _WIN32
	pragma::string::to_lower(path);
#endif
	if((includeFlags & SearchFlags::Virtual) == SearchFlags::Virtual) {
		VFile *f = m_vroot.GetFile(path);
		if(f != NULL) {
			auto pfile = std::make_shared<VFilePtrInternalVirtual>(f);
			pfile->m_bBinary = bBinary;
			pfile->m_bRead = true;
			return pfile;
		}
	}
	if((includeFlags & SearchFlags::Package) == SearchFlags::Package) {
		std::unique_lock lock {g_packageMutex};
		for(auto &pair : m_packages) {
			pfile = pair.second->OpenFile(path, bBinary, includeFlags, excludeFlags);
			if(pfile != nullptr)
				return pfile;
		}
	}
	if((includeFlags & SearchFlags::Local) == SearchFlags::None)
		return NULL;

	bool bFound = false;
	std::string fpath;
	std::shared_lock lock {g_customMountMutex};
	for(auto &rootPath : get_absolute_root_paths()) {
		auto appPath = rootPath.GetString();
		bool bAbsolute = false;
		if((includeFlags & SearchFlags::NoMounts) == SearchFlags::None) {
			MountIterator it(m_customMount);
			std::string mountPath;
			while(bFound == false && it.GetNextDirectory(mountPath, includeFlags, excludeFlags, bAbsolute)) {
				fpath = GetNormalizedPath(mountPath + DIR_SEPARATOR + path);
				bFound = ((update_file_insensitive_path_components_and_get_flags(appPath, mountPath, bAbsolute, path) & (FVFile::Invalid | FVFile::Directory)) == FVFile::None) ? true : false;
			}
		}
		if(bFound) {
			if(!bAbsolute)
				fpath = util::FilePath(appPath, fpath).GetString();
			break;
		}
	}
	if(bFound == false)
		fpath = path;
	auto ptrReal = std::make_shared<VFilePtrInternalReal>();
	int err = 0;
	if(!ptrReal->Construct(fpath.c_str(), mode, &err)) {
		if(optOutErr)
			*optOutErr = std::strerror(err);
		return NULL;
	}
	pfile = ptrReal;
	pfile->m_bBinary = bBinary;
	pfile->m_bRead = true;
	return pfile;
}

pragma::filesystem::Package *pragma::filesystem::FileManager::LoadPackage(std::string package, SearchFlags searchMode)
{
	std::unique_lock lock {g_packageMutex};
	for(auto &pair : m_packages) {
		auto *pck = pair.second->LoadPackage(package, searchMode);
		if(pck != nullptr)
			return pck;
	}
	return nullptr;
}

void pragma::filesystem::FileManager::ClearPackages(SearchFlags searchMode)
{
	std::unique_lock lock {g_packageMutex};
	for(auto &pair : m_packages)
		pair.second->ClearPackages(searchMode);
}

void pragma::filesystem::FileManager::RegisterPackageManager(const std::string &name, std::unique_ptr<PackageManager> pm)
{
	std::unique_lock lock {g_packageMutex};
	m_packages.insert(std::make_pair(name, std::move(pm)));
}

bool pragma::filesystem::FileManager::RemoveSystemFile(const char *file)
{
	std::string path = GetCanonicalizedPath(file);
	std::replace(path.begin(), path.end(), '\\', '/');
#ifdef _WIN32
	auto wpath = string_to_wstring(path);
	if(!wpath)
		return false;
	if(_wremove(wpath->data()) == 0) {
#else
	if(remove(path.c_str()) == 0) {
#endif
		update_file_index_cache(path, true);
		return true;
	}
	return false;
}

bool pragma::filesystem::FileManager::RemoveFile(const char *file)
{
	auto fcanon = GetCanonicalizedPath(file);
	auto r = RemoveSystemFile((GetRootPath() + '\\' + fcanon).c_str());
	if(r)
		update_file_index_cache(fcanon, true);
	return r;
}

static bool remove_directory(const std::string &rootPath, const char *cdir, const std::function<void(const char *, std::vector<std::string> *, std::vector<std::string> *, bool)> &fFindFiles, const std::function<bool(const char *)> &fRemoveFile)
{
	std::string dir = pragma::filesystem::FileManager::GetCanonicalizedPath(cdir);
	std::string dirSub = dir;
	dirSub += DIR_SEPARATOR;
	std::string dirSearch = dirSub;
	dirSearch += '*';
	std::vector<std::string> files;
	std::vector<std::string> dirs;
	fFindFiles(dirSearch.c_str(), &files, &dirs, false);
	for(auto it = dirs.begin(); it != dirs.end(); it++)
		remove_directory(rootPath, (dirSub + *it).c_str(), fFindFiles, fRemoveFile);
	for(auto it = files.begin(); it != files.end(); it++)
		fRemoveFile((dirSub + *it).c_str());
	std::string p = rootPath + dir;
#ifdef _WIN32
	auto wp = string_to_wstring(p);
	if(!wp)
		return false;
	auto r = ::RemoveDirectoryW(wp->c_str());
	if(r != 0)
		pragma::filesystem::update_file_index_cache(p, true);
	return (r != 0) ? true : false;
#else
	std::replace(p.begin(), p.end(), '\\', '/');
	auto r = rmdir(p.c_str());
	if(r == 0)
		pragma::filesystem::update_file_index_cache(p, true);
	return (r == 0) ? true : false;
#endif
}

bool pragma::filesystem::FileManager::RemoveSystemDirectory(const char *cdir) { return ::remove_directory("", cdir, FindSystemFiles, RemoveSystemFile); }
bool pragma::filesystem::FileManager::RemoveDirectory(const char *cdir)
{
	return ::remove_directory(GetRootPath() + '\\', cdir, [](const char *cfind, std::vector<std::string> *resfiles, std::vector<std::string> *resdirs, bool bKeepPath) { FindFiles(cfind, resfiles, resdirs, bKeepPath); }, RemoveFile);
}

static bool rename_file(const std::string &root, const std::string &file, const std::string &fNewName)
{
	std::string path = root + pragma::filesystem::FileManager::GetCanonicalizedPath(file);
	std::string pathNew = root + pragma::filesystem::FileManager::GetCanonicalizedPath(fNewName);
	std::replace(path.begin(), path.end(), '\\', '/');
	std::replace(pathNew.begin(), pathNew.end(), '\\', '/');
	pragma::filesystem::impl::to_case_sensitive_path(path);
#ifdef _WIN32
	auto wpath = string_to_wstring(path);
	auto wpathNew = string_to_wstring(pathNew);
	if(!wpath || !wpathNew)
		return false;
	if(_wrename(wpath->c_str(), wpathNew->c_str()) == 0) {
#else
	if(rename(path.c_str(), pathNew.c_str()) == 0) {
#endif
		pragma::filesystem::update_file_index_cache(path, true);
		pragma::filesystem::update_file_index_cache(pathNew, true);
		return true;
	}
	return false;
}

bool pragma::filesystem::FileManager::RenameSystemFile(const char *file, const char *fNewName) { return ::rename_file("", file, fNewName); }

bool pragma::filesystem::FileManager::RenameFile(const char *file, const char *fNewName) { return ::rename_file(GetRootPath() + '\\', file, fNewName); }

std::vector<std::string> pragma::filesystem::FileManager::FindAbsolutePaths(std::string path, SearchFlags includeFlags, SearchFlags excludeFlags, bool exitEarly)
{
	NormalizePath(path);
	std::vector<std::string> paths;
	std::shared_lock lock {g_customMountMutex};
	for(auto &rootPath : get_absolute_root_paths()) {
		auto appPath = rootPath.GetString();
		MountIterator it(m_customMount);
		std::string mountPath;
		auto bAbsolute = false;
		while(it.GetNextDirectory(mountPath, includeFlags, excludeFlags, bAbsolute)) {
			auto fpath = GetNormalizedPath(mountPath + DIR_SEPARATOR + path);
			impl::to_case_sensitive_path(fpath);
			if((update_file_insensitive_path_components_and_get_flags(appPath, mountPath, bAbsolute, path) & FVFile::Invalid) == FVFile::None) {
				std::string rpath = fpath;
				if(bAbsolute == false)
					rpath = appPath + rpath;
				util::canonicalize_path(rpath);
#ifdef __linux__
				std::replace(rpath.begin(), rpath.end(), '\\', '/');
#endif
				if(paths.size() == paths.capacity())
					paths.reserve(paths.size() * 2 + 5);
				paths.push_back(std::move(rpath));
				if(exitEarly)
					goto endLoop;
			}
		}
	}

endLoop:

	return paths;
}

std::vector<std::string> pragma::filesystem::FileManager::FindAbsolutePaths(std::string path, SearchFlags includeFlags, SearchFlags excludeFlags) { return FindAbsolutePaths(path, includeFlags, excludeFlags, false); }

bool pragma::filesystem::FileManager::FindAbsolutePath(std::string path, std::string &rpath, SearchFlags includeFlags, SearchFlags excludeFlags)
{
	auto paths = FindAbsolutePaths(path, includeFlags, excludeFlags, true);
	if(paths.empty())
		return false;
	rpath = std::move(paths.front());
	return true;
}

bool pragma::filesystem::FileManager::FindRelativePath(std::string path, std::string &rpath)
{
	util::Path ppath {path};
	std::shared_lock lock {g_customMountMutex};
	for(auto &rootPath : get_absolute_root_paths()) {
		auto appPath = util::DirPath(rootPath.GetString());
		if(ppath.MakeRelative(appPath)) {
			rpath = ppath.GetString();
			return true;
		}
	}
	return false;
}

bool pragma::filesystem::FileManager::AbsolutePathToCustomMountPath(const std::string &strPath, std::string &outMountPath, std::string &relativePath, SearchFlags includeFlags, SearchFlags excludeFlags)
{
	std::shared_lock lock {g_customMountMutex};
	MountIterator it(m_customMount);
	std::string mountPath;
	util::Path path {strPath};
	auto bAbsolute = false;
	while(it.GetNextDirectory(mountPath, includeFlags, excludeFlags, bAbsolute)) {
		if(bAbsolute == true)
			continue;
		if(path.MakeRelative(mountPath) == false)
			continue;
		outMountPath = std::move(mountPath);
		relativePath = path.Move();
		return true;
	}
	return false;
}

bool pragma::filesystem::FileManager::FindLocalPath(std::string path, std::string &rpath, SearchFlags includeFlags, SearchFlags excludeFlags)
{
	NormalizePath(path);
	std::shared_lock lock {g_customMountMutex};
	for(auto &rootPath : get_absolute_root_paths()) {
		auto appPath = rootPath.GetString();
		MountIterator it(m_customMount);
		std::string mountPath;
		auto bAbsolute = false;
		while(it.GetNextDirectory(mountPath, includeFlags, excludeFlags, bAbsolute)) {
			if(bAbsolute == true)
				continue;
			auto fpath = GetNormalizedPath(mountPath + "\\" + path);
			if((update_file_insensitive_path_components_and_get_flags(appPath, mountPath, path) & FVFile::Invalid) == FVFile::None) {
				rpath = fpath;
				return true;
			}
		}
	}
	return false;
}

static void find_files(const std::string &path, const std::string &ctarget, const std::string &localMountPath, std::vector<std::string> *resfiles, std::vector<std::string> *resdirs, bool bKeepPath, size_t szFiles = 0, size_t szDirs = 0)
{
	auto numFilesSpecial = (resfiles != nullptr) ? (resfiles->size() - szFiles) : 0;
	auto numDirsSpecial = (resdirs != nullptr) ? (resdirs->size() - szDirs) : 0;
#ifdef __linux__
	std::string __path = path;
	std::replace(__path.begin(), __path.end(), '\\', '/');
	DIR *dir = opendir(__path.c_str());
	if(dir != nullptr) {
		class stat st;
		dirent *ent;
		while((ent = readdir(dir)) != nullptr) {
			std::string fileName = ent->d_name;
			const std::string fullFileName = __path + fileName;
			if(stat(fullFileName.c_str(), &st) != -1) {
				const bool isDir = (st.st_mode & S_IFDIR) != 0;
				if(resfiles != nullptr && isDir == false) {
					if(pragma::string::match(fileName.c_str(), ctarget, false)) {
						auto pathName = bKeepPath == false ? fileName : (localMountPath + fileName);
						if(!pragma::filesystem::impl::has_value(resfiles, szFiles, szFiles + numFilesSpecial, fileName, true))
							resfiles->push_back(pathName);
					}
				}
				if(resdirs != nullptr && isDir == true) {
					if(fileName != "." && fileName != ".." && pragma::string::match(fileName.c_str(), ctarget, false)) {
						auto pathName = bKeepPath == false ? fileName : (localMountPath + fileName);
						if(!pragma::filesystem::impl::has_value(resdirs, szDirs, szDirs + numDirsSpecial, fileName, true))
							resdirs->push_back(pathName);
					}
				}
			}
		}
		closedir(dir);
	}
#else
	auto searchPath = path + '*';
	auto wsearchPath = string_to_wstring(searchPath);
	if(!wsearchPath)
		return;
	WIN32_FIND_DATAW data;
	HANDLE hFind = FindFirstFileW(wsearchPath->c_str(), &data);
	while(hFind != INVALID_HANDLE_VALUE) {
		if(resfiles != NULL) {
			if((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
				auto fileName = pragma::string::wstring_to_string(data.cFileName);
				if(pragma::string::match(fileName, ctarget, false)) {
					auto pathName = bKeepPath == false ? fileName : (localMountPath + fileName);
					if(!pragma::filesystem::impl::has_value(resfiles, szFiles, szFiles + numFilesSpecial, fileName))
						resfiles->push_back(pathName);
				}
			}
		}
		if(resdirs != NULL) {
			if((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
				std::string fileName = pragma::string::wstring_to_string(data.cFileName);
				if(fileName != "." && fileName != ".." && pragma::string::match(fileName, ctarget, false)) {
					auto pathName = bKeepPath == false ? fileName : (localMountPath + fileName);
					if(!pragma::filesystem::impl::has_value(resdirs, szDirs, szDirs + numDirsSpecial, fileName))
						resdirs->push_back(pathName);
				}
			}
		}
		if(!FindNextFileW(hFind, &data)) {
			FindClose(hFind);
			break;
		}
	}
#endif
}

void pragma::filesystem::FileManager::FindFiles(const char *cfind, std::vector<std::string> *resfiles, std::vector<std::string> *resdirs, SearchFlags includeFlags, SearchFlags excludeFlags) { FindFiles(cfind, resfiles, resdirs, false, includeFlags, excludeFlags); }

static void get_find_string(const char *cfind, std::string &path, std::string &target, size_t &lbr)
{
	std::string find = cfind;
	NormalizePath(find);
	lbr = find.rfind(DIR_SEPARATOR);
	if(lbr == pragma::string::NOT_FOUND) {
		target = find;
		path = "";
	}
	else {
		target = find.substr(lbr + 1);
		path = find.substr(0, lbr);
		path += DIR_SEPARATOR;
	}
}

void pragma::filesystem::FileManager::FindFiles(const char *cfind, std::vector<std::string> *resfiles, std::vector<std::string> *resdirs, bool bKeepPath, SearchFlags includeFlags, SearchFlags excludeFlags)
{
	std::string path;
	std::string target;
	size_t lbr;
	get_find_string(cfind, path, target, lbr);

	auto szFiles = (resfiles != nullptr) ? resfiles->size() : 0;
	auto szDirs = (resdirs != nullptr) ? resdirs->size() : 0;
	if((includeFlags & SearchFlags::Virtual) == SearchFlags::Virtual) {
		VDirectory *dir = &m_vroot;
		if(lbr != string::NOT_FOUND)
			dir = dir->GetDirectory(path);
		if(dir != NULL) {
			std::vector<VData *> &files = dir->GetFiles();
			for(unsigned int i = 0; i < files.size(); i++) {
				VData *data = files[i];
				std::string name = data->GetName();
				if(data->IsFile()) {
					if(resfiles != NULL && string::match(name.c_str(), target, false)) {
						auto pathName = bKeepPath == false ? name : (path + name);
						if(!impl::has_value(resfiles, 0, resfiles->size(), name))
							resfiles->push_back(pathName);
					}
				}
				else {
					if(resdirs != NULL && string::match(name.c_str(), target, false)) {
						auto pathName = bKeepPath == false ? name : (path + name);
						if(!impl::has_value(resdirs, 0, resdirs->size(), name))
							resdirs->push_back(pathName);
					}
				}
			}
		}
	}
	if((includeFlags & SearchFlags::Package) == SearchFlags::Package) {
		std::unique_lock lock {g_packageMutex};
		for(auto &pair : m_packages)
			pair.second->FindFiles(cfind, path, resfiles, resdirs, bKeepPath, includeFlags);
	}
	if((includeFlags & SearchFlags::Local) == SearchFlags::None)
		return;
	std::string localPath = path;
	std::shared_lock lock {g_customMountMutex};
	for(auto &rootPath : get_absolute_root_paths()) {
		auto appPath = rootPath.GetString();
		MountIterator it(m_customMount);
		std::string mountPath;
		bool bAbsolute = false;
		while(it.GetNextDirectory(mountPath, includeFlags, excludeFlags, bAbsolute)) {
			std::string localMountPath = mountPath + DIR_SEPARATOR + localPath;
			if(bAbsolute == false)
				path = appPath + DIR_SEPARATOR + localMountPath;
			else
				path = localMountPath;
			::find_files(path, target, localMountPath, resfiles, resdirs, bKeepPath, szFiles, szDirs);
		}
	}
}

char pragma::filesystem::FileManager::GetDirectorySeparator() { return DIR_SEPARATOR; }

void pragma::filesystem::FileManager::FindSystemFiles(const char *path, std::vector<std::string> *resfiles, std::vector<std::string> *resdirs, bool bKeepPath)
{
	std::string npath;
	std::string target;
	size_t lbr;
	get_find_string(path, npath, target, lbr);
	::find_files(npath, target, "", resfiles, resdirs, bKeepPath);
}

std::string pragma::filesystem::FileManager::GetCanonicalizedPath(std::string path)
{
	util::canonicalize_path(path);
	std::replace(path.begin(), path.end(), DIR_SEPARATOR_OTHER, DIR_SEPARATOR);
	return path;
}

std::string pragma::filesystem::FileManager::GetSubPath(const std::string &root, std::string path)
{
	path = GetCanonicalizedPath(path);
	if(path.empty())
		return path;
	if(path[0] != DIR_SEPARATOR)
		path = DIR_SEPARATOR + path;
	return root + path;
}

std::string pragma::filesystem::FileManager::GetSubPath(std::string path) { return GetSubPath(GetRootPath(), path); }

static bool create_path(const std::string &root, const char *path)
{
	std::string p = path;
	p = pragma::filesystem::FileManager::GetCanonicalizedPath(p);
	if(p.empty())
		return true;
	size_t pos = 0;
	do {
		pos = p.find_first_of(DIR_SEPARATOR, pos + 1);
#ifdef __linux__
		struct stat st = {0};
		std::string subPath = pragma::filesystem::FileManager::GetSubPath(root, p.substr(0, pos));
		std::replace(subPath.begin(), subPath.end(), '\\', '/');
		const char *pSub = subPath.c_str();
		if(stat(pSub, &st) == -1) {
			try {
				if(!std::filesystem::create_directory(pSub))
					return false;
			}
			catch(const std::runtime_error &e) {
				return false;
			}
		}
#else
		auto wstr = string_to_wstring(pragma::filesystem::FileManager::GetSubPath(root, p.substr(0, pos)));
		if(!wstr)
			return false;
		if(CreateDirectoryW(wstr->c_str(), NULL) == 0 && GetLastError() != ERROR_ALREADY_EXISTS)
			return false;
#endif
	} while(pos != pragma::string::NOT_FOUND);
	return true;
}

bool pragma::filesystem::FileManager::CreateSystemPath(const std::string &root, const char *path) { return ::create_path(root, path); }

bool pragma::filesystem::FileManager::CreatePath(const char *path) { return ::create_path(GetRootPath(), path); }

#undef CreateDirectory
bool pragma::filesystem::FileManager::CreateSystemDirectory(const char *dir)
{
	std::string p = dir;
	p = GetCanonicalizedPath(p);
#ifdef __linux__
	std::replace(p.begin(), p.end(), '\\', '/');
	const char *pSub = p.c_str();
	struct stat st = {0};
	if(stat(pSub, &st) == -1) {
		try {
			if(!std::filesystem::create_directory(pSub))
				return false;
		}
		catch(const std::filesystem::filesystem_error &err) {
			return false;
		}
	}
#else
	auto wstr = string_to_wstring(p);
	if(!wstr)
		return false;
	if(CreateDirectoryW(wstr->c_str(), NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS)
		return true;
#endif
	return false;
}
bool pragma::filesystem::FileManager::CreateDirectory(const char *dir)
{
	std::string p = dir;
	p = GetCanonicalizedPath(p);
	return CreateSystemDirectory(GetSubPath(p).c_str());
}

void pragma::filesystem::FileManager::Close()
{
	std::unique_lock lock {g_packageMutex};
	m_packages.clear();
}

std::string pragma::filesystem::FileManager::GetPath(std::string &path)
{
	size_t lpos = path.find_last_of("/\\");
	if(lpos == string::NOT_FOUND)
		return "";
	return path.substr(0, lpos + 1);
}

std::string pragma::filesystem::FileManager::GetFile(std::string &path)
{
	size_t lpos = path.find_last_of("/\\");
	if(lpos == string::NOT_FOUND)
		return path;
	return path.substr(lpos + 1);
}

std::string pragma::filesystem::FileManager::ReadString(FILE *f)
{
	unsigned char c;
	std::string name = "";
	c = Read<char>(f);
	while(c != '\0') {
		name += c;
		c = Read<char>(f);
		if(feof(f))
			break;
	}
	return name;
}

void pragma::filesystem::FileManager::WriteString(FILE *f, std::string str, bool bBinary)
{
	if(str.length() > 0)
		fwrite(&str[0], str.length(), 1, f);
	if(bBinary == false)
		return;
	char n[1] = {'\0'};
	fwrite(n, 1, 1, f);
}

std::optional<std::filesystem::file_time_type> pragma::filesystem::FileManager::GetLastWriteTime(const std::string_view &path, SearchFlags includeFlags, SearchFlags excludeFlags)
{
	std::string rpath;
	if(!FindLocalPath(std::string {path}, rpath, includeFlags, excludeFlags))
		return {};
	try {
		auto ftime = std::filesystem::last_write_time(rpath);
		return ftime;
	}
	catch(std::filesystem::filesystem_error &e) {
		return {};
	}
	return {};
}

std::uint64_t pragma::filesystem::FileManager::GetFileSize(std::string name, SearchFlags fsearchmode)
{
	NormalizePath(name);
	if((fsearchmode & SearchFlags::Virtual) == SearchFlags::Virtual) {
		VData *vdata = GetVirtualData(name);
		if(vdata != NULL) {
			if(vdata->IsDirectory())
				return 0;
			VFile *file = static_cast<VFile *>(vdata);
			return file->GetSize();
		}
	}
	if((fsearchmode & SearchFlags::Package) == SearchFlags::Package) {
		std::unique_lock lock {g_packageMutex};
		uint64_t size = 0;
		for(auto &pair : m_packages) {
			if(pair.second->GetSize(name, size) == true)
				return size;
		}
	}
	if((fsearchmode & SearchFlags::Local) == SearchFlags::None)
		return 0;
	auto f = OpenFile(name.c_str(), "rb", nullptr, fsearchmode);
	if(f == NULL)
		return 0;
	return f->GetSize();
}

pragma::filesystem::VData *pragma::filesystem::FileManager::GetVirtualData(std::string path)
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

bool pragma::filesystem::FileManager::Exists(std::string name, SearchFlags includeFlags, SearchFlags excludeFlags)
{
	NormalizePath(name);
	if(name.empty())
		return false;
	if((includeFlags & SearchFlags::Virtual) == SearchFlags::Virtual && GetVirtualData(name) != NULL)
		return true;
	if((includeFlags & SearchFlags::Package) == SearchFlags::Package) {
		std::unique_lock lock {g_packageMutex};
		for(auto &pair : m_packages) {
			if(pair.second->Exists(name, includeFlags) == true)
				return true;
		}
	}
	if((includeFlags & SearchFlags::Local) == SearchFlags::None)
		return false;

	auto *fic = get_root_path_file_cache_manager();
	if(fic && fic->IsComplete())
		return fic->Exists(name);

	std::shared_lock lock {g_customMountMutex};
	for(auto &rootPath : get_absolute_root_paths()) {
		auto appPath = rootPath.GetString();
		MountIterator it(m_customMount);
		std::string mountPath;
		bool bAbsolute = false;
		while(it.GetNextDirectory(mountPath, includeFlags, excludeFlags, bAbsolute)) {
			if((update_file_insensitive_path_components_and_get_flags(appPath, mountPath, bAbsolute, name) & FVFile::Invalid) == FVFile::None)
				return true;
		}
	}
	return false;
}

#ifdef __linux__
#define FILE_ATTRIBUTE_NORMAL 0x80
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define INVALID_FILE_ATTRIBUTES ((unsigned int)-1)
#endif

static unsigned long long get_file_attributes(const std::string &fpath)
{
#ifdef __linux__
	class stat st;
	std::string npath = fpath;
	std::replace(npath.begin(), npath.end(), '\\', '/');
	if(stat(npath.c_str(), &st) == -1)
		return INVALID_FILE_ATTRIBUTES;
	const bool isDir = (st.st_mode & S_IFDIR) != 0;
	if(isDir == true)
		return FILE_ATTRIBUTE_DIRECTORY;
	return FILE_ATTRIBUTE_NORMAL;
#else
	auto wstr = string_to_wstring(fpath);
	if(!wstr)
		return INVALID_FILE_ATTRIBUTES;
	return GetFileAttributesW(wstr->c_str());
#endif
}

#undef GetFileAttributes
std::uint64_t pragma::filesystem::FileManager::GetFileAttributes(std::string name)
{
	NormalizePath(name);
	std::shared_lock lock {g_customMountMutex};
	for(auto &rootPath : get_absolute_root_paths()) {
		auto appPath = rootPath.GetString();
		MountIterator it(m_customMount);
		std::string mountPath;
		bool bAbsolute = false;
		while(it.GetNextDirectory(mountPath, SearchFlags::Local, SearchFlags::None, bAbsolute)) {
			std::string fpath = mountPath + DIR_SEPARATOR + name;
			if(bAbsolute == false)
				fpath = appPath + DIR_SEPARATOR + fpath;
			auto attrs = ::get_file_attributes(fpath);
			if(attrs != INVALID_FILE_ATTRIBUTES)
				return attrs;
		}
	}
	return INVALID_FILE_ATTRIBUTES;
}

pragma::filesystem::FVFile pragma::filesystem::FileManager::GetFileFlags(std::string name, SearchFlags includeFlags, SearchFlags excludeFlags)
{
	NormalizePath(name);
	if((includeFlags & SearchFlags::Virtual) == SearchFlags::Virtual) {
		VData *vdata = GetVirtualData(name);
		if(vdata != NULL) {
			auto flags = (FVFile::ReadOnly | FVFile::Virtual);
			if(vdata->IsDirectory())
				flags |= FVFile::Directory;
			return flags;
		}
	}
	if((includeFlags & SearchFlags::Package) == SearchFlags::Package) {
		std::unique_lock lock {g_packageMutex};
		FVFile flags = FVFile::None;
		for(auto &pair : m_packages) {
			if(pair.second->GetFileFlags(name, includeFlags, flags) == true)
				return flags;
		}
	}
	if((includeFlags & SearchFlags::Local) == SearchFlags::None)
		return FVFile::Invalid;

	auto *fic = get_root_path_file_cache_manager();
	if(fic && fic->IsComplete()) {
		auto type = fic->FindFileType(name);
		FVFile flags = FVFile::None;
		switch(type) {
		case FileIndexCache::Type::Directory:
			flags |= FVFile::Directory;
			break;
		case FileIndexCache::Type::Invalid:
			return FVFile::Invalid;
		default:
			break;
		}
		return flags;
	}

	std::shared_lock lock {g_customMountMutex};
	for(auto &rootPath : get_absolute_root_paths()) {
		auto appPath = rootPath.GetString();
		MountIterator it(m_customMount);
		std::string mountPath;
		bool bAbsolute = false;
		while(it.GetNextDirectory(mountPath, includeFlags, excludeFlags, bAbsolute)) {
			auto flags = update_file_insensitive_path_components_and_get_flags(appPath, mountPath, bAbsolute, name);
			if(flags != FVFile::Invalid)
				return flags;
		}
	}
	return FVFile::Invalid;
}

bool pragma::filesystem::FileManager::IsFile(std::string name, SearchFlags fsearchmode)
{
	auto flags = GetFileFlags(name, fsearchmode);
	return (flags != FVFile::Invalid && (flags & FVFile::Directory) == FVFile::None) ? true : false;
}

bool pragma::filesystem::FileManager::IsDir(std::string name, SearchFlags fsearchmode) { return (GetFileFlags(name, fsearchmode) & FVFile::Directory) == FVFile::Directory; }

bool pragma::filesystem::FileManager::ExistsSystem(std::string name)
{
	name = GetNormalizedPath(name);
#ifdef __linux__
	std::replace(name.begin(), name.end(), '\\', '/');
#endif
	impl::to_case_sensitive_path(name);
	return (::get_file_flags(name) & FVFile::Invalid) == FVFile::None;
}
bool pragma::filesystem::FileManager::IsSystemFile(std::string name)
{
	name = GetNormalizedPath(name);
#ifdef __linux__
	std::replace(name.begin(), name.end(), '\\', '/');
#endif
	impl::to_case_sensitive_path(name);
	return (::get_file_flags(name) & (FVFile::Directory | FVFile::Invalid)) == FVFile::None;
}
bool pragma::filesystem::FileManager::IsSystemDir(std::string name)
{
	name = GetNormalizedPath(name);
#ifdef __linux__
	std::replace(name.begin(), name.end(), '\\', '/');
#endif
	impl::to_case_sensitive_path(name);
	return (::get_file_flags(name) & (FVFile::Directory | FVFile::Invalid)) == FVFile::Directory;
}

bool pragma::filesystem::FileManager::CopySystemFile(const char *cfile, const char *cfNewPath)
{
	std::string file = GetCanonicalizedPath(cfile);
	auto src = OpenSystemFile(file.c_str(), "rb");
	if(src == NULL)
		return false;
	std::string fNewPath = GetCanonicalizedPath(cfNewPath);
	auto tgt = OpenSystemFile(fNewPath.c_str(), "wb");
	if(tgt == NULL)
		return false;
	unsigned long long size = src->GetSize();
	char *data = (size > 32768) ? (new char[32768]) : (new char[size]);
	while(size > 32768) {
		src->Read(&data[0], 32768);
		tgt->Write(&data[0], 32768);
		size -= 32768;
	}
	if(size > 0) {
		src->Read(&data[0], size);
		tgt->Write(&data[0], size);
	}
	delete[] data;
	if(is_executable(file))
		make_executable(fNewPath);
	return true;
}

bool pragma::filesystem::FileManager::CopyFile(const char *cfile, const char *cfNewPath)
{
	std::string file = GetCanonicalizedPath(cfile);
	auto src = OpenFile(file.c_str(), "rb");
	if(src == NULL)
		return false;
	std::string fNewPath = GetCanonicalizedPath(cfNewPath);
	auto tgt = OpenFile<VFilePtrReal>(fNewPath.c_str(), "wb");
	if(tgt == NULL)
		return false;
	unsigned long long size = src->GetSize();
	char *data = (size > 32768) ? (new char[32768]) : (new char[size]);
	while(size > 32768) {
		src->Read(&data[0], 32768);
		tgt->Write(&data[0], 32768);
		size -= 32768;
	}
	if(size > 0) {
		src->Read(&data[0], size);
		tgt->Write(&data[0], size);
	}
	delete[] data;
	auto *srcR = dynamic_cast<VFilePtrInternalReal *>(src.get());
	auto *tgtR = dynamic_cast<VFilePtrInternalReal *>(tgt.get());
	if(srcR && tgtR && is_executable(srcR->GetPath()))
		make_executable(tgtR->GetPath());
	return true;
}
bool pragma::filesystem::FileManager::MoveFile(const char *cfile, const char *cfNewPath)
{
	if(IsFile(cfile, SearchFlags::Local) == true) // It's a local file, just use the os' move functions
	{
		std::string root = GetRootPath();
		auto in = root + DIR_SEPARATOR;
		in += GetNormalizedPath(cfile);

		auto out = root + DIR_SEPARATOR;
		out += GetNormalizedPath(cfNewPath);
#ifdef _WIN32
		auto win = string_to_wstring(in);
		auto wout = string_to_wstring(out);
		if(!win || !wout)
			return false;
		auto r = MoveFileW(win->c_str(), wout->c_str());
		return (r != 0) ? true : false;
#else
		std::replace(in.begin(), in.end(), '\\', '/');
		std::replace(out.begin(), out.end(), '\\', '/');
		auto r = rename(in.c_str(), out.c_str());
		return (r == 0) ? true : false;
#endif
	}
	if(CopyFile(cfile, cfNewPath) == false)
		return false;
	RemoveFile(cfile);
	return true;
}

bool pragma::filesystem::FileManager::ComparePath(const std::string &a, const std::string &b)
{
	auto na = GetCanonicalizedPath(a);
	auto nb = GetCanonicalizedPath(b);
	return string::compare(na, nb, false);
}

//////////////////////////

pragma::filesystem::VFile *pragma::filesystem::VDirectory::GetFile(std::string path)
{
	string::to_lower(path);
	NormalizePath(path);
	size_t sp = path.find_first_of("/\\");
	if(sp != string::NOT_FOUND) {
		std::string subDir = path.substr(0, sp);
		path = path.substr(sp + 1);
		VDirectory *sub = GetDirectory(subDir);
		if(sub == NULL)
			return NULL;
		return sub->GetFile(path);
	}
	for(unsigned int i = 0; i < m_files.size(); i++) {
		if(m_files[i]->IsFile() && string::compare(m_files[i]->GetName().c_str(), path.c_str(), false))
			return static_cast<VFile *>(m_files[i]);
	}
	return NULL;
}

pragma::filesystem::VDirectory *pragma::filesystem::VDirectory::GetDirectory(std::string path)
{
	NormalizePath(path);
	if(!path.empty() && path.back() == DIR_SEPARATOR)
		path = path.substr(0, path.length() - 1);
	size_t sp = path.find_first_of("/\\");
	if(sp != string::NOT_FOUND) {
		std::string subDir = path.substr(0, sp);
		path = path.substr(sp + 1);
		VDirectory *sub = GetDirectory(subDir);
		if(sub == NULL)
			return NULL;
		return sub->GetDirectory(path);
	}
	for(unsigned int i = 0; i < m_files.size(); i++) {
		if(m_files[i]->IsDirectory() && m_files[i]->GetName() == path)
			return static_cast<VDirectory *>(m_files[i]);
	}
	return NULL;
}

pragma::filesystem::VDirectory *pragma::filesystem::VDirectory::AddDirectory(std::string name)
{
	size_t sp = name.find_first_of("\\/");
	if(sp != string::NOT_FOUND) {
		std::string dir = name.substr(0, sp);
		VDirectory *subdir = AddDirectory(dir);
		std::string sub = name.substr(sp + 1);
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

size_t pragma::filesystem::VFilePtrInternalReal::Read(void *ptr, size_t size) { return fread(ptr, 1, size, m_file); }

size_t pragma::filesystem::VFilePtrInternalReal::Write(const void *ptr, size_t size) { return fwrite(ptr, size, 1, m_file); }

unsigned long long pragma::filesystem::VFilePtrInternalReal::Tell() { return ftell(m_file); }

void pragma::filesystem::VFilePtrInternalReal::Seek(unsigned long long offset) { fseek(m_file, static_cast<long>(offset), SEEK_SET); }

int pragma::filesystem::VFilePtrInternalReal::Eof() { return feof(m_file); }

int pragma::filesystem::VFilePtrInternalReal::ReadChar() { return fgetc(m_file); }

int pragma::filesystem::VFilePtrInternalReal::WriteString(const std::string_view &sv, bool withBinaryZeroByte)
{
	auto len = sv.length();
	if(len == 0)
		return 0;
	len = fwrite(sv.data(), 1, len, m_file);
	if(withBinaryZeroByte && m_bBinary) {
		char n[1] = {'\0'};
		len += fwrite(n, 1, 1, m_file);
	}
	return len;
}

//////////////////////////

size_t pragma::filesystem::VFilePtrInternalVirtual::Read(void *ptr, size_t size)
{
	if(Eof() == EOF)
		return 0;
	unsigned long long szMin = m_file->GetSize() - m_offset;
	if(size > szMin)
		size = szMin;
	auto data = m_file->GetData();
	memcpy(ptr, data->data() + m_offset, size);
	m_offset += size;
	return size;
}

unsigned long long pragma::filesystem::VFilePtrInternalVirtual::Tell() { return m_offset; }

void pragma::filesystem::VFilePtrInternalVirtual::Seek(unsigned long long offset) { m_offset = offset; }

int pragma::filesystem::VFilePtrInternalVirtual::Eof() { return ((m_offset < m_file->GetSize()) ? 0 : EOF); }

int pragma::filesystem::VFilePtrInternalVirtual::ReadChar()
{
	if(Eof() == EOF)
		return EOF;
	auto data = m_file->GetData();
	char c = data->at(m_offset);
	m_offset++;
	return c;
}
