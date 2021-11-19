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
#include <sharedutils/util_path.hpp>
#include "fsys/fsys_searchflags.hpp"
#include "fsys/fsys_package.hpp"
#include "fsys/file_index_cache.hpp"
#include "impl_fsys_util.hpp"
#include <filesystem>
#include <array>
#include <iostream>

static std::unique_ptr<fsys::FileIndexCache> g_fileIndexCache {};
void filemanager::set_use_file_index_cache(bool useCache)
{
	if(!useCache)
	{
		g_fileIndexCache = nullptr;
		return;
	}
	g_fileIndexCache = std::make_unique<fsys::FileIndexCache>();
	reset_file_index_cache();
}
fsys::FileIndexCache *filemanager::get_file_index_cache() {return g_fileIndexCache.get();}
bool filemanager::is_file_index_cache_enabled() {return g_fileIndexCache != nullptr;}
void filemanager::reset_file_index_cache()
{
	if(!g_fileIndexCache)
		return;
	g_fileIndexCache->Reset(get_root_path());
}

std::string filemanager::detail::to_string_mode(filemanager::FileMode mode)
{
	std::string strMode;
	if(umath::is_flag_set(mode,filemanager::FileMode::Read | filemanager::FileMode::Write))
		strMode = "w+";
	if(umath::is_flag_set(mode,filemanager::FileMode::Read | filemanager::FileMode::Append))
		strMode = "a+";
	if(umath::is_flag_set(mode,filemanager::FileMode::Read))
		strMode = "r";
	if(umath::is_flag_set(mode,filemanager::FileMode::Write))
		strMode = "w";
	if(umath::is_flag_set(mode,filemanager::FileMode::Append))
		strMode = "a";
	if(umath::is_flag_set(mode,filemanager::FileMode::Binary))
		strMode += "b";
	return strMode;
}

VFilePtr filemanager::open_file(const std::string_view &path,FileMode mode,fsys::SearchFlags includeFlags,fsys::SearchFlags excludeFlags)
{
	return FileManager::OpenFile(path.data(),detail::to_string_mode(mode).c_str(),includeFlags,excludeFlags);
}

bool filemanager::write_file(const std::string_view &path,const std::string_view &contents)
{
	auto f = open_file<VFilePtrReal>(path,filemanager::FileMode::Write);
	if(!f)
		return false;
	f->WriteString(contents.data());
	return true;
}
std::optional<std::string> filemanager::read_file(const std::string_view &path)
{
	auto f = open_file(path,filemanager::FileMode::Read);
	if(!f)
		return {};
	return f->ReadString();
}

template<class T>
	T filemanager::open_file(const std::string_view &path,FileMode mode,fsys::SearchFlags includeFlags,fsys::SearchFlags excludeFlags);
	
std::string filemanager::get_program_path() {return util::get_program_path();}
void filemanager::add_custom_mount_directory(const std::string_view &cpath,fsys::SearchFlags searchMode) {FileManager::AddCustomMountDirectory(cpath.data(),searchMode);}
void filemanager::add_custom_mount_directory(const std::string_view &cpath,bool bAbsolutePath,fsys::SearchFlags searchMode) {FileManager::AddCustomMountDirectory(cpath.data(),bAbsolutePath,searchMode);}
void filemanager::remove_custom_mount_directory(const std::string_view &path) {FileManager::RemoveCustomMountDirectory(path.data());}
void filemanager::clear_custom_mount_directories() {FileManager::ClearCustomMountDirectories();}
VFilePtrReal filemanager::open_system_file(const std::string_view &cpath,FileMode mode) {return FileManager::OpenSystemFile(cpath.data(),detail::to_string_mode(mode).c_str());}
bool filemanager::create_path(const std::string_view &path) {return FileManager::CreatePath(path.data());}
bool filemanager::create_directory(const std::string_view &dir) {return FileManager::CreateDirectory(dir.data());}
std::pair<VDirectory*,VFile*> filemanager::add_virtual_file(const std::string_view &path,const std::shared_ptr<std::vector<uint8_t>> &data) {return FileManager::AddVirtualFile(path.data(),data);}
VDirectory *filemanager::get_root_directory() {return FileManager::GetRootDirectory();}
fsys::Package *filemanager::load_package(const std::string_view &package,fsys::SearchFlags searchMode) {return FileManager::LoadPackage(package.data(),searchMode);}
void filemanager::clear_packages(fsys::SearchFlags searchMode) {FileManager::ClearPackages(searchMode);}
void filemanager::register_packet_manager(const std::string_view &name,std::unique_ptr<fsys::PackageManager> pm) {FileManager::RegisterPackageManager(std::string{name},std::move(pm));}

