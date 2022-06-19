#include "variable_selector.h"
#include "../Engine/solver_state.h"

namespace Pumpkin
{

BooleanVariableInternal VariableSelector::PeekNextVariable(SolverState* state)
{//isolated this method in a separate file to avoid circular dependency with the state
	if (heap_.Empty()) { return BooleanVariableInternal(); }

	//make sure the top variable is not assigned
	//iteratively remove top variables until either the heap is empty or the top variable is unassigned
	//note that the data structure is lazy: once a variable is assigned, it may not be removed from the heap, and this is why assigned variable may still be present in the heap
	while (!heap_.Empty() && state->assignments_.IsInternalBooleanVariableAssigned(heap_.PeekMaxKey() + 1))
	{
		heap_.PopMax();
	}

	if (heap_.Empty())
	{
		return BooleanVariableInternal();
	}
	else
	{
		int selected_index = heap_.PeekMaxKey() + 1;
		return state->GetInternalBooleanVariable(selected_index);
	}
}

} //end Pumpkin namespace 65801