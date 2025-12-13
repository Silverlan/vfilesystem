// SPDX-FileCopyrightText: (c) 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

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
#include "bzlib_wrapper.hpp"

module pragma.filesystem;

import :file_system;

#undef CreateDirectory
#undef GetFileAttributes
#undef MoveFile
#undef CreateFile
#undef CopyFile

static std::unique_ptr<pragma::filesystem::RootPathFileCacheManager> g_rootPathFileCacheManager {};
void pragma::filesystem::set_use_file_index_cache(bool useCache)
{
	if(!useCache) {
		g_rootPathFileCacheManager = nullptr;
		return;
	}
	g_rootPathFileCacheManager = std::make_unique<RootPathFileCacheManager>();
	reset_file_index_cache();
}
pragma::filesystem::RootPathFileCacheManager *pragma::filesystem::get_root_path_file_cache_manager() { return g_rootPathFileCacheManager.get(); }
unsigned long long get_file_attributes(const std::string &fpath);

#ifdef __linux__
#define FILE_ATTRIBUTE_NORMAL 0x80
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define INVALID_FILE_ATTRIBUTES ((unsigned int)-1)
#endif

static void update_file_index_cache(const std::string_view &path, bool absolutePath, std::optional<pragma::filesystem::FileIndexCache::Type> forceAddType)
{
	if(!g_rootPathFileCacheManager)
		return;
	if(absolutePath) {
		auto rootPath = pragma::util::Path::CreatePath(pragma::filesystem::get_root_path());
		auto fpath = pragma::util::Path::CreateFile(std::string {path});
		if(fpath.MakeRelative(rootPath)) {
			std::string mountPath;
			std::string relPath;
			if(pragma::filesystem::FileManager::AbsolutePathToCustomMountPath(fpath.GetString(), mountPath, relPath)) {
				update_file_index_cache(relPath, false, forceAddType);
				update_file_index_cache(mountPath + '/' + relPath, false, forceAddType);
			}
			else
				update_file_index_cache(fpath.GetString(), false, forceAddType);
		}
		return;
	}
	auto &primaryCache = g_rootPathFileCacheManager->GetPrimaryCache();
	if(forceAddType.has_value()) {
		primaryCache.Add(path, *forceAddType);
		return;
	}
	auto attrs = pragma::filesystem::get_file_attributes(path);
	if(attrs == INVALID_FILE_ATTRIBUTES)
		primaryCache.Remove(path);
	else
		primaryCache.Add(path, (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0 ? pragma::filesystem::FileIndexCache::Type::File : pragma::filesystem::FileIndexCache::Type::Directory);
}
void pragma::filesystem::add_to_file_index_cache(const std::string_view &path, bool absolutePath, bool file) { ::update_file_index_cache(path, absolutePath, file ? FileIndexCache::Type::File : FileIndexCache::Type::Directory); }
void pragma::filesystem::update_file_index_cache(const std::string_view &path, bool absolutePath) { ::update_file_index_cache(path, absolutePath, {}); }
bool pragma::filesystem::is_file_index_cache_enabled() { return g_rootPathFileCacheManager != nullptr; }
void pragma::filesystem::reset_file_index_cache()
{
	if(!g_rootPathFileCacheManager)
		return;
	g_rootPathFileCacheManager->SetPrimaryRootLocation(get_root_path());
}

bool pragma::filesystem::clone_to_program_write_path(const std::string_view &path, bool overwriteIfExists)
{
	auto programPath = util::DirPath(get_program_path());
	auto writePath = util::DirPath(get_program_write_path());
	if(writePath == programPath)
		return true; // Nothing to do
	auto dstPath = pragma::util::FilePath(writePath, path);
	if(!overwriteIfExists) {
		if(exists_system(dstPath.GetString()))
			return true; // File already exists at destination, nothing to do
	}
	auto srcPath = pragma::util::FilePath(programPath, path);
	if(exists_system(srcPath.GetString()) == false)
		return false; // Source file doesn't exist
	try {
		std::filesystem::create_directories(dstPath.GetPath());
	}
	catch(const std::filesystem::filesystem_error &e) {
		// Ignore errors
	}
	return copy_system_file(srcPath.GetString(), dstPath.GetString());
}

bool pragma::filesystem::is_executable(const std::string_view &path)
{
#ifdef _WIN32
	return true;
#else
	// First: make sure the file exists and is a regular file
	std::error_code ec;
	auto st = std::filesystem::status(path, ec);
	if(ec || !std::filesystem::is_regular_file(st))
		return false;

	// On POSIX, check any of the exec bits (owner, group, others)
	auto perms = st.permissions();
	constexpr auto exec_bits = std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec;
	return (perms & exec_bits) != std::filesystem::perms::none;
#endif
}

bool pragma::filesystem::make_executable(const std::string_view &path)
{
#ifdef _WIN32
	return true;
#else
	try {
		std::filesystem::permissions(path, std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec, std::filesystem::perm_options::add);
		return true;
	}
	catch(const std::filesystem::filesystem_error &err) {
		// Ignore the error
	}
	return false;
#endif
}

std::string pragma::filesystem::detail::to_string_mode(FileMode mode)
{
	std::string strMode;
	if(math::is_flag_set(mode, FileMode::Read | FileMode::Write))
		strMode = "w+";
	if(math::is_flag_set(mode, FileMode::Read | FileMode::Append))
		strMode = "a+";
	if(math::is_flag_set(mode, FileMode::Read))
		strMode = "r";
	if(math::is_flag_set(mode, FileMode::Write))
		strMode = "w";
	if(math::is_flag_set(mode, FileMode::Append))
		strMode = "a";
	if(math::is_flag_set(mode, FileMode::Binary))
		strMode += "b";
	return strMode;
}

pragma::filesystem::VFilePtr pragma::filesystem::open_file(const std::string_view &path, FileMode mode, std::string *optOutErr, SearchFlags includeFlags, SearchFlags excludeFlags) { return FileManager::OpenFile(path.data(), detail::to_string_mode(mode).c_str(), optOutErr, includeFlags, excludeFlags); }

bool pragma::filesystem::write_file(const std::string_view &path, const std::string_view &contents)
{
	auto f = open_file<fs::VFilePtrReal>(path, FileMode::Write | FileMode::Binary);
	if(!f)
		return false;
	f->WriteString(contents, false);
	return true;
}
std::optional<std::string> pragma::filesystem::read_file(const std::string_view &path)
{
	auto f = open_file(path, FileMode::Read | FileMode::Binary);
	if(!f)
		return {};
	auto sz = f->GetSize();
	if(sz == 0)
		return {};
	std::string str(sz, '\0');
	auto readSize = f->Read(&str[0], 1, sz);
	if(readSize != sz)
		str.resize(readSize);
	return str;
}

template<class T>
T pragma::filesystem::open_file(const std::string_view &path, FileMode mode, std::string *optOutErr, SearchFlags includeFlags, SearchFlags excludeFlags);

void pragma::filesystem::add_custom_mount_directory(const std::string_view &cpath, SearchFlags searchMode) { FileManager::AddCustomMountDirectory(cpath.data(), searchMode); }
void pragma::filesystem::add_custom_mount_directory(const std::string_view &cpath, bool bAbsolutePath, SearchFlags searchMode) { FileManager::AddCustomMountDirectory(cpath.data(), bAbsolutePath, searchMode); }
void pragma::filesystem::remove_custom_mount_directory(const std::string_view &path) { FileManager::RemoveCustomMountDirectory(path.data()); }
void pragma::filesystem::clear_custom_mount_directories() { FileManager::ClearCustomMountDirectories(); }
pragma::filesystem::VFilePtrReal pragma::filesystem::open_system_file(const std::string_view &cpath, FileMode mode, std::string *optOutErr) { return FileManager::OpenSystemFile(cpath.data(), detail::to_string_mode(mode).c_str(), optOutErr); }
bool pragma::filesystem::create_path(const std::string_view &path) { return FileManager::CreatePath(std::string {path}.c_str()); }
bool pragma::filesystem::create_directory(const std::string_view &dir) { return FileManager::CreateDirectory(dir.data()); }
std::pair<pragma::filesystem::VDirectory *, pragma::filesystem::VFile *> pragma::filesystem::add_virtual_file(const std::string_view &path, const std::shared_ptr<std::vector<uint8_t>> &data) { return FileManager::AddVirtualFile(path.data(), data); }
pragma::filesystem::VDirectory *pragma::filesystem::get_root_directory() { return FileManager::GetRootDirectory(); }
pragma::filesystem::Package *pragma::filesystem::load_package(const std::string_view &package, SearchFlags searchMode) { return FileManager::LoadPackage(package.data(), searchMode); }
void pragma::filesystem::clear_packages(SearchFlags searchMode) { FileManager::ClearPackages(searchMode); }
void pragma::filesystem::register_packet_manager(const std::string_view &name, std::unique_ptr<PackageManager> pm) { FileManager::RegisterPackageManager(std::string {name}, std::move(pm)); }

bool pragma::filesystem::remove_file(const std::string_view &file) { return FileManager::RemoveFile(file.data()); }
bool pragma::filesystem::remove_directory(const std::string_view &dir)
{
	auto absPath = FileManager::GetRootPath() + '\\' + std::string {dir};
	if(std::filesystem::is_directory(absPath) == false)
		return false;
	try {
		return std::filesystem::remove_all(absPath);
	}
	catch(const std::filesystem::filesystem_error &err) {
		return false;
	}
	return false;
}
bool pragma::filesystem::rename_file(const std::string_view &file, const std::string_view &fNewName) { return FileManager::RenameFile(file.data(), fNewName.data()); }
void pragma::filesystem::close() { return FileManager::Close(); }
std::string pragma::filesystem::get_path(const std::string_view &path)
{
	std::string spath {path};
	return FileManager::GetPath(spath);
}
std::string pragma::filesystem::get_file(const std::string_view &path)
{
	std::string spath {path};
	return FileManager::GetFile(spath);
}
std::string pragma::filesystem::get_canonicalized_path(const std::string_view &path) { return FileManager::GetCanonicalizedPath(std::string {path}); }
std::string pragma::filesystem::get_sub_path(const std::string_view &path) { return FileManager::GetSubPath(std::string {path}); }
std::optional<std::filesystem::file_time_type> pragma::filesystem::get_last_write_time(const std::string_view &path, SearchFlags includeFlags, SearchFlags excludeFlags) { return FileManager::GetLastWriteTime(path, includeFlags, excludeFlags); }
std::uint64_t pragma::filesystem::get_file_size(const std::string_view &name, SearchFlags fsearchmode) { return FileManager::GetFileSize(std::string {name}, fsearchmode); }
bool pragma::filesystem::exists(const std::string_view &name, SearchFlags includeFlags, SearchFlags excludeFlags) { return fs::exists(std::string {name}, includeFlags, excludeFlags); }
bool pragma::filesystem::is_file(const std::string_view &name, SearchFlags fsearchmode) { return FileManager::IsFile(std::string {name}, fsearchmode); }
bool pragma::filesystem::is_dir(const std::string_view &name, SearchFlags fsearchmode) { return FileManager::IsDir(std::string {name}, fsearchmode); }
bool pragma::filesystem::exists_system(const std::string_view &name) { return FileManager::ExistsSystem(std::string {name}); }
bool pragma::filesystem::is_system_file(const std::string_view &name) { return FileManager::IsSystemFile(std::string {name}); }
bool pragma::filesystem::is_system_dir(const std::string_view &name) { return fs::is_system_dir(std::string {name}); }
std::uint64_t pragma::filesystem::get_file_attributes(const std::string_view &name) { return FileManager::GetFileAttributes(std::string {name}); }
pragma::filesystem::FVFile pragma::filesystem::get_file_flags(const std::string_view &name, SearchFlags includeFlags, SearchFlags excludeFlags) { return FileManager::GetFileFlags(std::string {name}, includeFlags, excludeFlags); }
std::string pragma::filesystem::get_normalized_path(const std::string_view &path) { return pragma::fs::get_normalized_path(std::string {path}); }
void pragma::filesystem::find_files(const std::string_view &cfind, std::vector<std::string> *resfiles, std::vector<std::string> *resdirs, bool bKeepPath, SearchFlags includeFlags, SearchFlags excludeFlag)
{
	FileManager::FindFiles(cfind.data(), resfiles, resdirs, bKeepPath, includeFlags, excludeFlag);
}
void pragma::filesystem::find_files(const std::string_view &cfind, std::vector<std::string> *resfiles, std::vector<std::string> *resdirs, SearchFlags includeFlags, SearchFlags excludeFlags) { FileManager::FindFiles(cfind.data(), resfiles, resdirs, includeFlags, excludeFlags); }
void pragma::filesystem::find_system_files(const std::string_view &path, std::vector<std::string> *resfiles, std::vector<std::string> *resdirs, bool bKeepPath) { FileManager::FindSystemFiles(path.data(), resfiles, resdirs, bKeepPath); }
bool pragma::filesystem::copy_file(const std::string_view &cfile, const std::string_view &cfNewPath) { return FileManager::CopyFile(cfile.data(), cfNewPath.data()); }
bool pragma::filesystem::copy_system_file(const std::string_view &cfile, const std::string_view &cfNewPath) { return FileManager::CopySystemFile(cfile.data(), cfNewPath.data()); }
bool pragma::filesystem::move_file(const std::string_view &cfile, const std::string_view &cfNewPath) { return FileManager::MoveFile(cfile.data(), cfNewPath.data()); }
void pragma::filesystem::set_root_path(const std::string_view &path) { return FileManager::SetRootPath(std::string {path}); }
std::string pragma::filesystem::get_root_path() { return FileManager::GetRootPath(); }

bool pragma::filesystem::find_local_path(const std::string_view &path, std::string &rpath, SearchFlags includeFlags, SearchFlags excludeFlags) { return FileManager::FindLocalPath(std::string {path}, rpath, includeFlags, excludeFlags); }
bool pragma::filesystem::find_absolute_path(const std::string_view &path, std::string &rpath, SearchFlags includeFlags, SearchFlags excludeFlags) { return FileManager::FindAbsolutePath(std::string {path}, rpath, includeFlags, excludeFlags); }
std::vector<std::string> pragma::filesystem::find_absolute_paths(const std::string_view &path, SearchFlags includeFlags, SearchFlags excludeFlags) { return FileManager::FindAbsolutePaths(std::string {path}, includeFlags, excludeFlags); }
bool pragma::filesystem::find_relative_path(const std::string_view &path, std::string &rpath) { return FileManager::FindRelativePath(std::string {path}, rpath); }
char pragma::filesystem::get_directory_separator() { return FileManager::GetDirectorySeparator(); }
bool pragma::filesystem::remove_system_file(const std::string_view &file) { return FileManager::RemoveSystemFile(file.data()); }
bool pragma::filesystem::remove_system_directory(const std::string_view &dir) { return FileManager::RemoveSystemDirectory(dir.data()); }
bool pragma::filesystem::rename_system_file(const std::string_view &file, const std::string_view &fNewName) { return FileManager::RenameSystemFile(file.data(), fNewName.data()); }
std::string pragma::filesystem::get_sub_path(const std::string_view &root, const std::string_view &path) { return FileManager::GetSubPath(root.data(), path.data()); }
bool pragma::filesystem::create_system_path(const std::string_view &root, const std::string_view &path) { return FileManager::CreateSystemPath(std::string {root}.c_str(), path.data()); }
bool pragma::filesystem::create_system_directory(const std::string_view &dir) { return FileManager::CreateSystemDirectory(dir.data()); }

pragma::filesystem::VFilePtr pragma::filesystem::open_package_file(const std::string_view &packageName, const std::string_view &cpath, FileMode mode, SearchFlags includeFlags, SearchFlags excludeFlags)
{
	return FileManager::OpenPackageFile(packageName.data(), cpath.data(), detail::to_string_mode(mode).data(), includeFlags, excludeFlags);
}
pragma::filesystem::PackageManager *pragma::filesystem::get_package_manager(const std::string_view &name) { return FileManager::GetPackageManager(name.data()); }

bool pragma::filesystem::compare_path(const std::string_view &a, const std::string_view &b) { return FileManager::ComparePath(a.data(), b.data()); }
