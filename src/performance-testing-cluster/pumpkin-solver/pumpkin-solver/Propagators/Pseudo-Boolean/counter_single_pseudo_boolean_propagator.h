#pragma once

#include "explanation_single_counter_pseudo_boolean_constraint.h"
#include "../propagator_generic.h"
#include "../../Utilities/small_helper_structures.h"
#include "../../Utilities/linear_boolean_function.h"
#include "../../Utilities/boolean_assignment_vector.h"

namespace Pumpkin
{
//represents a constraint of the form \sum w_i x_i <= UB
//however this is an implementation of a very special case
//	which assumes there is only one such propagator and it is used after all other propagators have propagated	
//	this is okay for MaxSAT but not in general constraint solving
class PropagatorCounterSinglePseudoBoolean : public PropagatorGeneric
{
public:
	PropagatorCounterSinglePseudoBoolean(SolverState &state, bool bump_objective_literals);

	bool NotifyDomainChange(LinearBooleanFunction &objective_function, int64_t upper_bound); //activate only at root level; initialises the propagator. Returns true if a conflict has been detected, otherwise false, indicating unsatisfiability
	void Deactivate();
	bool StrengthenUpperBound(int64_t new_upper_bound); //returns true if a conflict has been detected while strengthening the bound (problem is unsat with the tighter bound), otherwise false.

	void Synchronise(SolverState& state);

	int DebugSum(BooleanAssignmentVector& sol);

	Clause* ExplainLiteralPropagation(BooleanLiteral literal); //returns the conjunction that forces the assignment of input literal to true. Assumes the input literal is not undefined.

//private:

	void BumpVariables(int end_index);
	int64_t MinWeight() const;

	PropagationStatus PropagateLiteral(BooleanLiteral true_literal);

	struct Entry { int64_t weight; std::vector<BooleanLiteral> literals; };
	std::vector<Entry> weighted_literals_;
	LinearBooleanFunction objective_function_;
	std::vector<BooleanLiteral> violating_literals;
	int64_t upper_bound_;
	int64_t root_slack_;
	bool activated_, bump_objective_literals_;
};

} //end Pumpkin namespace