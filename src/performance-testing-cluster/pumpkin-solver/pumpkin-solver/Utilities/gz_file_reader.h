#pragma once

#include "zlib-1.2.11/zlib.h"

#include <string>
#include <vector>

namespace Pumpkin
{
class GzFileReader
{
public:
	GzFileReader(const std::string &file_location);
	char PeekNextChar();
	GzFileReader& operator>>(char& c);
	GzFileReader& operator>>(std::string& s);
	GzFileReader& operator>>(int64_t& number);
	void ReadIntegerLine(std::vector<int64_t> &integers); //erases contents of input vector and then reads a line of integers, the integers are placed in the input vector.
	void SkipWhitespaces();
	void SkipWhitespacesExceptNewLine();
	void SkipLine();

	bool IsEndOfFile();
	bool IsOK() const;
	explicit operator bool() const;
	
private:	
	
	bool IsWhiteSpace(char c);

	gzFile file_pointer_;
	char* buffer_;
	int current_buffer_size_;
	int position_in_buffer_;
	int MAX_BUFFER_SIZE;
	bool is_state_ok_;
};
}