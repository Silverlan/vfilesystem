// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

export module pragma.filesystem:search_flags;

import pragma.math;

export {
	namespace fsys {
		enum class SearchFlags : uint32_t { None = 0, Virtual = 1, Package = 2, Local = 4, NoMounts = 8, LocalRoot = NoMounts | Local, All = static_cast<uint32_t>(-1) & ~NoMounts };
		using namespace umath::scoped_enum::bitwise;
	};
	namespace umath::scoped_enum::bitwise {
		template<>
		struct enable_bitwise_operators<fsys::SearchFlags> : std::true_type {};
	}
}
