/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FILESYSTEM_STREAM_HPP__
#define __FILESYSTEM_STREAM_HPP__

#include <fsys/filesystem.h>
#include <iostream>
#include <array>

namespace fsys
{
	class DLLFSYSTEM BaseStreamBuf
		: public std::streambuf
	{
	public:
		virtual bool open(const std::string &fileName);

		BaseStreamBuf()=default;
		virtual ~BaseStreamBuf() override=default;
		bool isvalid() const;
	protected:
		VFilePtr mFile = nullptr;
	};

	class DLLFSYSTEM StreamBuf
		: public BaseStreamBuf
	{
	public:
		StreamBuf(std::size_t buff_sz=256,std::size_t put_back=8);
		virtual ~StreamBuf() override=default;

		virtual int_type underflow() override;
	protected:
        const std::size_t put_back;
        std::vector<char> buffer;
	};

	// Inherit from std::istream to use our custom streambuf
	class DLLFSYSTEM Stream
		: public std::istream
	{
	public:
		Stream(const char *filename,BaseStreamBuf *buf);
		Stream(const char *filename);
		virtual ~Stream() override;
		bool isvalid() const;
	};
};

#endif
