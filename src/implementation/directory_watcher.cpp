// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;


#include <efsw/efsw.hpp>

module pragma.filesystem;

import :file_system;
import :directory_watcher;
import pragma.util;

namespace filemanager {
	class DirectoryWatcherManager : public std::enable_shared_from_this<DirectoryWatcherManager> {
	  public:
		DirectoryWatcherManager();
		static const std::shared_ptr<DirectoryWatcherManager> &Get();
		std::optional<DirectoryWatchListenerSet> AddWatch(const std::string_view &path, bool absolutePath, bool recursive);
		void RemoveWatch(efsw::WatchID watchId);
	  private:
		std::unique_ptr<efsw::FileWatcher> m_fileWatcher;
	};
};

class DirectoryWatchListener : public efsw::FileWatchListener {
  public:
	DirectoryWatchListener(filemanager::DirectoryWatcherManager &watcherManager, const util::Path &rootPath);
	~DirectoryWatchListener();
	void SetWatchId(efsw::WatchID watchId);
	void SetEnabled(bool enabled);
	void handleFileAction(efsw::WatchID watchid, const std::string &dir, const std::string &filename, efsw::Action action, std::string oldFilename) override;

	uint32_t Poll(const std::function<void(const std::string &)> &onModified);
  private:
	struct FileEvent {
		FileEvent(const std::string &fName);
		std::string fileName;
		std::chrono::steady_clock::time_point time;
	};
	std::unordered_map<std::string, FileEvent> m_fileStack;
	std::atomic<bool> m_enabled = false;
	std::mutex m_fileMutex;
	util::Path m_rootPath;
	efsw::WatchID m_watchId;
	std::weak_ptr<filemanager::DirectoryWatcherManager> m_watcherManager;
};

void DirectoryWatchListener::SetEnabled(bool enabled) { m_enabled = enabled; }

DirectoryWatchListener::DirectoryWatchListener(filemanager::DirectoryWatcherManager &watcherManager, const util::Path &rootPath) : m_watcherManager {watcherManager.shared_from_this()}, m_rootPath {rootPath} {}

DirectoryWatchListener::~DirectoryWatchListener()
{
	if(m_watcherManager.expired())
		return;
	m_watcherManager.lock()->RemoveWatch(m_watchId);
}

DirectoryWatchListener::FileEvent::FileEvent(const std::string &fName) : fileName(fName), time(std::chrono::steady_clock::now()) {}

void DirectoryWatchListener::SetWatchId(efsw::WatchID watchId) { m_watchId = watchId; }
void DirectoryWatchListener::handleFileAction(efsw::WatchID watchid, const std::string &dir, const std::string &filename, efsw::Action action, std::string oldFilename)
{
	if(!m_enabled)
		return;
	switch(action) {
	case efsw::Actions::Add:
		//std::cout << "DIR (" << dir << ") FILE (" << filename << ") has event Added"
		//			<< std::endl;
		break;
	case efsw::Actions::Delete:
		//std::cout << "DIR (" << dir << ") FILE (" << filename << ") has event Delete"
		//			<< std::endl;
		break;
	case efsw::Actions::Modified:
		{
			auto path = util::FilePath(dir, filename);
			path.MakeRelative(m_rootPath);
			auto &normPath = path.GetString();
			std::unique_lock lock {m_fileMutex};
			if(m_fileStack.find(normPath) == m_fileStack.end()) {
				FileEvent ev {normPath};
				ev.time = std::chrono::steady_clock::now();
				m_fileStack.insert(std::make_pair(normPath, ev));
				//std::cout << "DIR (" << dir << ") FILE (" << filename << ") has event Modified"
				//			<< std::endl;
			}
			break;
		}
	case efsw::Actions::Moved:
		//std::cout << "DIR (" << dir << ") FILE (" << filename << ") has event Moved from ("
		//			<< oldFilename << ")" << std::endl;
		break;
	default:
		// Unreachable
		break;
	}
}

uint32_t DirectoryWatchListener::Poll(const std::function<void(const std::string &)> &onModified)
{
	uint32_t numChanged = 0;
	std::unique_lock lock {m_fileMutex};
	if(!m_fileStack.empty()) {
		auto t = std::chrono::steady_clock::now();
		for(auto it = m_fileStack.begin(); it != m_fileStack.end();) {
			auto &name = it->first;
			auto &item = it->second;
			auto tDelta = std::chrono::duration_cast<std::chrono::milliseconds>(t - item.time).count();
			if(tDelta <= 100) {
				// Wait for a tenth of a second before relaying the event
				++it;
				continue;
			}
			++numChanged;
			auto path = util::FilePath(item.fileName);
			path.MakeRelative(m_rootPath);
			onModified(path.GetString());

			it = m_fileStack.erase(it);
		}
	}
	return numChanged;
}

struct DirectoryWatchListenerSet {
	DirectoryWatchListenerSet() = default;
	std::vector<std::shared_ptr<DirectoryWatchListener>> listeners;
	uint32_t Poll(const std::function<void(const std::string &)> &onModified)
	{
		uint32_t count = 0;
		for(auto &listener : listeners)
			count += listener->Poll(onModified);
		return count;
	}
};

