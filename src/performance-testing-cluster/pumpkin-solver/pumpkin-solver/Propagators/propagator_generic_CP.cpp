#include "propagator_generic_CP.h"
#include "../Engine/solver_state.h"

#include <algorithm>

namespace Pumpkin
{

std::vector<IntegerVariable> PropagatorGenericCP::helper_variables_involved_;

SimpleBoundTracker PropagatorGenericCP::helper_bound_tracker_;

bool PropagatorGenericCP::DebugCheckPropagationExplanation(const BooleanLiteral* explanation_clause, int num_literals)
{
	//check that all literals are assigned in the clause
	for (int i = 0; i < num_literals; i++)
	{
		BooleanLiteral literal = explanation_clause[i];
		pumpkin_assert_simple(state_->assignments_.IsAssigned(literal), "An unassigned literal has been detected as part of a propagation explanation.");
		pumpkin_assert_simple(i == 0 || state_->assignments_.IsAssignedFalse(literal), "By convention, all literals apart from the first one in the explanation should be set to false but this does not hold.");
		pumpkin_assert_simple(i > 0 || state_->assignments_.IsAssignedTrue(literal), "By convention, the first literal in the explanation is the propagated literal and it should be true but it is false.");
	}
	pumpkin_assert_simple(state_->GetCurrentDecisionLevel() == 0 || state_->assignments_.GetAssignmentLevel(explanation_clause[0]) > 0, "The propagated literal is set to true at the root level but propagation did not take place at the root.");

	InitialiseVariablesInvolvedInExplanation(explanation_clause, num_literals);
	InitialiseSimpleBoundTracker(explanation_clause, num_literals, true);

	bool conflict_detected = DebugCheckInfeasibility(helper_variables_involved_, helper_bound_tracker_);
	//if it turns out to be feasible, there is an inconsistency in the propagator
	//print out debug information
	if (!conflict_detected)
	{
		std::cout << "Error: explanation inconsistent:\n";
		for (int i = 0; i < num_literals - 1; i++)
		{
			BooleanLiteral negated_literal = ~explanation_clause[i];
			std::cout << state_->PrettyPrintLiteral(negated_literal) << " & ";
		}
		std::cout << state_->PrettyPrintLiteral(~explanation_clause[num_literals - 1]) << "\n";
		std::cout << "should lead to infeasibility, but the propagator cannot confirm.\n";
		abort();
	}

	//now check satisfiability of the clause
	bool clause_satisfiable = false;
	for (int i = 0; i < num_literals; i++)
	{
		//we skip checking literals that are false at the root
		//	this is because it is not clear what the domain tracker should do in this case
		if (state_->assignments_.IsRootAssignment(explanation_clause[i]) && state_->assignments_.IsAssignedFalse(explanation_clause[i]))
		{
			continue;
		}

		InitialiseVariablesInvolvedInExplanation(&explanation_clause[i], 1);
		InitialiseSimpleBoundTracker(&explanation_clause[i], 1, false);
		bool infeasible = DebugCheckInfeasibility(helper_variables_involved_, helper_bound_tracker_);
		if (!infeasible)
		{
			clause_satisfiable = true;
			break;
		}		
	}
	if (!clause_satisfiable)
	{
		std::cout << "Error: explanation clause deemed infeasible\n";
		for (int i = 0; i < num_literals - 1; i++)
		{
			BooleanLiteral literal = explanation_clause[i];
			std::cout << state_->PrettyPrintLiteral(literal) << " | ";
		}
		std::cout << state_->PrettyPrintLiteral(explanation_clause[num_literals - 1]) << "\n";
		std::cout << "should be satisfiable at the root, but the propagator report infeasibility.\n";
		abort();
	}

	return true;
}

bool PropagatorGenericCP::DebugCheckFailureExplanation(const BooleanLiteral* failure_clause, int num_literals)
{
	pumpkin_assert_simple(failure_clause != 0, "Sanity check.");

	//initial check that all literals are assigned to false
	for (int i = 0; i < num_literals; i++)
	{
		BooleanLiteral literal = failure_clause[i];
		pumpkin_assert_simple(state_->assignments_.IsAssigned(literal), "An unassigned literal detected as part of a failure explanation.");
		pumpkin_assert_simple(!state_->assignments_.IsAssignedTrue(literal), "A true literal detected as a part of a failure, this cannot be the case.");
		pumpkin_assert_simple(state_->assignments_.IsAssignedFalse(literal), "All literals in a failure clause are suppose to be set to false but this is not the case.");
	}

	InitialiseVariablesInvolvedInExplanation(failure_clause, num_literals);
	InitialiseSimpleBoundTracker(failure_clause, num_literals, true);

	bool conflict_detected = DebugCheckInfeasibility(helper_variables_involved_, helper_bound_tracker_);
	//if no infeasibility is detected, there is an inconsistency in the propagator
	//print out debug information
	if (!conflict_detected)
	{
		std::cout << "Error: failure clause inconsistent:\n";
		for (int i = 0; i < num_literals - 1; i++)
		{
			BooleanLiteral negated_literal = ~failure_clause[i];
			std::cout << state_->PrettyPrintLiteral(negated_literal) << " & ";
		}
		std::cout << state_->PrettyPrintLiteral(~failure_clause[num_literals - 1]) << "\n";
		std::cout << "should lead to infeasibility, but the propagator cannot confirm.\n";
		abort();
	}

	//now check satisfiability of the clause
	bool clause_satisfiable = false;
	for (int i = 0; i < num_literals; i++)
	{
		//we skip checking literals that are false at the root
		//	this is because it is not clear what the domain tracker should do in this case
		if (state_->assignments_.IsRootAssignment(failure_clause[i]) && state_->assignments_.IsAssignedFalse(failure_clause[i]))
		{
			continue;
		}

		InitialiseVariablesInvolvedInExplanation(&failure_clause[i], 1);
		InitialiseSimpleBoundTracker(&failure_clause[i], 1, false);
		bool infeasible = DebugCheckInfeasibility(helper_variables_involved_, helper_bound_tracker_);
		if (!infeasible)
		{
			clause_satisfiable = true;
			break;
		}
	}
	//the clause may not need to be satisfiable at the root, e.g., it may be the final proof of infeasibility of the problem
	if (!clause_satisfiable && state_->GetCurrentDecisionLevel() > 0)
	{
		std::cout << "Error: explanation clause deemed infeasible\n";
		for (int i = 0; i < num_literals - 1; i++)
		{
			BooleanLiteral literal = failure_clause[i];
			std::cout << state_->PrettyPrintLiteral(literal) << " | ";
		}
		std::cout << state_->PrettyPrintLiteral(failure_clause[num_literals - 1]) << "\n";
		std::cout << "should be satisfiable at the root, but the propagator report infeasibility.\n";
		abort();
	}

	return true;
}

void PropagatorGenericCP::InitialiseVariablesInvolvedInExplanation(const BooleanLiteral* explanation_clause, int num_literals)
{
	helper_variables_involved_.clear();

	//extract variables from the literals
	for (int i = 0; i < num_literals; i++)
	{
		BooleanLiteral literal = explanation_clause[i];
		if (literal == state_->false_literal_) { continue; }

		auto lit_info = state_->GetLiteralInformation(literal);
		helper_variables_involved_.push_back(lit_info.integer_variable);
	}

	//remove duplicates
	//	first sort by id...
	std::sort(helper_variables_involved_.begin(), helper_variables_involved_.end(), [](const IntegerVariable& var1, const IntegerVariable& var2)->bool { return var1.id < var2.id; });
	//  ...and then filter out repeating numbers
	int last_position = 0;
	for (int i = 1; i < helper_variables_involved_.size(); i++)
	{
		if (helper_variables_involved_[i] != helper_variables_involved_[last_position])
		{
			last_position++;
			helper_variables_involved_[last_position] = helper_variables_involved_[i];
		}
	}
	helper_variables_involved_.resize(last_position + 1);
}

void PropagatorGenericCP::InitialiseSimpleBoundTracker(const BooleanLiteral* explanation_clause, int num_literals, bool negate_literals)
{
	pumpkin_assert_simple(!helper_variables_involved_.empty(), "The helper vector not initialised.");

	helper_bound_tracker_.Clear();
	//set the bound tracker to contain the root bounds
	for (IntegerVariable variable : helper_variables_involved_)
	{
		int root_lb = state_->domain_manager_.GetRootLowerBound(variable);
		int root_ub = state_->domain_manager_.GetRootUpperBound(variable);
		helper_bound_tracker_.RegisterVariable(variable, root_lb, root_ub);
	}
	//now update the bounds based on the explanation clause
	for (int i = 0; i < num_literals; i++)
	{
		BooleanLiteral literal = negate_literals ? ~explanation_clause[i] : explanation_clause[i];
		if (literal == state_->false_literal_ || literal == state_->true_literal_) { continue; }

		auto lit_info = state_->GetLiteralInformation(literal);
		helper_bound_tracker_.UpdateDomain(lit_info.integer_variable, lit_info.operation, lit_info.right_hand_side);
		int m = 0;
	}
}

}