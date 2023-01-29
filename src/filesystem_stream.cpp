/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "fsys/filesystem_stream.hpp"
#include <cstring>

fsys::StreamBuf::StreamBuf(std::size_t buff_sz, std::size_t put_back) : BaseStreamBuf(), put_back(std::max(put_back, size_t(1))), buffer(std::max(buff_sz, put_back) + put_back)
{
	auto *end = &buffer.front() + buffer.size();
	setg(end, end, end);
}

std::streambuf::int_type fsys::StreamBuf::underflow()
{
	if(gptr() < egptr()) // buffer not exhausted
		return traits_type::to_int_type(*gptr());

	auto *base = &buffer.front();
	auto *start = base;
	if(eback() == base) // true when this isn't the first fill
	{
		// Make arrangements for putback characters
		std::memmove(base, egptr() - put_back, put_back);
		start += put_back;
	}

	// start is now the start of the buffer, proper.
	// Read from fptr_ in to the provided buffer
	auto n = mFile->Read(start, buffer.size() - (start - base));
	if(n == 0)
		return traits_type::eof();

	// Set buffer pointers
	setg(base, start, start + n);

	return traits_type::to_int_type(*gptr());
}

bool fsys::BaseStreamBuf::open(const std::string &fileName)
{
	mFile = FileManager::OpenFile(fileName.c_str(), "rb");
	if(mFile == nullptr)
		return false;
	return true;
}

bool fsys::BaseStreamBuf::isvalid() const { return (mFile != nullptr); }

///////////////////

// Inherit from std::istream to use our custom streambuf
fsys::Stream::Stream(const char *filename, BaseStreamBuf *buf) : std::istream(buf)
{
	// Set the failbit if the file failed to open.
	if(!(static_cast<BaseStreamBuf *>(rdbuf())->open(filename)))
		clear(failbit);
}
fsys::Stream::Stream(const char *filename) : Stream(filename, new StreamBuf()) {}
fsys::Stream::~Stream() { delete rdbuf(); }
bool fsys::Stream::isvalid() const { return static_cast<BaseStreamBuf *>(rdbuf())->isvalid(); }
