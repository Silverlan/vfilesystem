// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

module pragma.filesystem;

import :package;
import :util;

pragma::filesystem::Package::Package(SearchFlags searchFlags) : m_searchFlags(searchFlags) {}
pragma::filesystem::SearchFlags pragma::filesystem::Package::GetSearchFlags() const { return m_searchFlags; }

/////////////////

bool pragma::filesystem::PackageManager::HasValue(std::vector<std::string> *values, size_t start, size_t end, std::string val, bool bKeepCase) const { return impl::has_value(values, start, end, val, bKeepCase); }
