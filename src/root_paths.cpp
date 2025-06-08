/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

static void update_ordered_absolute_root_paths()
{
    std::vector<std::pair<size_t, int32_t>> orderedList;
    orderedList.reserve(g_absoluteRootPaths.size());
    for(size_t i = 0; auto &pathInfo : g_absoluteRootPaths)
        orderedList.push_back({i++, pathInfo.priority});
    std::sort(orderedList.begin(), orderedList.end(), [](const std::pair<size_t, int32_t> &a, const std::pair<size_t, int32_t> &b) {
        return a.second > b.second;
    });

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
	auto dirPath = util::DirPath(path);
	if(g_absoluteRootPaths.empty())
		g_absoluteRootPaths.push_back({"root", std::string {path}, mountPriority});
	else
		g_absoluteRootPaths.front() = {"root", std::string {path}, mountPriority};
    update_ordered_absolute_root_paths();
	return FileManager::SetAbsoluteRootPath(dirPath.GetString());
}
void filemanager::add_secondary_absolute_read_only_root_path(const std::string &identifier, const std::string_view &path, int32_t mountPriority)
{
	auto dirPath = util::DirPath(path);
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
