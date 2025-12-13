// SPDX-FileCopyrightText: (c) 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

module pragma.filesystem;

import :file_index_cache;

static bool path_to_string(const std::filesystem::path &path, std::string &str)
{
	try {
#ifdef _WIN32

		str = pragma::string::wstring_to_string(path.wstring());
#else
		str = path.string();
#endif
		return true;
	}
	catch(const std::exception &err) {
		// Path probably contains non-ASCII characters; Skip
		return false;
	}
	return false;
}
size_t pragma::filesystem::FileIndexCache::Hash(const std::string_view &key, bool isAbsolutePath) const
{
	// djb2 hash
	size_t hash = 5381;
	auto len = key.length();
	auto offset = isAbsolutePath ? m_rootPath.length() : 0;
	for(auto i = offset; i < key.length(); ++i) {
		auto idx = i - offset;
		auto c = (unsigned char)key[i];
		if(c == '\\')
			c = '/';
		if(c == '/') {
			if(idx == 0 || idx == (len - 1))
				continue;
		}
		c = std::tolower(static_cast<unsigned char>(c));

		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}
	return hash;
}

pragma::filesystem::FileIndexCache::FileIndexCache() : m_pool {5}
{
	auto n = m_pool.size();
	for(auto i = decltype(n) {0u}; i < n; ++i)
		util::set_thread_name(m_pool.get_thread(i), "fsys_index_cache");
}

pragma::filesystem::FileIndexCache::~FileIndexCache() { m_pool.stop(); }

void pragma::filesystem::FileIndexCache::NormalizePath(std::string &path) const
{
	for(auto &c : path) {
		if(c == '\\')
			c = '/';
		if(!m_caseSensitive)
			c = std::tolower(static_cast<unsigned char>(c));
	}
	if(!path.empty() && path.front() == '/')
		path = path.substr(1);
	if(!path.empty() && path.back() == '/')
		path = path.substr(0, path.size() - 1);
}

bool pragma::filesystem::FileIndexCache::Exists(std::string path) const
{
	auto hash = Hash(path, false);
	std::unique_lock lock {m_cacheMutex};
	auto it = m_indexCache.find(hash);
	return it != m_indexCache.end();
}

void pragma::filesystem::FileIndexCache::Add(const std::string_view &path, Type type)
{
	auto hash = Hash(path, false);
	std::unique_lock lock {m_cacheMutex};
	m_indexCache[hash].type = type;
}
void pragma::filesystem::FileIndexCache::Remove(const std::string_view &path)
{
	auto hash = Hash(path, false);
	std::unique_lock lock {m_cacheMutex};
	auto it = m_indexCache.find(hash);
	if(it == m_indexCache.end())
		return;
	m_indexCache.erase(it);
}

std::optional<pragma::filesystem::FileIndexCache::ItemInfo> pragma::filesystem::FileIndexCache::FindItemInfo(std::string path) const
{
	auto hash = Hash(path, false);
	std::unique_lock lock {m_cacheMutex};
	auto it = m_indexCache.find(hash);
	return (it != m_indexCache.end()) ? it->second : std::optional<ItemInfo> {};
}

pragma::filesystem::FileIndexCache::Type pragma::filesystem::FileIndexCache::FindFileType(std::string path) const
{
	auto hash = Hash(path, false);
	std::unique_lock lock {m_cacheMutex};
	auto it = m_indexCache.find(hash);
	return (it != m_indexCache.end()) ? it->second.type : Type::Invalid;
}

void pragma::filesystem::FileIndexCache::Reset(std::string rootPath)
{
	rootPath = util::DirPath(rootPath).GetString();
	m_pool.clear_queue();
	while(m_pool.n_idle() != m_pool.size())
		;
	m_pending = 0;
	Wait();
	m_indexCache.clear();

	m_rootPath = std::move(rootPath);
	QueuePath(m_rootPath);
}

bool pragma::filesystem::FileIndexCache::IsComplete() const { return m_pending == 0; }

void pragma::filesystem::FileIndexCache::Wait()
{
	auto ul = std::unique_lock<std::mutex>(m_taskCompletedMutex);
	m_taskCompleteCondition.wait(ul, [this]() { return m_pending == 0; });
}

void pragma::filesystem::FileIndexCache::QueuePath(const std::filesystem::path &path)
{
	std::filesystem::directory_entry entry {path};
	if(entry.exists() == false)
		return;
	++m_pending;

	std::string strPath;
	if(path_to_string(path, strPath) == false)
		return;
	auto len = strPath.length();
	m_pool.push([this, entry, len](int id) {
		IterateFiles(len, entry);
		DecrementPending();
	});
}

