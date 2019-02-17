/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "fsys/filesystem.h"
#include "fsys/fsys_package.hpp"
#include "impl_fsys_util.hpp"
#include <sharedutils/util_string.h>
#include <sharedutils/util_file.h>
#include <array>

fsys::Package::Package(fsys::SearchFlags searchFlags)
	: m_searchFlags(searchFlags)
{}
fsys::SearchFlags fsys::Package::GetSearchFlags() const {return m_searchFlags;}

/////////////////

bool fsys::PackageManager::HasValue(std::vector<std::string> *values,size_t start,size_t end,std::string val,bool bKeepCase) const {return fsys::impl::has_value(values,start,end,val,bKeepCase);}
