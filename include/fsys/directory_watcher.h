/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __DIRECTORY_WATCHER_H__
#define __DIRECTORY_WATCHER_H__

#include <fsys/filesystem.h>
#include <string>
#ifdef _WIN32
#include <Windows.h>
#endif
#include <thread>
#include <atomic>
#include <array>
#include <vector>
#include <mutex>
#include <chrono>
#include <mathutil/umath.h>

class DLLFSYSTEM DirectoryWatcher
{
public:
	class ConstructException
		: public std::runtime_error
	{
	public:
		using std::runtime_error::runtime_error;
	};

	enum class WatchFlags : uint8_t
	{
		None = 0u,
		WatchSubDirectories = 1u,
		AbsolutePath = WatchSubDirectories<<1u,
		StartDisabled = AbsolutePath<<1u,
		WatchDirectoryChanges = StartDisabled<<1u
	};

	DirectoryWatcher(const std::string &path,WatchFlags flags=WatchFlags::None);
	virtual ~DirectoryWatcher();
	uint32_t Poll();
	const std::string &GetPath() const {return m_path;}

	void SetEnabled(bool enabled);
	bool IsEnabled() const;
protected:
	virtual void OnFileModified(const std::string &fName)=0;
private:
	std::thread m_thread;
	std::atomic<bool> m_bRunning;
	std::atomic<bool> m_enabled = true;
	std::string m_path;
	struct FileEvent
	{
		FileEvent(const std::string &fName);
		std::string fileName;
		std::chrono::high_resolution_clock::time_point time;
	};
	std::vector<FileEvent> m_fileStack;
	std::mutex m_fileMutex;
#ifdef _WIN32
	HANDLE m_exitEvent;
#endif
};
REGISTER_BASIC_BITWISE_OPERATORS(DirectoryWatcher::WatchFlags);

class DLLFSYSTEM DirectoryWatcherCallback
	: public DirectoryWatcher
{
protected:
	std::function<void(const std::string&)> m_onFileModified;
	virtual void OnFileModified(const std::string &fName) override;
public:
	DirectoryWatcherCallback(const std::string &path,const std::function<void(const std::string&)> &onFileModified,WatchFlags flags=WatchFlags::None);
};
DLLFSYSTEM std::ostream &operator<<(std::ostream &out,const DirectoryWatcherCallback &o);

#endif
