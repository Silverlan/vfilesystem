// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __FSYS_SEARCHFLAGS_HPP__
#define __FSYS_SEARCHFLAGS_HPP__

#include "fsys_definitions.hpp"
#include <mathutil/umath.h>
#include <cinttypes>

namespace fsys {
	enum class DLLFSYSTEM SearchFlags : uint32_t { None = 0, Virtual = 1, Package = 2, Local = 4, NoMounts = 8, LocalRoot = NoMounts | Local, All = static_cast<uint32_t>(-1) & ~NoMounts };
};
REGISTER_BASIC_BITWISE_OPERATORS(fsys::SearchFlags);

#endif
