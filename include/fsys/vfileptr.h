/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
