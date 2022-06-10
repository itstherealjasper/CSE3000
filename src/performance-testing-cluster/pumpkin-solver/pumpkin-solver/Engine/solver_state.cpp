#include "solver_state.h"
#include "variable_selector.h"
#include "../Utilities/runtime_assert.h"

#include <assert.h>
#include <algorithm>
#include <iostream>

namespace Pumpkin
{

SolverState::SolverState(int64_t num_Boolean_variables, ParameterHandler &params):
	variable_selector_(num_Boolean_variables, params.GetFloatParameter("decay-factor-variables")),
	value_selector_(num_Boolean_variables),
	assignments_(num_Boolean_variables), //note that the 0th position is not used
	//propagator_clausal_binary_(num_Boolean_variables * params.GetBooleanParameter("binary-clause-propagator")),
	propagator_clausal_(*this, num_Boolean_variables, params.GetFloatParameter("decay-factor-learned-clause"), params.GetIntegerParameter("lbd-threshold"), params.GetIntegerParameter("limit-num-temporary-clauses"), params.GetBooleanParameter("lbd-sorting-temporary-clauses"), params.GetFloatParameter("garbage-tolerance-factor")),
	decision_level_(0),
	simple_moving_average_lbd(params.GetIntegerParameter("glucose-queue-lbd-limit")),
	simple_moving_average_block(params.GetIntegerParameter("glucose-queue-reset-limit")),
	saved_state_num_permanent_clauses_(-1),
	saved_state_num_learnt_clauses_(-1),
	counters_(params.GetIntegerParameter("num-min-conflicts-per-restart")),
	failure_clause_(0),
	integer_variable_to_literal_info_(1), //index 0 not used for integers
	literal_information_(2), //the first two literals are not used
	domain_manager_(*this),
	next_cp_propagator_id_(UINT32_MAX),
	next_domain_update_trail_position_(0)
{	
	trail_.capacity(num_Boolean_variables);
	runtime_assert(num_Boolean_variables == 0); //need to change constructor parameters, since in the new version boolean variables are not directly created by the user
	IntegerVariable root_variable = CreateNewIntegerVariable(0, 1);
	true_literal_ = GetEqualityLiteral(root_variable, 1);
	false_literal_ = GetEqualityLiteral(root_variable, 0);
	pumpkin_assert_permanent(true_literal_ == ~false_literal_, "");
	bool conflict_detected = propagator_clausal_.AddUnitClause(true_literal_);
	pumpkin_assert_permanent(!conflict_detected, "");
}

SolverState::~SolverState()
{
	for (PropagatorGeneric* propagator : additional_propagators_) { delete propagator; }
}

int SolverState::ComputeLBDApproximation(BooleanLiteral* literals, int size)
{
	assert(lbd_flag.size() <= GetNumberOfInternalBooleanVariables() + 1);
	++lbd_counter;
	lbd_flag[0] = lbd_counter; //the zeroth level does not count towards the LBD score

	int lbd = 0;	
	for (int i = 0; i < size; i++)
	{
		runtime_assert(assignments_.IsAssigned(literals[i]));
		int level = assignments_.GetAssignmentLevel(literals[i]);
		lbd += (lbd_flag[level] != lbd_counter);
		lbd_flag[level] = lbd_counter;
		/*if (lbd_flag[level] != lbd_counter)
		{
			lbd_flag[level] = lbd_counter;
			lbd++;
		}*/		
	}
	return lbd;
}

int SolverState::ComputeLBD(const BooleanLiteral* literals, uint32_t size)
{//reasonably efficient but possibly could be improved
	static vec<bool> seen_decision_level(GetNumberOfInternalBooleanVariables() + 1, false);
	if (seen_decision_level.size() < GetNumberOfInternalBooleanVariables() + 1) { seen_decision_level.resize(GetNumberOfInternalBooleanVariables() + 1, false); } //+1 since variables are indexed starting from 1
																																						//count the number of unique decision levels by using the seen_decision_level to store whether or not a particular decision level has already been counted
	int num_nodes_visited = 0;
	for (int i = 0; i < size; i++)
	{
		BooleanLiteral lit = literals[i];
		if (assignments_.IsAssigned(lit))
		{
			int decision_level = assignments_.GetAssignmentLevel(lit.Variable());
			num_nodes_visited += (decision_level != 0 && seen_decision_level[decision_level] == false);
			seen_decision_level[decision_level] = true;
		}
		else //for unassigned literals, pesimistically assume that that each unassigned literal will be set at a different decision level
		{
			num_nodes_visited++;
		}
	}
	//clear the seen_decision_level data structure - all of its values should be false for the next function call
	for (int i = 0; i < size; i++)
	{
		BooleanLiteral lit = literals[i];
		if (assignments_.IsAssigned(lit))
		{
			int decision_level = assignments_.GetAssignmentLevel(lit.Variable());
			seen_decision_level[decision_level] = false;
		}
	}
	return num_nodes_visited;
}

PropagationStatus SolverState::PropagateEnqueuedLiterals()
{
	//clauses are propagated until completion
	//for the remaining propagators, in order of propagators: one literal is considered, and if it propagates, the process restarts from the clausal propagator, otherwise the next propagator is considered for propagation.
	//the intuition is to perform simpler propagations first - might change in the future though
	//todo fix this comment with the new version
	int64_t num_assigned_variables_old = GetNumberOfAssignedInternalBooleanVariables();
	PropagationStatus propagation_status = false;
	while (!propagation_status.conflict_detected && !IsPropagationComplete())
	{
		propagation_status = propagator_clausal_.Propagate();
		if (propagation_status.conflict_detected) { break; }

		while (next_domain_update_trail_position_ < trail_.size())		
		{
			if (trail_[next_domain_update_trail_position_].GetFlag())
			{
				const LiteralInformation& lit_info = GetLiteralInformation(trail_[next_domain_update_trail_position_]);

				domain_manager_.UpdateDomain(lit_info.integer_variable, lit_info.operation, lit_info.right_hand_side);				
			}			
			next_domain_update_trail_position_++;
		}	

		//todo probably these can be removed but not entirely sure
		for (PropagatorGeneric* propagator : additional_propagators_)
		{
			int num_assignments_before = GetNumberOfAssignedInternalBooleanVariables();

			propagation_status = propagator->PropagateOneLiteral();

			if (propagation_status.conflict_detected) { break; }
			if (num_assignments_before != GetNumberOfAssignedInternalBooleanVariables()) { break; } //if at least one literal was propagated, go to the clausal propagator
		}

		while (!propagator_queue_.IsEmpty())
		{
			int num_assignments_before = GetNumberOfAssignedInternalBooleanVariables();
			PropagatorGenericCP* propagator = propagator_queue_.GetNextPropagator();
			propagator_queue_.PopNextPropagator();

			propagation_status = propagator->Propagate();

			if (propagation_status.conflict_detected) { failure_clause_ = propagator->ExplainFailure(); break; } // If there was a conflict, then the main loop will break since the code will then proceed to conflict analysis

			if (num_assignments_before != GetNumberOfAssignedInternalBooleanVariables()) { break; } //if at least one literal was propagated, go to the clausal propagator. 
		}
	}
	counters_.num_conflicts += propagation_status.conflict_detected;
	counters_.num_conflicts_until_restart -= propagation_status.conflict_detected;
	counters_.propagations += (GetNumberOfAssignedInternalBooleanVariables() - num_assigned_variables_old);
	pumpkin_assert_extreme(DebugCheckFixedPointPropagation(), "Sanity check.");
	return propagation_status;
}

void SolverState::Backtrack(int backtrack_level)
{
	runtime_assert(backtrack_level >= 0 && backtrack_level < decision_level_);

	int num_assignments_for_removal = trail_.size() - trail_delimiter_[backtrack_level];//Back());
	for (int i = 0; i < num_assignments_for_removal; i++)
	{
		BooleanVariableInternal last_assigned_variable = trail_.last().Variable();
		variable_selector_.Readd(last_assigned_variable);
		value_selector_.UpdatePolarity(last_assigned_variable, trail_.last().IsPositive());
		assignments_.UnassignVariable(last_assigned_variable);

		//for now we have explicitly the != from domain even when bounds change -> later when we do things more lazily this needs to be changed
		if (next_domain_update_trail_position_ == trail_.size())
		{
			next_domain_update_trail_position_--;

			if (trail_.last().GetFlag())
			{
				const LiteralInformation& lit_info = GetLiteralInformation(trail_.last());
				if (lit_info.operation.IsNotEqual())
				{
					domain_manager_.ReaddToDomain(lit_info.integer_variable, lit_info.right_hand_side);
				}
				//integers with binary domains have special treatment, since [x!=0] is shared by using [x==1]
				else if (GetEqualityLiteral(lit_info.integer_variable, 0) == ~trail_.last())
				{
					pumpkin_assert_moderate(lit_info.operation.IsEquality() && lit_info.right_hand_side == 1, "Sanity check.");
					domain_manager_.ReaddToDomain(lit_info.integer_variable, 0);
				}
			}			
		}		
		trail_.pop();
	}
	trail_delimiter_.resize(backtrack_level);
	decision_level_ = backtrack_level;
	failure_clause_ = 0;

	propagator_clausal_.Synchronise();
	for (PropagatorGeneric* propagator : additional_propagators_) { propagator->Synchronise(); } //this is likely to be removed soon	

	propagator_queue_.Clear();
	for (PropagatorGenericCP* propagator : cp_propagators_) 
	{ 
		if (propagator) { propagator->Synchronise(); } //todo, this should be fixed, removing propagator leaves behind null pointers, so we need to avoid calling on those
	}		
}

void SolverState::BacktrackOneLevel()
{
	runtime_assert(1 == 2); //not sure if it takes into account CP stuff

	int num_assignments_for_removal = int(trail_.size() - trail_delimiter_.last());
	assert(num_assignments_for_removal >= 0);
	for (int i = 0; i < num_assignments_for_removal; i++)
	{
		UndoLastAssignment();
	}
	trail_delimiter_.pop();
	decision_level_--;
}

void SolverState::UndoLastAssignment()
{//the code below is copy pasted from backtracking, should be cleaned up in the future todo
	pumpkin_assert_permanent(failure_clause_ == 0 && propagator_queue_.IsEmpty(), "Error: we expect a conflict-free and empty-cp-queue state when we remove one assignment.");

	int num_assignments_for_removal = 1;
	for (int i = 0; i < num_assignments_for_removal; i++)
	{
		BooleanVariableInternal last_assigned_variable = trail_.last().Variable();
		variable_selector_.Readd(last_assigned_variable);
		value_selector_.UpdatePolarity(last_assigned_variable, trail_.last().IsPositive());
		assignments_.UnassignVariable(last_assigned_variable);

		//for now we have explicitly the != from domain even when bounds change -> later when we do things more lazily this needs to be changed
		if (next_domain_update_trail_position_ == trail_.size())
		{
			next_domain_update_trail_position_--;

			if (trail_.last().GetFlag())
			{
				const LiteralInformation& lit_info = GetLiteralInformation(trail_.last());
				if (lit_info.operation.IsNotEqual())
				{
					domain_manager_.ReaddToDomain(lit_info.integer_variable, lit_info.right_hand_side);
				}
				//integers with binary domains have special treatment, since [x!=0] is shared by using [x==1]
				else if (GetEqualityLiteral(lit_info.integer_variable, 0) == ~trail_.last())
				{
					pumpkin_assert_moderate(lit_info.operation.IsEquality() && lit_info.right_hand_side == 1, "Sanity check.");
					domain_manager_.ReaddToDomain(lit_info.integer_variable, 0);
				}
			}
		}
		trail_.pop();
	}
	propagator_clausal_.Synchronise();
	for (PropagatorGeneric* propagator : additional_propagators_) { propagator->Synchronise(); }

	propagator_queue_.Clear();
	for (PropagatorGenericCP* propagator : cp_propagators_) { propagator->Synchronise(); }
}

IntegerAssignmentVector SolverState::GetOutputAssignment()
{
	IntegerAssignmentVector output(NumIntegerVariables());
	for (int i = 1; i < integer_variable_to_literal_info_.size(); i++) //recall that index 0 is not used for variables
	{
		IntegerVariable variable(i);		
		output[variable] = GetIntegerAssignment(variable);
	}
	return output;
}

void SolverState::PrintTrail() const
{
	std::cout << "Trail\n";
	for (int i = 0; i < GetNumberOfAssignedInternalBooleanVariables(); i++)
	{
		std::cout << GetLiteralFromTheBackOfTheTrail(i).VariableIndex() << "\n";
	}
	std::cout << "end trail\n";
}

std::string SolverState::PrettyPrintLiteral(BooleanLiteral lit)
{
	std::string s;
	auto lit_info = GetLiteralInformation(lit);

	s += "[x";
	s += std::to_string(lit_info.integer_variable.id);
	s += " ";

	if (lit_info.operation.IsEquality()) { s += "="; }
	else if (lit_info.operation.IsNotEqual()) { s += "!="; }
	else if (lit_info.operation.IsLessOrEqual()) { s += "<="; }
	else if (lit_info.operation.IsGreaterOrEqual()) { s += ">="; }
	else { std::cout << "error?\n"; exit(1); }

	s += " ";
	s += std::to_string(lit_info.right_hand_side);
	s += "]";

	return s;
}

std::string SolverState::PrettyPrintClause(vec<BooleanLiteral>& literals)
{
	std::string s;
	for (BooleanLiteral lit : literals)
	{
		s += PrettyPrintLiteral(lit);
		//s += "(" + std::to_string(state_.assignments_.IsDecision(lit)) + " - lvl" + std::to_string(state_.assignments_.GetAssignmentLevel(lit)) + ")";
		s += " ";
	}
	return s;
}

BooleanAssignmentVector SolverState::ConvertIntegerSolutionToBooleanAssignments(const IntegerAssignmentVector& integer_solution)
{
	BooleanAssignmentVector boolean_assignment(GetNumberOfInternalBooleanVariables());
	for (int boolean_var_id = 1; boolean_var_id <= GetNumberOfInternalBooleanVariables(); boolean_var_id++)
	{
		BooleanLiteral positive_literal(GetInternalBooleanVariable(boolean_var_id), true);
		auto lit_info = GetLiteralInformation(positive_literal);

		//standard case, the integer is part of the input solution
		if (lit_info.integer_variable.id <= integer_solution.NumVariables())
		{
			boolean_assignment.AssignLiteral(positive_literal, integer_solution.IsSatisfied(lit_info.integer_variable, lit_info.operation, lit_info.right_hand_side));
		}
		//otherwise set the literal to false
		//	note that after encoding the objective, new variables are introduced that may not be in the input solution - todo should find a better solution than this current one, e.g., solving the problem under assumptions to get proper values for the remaining variables
		else
		{
			boolean_assignment.AssignLiteral(positive_literal, false);
		}
		
	}
	return boolean_assignment;
}

bool SolverState::DebugCheckFixedPointPropagation()
{
	//only check for fixed point propagation if no conflicts have been detected
	if (failure_clause_ == 0)
	{
		for (PropagatorGenericCP* propagator : cp_propagators_)
		{
			int num_assignments_before = GetNumberOfAssignedInternalBooleanVariables();
			PropagationStatus propagation_status = propagator->PropagateFromScratch();
			int num_assignments_after = GetNumberOfAssignedInternalBooleanVariables();

			pumpkin_assert_permanent(!propagation_status.conflict_detected, "Propagation loop not correct: there is a propagator that should report a conflict but it did not.");
			pumpkin_assert_permanent(num_assignments_before - num_assignments_after == 0, "Propagation loop not correct: there is a propagator that was not propagated to fixed point.");
			int m = 0;
		}
		pumpkin_assert_permanent(propagator_clausal_.IsInGoodState(), "Issue with the clausal propagator!");
	}
	return true;
}

BooleanVariableInternal SolverState::CreateNewInternalBooleanVariable()
{
	BooleanVariableInternal new_variable(GetNumberOfInternalBooleanVariables() + 1, false);

	variable_selector_.Grow();
	value_selector_.Grow();
	assignments_.Grow();
	propagator_clausal_.watch_list_.push();
	propagator_clausal_.watch_list_.push();

	//when a Boolean variable is created, its literals do not have a special meaning
	//todo explain
	literal_information_.emplace_back();
	literal_information_.emplace_back();

	return new_variable;
}

int SolverState::GetIntegerAssignment(IntegerVariable variable)
{
	//if the variable is watched, we explicitly track the lower and upper bound, so no issues there
	if (watch_list_CP_.IsVariableWatched(variable))
	{//now we do unnecessary work for debugging purposes, but this should be removed
		pumpkin_assert_permanent(IsAssigned(variable), "Error: either the bounds are not properly maintained, the variable is not watched properly, or somehow the variable does not appear in variable selection.");//this is mainly for debugging, it is not necessary since the lower and upper bound of the variable are fixed
		int variable_assignment = INT32_MAX;
		int num_assignments = 0;
		for (int val = 0; val < domain_manager_.domains_[variable.id].is_value_in_domain.Size(); val++)
		{
			//BooleanLiteral lit = GetEqualityLiteral(variable, val);
			BooleanLiteral lit_lb = GetLowerBoundLiteral(variable, val);
			BooleanLiteral lit_ub = GetUpperBoundLiteral(variable, val);
			if (assignments_.IsAssignedTrue(lit_lb) && assignments_.IsAssignedTrue(lit_ub))
			{
				num_assignments++;
				variable_assignment = val;
			}
		}
		pumpkin_assert_permanent(num_assignments == 1 && variable_assignment == domain_manager_.GetLowerBound(variable), "");
		return variable_assignment;
	}
	//if the variable is not watched, we do not explicitly track the lower and upper bound
	//but rather need to extract the value based on the literal assignments
	else
	{
		int root_lb = domain_manager_.GetRootLowerBound(variable);
		int root_ub = domain_manager_.GetRootUpperBound(variable);
		for (int value = root_lb; value <= root_ub; value++)
		{
			BooleanLiteral lit_lb = GetLowerBoundLiteral(variable, value);
			BooleanLiteral lit_ub = GetUpperBoundLiteral(variable, value);

			if (assignments_.IsAssignedTrue(lit_lb) && assignments_.IsAssignedTrue(lit_ub))
			{
				for (int i = value + 1; i <= root_ub; i++)
				{
					runtime_assert(assignments_.IsAssignedFalse(GetLowerBoundLiteral(variable, i)));
				}			
				return value;
			}
		}
		pumpkin_assert_permanent(1 == 2, "Problem with retreaving the assignment of a variable, no equality literals have been assigned in the solution.");
		int debug = 0;
	}	
}

void SolverState::CreateBooleanVariablesUpToIndex(int largest_variable_index)
{
	for (int i = GetNumberOfInternalBooleanVariables() + 1; i <= largest_variable_index; i++) 
	{ 
		CreateNewInternalBooleanVariable(); 
	}
}

void SolverState::SetStateResetPoint()
{
	runtime_assert(decision_level_ == 0);
	saved_state_num_permanent_clauses_ = propagator_clausal_.permanent_clauses_.size();
	saved_state_num_learnt_clauses_ = propagator_clausal_.NumTemporaryClauses();
	saved_state_num_root_literal_assignments_ = trail_.size();
}

void SolverState::PerformStateReset()
{
	//not very efficient since it removes one clause at a time instead of removing in bulk
	runtime_assert(decision_level_ == 0);
	runtime_assert(saved_state_num_permanent_clauses_ != -1);
	runtime_assert(propagator_clausal_.permanent_clauses_.size() >= saved_state_num_permanent_clauses_);
	pumpkin_assert_permanent(failure_clause_ == 0 && propagator_queue_.IsEmpty(), "Error: we expect a conflict-free and empty-cp-queue state when we remove one assignment.");

	int64_t num_removed_clauses = 0;
	int64_t num_removed_unit_clauses = 0;

	while (propagator_clausal_.permanent_clauses_.size() != saved_state_num_permanent_clauses_)
	{
		ClauseLinearReference clause_reference = propagator_clausal_.permanent_clauses_.last();
		Clause* clause = propagator_clausal_.clause_allocator_->GetClausePointer(clause_reference);
		propagator_clausal_.RemoveClauseFromWatchList(clause_reference);
		propagator_clausal_.permanent_clauses_.pop();
		propagator_clausal_.clause_allocator_->DeleteClause(clause_reference);
		num_removed_clauses++;
	}

	runtime_assert(trail_delimiter_.size() == 0);
	runtime_assert(trail_.size() >= saved_state_num_root_literal_assignments_);
	while (trail_.size() != saved_state_num_root_literal_assignments_)
	{
		num_removed_unit_clauses++;
		UndoLastAssignment();
	}

	int64_t num_removed_learned_clauses = 0;
	//only remove learned clauses if we actually did something since the last check point
	// std::cout << "c TODO: state reset might be problematic in general, fix\n";
	//	the problem is that it could be that solve added some learned clauses since last time even though the IF statement is satistified
	//	for the current version of the code this works fine, but in general it might be problematic
	//	even just comparing the number of learnt clauses is not enough
	//	a correct version would be to check if the learned clauses are identical to when we saved the state last time
	//		however this would mean that we need to store a copy of the clauses
	//		probably need a better way to do this -> this is left for the todo
	if (num_removed_clauses > 0 || num_removed_unit_clauses > 0 || propagator_clausal_.NumTemporaryClauses() != saved_state_num_learnt_clauses_)
	{
		num_removed_learned_clauses = propagator_clausal_.NumLearnedClauses();
		propagator_clausal_.RemoveAllLearnedClauses();
	}

	propagator_clausal_.Synchronise();
	for (PropagatorGeneric* propagator : additional_propagators_) { propagator->Synchronise(); }
	variable_selector_.Reset(); //could use different seeds here..perhaps every time a reset happens we increment the seed by one?

	// std::cout << "c state reset, removed " << num_removed_clauses << " hard clauses, " << num_removed_unit_clauses << " unit clauses, and " << num_removed_learned_clauses << " learned clauses\n";
}

BooleanVariableInternal SolverState::GetInternalBooleanVariable(int index)
{
	pumpkin_assert_moderate(index >= 1 && index <= GetNumberOfInternalBooleanVariables(), "Sanity check."); //recall that the 0th index is not used, so the largest index is the number of variables (not minus one)
	return BooleanVariableInternal(index, ComputeFlagFromScratch(BooleanLiteral(BooleanVariableInternal(index, 0), true)));
}

IntegerVariable SolverState::CreateNewIntegerVariable(int lower_bound, int upper_bound)
{
	pumpkin_assert_simple(lower_bound >= 0 && lower_bound <= upper_bound, "Sanity check.");

	//encode the unary representation of the integer variable
	//for now we are doing everything eagerly, todo improve later on
	
	int new_id = integer_variable_to_literal_info_.size();

	IntegerVariable new_integer_variable(new_id);
	domain_manager_.Grow(lower_bound, upper_bound);
	watch_list_CP_.Grow();

	integer_variable_to_literal_info_.push_back(IntegerVariableToLiteralInformation());
	std::vector<BooleanLiteral>& equality_literals = integer_variable_to_literal_info_.back().equality_literals;
	std::vector<BooleanLiteral>& greater_or_equal_literals = integer_variable_to_literal_info_.back().greater_or_equal_literals;

	//(upper bound) - (lower bound) number of Boolean variables needed for a unary representation
	//	but now we unnecessarily create variables as if the lower bound is zero, todo
	equality_literals.resize(upper_bound - 0 + 1);
	greater_or_equal_literals.resize(upper_bound - 0 + 1); //the zeros literal is not going to be used

	//[x == 0] and [x == upper_bound] are corner cases
	//	these are shared with the greater or equal literals as follows:
	//	[x == 0] <-> ~[x >= 1]
	//	[x == upper_bound] <-> [x >= upper_bound]
	//this is defined after the next loop

	//todo better comments

	for (int i = 1; i <= upper_bound; i++)
	{
		BooleanVariableInternal bool_var = CreateNewInternalBooleanVariable();
		equality_literals[i] = BooleanLiteral(bool_var, true);

		literal_information_[equality_literals[i].ToPositiveInteger()].integer_variable = new_integer_variable;
		literal_information_[equality_literals[i].ToPositiveInteger()].operation = DomainOperation(DomainOperation::DomainOperationCodes::EQUAL);
		literal_information_[equality_literals[i].ToPositiveInteger()].right_hand_side = i;

		literal_information_[(~equality_literals[i]).ToPositiveInteger()].integer_variable = new_integer_variable;
		literal_information_[(~equality_literals[i]).ToPositiveInteger()].operation = DomainOperation(DomainOperation::DomainOperationCodes::NOT_EQUAL);
		literal_information_[(~equality_literals[i]).ToPositiveInteger()].right_hand_side = i;
	}

	//now create the lower bound variables but ignore the following cases:
	//	skipping the index zero ([x >= 0]) since it is trivially true
	//	index 1 ([x >=1 ]) will be replaced by ~[x == 0]
	//	the last index ([x >= upper_bound]) will be replaced by [x == upper_bound]
	//	replacements take place after the loop

	//todo check the comments above -> I think I changed that the lower bound literals are the main literals, and equality literals may use some of those literals for corner cases
	for (int i = 1; i < upper_bound; i++)
	{
		BooleanVariableInternal bool_var = CreateNewInternalBooleanVariable();
		greater_or_equal_literals[i] = BooleanLiteral(bool_var, true);

		literal_information_[greater_or_equal_literals[i].ToPositiveInteger()].integer_variable = new_integer_variable;
		literal_information_[greater_or_equal_literals[i].ToPositiveInteger()].operation = DomainOperation(DomainOperation::DomainOperationCodes::GREATER_OR_EQUAL);
		literal_information_[greater_or_equal_literals[i].ToPositiveInteger()].right_hand_side = i;

		literal_information_[(~greater_or_equal_literals[i]).ToPositiveInteger()].integer_variable = new_integer_variable;
		literal_information_[(~greater_or_equal_literals[i]).ToPositiveInteger()].operation = DomainOperation(DomainOperation::DomainOperationCodes::LESS_OR_EQUAL);
		literal_information_[(~greater_or_equal_literals[i]).ToPositiveInteger()].right_hand_side = i-1;
	}
	//define equality literals
	//we encode the following: [var == val] <-> [var >= val] AND ~[var >= val+1]
	// 
	// 	   first corner cases
	//equality_literals[0] = ~greater_or_equal_literals[1];
	//equality_literals[upper_bound] = greater_or_equal_literals.back();

	//
	if (upper_bound == 1)
	{
		equality_literals[0] = ~equality_literals[1];
	}
	else
	{
		equality_literals[0] = ~greater_or_equal_literals[1];

		//change the literal information to display equality for the eq_lit[0] instead of greater or equal
		//(cosmetic change, but important since backtracking only looks at != to update domain)
		literal_information_[equality_literals[0].ToPositiveInteger()].integer_variable = new_integer_variable;
		literal_information_[equality_literals[0].ToPositiveInteger()].operation = DomainOperation(DomainOperation::DomainOperationCodes::EQUAL);
		literal_information_[equality_literals[0].ToPositiveInteger()].right_hand_side = 0;

		literal_information_[(~equality_literals[0]).ToPositiveInteger()].integer_variable = new_integer_variable;
		literal_information_[(~equality_literals[0]).ToPositiveInteger()].operation = DomainOperation(DomainOperation::DomainOperationCodes::NOT_EQUAL);
		literal_information_[(~equality_literals[0]).ToPositiveInteger()].right_hand_side = 0;
	}

	greater_or_equal_literals[0] = true_literal_; //currently all variables are non-negative
	greater_or_equal_literals[1] = ~equality_literals[0];
	greater_or_equal_literals[upper_bound] = equality_literals[upper_bound];

	//fix the literals below the LB to false
	//would be better to never create those variables in the first place but for simplicity it is like this now
	for (int i = 1; i <= lower_bound; i++)
	{
		bool conflict_detected = propagator_clausal_.AddUnitClause(greater_or_equal_literals[i]);
		pumpkin_assert_simple(!conflict_detected, "Sanity check.");
	}

	//define equality literals
	//	[var == value] <-> [var >= value] AND ~[var >= value]
	//	two special cases (skipped in the loop): 
	//		[var == 0] <-> ~[var >= 1]
	//		[var == upper_bound] <-> [var >= upper_bound]
	for (int value = 1; value < upper_bound; value++)
	{		
		//one side of the implication <-
		propagator_clausal_.AddTernaryClause(~greater_or_equal_literals[value], greater_or_equal_literals[value + 1], equality_literals[value]);
		//the other side of the implication ->
		propagator_clausal_.AddImplication(equality_literals[value], greater_or_equal_literals[value]);
		propagator_clausal_.AddImplication(equality_literals[value], ~greater_or_equal_literals[value+1]);
	}

	//define greater or equal literals
	//	[var >= value+1] -> [val >= value]
	//	special case (skipped in the loop): [var >= 1] -> [var >= 0], but since [var >= 0] is trivially true this is ignored
	for (int value = 1; value < upper_bound; value++)
	{
		propagator_clausal_.AddImplication(greater_or_equal_literals[value + 1], greater_or_equal_literals[value]);
	}

	return new_integer_variable;
}

IntegerVariable SolverState::CreateNewSimpleBoundedSumVariable(std::vector<BooleanLiteral>& literals, int sum_lower_bound)
{
	pumpkin_assert_simple(sum_lower_bound >= 1, "Sanity check.");

	//encode the unary representation of the integer variable
	//for now we are doing everything eagerly, todo improve later on

	int new_id = integer_variable_to_literal_info_.size();
	int upper_bound = literals.size();

	IntegerVariable new_integer_variable(new_id);
	domain_manager_.Grow(sum_lower_bound, upper_bound);
	watch_list_CP_.Grow();

	integer_variable_to_literal_info_.push_back(IntegerVariableToLiteralInformation());
	std::vector<BooleanLiteral>& equality_literals = integer_variable_to_literal_info_.back().equality_literals;
	std::vector<BooleanLiteral>& greater_or_equal_literals = integer_variable_to_literal_info_.back().greater_or_equal_literals;

	//(upper bound) - (lower bound) number of Boolean variables needed for a unary representation
	//	but now we unnecessarily create variables as if the lower bound is zero, todo
	equality_literals.resize(upper_bound - 0 + 1);
	//todo -> for now we set all equality literals to undefined! This will be solved in the future with lazy equality literal generation, but for now we just do undefined, since anyway for these variables we currently never use equality literals
	
	greater_or_equal_literals.resize(upper_bound - 0 + 1); //the zeros literal is not going to be used

	//now set the lower bound variables

	//	fix the literals below the LB to false
	for (int i = 1; i <= sum_lower_bound; i++)
	{
		bool conflict_detected = propagator_clausal_.AddUnitClause(literals[i - 1]);
		runtime_assert(!conflict_detected);
	}
	//	now define the variables in the internal data structures
	//		first for the trivial cases
	for (int i = 0; i <= sum_lower_bound; i++)
	{
		greater_or_equal_literals[i] = true_literal_;
	}
	//		and now in general
	for (int i = sum_lower_bound+1; i <= upper_bound; i++)
	{
		greater_or_equal_literals[i] = literals[i - 1];

		literal_information_[greater_or_equal_literals[i].ToPositiveInteger()].integer_variable = new_integer_variable;
		literal_information_[greater_or_equal_literals[i].ToPositiveInteger()].operation = DomainOperation(DomainOperation::DomainOperationCodes::GREATER_OR_EQUAL);
		literal_information_[greater_or_equal_literals[i].ToPositiveInteger()].right_hand_side = i;

		literal_information_[(~greater_or_equal_literals[i]).ToPositiveInteger()].integer_variable = new_integer_variable;
		literal_information_[(~greater_or_equal_literals[i]).ToPositiveInteger()].operation = DomainOperation(DomainOperation::DomainOperationCodes::LESS_OR_EQUAL);
		literal_information_[(~greater_or_equal_literals[i]).ToPositiveInteger()].right_hand_side = i - 1;
	}

	return new_integer_variable;
}

IntegerVariable SolverState::CreateNewEquivalentVariable(BooleanLiteral ref_lit)
{
	pumpkin_assert_simple(!ref_lit.IsUndefined(), "Sanity check.");

	//encode the unary representation of the integer variable
	//for now we are doing everything eagerly, todo improve later on

	int new_id = integer_variable_to_literal_info_.size();

	IntegerVariable new_integer_variable(new_id);
	domain_manager_.Grow(0, 1);
	watch_list_CP_.Grow();

	integer_variable_to_literal_info_.push_back(IntegerVariableToLiteralInformation());
	std::vector<BooleanLiteral>& equality_literals = integer_variable_to_literal_info_.back().equality_literals;
	std::vector<BooleanLiteral>& greater_or_equal_literals = integer_variable_to_literal_info_.back().greater_or_equal_literals;

	equality_literals.resize(2); //the zeros literal is not going to be used
	greater_or_equal_literals.resize(2); //the zeros literal is not going to be used

	//set equality literals
	equality_literals[0] = ~ref_lit;
	equality_literals[1] = ref_lit;

	//set lower bound literals
	greater_or_equal_literals[0] = true_literal_;
	greater_or_equal_literals[1] = ref_lit;
		
	return new_integer_variable;
}

IntegerVariable SolverState::CreateNewThresholdExceedingVariable(IntegerVariable variable, int threshold)
{
	pumpkin_assert_simple(domain_manager_.GetUpperBound(variable) > threshold && threshold > 0, "Sanity check.");

	int new_id = integer_variable_to_literal_info_.size();

	IntegerVariable new_integer_variable(new_id);
	int upper_bound = domain_manager_.GetUpperBound(variable) - threshold;
	domain_manager_.Grow(0, upper_bound);
	watch_list_CP_.Grow();

	integer_variable_to_literal_info_.push_back(IntegerVariableToLiteralInformation());
	std::vector<BooleanLiteral>& equality_literals = integer_variable_to_literal_info_.back().equality_literals;
	std::vector<BooleanLiteral>& greater_or_equal_literals = integer_variable_to_literal_info_.back().greater_or_equal_literals;

	equality_literals.resize(upper_bound - 0 + 1);
	greater_or_equal_literals.resize(upper_bound - 0 + 1); //the zeros literal is not going to be used

	//set equality literals
	//todo -> for now we set all equality literals to undefined! This will be solved in the future with lazy equality literal generation, but for now we just do undefined, since anyway for these variables we currently never use equality literals
	/*equality_literals[0] = GetUpperBoundLiteral(variable, threshold);
	for (int i = 1; i <= upper_bound; i++)
	{
		//equality_literals[i] = GetEqua
	}*/

	//set lower bound literals
	greater_or_equal_literals[0] = true_literal_;
	for (int i = 1; i <= upper_bound; i++)
	{
		greater_or_equal_literals[i] = GetLowerBoundLiteral(variable, i + threshold);
	}
	return new_integer_variable;
}

} //end Pumpkin namespace