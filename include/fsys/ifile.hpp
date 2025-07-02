// SPDX-FileCopyrightText: (c) 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __FSYS_IFILE_HPP__
#define __FSYS_IFILE_HPP__

#include "fsys/filesystem.h"
#include <sharedutils/util_ifile.hpp>
#include <string>

namespace fsys {
	struct DLLFSYSTEM File : public ufile::IFile {
		File() = default;
		File(const ::VFilePtr &f);
		virtual ~File() override = default;
		virtual size_t Read(void *data, size_t size) override;
		virtual size_t Write(const void *data, size_t size) override;
		virtual size_t Tell() override;
		virtual void Seek(size_t offset, Whence whence = Whence::Set) override;
		virtual int32_t ReadChar() override;
		virtual size_t GetSize() override;
		virtual bool Eof() override;
		virtual std::optional<std::string> GetFileName() const override;
		::VFilePtr &GetFile() { return m_file; }
	  private:
		::VFilePtr m_file;
	};
};

#endif
