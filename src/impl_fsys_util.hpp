/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __IMPL_FSYS_UTIL_HPP__
#define __IMPL_FSYS_UTIL_HPP__

#include "fsys/fsys_definitions.hpp"
#include <vector>
#include <string>
#include <sharedutils/util_string.h>

namespace fsys {
	namespace impl {
		DLLFSYSTEM bool has_value(std::vector<std::string> *values, size_t start, size_t end, std::string val, bool bKeepCase = false);
		void to_case_sensitive_path(std::string &inOutCaseInsensitivePath);
	};
};

#endif
