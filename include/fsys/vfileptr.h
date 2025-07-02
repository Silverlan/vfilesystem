// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __VFILEPTR_H__
#define __VFILEPTR_H__

#include <memory>

class VFilePtrInternal;
class VFilePtrInternalReal;
class VFilePtrInternalVirtual;
class VFilePtrInternalPack;
using VFilePtr = std::shared_ptr<VFilePtrInternal>;
using VFilePtrReal = std::shared_ptr<VFilePtrInternalReal>;
using VFilePtrVirtual = std::shared_ptr<VFilePtrInternalVirtual>;
using VFilePtrPack = std::shared_ptr<VFilePtrInternalPack>;

#endif
