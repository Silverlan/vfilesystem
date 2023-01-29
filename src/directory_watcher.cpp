/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "fsys/directory_watcher.h"
#include <sharedutils/util.h>
#include <sharedutils/util_path.hpp>
#include <assert.h>

DirectoryWatcher::FileEvent::FileEvent(const std::string &fName) : fileName(fName), time(std::chrono::high_resolution_clock::now()) {}

DirectoryWatcher::DirectoryWatcher(const std::string &path, WatchFlags flags) : m_bRunning(true)
{
	if(umath::is_flag_set(flags, WatchFlags::StartDisabled))
		SetEnabled(false);
	auto dstPath = FileManager::GetCanonicalizedPath(((flags & WatchFlags::AbsolutePath) != WatchFlags::None) ? path : (FileManager::GetProgramPath() + '\\' + FileManager::GetNormalizedPath(path)));
#ifdef _WIN32
	auto notifyFilter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE;
	if(umath::is_flag_set(flags, WatchFlags::WatchDirectoryChanges))
		notifyFilter |= FILE_NOTIFY_CHANGE_DIR_NAME;

	auto hDir = std::make_shared<HANDLE>(CreateFileA(dstPath.c_str(), GENERIC_READ | FILE_LIST_DIRECTORY, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL));
	if(*hDir == INVALID_HANDLE_VALUE) {
		throw ConstructException("Unable to create directory handle");
		return;
	}
	//auto hNotification = std::make_shared<HANDLE>(FindFirstChangeNotification(dstPath.c_str(),bSubTree,notifyFilter));
	auto pollingOverlap = std::make_shared<OVERLAPPED>();
	pollingOverlap->OffsetHigh = 0;
	pollingOverlap->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if(pollingOverlap->hEvent == INVALID_HANDLE_VALUE) {
		throw ConstructException("Unable to initialize change notification");
		return;
	}
	m_exitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if(m_exitEvent == INVALID_HANDLE_VALUE) {
		throw ConstructException("Unable to initialize exit event");
		return;
	}
	auto bSubTree = (flags & WatchFlags::WatchSubDirectories) != WatchFlags::None;
	m_thread = std::thread([this, pollingOverlap, hDir, notifyFilter, bSubTree, dstPath]() {
		FILE_NOTIFY_INFORMATION strFileNotifyInfo[1024];
		std::array<HANDLE, 2> waitHandles = {pollingOverlap->hEvent, m_exitEvent};
		auto bExit = false;
		DWORD dwBytesReturned = 0;
		do {
			DWORD waitStatus = WAIT_OBJECT_0 + 1;
			if(ReadDirectoryChangesW(*hDir, (LPVOID)&strFileNotifyInfo, sizeof(strFileNotifyInfo), bSubTree, notifyFilter, &dwBytesReturned, pollingOverlap.get(), NULL) == TRUE)
				waitStatus = WaitForMultipleObjects(2, waitHandles.data(), false, INFINITE); // TODO: Why wait for 2 objects?
			switch(waitStatus) {
			case WAIT_OBJECT_0: // A file was created, renamed or deleted
				{
					auto *pInfo = &strFileNotifyInfo[0];
					if(pInfo->Action == FILE_ACTION_REMOVED)
						break;

					if(pInfo->Action == FILE_ACTION_RENAMED_OLD_NAME) {
						assert(pInfo->NextEntryOffset != 0);
						if(pInfo->NextEntryOffset == 0)
							break;
						pInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION *>(reinterpret_cast<uint8_t *>(strFileNotifyInfo) + pInfo->NextEntryOffset);
						assert(pInfo->Action == FILE_ACTION_RENAMED_NEW_NAME);
					}

					if(m_enabled) {
						std::wstring wstr = pInfo->FileName;
						std::string str(wstr.begin(), wstr.begin() + (pInfo->FileNameLength / sizeof(FILE_NOTIFY_INFORMATION::FileName[0])));
						m_fileMutex.lock();
						auto it = std::find_if(m_fileStack.begin(), m_fileStack.end(), [&str](const FileEvent &ev) { return (ev.fileName == str) ? true : false; });
						if(it == m_fileStack.end())
							m_fileStack.push_back({str});
						m_fileMutex.unlock();
					}
					break;
				}
			case WAIT_OBJECT_0 + 1: // Exit Event
			default:                // Error?
				{
					bExit = true;
					break;
				}
			}
		} while(bExit == false);
		CloseHandle(pollingOverlap->hEvent);
		CloseHandle(m_exitEvent);
	});
	auto relPath = util::Path::CreatePath(dstPath);
	relPath.MakeRelative(util::get_program_path());
	util::set_thread_name(m_thread, "dir_watch_" + relPath.GetString());
#else
	throw ConstructException("Only supported on Windows systems");
#endif
}

DirectoryWatcher::~DirectoryWatcher()
{
#ifdef _WIN32
	SetEvent(m_exitEvent);
#endif
	m_thread.join();
}

void DirectoryWatcher::SetEnabled(bool enabled) { m_enabled = enabled; }
bool DirectoryWatcher::IsEnabled() const { return m_enabled; }

uint32_t DirectoryWatcher::Poll()
{
	uint32_t numChanged = 0;
#ifdef _WIN32
	m_fileMutex.lock();
	if(!m_fileStack.empty()) {
		auto t = std::chrono::high_resolution_clock::now();
		for(auto it = m_fileStack.begin(); it != m_fileStack.end();) {
			auto &f = *it;
			auto tDelta = std::chrono::duration_cast<std::chrono::milliseconds>(t - f.time).count();
			if(tDelta > 100) // Wait for a tenth of a second before relaying the event
			{
				++numChanged;
				OnFileModified(f.fileName);
				it = m_fileStack.erase(it);
			}
			else
				++it;
		}
	}
	m_fileMutex.unlock();
#endif
	return numChanged;
}

///////////////////////

DirectoryWatcherCallback::DirectoryWatcherCallback(const std::string &path, const std::function<void(const std::string &)> &onFileModified, WatchFlags flags) : DirectoryWatcher(path, flags), m_onFileModified(onFileModified) {}

void DirectoryWatcherCallback::OnFileModified(const std::string &fName) { m_onFileModified(fName); }

std::ostream &operator<<(std::ostream &out, const DirectoryWatcherCallback &o)
{
	out << "DirectoryListener[" << o.GetPath() << "]";
	return out;
}
