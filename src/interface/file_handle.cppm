// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

export module pragma.filesystem:file_handle;

export import std.compat;

export namespace pragma::filesystem {
	class VFilePtrInternal;
	class VFilePtrInternalReal;
	class VFilePtrInternalVirtual;
	using VFilePtr = std::shared_ptr<VFilePtrInternal>;
	using VFilePtrReal = std::shared_ptr<VFilePtrInternalReal>;
	using VFilePtrVirtual = std::shared_ptr<VFilePtrInternalVirtual>;
}
