#pragma once

#include "../Propagators/Clausal/clause.h"
#include "../Utilities/propagation_status.h"

namespace Pumpkin
{

class SolverState;

class PropagatorGeneric
{
public:
	PropagatorGeneric(SolverState &state);

	virtual PropagationStatus Propagate(); //does full propagation, i.e. until there is nothing else left to propagate. Returns true if a conflict has been detected, and false otherwise.
	PropagationStatus PropagateOneLiteral(); //looks at the next literal on the trail and does propagation. Returns true if a conflict has been detected and false otherwise. Considering one literal at a time is useful since it allows us to then ask simpler propagators to propagate with respect to the new enqueued literal before going further with this propagator.
	virtual void Synchronise(); //after the state backtracks, it should call this synchronise method which will internally set the pointer of the trail to the new correct position

	virtual Clause * ExplainLiteralPropagation(BooleanLiteral literal) = 0; //returns the explanation of the propagation. Assumes the input literal is not undefined.
	   
	bool IsPropagationComplete();

protected:
	//Returns true if a conflict has been detected and false otherwise.
	virtual PropagationStatus PropagateLiteral(BooleanLiteral true_literal) = 0;

	BooleanLiteral GetAndPopNextLiteralToPropagate();
	
	//tracks the position of the literals on the trail that needs to be propagated
	//needs to be updated each Backtrack using Synchronise
	SolverState& state_;
	size_t next_position_on_trail_to_propagate_;
};

} //end Pumpkin namespace
