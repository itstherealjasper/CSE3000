#ifndef CUMULATIVE_MOVING_AVERAGE_H
#define CUMULATIVE_MOVING_AVERAGE_H

namespace Pumpkin
{

class CumulativeMovingAverage
{
public:

	CumulativeMovingAverage();

	void AddTerm(int new_term); //add new_term to the moving average

	double GetCurrentValue() const; //returns the simle moving average. Assumes that at least "window" elements have been inserted.
private:
	long long sum_;
	long long num_terms;
};

} //end Pumpkin namespace

#endif // !CUMULATIVE_MOVING_AVERAGE_H