bool filemanager::remove_file(const std::string_view &file) {return FileManager::RemoveFile(file.data());}
bool filemanager::remove_directory(const std::string_view &dir)
{
	auto absPath = FileManager::GetRootPath() +'\\' +std::string{dir};
	if(std::filesystem::is_directory(absPath) == false)
		return false;
	try
	{
		return std::filesystem::remove_all(absPath);
	}
	catch(const std::filesystem::filesystem_error &err)
	{
		return false;
	}
	return false;
}
bool filemanager::rename_file(const std::string_view &file,const std::string_view &fNewName) {return FileManager::RenameFile(file.data(),fNewName.data());}
void filemanager::close() {return FileManager::Close();}
std::string filemanager::get_path(const std::string_view &path)
{
	std::string spath{path};
	return FileManager::GetPath(spath);
}
std::string filemanager::get_file(const std::string_view &path)
{
	std::string spath{path};
	return FileManager::GetFile(spath);
}
std::string filemanager::get_canonicalized_path(const std::string_view &path) {return FileManager::GetCanonicalizedPath(std::string{path});}
std::string filemanager::get_sub_path(const std::string_view &path) {return FileManager::GetSubPath(std::string{path});}
unsigned long long filemanager::get_file_size(const std::string_view &name,fsys::SearchFlags fsearchmode) {return FileManager::GetFileSize(std::string{name},fsearchmode);}
bool filemanager::exists(const std::string_view &name,fsys::SearchFlags includeFlags,fsys::SearchFlags excludeFlags) {return FileManager::Exists(std::string{name},includeFlags,excludeFlags);}
bool filemanager::is_file(const std::string_view &name,fsys::SearchFlags fsearchmode) {return FileManager::IsFile(std::string{name},fsearchmode);}
bool filemanager::is_dir(const std::string_view &name,fsys::SearchFlags fsearchmode) {return FileManager::IsDir(std::string{name},fsearchmode);}
bool filemanager::exists_system(const std::string_view &name) {return FileManager::ExistsSystem(std::string{name});}
bool filemanager::is_system_file(const std::string_view &name) {return FileManager::IsSystemFile(std::string{name});}
bool filemanager::is_system_dir(const std::string_view &name) {return FileManager::IsSystemDir(std::string{name});}
unsigned long long filemanager::get_file_attributes(const std::string_view &name) {return FileManager::GetFileAttributes(std::string{name});}
unsigned long long filemanager::get_file_flags(const std::string_view &name,fsys::SearchFlags includeFlags,fsys::SearchFlags excludeFlags) {return FileManager::GetFileFlags(std::string{name},includeFlags,excludeFlags);}
std::string filemanager::get_normalized_path(const std::string_view &path) {return FileManager::GetNormalizedPath(std::string{path});}
void filemanager::find_files(const std::string_view &cfind,std::vector<std::string> *resfiles,std::vector<std::string> *resdirs,bool bKeepPath,fsys::SearchFlags includeFlags,fsys::SearchFlags excludeFlag)
{
	FileManager::FindFiles(cfind.data(),resfiles,resdirs,bKeepPath,includeFlags,excludeFlag);
}
void filemanager::find_files(const std::string_view &cfind,std::vector<std::string> *resfiles,std::vector<std::string> *resdirs,fsys::SearchFlags includeFlags,fsys::SearchFlags excludeFlags)
{
	FileManager::FindFiles(cfind.data(),resfiles,resdirs,includeFlags,excludeFlags);
}
void filemanager::find_system_files(const std::string_view &path,std::vector<std::string> *resfiles,std::vector<std::string> *resdirs,bool bKeepPath)
{
	FileManager::FindSystemFiles(path.data(),resfiles,resdirs,bKeepPath);
}
bool filemanager::copy_file(const std::string_view &cfile,const std::string_view &cfNewPath)
{
	return FileManager::CopyFile(cfile.data(),cfNewPath.data());
}
bool filemanager::copy_system_file(const std::string_view &cfile,const std::string_view &cfNewPath)
{
	return FileManager::CopySystemFile(cfile.data(),cfNewPath.data());
}
bool filemanager::move_file(const std::string_view &cfile,const std::string_view &cfNewPath)
{
	return FileManager::MoveFile(cfile.data(),cfNewPath.data());
}
void filemanager::set_absolute_root_path(const std::string_view &path)
{
	return FileManager::SetAbsoluteRootPath(std::string{path});
}
void filemanager::set_root_path(const std::string_view &path)
{
	return FileManager::SetRootPath(std::string{path});
}
std::string filemanager::get_root_path()
{
	return FileManager::GetRootPath();
}

bool filemanager::find_local_path(const std::string_view &path,const std::string_view &rpath,fsys::SearchFlags includeFlags,fsys::SearchFlags excludeFlags)
{
	std::string spath {rpath};
	return FileManager::FindLocalPath(std::string{path},spath,includeFlags,excludeFlags);
}
bool filemanager::find_absolute_path(const std::string_view &path,const std::string_view &rpath,fsys::SearchFlags includeFlags,fsys::SearchFlags excludeFlags)
{
	std::string spath {rpath};
	return FileManager::FindAbsolutePath(std::string{path},spath,includeFlags,excludeFlags);
}
char filemanager::get_directory_separator()
{
	return FileManager::GetDirectorySeparator();
}
bool filemanager::remove_system_file(const std::string_view &file)
{
	return FileManager::RemoveSystemFile(file.data());
}
bool filemanager::remove_system_directory(const std::string_view &dir)
{
	return FileManager::RemoveSystemDirectory(dir.data());
}
bool filemanager::rename_system_file(const std::string_view &file,const std::string_view &fNewName)
{
	return FileManager::RenameSystemFile(file.data(),fNewName.data());
}
std::string filemanager::get_sub_path(const std::string_view &root,const std::string_view &path)
{
	return FileManager::GetSubPath(root.data(),path.data());
}
bool filemanager::create_system_path(const std::string_view &root,const std::string_view &path)
{
	return FileManager::CreateSystemPath(root.data(),path.data());
}
bool filemanager::create_system_directory(const std::string_view &dir)
{
	return FileManager::CreateSystemDirectory(dir.data());
}

VFilePtr filemanager::open_package_file(const std::string_view &packageName,const std::string_view &cpath,FileMode mode,fsys::SearchFlags includeFlags,fsys::SearchFlags excludeFlags)
{
	return FileManager::OpenPackageFile(packageName.data(),cpath.data(),detail::to_string_mode(mode).data(),includeFlags,excludeFlags);
}
fsys::PackageManager *filemanager::get_package_manager(const std::string_view &name)
{
	return FileManager::GetPackageManager(name.data());
}

bool filemanager::compare_path(const std::string_view &a,const std::string_view &b)
{
	return FileManager::ComparePath(a.data(),b.data());
}
