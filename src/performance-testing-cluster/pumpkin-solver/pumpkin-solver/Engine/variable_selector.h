#pragma once

#include "../Utilities/boolean_literal.h"
#include "../Utilities/boolean_variable_internal.h"
#include "../Utilities/key_value_heap.h"
#include "../Utilities/directly_hashed_integer_set.h"
#include "../Utilities/runtime_assert.h"

#include <vector>
#include <stack>

namespace Pumpkin
{

class SolverState;
/*
A priority queue with the logic of the VSIDS variable selection in SAT solvers.
It is a lazy data structure: variables that have already been assigned values might be present during execution.
It is best to access it through the SATsolverState when calling for the best variable
*/
class VariableSelector
{
public:
	//create a variable selector that considers 'num_variables' Boolean variables with activities set to zero.
	VariableSelector(int num_variables, double decay_factor); 
	
	void BumpActivity(BooleanVariableInternal boolean_variable); //bumps the activity of the Boolean variable
	void BumpActivity(BooleanVariableInternal variable, double bump_multiplier); //bumps the variable but multiplies the increment by bump_multiplier. More efficient than repeatedly calling the above method.
	void DecayActivities();//decays the activities of all variables
	
	BooleanVariableInternal PeekNextVariable(SolverState * state);//returns the next unassigned variable, or returns the undefined literal if there are no unassigned variables left. This method does +not+ remove the variable.
	void Remove(BooleanVariableInternal); //removes the Boolean variable (temporarily) from further consideration. Its activity remains recorded internally and is available upon readding it to the data structure. The activity can still be subjected to DecayActivities(). O(logn)

	void Readd(BooleanVariableInternal); //readd the Boolean variable into consideration. This call is ignored if the variable is already present. Assumes it was present in the data structure before. Its activity is kept to the previous value used before Remove(bv) was called. O(logn)
	void Grow(); //increases the number of variables considered by one. The new variable will have its activity assigned to zero.

	int Size() const;
	bool IsVariablePresent(BooleanVariableInternal) const; //returns whether or not the variable is a candidate for selection

	void Reset(int seed = -1); //resets all activities of the variable currently present to zero. The seed is used to set the initial order of variables. Seed = -1 means variables are ordered by their index.

//private:
	void RescaleActivities();//divides all activities with a large number when the maximum activity becomes too large

	KeyValueHeap heap_; //the heap stores indicies of the variables minus one. The minus is applied since the heap operates in the range [0, ...) while variables are indexed [1, ...). Todo should consider changing this at some point.
	double increment_, max_threshold_, decay_factor_;
};

inline VariableSelector::VariableSelector(int num_variables, double decay_factor) :
	heap_(num_variables),
	increment_(1.0),
	max_threshold_(1e100),
	decay_factor_(decay_factor)
{
}

inline void VariableSelector::BumpActivity(BooleanVariableInternal boolean_variable)
{
	double value = heap_.GetKeyValue(boolean_variable.index - 1);
	if (value + increment_ >= max_threshold_)
	{
		heap_.DivideValues(max_threshold_);
		increment_ /= max_threshold_;
	}
	heap_.Increment(boolean_variable.index - 1, increment_);
}

inline void VariableSelector::BumpActivity(BooleanVariableInternal variable, double bump_multiplier)
{
	double value = heap_.GetKeyValue(variable.index - 1);
	if (value + bump_multiplier * increment_ >= max_threshold_)
	{
		heap_.DivideValues(max_threshold_);
		increment_ /= max_threshold_;
	}
	heap_.Increment(variable.index - 1, bump_multiplier * increment_);
}

inline void VariableSelector::DecayActivities()
{
	increment_ *= (1.0 / decay_factor_);
}



inline void VariableSelector::Remove(BooleanVariableInternal boolean_variable)
{
	heap_.Remove(boolean_variable.index - 1);
}

inline void VariableSelector::Readd(BooleanVariableInternal boolean_variable)
{
	if (heap_.IsKeyPresent(boolean_variable.index - 1) == false)
	{
		heap_.Readd(boolean_variable.index - 1);
	}
}


inline int VariableSelector::Size() const
{
	return heap_.Size();
}

inline void VariableSelector::Grow()
{
	heap_.Grow();
}

inline bool VariableSelector::IsVariablePresent(BooleanVariableInternal boolean_variable) const
{
	return heap_.IsKeyPresent(boolean_variable.index - 1);
}

inline void VariableSelector::Reset(int seed)
{
	heap_.Reset(seed);
}

inline void VariableSelector::RescaleActivities()
{
	heap_.DivideValues(max_threshold_);
}

} //end Pumpkin namespace