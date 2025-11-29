// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "util_enum_flags.hpp"

export module pragma.filesystem:enums;

import pragma.math;

export {
	namespace fsys {
		enum class FVFile : uint32_t {
			None = 0,
			Invalid = 2048,
			Package = 1,
			Compressed = 2,
			Directory = 4,
			Encrypted = 8,
			Virtual = 16,
			ReadOnly = 32,

			FileNotFound = std::numeric_limits<uint32_t>::max(),
		};
		using namespace umath::scoped_enum::bitwise;
	};
	REGISTER_ENUM_FLAGS(fsys::FVFile)

	enum class EVFile : uint32_t { Virtual = 0, Local = 1, Package = 2 };

	namespace fsys {
		enum class SearchFlags : uint32_t { None = 0, Virtual = 1, Package = 2, Local = 4, NoMounts = 8, LocalRoot = NoMounts | Local, All = static_cast<uint32_t>(-1) & ~NoMounts };
		using namespace umath::scoped_enum::bitwise;
	};
		REGISTER_ENUM_FLAGS(fsys::SearchFlags)

}
