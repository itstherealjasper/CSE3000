#include "propagator_generic.h"
#include "../Engine/solver_state.h"

namespace Pumpkin
{

PropagatorGeneric::PropagatorGeneric(SolverState& state)
	:next_position_on_trail_to_propagate_(0),
	state_(state)
{
}

PropagationStatus PropagatorGeneric::Propagate()
{
	while (!IsPropagationComplete())
	{
		BooleanLiteral propagation_literal = GetAndPopNextLiteralToPropagate();
		PropagationStatus propagation_status = PropagateLiteral(propagation_literal);
		if (propagation_status.conflict_detected) { return true; }
	}
	return false; //no conflicts occurred during propagation
}

PropagationStatus PropagatorGeneric::PropagateOneLiteral()
{
	if (!IsPropagationComplete())
	{
		BooleanLiteral propagation_literal = GetAndPopNextLiteralToPropagate();
		PropagationStatus propagation_status = PropagateLiteral(propagation_literal);
		if (propagation_status.conflict_detected) { return true; }
	}
	return false; //no conflicts occurred during propagation
}

void PropagatorGeneric::Synchronise()
{
	next_position_on_trail_to_propagate_ = std::min(next_position_on_trail_to_propagate_, state_.GetNumberOfAssignedInternalBooleanVariables());
}

BooleanLiteral PropagatorGeneric::GetAndPopNextLiteralToPropagate()
{
	BooleanLiteral return_literal = state_.GetLiteralFromTrailAtPosition(next_position_on_trail_to_propagate_);
	next_position_on_trail_to_propagate_++;
	return return_literal;
}

bool PropagatorGeneric::IsPropagationComplete()
{
	assert(next_position_on_trail_to_propagate_ <= state_.GetNumberOfAssignedInternalBooleanVariables());
	return next_position_on_trail_to_propagate_ == state_.GetNumberOfAssignedInternalBooleanVariables();
}

} //end Pumpkin namespace