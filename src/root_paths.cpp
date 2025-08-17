// SPDX-FileCopyrightText: (c) 2025 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "fsys/filesystem.h"
#include "fsys/file_index_cache.hpp"
#include <sharedutils/util.h>
#include <sharedutils/util_path.hpp>
#include <cassert>

struct RootPathInfo {
	std::string identifier;
	util::Path path;
	int32_t priority = -1;
};
static std::vector<RootPathInfo> g_absoluteRootPaths {};
static std::vector<util::Path> g_orderedAbsoluteRootPaths {};

static std::string resolve_home_dir(const std::string_view &sv, bool filePath)
{
#ifdef __linux__
	if(sv.empty() || sv.front() != '~')
		return std::string {sv};
	auto strHome = util::get_env_variable("HOME");
	if(!strHome)
		return std::string {sv};
	if(filePath)
		return util::FilePath(*strHome, sv.substr(1)).GetString();
	return util::DirPath(*strHome, sv.substr(1)).GetString();
#else
	return std::string {sv};
#endif
}

static void update_ordered_absolute_root_paths()
{
	std::vector<std::pair<size_t, int32_t>> orderedList;
	orderedList.reserve(g_absoluteRootPaths.size());
	for(size_t i = 0; auto &pathInfo : g_absoluteRootPaths)
		orderedList.push_back({i++, pathInfo.priority});
	std::sort(orderedList.begin(), orderedList.end(), [](const std::pair<size_t, int32_t> &a, const std::pair<size_t, int32_t> &b) { return a.second > b.second; });

	g_orderedAbsoluteRootPaths.clear();
	g_orderedAbsoluteRootPaths.reserve(g_absoluteRootPaths.size());
	for(auto &[idx, priority] : orderedList)
		g_orderedAbsoluteRootPaths.push_back(g_absoluteRootPaths[idx].path);
}

std::string filemanager::get_program_path() { return util::get_program_path(); }
std::string filemanager::get_program_write_path()
{
	if(!g_absoluteRootPaths.empty())
		return g_absoluteRootPaths.front().path.GetString();
	return get_program_path();
}
void filemanager::set_absolute_root_path(const std::string_view &path, int32_t mountPriority)
{
	auto dirPath = util::DirPath(resolve_home_dir(path, false));
	try {
		std::filesystem::create_directories(dirPath.GetString());
	}
	catch(const std::filesystem::filesystem_error &e) {
	}
	if(g_absoluteRootPaths.empty())
		g_absoluteRootPaths.push_back({"root", dirPath.GetString(), mountPriority});
	else
		g_absoluteRootPaths.front() = {"root", dirPath.GetString(), mountPriority};
	update_ordered_absolute_root_paths();
	return FileManager::SetAbsoluteRootPath(dirPath.GetString());
}
void filemanager::add_secondary_absolute_read_only_root_path(const std::string &identifier, const std::string_view &path, int32_t mountPriority)
{
	auto dirPath = util::DirPath(resolve_home_dir(path, false));
	try {
		std::filesystem::create_directories(dirPath.GetString());
	}
	catch(const std::filesystem::filesystem_error &e) {
	}
	g_absoluteRootPaths.push_back({identifier, dirPath, mountPriority});
	update_ordered_absolute_root_paths();

	auto *cacheManager = get_root_path_file_cache_manager();
	if(!cacheManager)
		return;
	cacheManager->AddRootReadOnlyLocation(identifier, dirPath.GetString());
}
const util::Path &filemanager::get_absolute_primary_root_path()
{
	assert(!g_absoluteRootPaths.empty());
	return g_absoluteRootPaths.front().path;
}
const std::vector<util::Path> &filemanager::get_absolute_root_paths() { return g_orderedAbsoluteRootPaths; }
