// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "util_enum_flags.hpp"

export module pragma.filesystem:search_flags;

import pragma.math;

export {
	namespace pragma::filesystem {
		enum class SearchFlags : uint32_t { None = 0, Virtual = 1, Package = 2, Local = 4, NoMounts = 8, LocalRoot = NoMounts | Local, All = static_cast<uint32_t>(-1) & ~NoMounts };
		using namespace pragma::math::scoped_enum::bitwise;
	};
	REGISTER_ENUM_FLAGS(pragma::filesystem::SearchFlags)
}
