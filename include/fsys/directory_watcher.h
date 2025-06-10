/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __DIRECTORY_WATCHER_H__
#define __DIRECTORY_WATCHER_H__

#include <fsys/filesystem.h>
#include <string>
#include <mathutil/umath.h>


namespace filemanager
{
	class DirectoryWatcherManager;
};
struct DirectoryWatchListenerSet;
class DLLFSYSTEM DirectoryWatcher {
  public:
	class ConstructException : public std::runtime_error {
	  public:
		using std::runtime_error::runtime_error;
	};

	enum class WatchFlags : uint8_t { None = 0u, WatchSubDirectories = 1u, AbsolutePath = WatchSubDirectories << 1u, StartDisabled = AbsolutePath << 1u, WatchDirectoryChanges = StartDisabled << 1u, };

	DirectoryWatcher(const std::string &path, WatchFlags flags = WatchFlags::None, filemanager::DirectoryWatcherManager *watcherManager = nullptr);
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
	filemanager::DirectoryWatcherManager &m_watcherManager;
};
REGISTER_BASIC_BITWISE_OPERATORS(DirectoryWatcher::WatchFlags);

class DLLFSYSTEM DirectoryWatcherCallback : public DirectoryWatcher {
  protected:
	std::function<void(const std::string &)> m_onFileModified;
	virtual void OnFileModified(const std::string &fName) override;
  public:
	DirectoryWatcherCallback(const std::string &path, const std::function<void(const std::string &)> &onFileModified, WatchFlags flags = WatchFlags::None, filemanager::DirectoryWatcherManager *watcherManager = nullptr);
};
DLLFSYSTEM std::ostream &operator<<(std::ostream &out, const DirectoryWatcherCallback &o);

namespace filemanager
{
	DLLFSYSTEM void close_file_watcher();
	DLLFSYSTEM std::shared_ptr<DirectoryWatcherManager> create_directory_watcher_manager();
};

#endif
