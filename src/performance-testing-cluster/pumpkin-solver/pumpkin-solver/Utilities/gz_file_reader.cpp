#include "gz_file_reader.h"
#include "pumpkin_assert.h"

#include <iostream>

namespace Pumpkin
{
GzFileReader::GzFileReader(const std::string &file_location):
	file_pointer_(0),
	buffer_(0),
	current_buffer_size_(0),
	position_in_buffer_(0),
	is_state_ok_(true),
	MAX_BUFFER_SIZE(1024)
{
	file_pointer_ = gzopen(file_location.c_str(), "r");
	if (!file_pointer_) { std::cout << "File " << file_location << " does not exist!\n"; exit(1); }
	buffer_ = new char[MAX_BUFFER_SIZE];
	pumpkin_assert_permanent(buffer_, "Error: no more memory left to read the input file?");	
}

GzFileReader& GzFileReader::operator>>(char& c)
{
	pumpkin_assert_permanent(is_state_ok_, "Error: trying to read in a character but the file reader is not ok");

	c = PeekNextChar();
	position_in_buffer_++;

	return *this;
}

GzFileReader& GzFileReader::operator>>(std::string& s)
{
	s.clear();
	SkipWhitespaces();
	char c = PeekNextChar();
	while (!IsWhiteSpace(c) && c != '\0') 
	{ 
		s += c; 
		position_in_buffer_++;
		c = PeekNextChar();
	}
	is_state_ok_ = (c != '\0');
	return *this;
}

GzFileReader& GzFileReader::operator>>(int64_t& number)
{
	number = 0;
	
	SkipWhitespaces();
	char c = PeekNextChar();
	pumpkin_assert_simple(c == '-' || (c >= '0' && c <= '9'), "Error: when reading file, expected number, but got " + c + " instead.");
			
	int64_t sign = (c == '-' ? -1: 1);
	if (sign == -1) 
	{ 
		position_in_buffer_++;
		c = PeekNextChar();
	}

	do
	{
		pumpkin_assert_simple((c >= '0' && c <= '9'), "Sanity check.");
		number = number * 10 + (c - '0');
		position_in_buffer_++;
		c = PeekNextChar();
	} while (c >= '0' && c <= '9' && c != '\0');

	number *= sign;
	is_state_ok_ = (c != '\0');
	return *this;
}

void GzFileReader::ReadIntegerLine(std::vector<int64_t>& integers)
{
	integers.clear();

	SkipWhitespacesExceptNewLine();
	char c = PeekNextChar();
	while (c != '\0' && c != '\n')
	{
		int64_t number;
		this->operator>>(number);
		integers.push_back(number);

		SkipWhitespacesExceptNewLine();
		c = PeekNextChar();
	}
	SkipLine(); //skip the newline symbol
}

void GzFileReader::SkipLine()
{
	//skip the current character
	char c = PeekNextChar();
	//iterate until you find the next line character
	while (c != '\n' && c != '\0')
	{
		position_in_buffer_++;
		c = PeekNextChar();
	}

	//now move the file pointer right after the new line character
	//	note that there can be several new lines but here we only skip one such line
	//...but only if we did not reach the end of the file
	if (c != '\0') { position_in_buffer_++; }
}

bool GzFileReader::IsEndOfFile()
{
	return PeekNextChar() == '\0';
}

bool GzFileReader::IsOK() const
{
	return is_state_ok_;
}

GzFileReader::operator bool() const
{
	return IsOK();
}

char GzFileReader::PeekNextChar()
{
	pumpkin_assert_simple(position_in_buffer_ <= current_buffer_size_, "Internal buffer reader overrun the limit.");

	if (position_in_buffer_ == current_buffer_size_)
	{
		position_in_buffer_ = 0;
		current_buffer_size_ = gzread(file_pointer_, buffer_, MAX_BUFFER_SIZE);

		if (current_buffer_size_ == 0)
		{
			current_buffer_size_ = 0;
			is_state_ok_ = false;			
			return '\0';
		}
	}
	char c = buffer_[position_in_buffer_];
	is_state_ok_ &= (c != '\0'); //not sure if this is necessary
	return c;	
}

void GzFileReader::SkipWhitespaces()
{
	char c = PeekNextChar();
	while (IsWhiteSpace(c)) 
	{ 
		position_in_buffer_++;
		c = PeekNextChar();
	}
}

void GzFileReader::SkipWhitespacesExceptNewLine()
{
	char c = PeekNextChar();
	while (c != '\n' && IsWhiteSpace(c))
	{
		position_in_buffer_++;
		c = PeekNextChar();
	}
}

bool GzFileReader::IsWhiteSpace(char c)
{
	return (9 <= c && c <= 13) || c == 32;
}
}
