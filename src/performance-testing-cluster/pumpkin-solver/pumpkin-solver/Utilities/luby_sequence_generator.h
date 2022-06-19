#pragma once

#include <stdint.h>

namespace Pumpkin
{
//The Luby sequence is a recursive sequence of the form:
//1, 1, 2, 1, 1, 2, 4, 1, 1, 2, 1, 1, 2, 4, 8, 1, 1, 2....
class LubySequenceGenerator
{
public:
	LubySequenceGenerator();

	int64_t GetNextElement();
	void Reset();

private:
	int64_t u_, v_;
};
}