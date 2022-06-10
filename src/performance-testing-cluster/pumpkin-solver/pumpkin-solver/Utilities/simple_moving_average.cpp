#include "simple_moving_average.h"

#include <assert.h>

namespace Pumpkin
{

SimpleMovingAverage::SimpleMovingAverage(int window)
	:window_size_(window), windowed_sum_(0), windowed_values_()
{
}

void SimpleMovingAverage::AddTerm(int new_term)
{
	assert(windowed_values_.size() <= window_size_);
	windowed_values_.push(new_term);
	windowed_sum_ += new_term;
	if (windowed_values_.size() > window_size_)
	{
		windowed_sum_ -= windowed_values_.front();
		windowed_values_.pop();
		assert(windowed_values_.size() == window_size_);
	}
}

void SimpleMovingAverage::Reset()
{
	windowed_sum_ = 0;
	int num_values_to_remove = int(windowed_values_.size());
	for (int i = 0; i < num_values_to_remove; i++) { windowed_values_.pop(); }
}

double SimpleMovingAverage::GetCurrentValue() const
{
	assert(IsWindowCovered() == true);
	return double(windowed_sum_)/window_size_;
}

bool SimpleMovingAverage::IsWindowCovered() const
{
	return int(windowed_values_.size()) == window_size_;
}

} //end Pumpkin namespace