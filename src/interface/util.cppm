// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "fsys/fsys_definitions.hpp"
#include <vector>
#include <string>

export module pragma.filesystem:util;

export namespace fsys {
	namespace impl {
		DLLFSYSTEM bool has_value(std::vector<std::string> *values, size_t start, size_t end, std::string val, bool bKeepCase = false);
		void to_case_sensitive_path(std::string &inOutCaseInsensitivePath);
	};
};
