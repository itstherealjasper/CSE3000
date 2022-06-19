#include "exponential_moving_average.h"

#include <algorithm>
#include <assert.h>

namespace Pumpkin
{

ExponentialMovingAverage::ExponentialMovingAverage(int X)
	:X_target_(X), X_current_(0), current_value_(0)
{
	assert(sizeof(long long) == 8); //we assume a 64 bit integer for the fixed point arithmetic
}

void ExponentialMovingAverage::AddTerm(long long new_term)
{
	//to take into account binary arithmetic the formula is computed as: 
	//G_{i+1} = g_{i}*2^{32-X} + (G_{i} - 2^{-X}*G_{i})
	long long first_part = new_term << (32 - X_current_);
	long long third_part = (current_value_ >> X_current_);
	current_value_ = first_part + (current_value_ - third_part);

	X_current_ = std::min(X_current_ + 1, X_target_); //considering increasing the X_current if its not yet at its target value, this is part of smoothing
}

void ExponentialMovingAverage::Reset()
{
	X_current_ = 0;
	current_value_ = 0;	
}

long long ExponentialMovingAverage::GetCurrentValue() const
{
	return current_value_;
}

double ExponentialMovingAverage::GetCurrentValueAsFloatPoint() const
{
	assert(sizeof(int) == 4);
	double integer_part = double(current_value_ >> 32);
	double decimal_part = double(current_value_ & 0xFFFFFFFF) / (1 << 32);
	return integer_part + decimal_part;
}

} //end Pumpkin namespace