void pragma::filesystem::FileIndexCache::QueuePath(size_t rootLen, const std::filesystem::directory_entry &path)
{
	++m_pending;

	m_pool.push([this, path, rootLen](int id) {
		IterateFiles(rootLen, path);
		DecrementPending();
	});
}

void pragma::filesystem::FileIndexCache::DecrementPending()
{
	if(--m_pending == 0)
		m_taskCompleteCondition.notify_all();
}

void pragma::filesystem::FileIndexCache::IterateFiles(size_t rootLen, const std::filesystem::directory_entry &path)
{
	std::vector<std::pair<size_t, ItemInfo>> localCache;
	auto addItem = [this, &localCache, rootLen](const std::filesystem::path &path, ItemInfo info) {
		std::string strPath;
		if(path_to_string(path, strPath) == false)
			return;
		auto hash = Hash(strPath.substr(rootLen), false);
		localCache.push_back(std::make_pair(hash, info));
	};
	for(auto &dir : std::filesystem::directory_iterator(path)) {
		if(localCache.size() == localCache.capacity())
			localCache.reserve(localCache.size() * 1.5 + 10);
		if(std::filesystem::is_directory(dir.status())) {
			QueuePath(rootLen, dir);

			ItemInfo itemInfo {};
			itemInfo.type = Type::Directory;
			addItem(dir.path(), itemInfo);
		}
		else if(std::filesystem::is_regular_file(dir.status())) {
			ItemInfo itemInfo {};
			itemInfo.type = Type::File;
			addItem(dir.path(), itemInfo);
		}
	}

	m_cacheMutex.lock();
	m_indexCache.reserve(m_indexCache.size() + localCache.size());
	for(auto &pair : localCache)
		m_indexCache[std::move(pair.first)] = pair.second;
	m_cacheMutex.unlock();
}

/////////////////////

pragma::filesystem::RootPathFileCacheManager::RootPathFileCacheManager()
{
	auto cache = std::make_unique<FileIndexCache>();
	m_primaryCache = cache.get();
	m_caches["primary"] = std::move(cache);
}

void pragma::filesystem::RootPathFileCacheManager::SetPrimaryRootLocation(const std::string &rootPath) { m_primaryCache->Reset(rootPath); }

pragma::filesystem::FileIndexCache &pragma::filesystem::RootPathFileCacheManager::GetPrimaryCache() { return *m_primaryCache; }

void pragma::filesystem::RootPathFileCacheManager::AddRootReadOnlyLocation(const std::string &identifier, const std::string_view &rootPath)
{
	if(identifier == "primary")
		throw std::runtime_error {"'primary' root location is reserved"};
	auto cache = std::make_unique<FileIndexCache>();
	cache->Reset(std::string {rootPath});
	m_caches[identifier] = std::move(cache);
}
pragma::filesystem::FileIndexCache *pragma::filesystem::RootPathFileCacheManager::GetCache(const std::string &identifier)
{
	auto it = m_caches.find(identifier);
	if(it == m_caches.end())
		return nullptr;
	return it->second.get();
}

void pragma::filesystem::RootPathFileCacheManager::QueuePath(const std::filesystem::path &path) { return m_primaryCache->QueuePath(path); }
void pragma::filesystem::RootPathFileCacheManager::Wait()
{
	for(auto &[name, cache] : m_caches)
		cache->Wait();
}
bool pragma::filesystem::RootPathFileCacheManager::IsComplete() const
{
	auto isComplete = true;
	for(auto &[name, cache] : m_caches) {
		if(cache->IsComplete() == false) {
			isComplete = false;
			break;
		}
	}
	return isComplete;
}
std::optional<pragma::filesystem::FileIndexCache::ItemInfo> pragma::filesystem::RootPathFileCacheManager::FindItemInfo(std::string path) const
{
	for(auto &[name, cache] : m_caches) {
		auto info = cache->FindItemInfo(path);
		if(info)
			return info;
	}
	return {};
}
pragma::filesystem::FileIndexCache::Type pragma::filesystem::RootPathFileCacheManager::FindFileType(std::string path) const
{
	for(auto &[name, cache] : m_caches) {
		auto type = cache->FindFileType(path);
		if(type != FileIndexCache::Type::Invalid)
			return type;
	}
	return FileIndexCache::Type::Invalid;
}
bool pragma::filesystem::RootPathFileCacheManager::Exists(std::string path) const
{
	for(auto &[name, cache] : m_caches) {
		if(cache->Exists(path))
			return true;
	}
	return false;
}
void pragma::filesystem::RootPathFileCacheManager::Add(const std::string_view &path, FileIndexCache::Type type) { m_primaryCache->Add(path, type); }
void pragma::filesystem::RootPathFileCacheManager::Remove(const std::string_view &path) { m_primaryCache->Remove(path); }
