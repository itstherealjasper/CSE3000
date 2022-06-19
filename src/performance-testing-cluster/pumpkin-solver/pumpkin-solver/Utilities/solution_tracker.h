#pragma once

#include "stopwatch.h"
#include "small_helper_structures.h"
#include "linear_function.h"
#include "integer_assignment_vector.h"

namespace Pumpkin
{
class SolutionTracker
{
public:
	SolutionTracker();
	SolutionTracker(const LinearFunction& objective_function, const Stopwatch &initial_stopwatch = Stopwatch());
	
	bool UpdateBestSolution(const IntegerAssignmentVector& solution); //only updates the best solution if it is better than any other solution seen so far. Returns true if the new solution has been accepted.
	void UpdateOptimalSolution(const IntegerAssignmentVector& solution); //should be a vector of BooleanLiteral/BooleanVariable that is referenced by index...or something like that
	void ExtendBestSolution(const IntegerAssignmentVector& solution); //only use to add values to additional variables, but the cost should be the case. Used within the upper bounding algorithm to make sure that auxiliary variables are assigned values.
	void UpdateLowerBound(int64_t new_lower_bound);

	IntegerAssignmentVector GetBestSolution() const;
	int64_t UpperBound() const;
	int64_t LowerBound() const;
	bool HasFeasibleSolution() const;
	bool HasOptimalSolution() const;

	int64_t ComputeCost(const IntegerAssignmentVector& solution) const;
	double ComputePrimalIntegral() const;

private:
	Stopwatch stopwatch_;
	int64_t upper_bound_, lower_bound_;
	IntegerAssignmentVector best_solution_;
	bool is_optimal_;
	TimeStamps time_stamps_;	
	LinearFunction objective_function_;
};
}