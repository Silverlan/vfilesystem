// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

module pragma.filesystem;

import :stream;

pragma::filesystem::StreamBuf::StreamBuf(std::size_t buff_sz, std::size_t put_back) : BaseStreamBuf(), put_back(std::max(put_back, size_t(1))), buffer(std::max(buff_sz, put_back) + put_back)
{
	auto *end = &buffer.front() + buffer.size();
	setg(end, end, end);
}

std::streambuf::int_type pragma::filesystem::StreamBuf::underflow()
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

bool pragma::filesystem::BaseStreamBuf::open(const std::string &fileName)
{
	mFile = FileManager::OpenFile(fileName.c_str(), "rb");
	if(mFile == nullptr)
		return false;
	return true;
}

bool pragma::filesystem::BaseStreamBuf::isvalid() const { return (mFile != nullptr); }

///////////////////

// Inherit from std::istream to use our custom streambuf
pragma::filesystem::Stream::Stream(const char *filename, BaseStreamBuf *buf) : std::istream(buf)
{
	// Set the failbit if the file failed to open.
	if(!(static_cast<BaseStreamBuf *>(rdbuf())->open(filename)))
		clear(failbit);
}
pragma::filesystem::Stream::Stream(const char *filename) : Stream(filename, new StreamBuf()) {}
pragma::filesystem::Stream::~Stream() { delete rdbuf(); }
bool pragma::filesystem::Stream::isvalid() const { return static_cast<BaseStreamBuf *>(rdbuf())->isvalid(); }
