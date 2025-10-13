// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include <memory>

export module pragma.filesystem:file_handle;

export {
    class VFilePtrInternal;
    class VFilePtrInternalReal;
    class VFilePtrInternalVirtual;
    class VFilePtrInternalPack;
    using VFilePtr = std::shared_ptr<VFilePtrInternal>;
    using VFilePtrReal = std::shared_ptr<VFilePtrInternalReal>;
    using VFilePtrVirtual = std::shared_ptr<VFilePtrInternalVirtual>;
    using VFilePtrPack = std::shared_ptr<VFilePtrInternalPack>;
}
