/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FSYS_PACKAGE_HPP__
#define __FSYS_PACKAGE_HPP__

#include "fsys_definitions.hpp"
#include "fsys_searchflags.hpp"
#include "vfileptr.h"
#include <memory>
#include <string>

namespace fsys
{
	class DLLFSYSTEM Package
	{
	public:
		virtual ~Package()=default;
		fsys::SearchFlags GetSearchFlags() const;
	protected:
		Package(fsys::SearchFlags searchFlags);
	private:
		fsys::SearchFlags m_searchFlags = fsys::SearchFlags::None;
	};
	class DLLFSYSTEM PackageManager
	{
	public:
		PackageManager()=default;
		virtual ~PackageManager()=default;
		virtual Package *LoadPackage(std::string package,SearchFlags searchMode=SearchFlags::Local)=0;
		virtual void ClearPackages(SearchFlags searchMode)=0;
		virtual void FindFiles(const std::string &target,const std::string &path,std::vector<std::string> *resfiles,std::vector<std::string> *resdirs,bool bKeepPath,fsys::SearchFlags includeFlags) const=0;
		virtual bool GetSize(const std::string &name,uint64_t &size) const=0;
		virtual bool Exists(const std::string &name,SearchFlags includeFlags) const=0;
		virtual bool GetFileFlags(const std::string &name,SearchFlags includeFlags,uint64_t &flags) const=0;
		virtual VFilePtr OpenFile(const std::string &path,bool bBinary,SearchFlags includeFlags,fsys::SearchFlags excludeFlags) const=0;
	protected:
		bool HasValue(std::vector<std::string> *values,size_t start,size_t end,std::string val,bool bKeepCase=false) const;
	};
};

#endif
