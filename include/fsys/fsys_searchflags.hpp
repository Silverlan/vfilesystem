/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
