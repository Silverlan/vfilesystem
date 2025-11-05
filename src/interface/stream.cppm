// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "definitions.hpp"

export module pragma.filesystem:stream;

export import :file_handle;

export {
	namespace fsys {
		class DLLFSYSTEM BaseStreamBuf : public std::streambuf {
		  public:
			virtual bool open(const std::string &fileName);

			BaseStreamBuf() = default;
			virtual ~BaseStreamBuf() override = default;
			bool isvalid() const;
		  protected:
			VFilePtr mFile = nullptr;
		};

		class DLLFSYSTEM StreamBuf : public BaseStreamBuf {
		  public:
			StreamBuf(std::size_t buff_sz = 256, std::size_t put_back = 8);
			virtual ~StreamBuf() override = default;

			virtual int_type underflow() override;
		  protected:
			const std::size_t put_back;
			std::vector<char> buffer;
		};

		// Inherit from std::istream to use our custom streambuf
		class DLLFSYSTEM Stream : public std::istream {
		  public:
			Stream(const char *filename, BaseStreamBuf *buf);
			Stream(const char *filename);
			virtual ~Stream() override;
			bool isvalid() const;
		};
	};
}
