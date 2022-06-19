#ifndef EXPONENTIAL_MOVING_AVERAGE_H
#define EXPONENTIAL_MOVING_AVERAGE_H

namespace Pumpkin
{

/*
This is an implementation of the exponential moving average as described in the paper "Evaluating CDCL Restart Schemes" by Biere and Fröhlich (page 14).
It uses a fixed-point arithmetic rather than floating points and includes the bias-smoothing technique.

The formula represented is: G_{i+1} = g_{i}*2^{32-X} + (1 - 2^{-X})*G_{i}, where g_i is the i-th term, G_i is the value after i terms, and X is the parameter
Note that, essentially, the upper/lower 32 bits are used as the value before/after the decimal point
It includes a smoothing factor to eliminate the strong bias of the first term. This is done as described in the paper, i.e. X_smooth is used instead of X, and X_smooth is gradually increased until it reaches X
*/

class ExponentialMovingAverage
{
public:
	ExponentialMovingAverage(int X);

	void AddTerm(long long new_term); //adds a term to the exponential moving average according to the formula above, including smoothing. The input term is expected to be in an integer, e.g. not given in fixed point arithmetic form.
	void Reset(); //removes all the terms, as if it was just initialised

	long long GetCurrentValue() const; //the return value is a 64 bit integer, where the upper/lower 32 bits represent the value before/after the decimal point
	double GetCurrentValueAsFloatPoint() const; //returns the exponential moving average as a double value rather than the 64 bit fixed point representation

private:
	int X_target_;
	int X_current_;
	long long current_value_;
};

} //end Pumpkin namespace

#endif // !EXPONENTIAL_MOVING_AVERAGE_H