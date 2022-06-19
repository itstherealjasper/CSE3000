#include "solution_tracker.h"
#include "runtime_assert.h"

#include <iostream>

namespace Pumpkin
{
SolutionTracker::SolutionTracker() :
	upper_bound_(INT64_MAX),
	lower_bound_(INT64_MIN),
	best_solution_(0),
	is_optimal_(false)
{
}

SolutionTracker::SolutionTracker(const LinearFunction& objective_function, const Stopwatch& initial_stopwatch):
	stopwatch_(initial_stopwatch),
	upper_bound_(INT64_MAX),
	lower_bound_(INT64_MIN),
	best_solution_(0),
	is_optimal_(false),
	objective_function_(objective_function)
{	
}

bool SolutionTracker::UpdateBestSolution(const IntegerAssignmentVector& solution)
{
	runtime_assert(!is_optimal_);

	int64_t new_upper_bound = objective_function_.Evaluate(solution);

	if (new_upper_bound <= upper_bound_)
	{
		if (new_upper_bound < upper_bound_) 
		{ 
			// std::cout << "o " << new_upper_bound << "\nc t = " << stopwatch_.TimeElapsedInSeconds() << "\n";
			upper_bound_ = new_upper_bound;			
			time_stamps_.AddTimePoint(time_t(stopwatch_.TimeElapsedInSeconds()), new_upper_bound);
		}
		else
		{
			// std::cout << "c solution same cost as best: " << upper_bound_ << "\n";			
		}
		best_solution_ = solution;
		return true;
	}
	else
	{
		// std::cout << "c failed solution update: " << new_upper_bound << " vs " << upper_bound_ << "\n";
	}
	return false;
}

void SolutionTracker::UpdateOptimalSolution(const IntegerAssignmentVector& solution)
{	
	runtime_assert(1 == 2); //when is this used? Strange to update optimal solution without the solution being necessarily optimal, i.e., raising the lower bound??
	runtime_assert(!is_optimal_ && objective_function_.Evaluate(solution) <= upper_bound_);
	UpdateBestSolution(solution);
}

void SolutionTracker::ExtendBestSolution(const IntegerAssignmentVector& solution)
{
	runtime_assert(ComputeCost(solution) == upper_bound_ && best_solution_.NumVariables() <= solution.NumVariables());
	best_solution_ = solution;
}

void SolutionTracker::UpdateLowerBound(int64_t new_lower_bound)
{
	runtime_assert(lower_bound_ <= new_lower_bound);
	lower_bound_ = new_lower_bound;
}

IntegerAssignmentVector SolutionTracker::GetBestSolution() const
{
	return best_solution_;
}

int64_t SolutionTracker::UpperBound() const
{
	return upper_bound_;
}

int64_t SolutionTracker::LowerBound() const
{
	runtime_assert(lower_bound_ != INT64_MIN);//for debugging for now
	return lower_bound_;
}

bool SolutionTracker::HasFeasibleSolution() const
{
	return upper_bound_ != INT64_MAX;
}

bool SolutionTracker::HasOptimalSolution() const
{
	return lower_bound_ == upper_bound_;
}

int64_t SolutionTracker::ComputeCost(const IntegerAssignmentVector& solution) const
{
	return objective_function_.Evaluate(solution);
}

double SolutionTracker::ComputePrimalIntegral() const
{
	return time_stamps_.ComputePrimalIntegral(stopwatch_.TimeElapsedInSeconds());
}

}
