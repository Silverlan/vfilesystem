// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "definitions.hpp"
#include "util_enum_flags.hpp"

export module pragma.filesystem:directory_watcher;

import pragma.math;

export namespace pragma::filesystem {
	class DirectoryWatcherManager;
	struct DirectoryWatchListenerSet;
	class DLLFSYSTEM DirectoryWatcher {
	  public:
		class ConstructException : public std::runtime_error {
		  public:
			using std::runtime_error::runtime_error;
		};

		enum class WatchFlags : uint8_t {
			None = 0u,
			WatchSubDirectories = 1u,
			AbsolutePath = WatchSubDirectories << 1u,
			StartDisabled = AbsolutePath << 1u,
			WatchDirectoryChanges = StartDisabled << 1u,
		};

		DirectoryWatcher(const std::string &path, WatchFlags flags = WatchFlags::None, DirectoryWatcherManager *watcherManager = nullptr);
		virtual ~DirectoryWatcher();
		uint32_t Poll();
		const std::string &GetPath() const { return m_path; }

		void SetEnabled(bool enabled);
		bool IsEnabled() const;
	  protected:
		void UpdateEnabledState();
		virtual void OnFileModified(const std::string &fName) = 0;
	  private:
		bool m_enabled = true;
		std::string m_path;
		WatchFlags m_watchFlags;
		std::unique_ptr<DirectoryWatchListenerSet> m_watchListenerSet;
		DirectoryWatcherManager &m_watcherManager;
	};

	class DLLFSYSTEM DirectoryWatcherCallback : public DirectoryWatcher {
	  protected:
		std::function<void(const std::string &)> m_onFileModified;
		virtual void OnFileModified(const std::string &fName) override;
	  public:
		DirectoryWatcherCallback(const std::string &path, const std::function<void(const std::string &)> &onFileModified, WatchFlags flags = WatchFlags::None, DirectoryWatcherManager *watcherManager = nullptr);
	};
	DLLFSYSTEM std::ostream &operator<<(std::ostream &out, const DirectoryWatcherCallback &o);

	DLLFSYSTEM void close_file_watcher();
	DLLFSYSTEM std::shared_ptr<DirectoryWatcherManager> create_directory_watcher_manager();
}

export {
	REGISTER_ENUM_FLAGS(pragma::filesystem::DirectoryWatcher::WatchFlags)
}
