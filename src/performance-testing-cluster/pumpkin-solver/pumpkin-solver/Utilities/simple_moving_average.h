#ifndef SIMPLE_MOVING_AVERAGE_H
#define SIMPLE_MOVING_AVERAGE_H

#include <queue>

namespace Pumpkin
{

class SimpleMovingAverage
{
public:
	SimpleMovingAverage(int window);

	void AddTerm(int new_term); //add new_term to the moving average

	void Reset();
	double GetCurrentValue() const; //returns the simle moving average. Assumes that at least "window" elements have been inserted.
	bool IsWindowCovered() const; //returnes if at least 'window_size_' elements have been inserted

private:
	int window_size_;
	int windowed_sum_;
	std::queue<int> windowed_values_;
};

} //end Pumpkin namespace

#endif // !SIMPLE_MOVING_AVERAGE_H