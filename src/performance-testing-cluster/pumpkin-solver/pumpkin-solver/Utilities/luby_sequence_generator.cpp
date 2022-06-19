#include "luby_sequence_generator.h"

#include <iostream>
#include <set>

namespace Pumpkin
{
LubySequenceGenerator::LubySequenceGenerator() { Reset(); }

//The implementation here follows Donald Knuth's 'reluctant doubling' formula
int64_t LubySequenceGenerator::GetNextElement()
{
	int64_t next_value = v_;
	if ((u_ & (-u_)) == v_)
	{
		++u_;
		v_ = 1;
	}
	else
	{
		v_ *= 2;
	}

	if (u_ >= ((int64_t(1) << 63))  && v_ == (int64_t(1) << 63)) { Reset(); }

	return next_value;
}

void LubySequenceGenerator::Reset()
{
	u_ = 1;
	v_ = 1;
}
}