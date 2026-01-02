// SPDX-FileCopyrightText: (c) 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

export module pragma.filesystem:file_index_cache;

export import pragma.util;

export {
	namespace pragma::filesystem {
		class DLLFSYSTEM FileIndexCache {
		  public:
			enum class Type : uint8_t {
				File = 0,
				Directory,

				Invalid = std::numeric_limits<uint8_t>::max()
			};
			struct ItemInfo {
				Type type;
#ifdef VFILESYSTEM_STORE_FILE_INDEX_CACHE_PATHS
				std::string path;
#endif
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
			const std::string &GetRootPath() const { return m_rootPath; }
			const std::unordered_map<size_t, ItemInfo> &GetIndexCache() const { return m_indexCache; };
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

		/////////////////////

		class DLLFSYSTEM RootPathFileCacheManager {
		  public:
			RootPathFileCacheManager();

			RootPathFileCacheManager(const RootPathFileCacheManager &) = delete;
			RootPathFileCacheManager &operator=(const RootPathFileCacheManager &) = delete;

			void SetPrimaryRootLocation(const std::string &rootPath);
			void AddRootReadOnlyLocation(const std::string &identifier, const std::string_view &rootPath);
			FileIndexCache *GetCache(const std::string &identifier);
			FileIndexCache &GetPrimaryCache();
			std::unordered_map<std::string, std::unique_ptr<FileIndexCache>> &GetCaches() { return m_caches; }

			void QueuePath(const std::filesystem::path &path);
			void Wait();
			bool IsComplete() const;
			std::optional<FileIndexCache::ItemInfo> FindItemInfo(std::string path) const;
			FileIndexCache::Type FindFileType(std::string path) const;
			bool Exists(std::string path) const;
			void Add(const std::string_view &path, FileIndexCache::Type type);
			void Remove(const std::string_view &path);
		  private:
			std::unordered_map<std::string, std::unique_ptr<FileIndexCache>> m_caches;
			FileIndexCache *m_primaryCache = nullptr;
		};
	};
}
