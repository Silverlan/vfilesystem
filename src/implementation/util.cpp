// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include <string>

module pragma.filesystem;

import :util;

bool fsys::impl::has_value(std::vector<std::string> *values, size_t start, size_t end, std::string val, bool bKeepCase)
{
	if(bKeepCase == false)
		ustring::to_lower(val);
	for(auto i = start; i != end; i++) {
		std::string valCmp = (*values)[i];
		if(bKeepCase == false)
			ustring::to_lower(valCmp);
		if(val == valCmp)
			return true;
	}
	return false;
}

void fsys::impl::to_case_sensitive_path(std::string &inOutCaseInsensitivePath)
{
#ifdef __linux__
	if(inOutCaseInsensitivePath.empty())
		return;
	char *r = static_cast<char *>(alloca(inOutCaseInsensitivePath.length() + 2));
	if(casepath(inOutCaseInsensitivePath.c_str(), r))
		inOutCaseInsensitivePath = r;
#endif
}