filemanager::DirectoryWatcherManager::DirectoryWatcherManager()
{
	m_fileWatcher = std::make_unique<efsw::FileWatcher>();
	m_fileWatcher->watch();
}

static std::shared_ptr<filemanager::DirectoryWatcherManager> g_DirectoryWatcherManager;
const std::shared_ptr<filemanager::DirectoryWatcherManager> &filemanager::DirectoryWatcherManager::Get()
{
	if(!g_DirectoryWatcherManager)
		g_DirectoryWatcherManager = std::shared_ptr<DirectoryWatcherManager> {new DirectoryWatcherManager {}};
	return g_DirectoryWatcherManager;
}

void filemanager::close_file_watcher() { g_DirectoryWatcherManager = nullptr; }

std::optional<DirectoryWatchListenerSet> filemanager::DirectoryWatcherManager::AddWatch(const std::string_view &path, bool absolutePath, bool recursive)
{
	auto normPath = util::DirPath(path);
	DirectoryWatchListenerSet set;
	auto addWatch = [this, &set, recursive](const util::Path &rootPath, const util::Path &relPath) {
		auto absPath = util::DirPath(rootPath, relPath);
		if(!filemanager::exists_system(absPath.GetString()))
			return;
		auto listener = std::make_shared<DirectoryWatchListener>(*this, rootPath);
		efsw::WatchID watchId = m_fileWatcher->addWatch(absPath.GetString(), listener.get(), recursive);
		if(watchId < 0) {
			// std::cout<<"Failed to watch directory: "<<efsw::Errors::Log::getLastErrorLog()<<std::endl;
			return;
		}

		listener->SetWatchId(watchId);
		set.listeners.push_back(listener);
	};
	if(absolutePath)
		addWatch({}, util::DirPath(path));
	else {
		auto &rootPaths = filemanager::get_absolute_root_paths();
		set.listeners.reserve(rootPaths.size());
		for(auto &rootPath : rootPaths)
			addWatch(rootPath, std::string {path});
	}
	if(set.listeners.empty())
		return {};
	return set;
}
void filemanager::DirectoryWatcherManager::RemoveWatch(efsw::WatchID watchId) { m_fileWatcher->removeWatch(watchId); }

DirectoryWatcher::DirectoryWatcher(const std::string &path, WatchFlags flags, filemanager::DirectoryWatcherManager *watcherManager) : m_path {path}, m_watchFlags {flags}, m_watcherManager {watcherManager ? *watcherManager : *filemanager::DirectoryWatcherManager::Get()}
{
	SetEnabled(!umath::is_flag_set(flags, WatchFlags::StartDisabled));
}

DirectoryWatcher::~DirectoryWatcher() {}

void DirectoryWatcher::UpdateEnabledState()
{
	if(!m_enabled) {
		if(m_watchListenerSet) {
			for(auto &listener : m_watchListenerSet->listeners)
				listener->SetEnabled(false);
		}
		//m_watchListenerSet = nullptr;
		return;
	}
	if(!m_watchListenerSet) {
		auto absolutePath = umath::is_flag_set(m_watchFlags, WatchFlags::AbsolutePath);
		auto recursive = umath::is_flag_set(m_watchFlags, WatchFlags::WatchSubDirectories);
		auto listenerSet = m_watcherManager.AddWatch(m_path, absolutePath, recursive);

		if(listenerSet)
			m_watchListenerSet = std::make_unique<DirectoryWatchListenerSet>(std::move(*listenerSet));
	}
	if(m_watchListenerSet) {
		for(auto &listener : m_watchListenerSet->listeners)
			listener->SetEnabled(true);
	}
}

void DirectoryWatcher::SetEnabled(bool enabled)
{
	m_enabled = enabled;
	UpdateEnabledState();
}
bool DirectoryWatcher::IsEnabled() const { return m_enabled; }

uint32_t DirectoryWatcher::Poll()
{
	if(!m_watchListenerSet)
		return 0;
	return m_watchListenerSet->Poll([this](const std::string &fileName) { OnFileModified(fileName); });
}

std::shared_ptr<filemanager::DirectoryWatcherManager> filemanager::create_directory_watcher_manager() { return std::shared_ptr<DirectoryWatcherManager> {new DirectoryWatcherManager {}}; }

///////////////////////

DirectoryWatcherCallback::DirectoryWatcherCallback(const std::string &path, const std::function<void(const std::string &)> &onFileModified, WatchFlags flags, filemanager::DirectoryWatcherManager *watcherManager)
    : DirectoryWatcher(path, flags, watcherManager), m_onFileModified(onFileModified)
{
}

void DirectoryWatcherCallback::OnFileModified(const std::string &fName) { m_onFileModified(fName); }

std::ostream &operator<<(std::ostream &out, const DirectoryWatcherCallback &o)
{
	out << "DirectoryListener[" << o.GetPath() << "]";
	return out;
}
