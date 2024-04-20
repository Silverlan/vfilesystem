/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FILE_INDEX_CACHE_HPP__
#define __FILE_INDEX_CACHE_HPP__

#include "fsys_definitions.hpp"
#include <iostream>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <condition_variable>
#include <sharedutils/ctpl_stl.h>

namespace fsys {
	class DLLFSYSTEM FileIndexCache {
	  public:
		enum class Type : uint8_t {
			File = 0,
			Directory,

			Invalid = std::numeric_limits<uint8_t>::max()
		};
		struct ItemInfo {
			Type type;
		};
		FileIndexCache();
		~FileIndexCache();

		void Reset(std::string rootPath);
		void QueuePath(const std::filesystem::path &path);
		void Wait();
		bool IsComplete() const;
		std::optional<ItemInfo> FindItemInfo(std::string path) const;
		Type FindFileType(std::string path) const;
		bool Exists(std::string path) const;
		void Add(const std::string_view &path, Type type);
		void Remove(const std::string_view &path);
	  private:
		void NormalizePath(std::string &path) const;
		size_t Hash(const std::string_view &key, bool isAbsolutePath) const;
		void QueuePath(size_t rootLen, const std::filesystem::directory_entry &path);
		void IterateFiles(size_t rootLen, const std::filesystem::directory_entry &path);
		void DecrementPending();

		mutable std::mutex m_cacheMutex;
		std::unordered_map<size_t, ItemInfo> m_indexCache;
		std::condition_variable m_taskCompleteCondition;
		std::mutex m_taskCompletedMutex;

		std::string m_rootPath;
		ctpl::thread_pool m_pool;
		bool m_caseSensitive = false;
		std::atomic<uint32_t> m_pending = 0;
	};
};

#endif
