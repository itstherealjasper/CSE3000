#pragma once

#include "value_selector.h"
#include "variable_selector.h"
#include "integer_variable_domain_manager.h"
#include "watch_list_CP.h"
#include "propagator_queue.h"
#include "../Propagators/Clausal/propagator_clausal.h"
#include "../Utilities/boolean_variable_internal.h"
#include "../Utilities/boolean_literal.h"
#include "../Utilities/integer_variable.h"
#include "../Utilities/domain_operation.h"
#include "../Utilities/small_helper_structures.h"
#include "../Utilities/solver_parameters.h"
#include "../Utilities/assignments.h"
#include "../Utilities/simple_moving_average.h"
#include "../Utilities/cumulative_moving_average.h"
#include "../Utilities/integer_assignment_vector.h"
#include "../Utilities/parameter_handler.h"
#include "../Utilities/Vec.h"
#include "../Utilities/counters.h"
#include "../Utilities/pumpkin_assert.h"

#include <vector>
#include <algorithm>

namespace Pumpkin
{

class PropagatorGeneric; //need a forward declaration since propagators use the state class

//A class representing the internal state of the SAT solver. 
class SolverState
{
public:

	SolverState(int64_t num_Boolean_variables, ParameterHandler&);
	~SolverState();

	//increases the current decision level and updates internal data structures accordingly
	//should only have one decision literal per decision level.
	void IncreaseDecisionLevel();

//methods for enqueuing and variable assignment--------------------

	//places the literal in the propagation queue, noting its assignment was the result of a decision rather than a propagation
	//Assumes the corresponding variable has not yet been set
	void EnqueueDecisionLiteral(BooleanLiteral); 

	//places the literal in the propagation queue, noting its assignment was the result of a propagation with the corresponding reason_code
	//Assumes the corresponding variable has not yet been set
	void EnqueuePropagatedLiteral(BooleanLiteral, uint32_t reason_code);

	//propagates all the enqueued literals
	//returns true if a conflict has been detected, otherwise false
	//in case of a conflict, the failure class is set
	PropagationStatus PropagateEnqueuedLiterals();

//methods for determining the reason of assignments------------------

	//Clause& GetReasonClause(BooleanLiteral); -> problematic since nothing to return in case of a decision, cannot return a null reference
	Clause* GetReasonClausePointer(BooleanLiteral);
	Clause& GetReasonClausePropagation(BooleanLiteral);
	Clause* GetReasonClausePointerPropagation(BooleanLiteral);
	Clause& GetReasonClauseFailure();

//methods for backtracking--------------------
	//reverts all assignments done up to level 'backtrack_level'  (does not unassign variables from 'backtrack_level')
	//After calling the method, the current decision level will be set to 'backtrack_level'
	//assumes the state is not at 'backtrack_level'. Use reset to backtrack to root to avoid testing if at backtrack level zero
	void Backtrack(int backtrack_level);
	//backtracks at level zero. Note that level zero assignments are still kept.
	void Reset();
	//Reverts assignments done in the current level
	void BacktrackOneLevel(); 
	//Removes the last assignment. Note that the assignment could have been done as part of a decision or a propagation. 
	//To remove all assignments from the current level see BacktrackOneLevel()
	void UndoLastAssignment(); //todo I think we need to remove these? not used?

//methods for getting information about the state--------------------

	//if there are no decision literals on the trail, returns an undefined literal
	BooleanLiteral GetLastDecisionLiteralOnTrail() const;
	//returns the decision that was made at level 'decision_level'. Assumes the decision level is at most the current decision level. At root level, returns the undefined literal
	BooleanLiteral GetDecisionLiteralForLevel(int decision_level) const; 	
	//returns true if the state is current at the root level, i.e., decision level zero
	bool IsRootLevel() const;
	//returns an integer representing the current decision level
	int GetCurrentDecisionLevel() const; 
	//returns the number of assigned variables. Note this includes both decision and propagated variables. 
	size_t GetNumberOfAssignedInternalBooleanVariables() const;
	//return the total number of internal Boolean variables in the problem (note that variables do not necessarily need to be present in the constraints)
	size_t GetNumberOfInternalBooleanVariables() const;
	int NumIntegerVariables() const;

	BooleanLiteral GetLiteralFromTrailAtPosition(size_t index) const;
	//returns a literal placed on the trail. In this case the trail behaves like a stack: 
	//[0] is the most recently pushed literal, [1] is the second most recent, and so forth.
	BooleanLiteral GetLiteralFromTheBackOfTheTrail(size_t index) const;

	//returns the satisfying assignment present in the current state
	//should only be called if the assignment has been built
	IntegerAssignmentVector GetOutputAssignment();

	//returns if all variables of the problem have been assigned a truth value. Internally might update lazy data structures
	bool IsAssignmentBuilt(); 
	bool IsPropagationComplete(); //returns true if all propagators have completed propagation for all literals on the trail, otherwise returns false
	
	vec<unsigned int> lbd_flag;
	unsigned int lbd_counter;
	int ComputeLBDApproximation(BooleanLiteral* literals, int size);
	int ComputeLBD(const BooleanLiteral* literals, uint32_t size);

	void PrintTrail() const;
	std::string PrettyPrintLiteral(BooleanLiteral lit);
	std::string PrettyPrintClause(vec<BooleanLiteral>& literals);

//methods for adding constraints--------------------

	int AddPropagator(PropagatorGeneric* propagator); //adds the propagator to the state and returns its assigned ID (useful in case the propagator needs to be modified, see GetPropagator). Ownership of the propagator is transfered to SolverState, e.g., the destructor of SolverState will delete the propagator and it is assumed the propagator will not be deleted by some other method
	PropagatorGeneric* GetPropagator(int propagator_id);
	bool HasPropagator(PropagatorGeneric* propagator);

	void AddPropagatorCP(PropagatorGenericCP* propagator);
	void RemovePropagatorCP(PropagatorGenericCP* propagator); //removes the propagator from the solver, but this should be done with care for now, i.e., any root propagations that were done by the propagator are _not_ undone
	PropagatorGenericCP* GetPropagatorCP(uint32_t id);
	std::vector<PropagatorGenericCP*> cp_propagators_;
	uint32_t next_cp_propagator_id_;

	//The input lbd is the lbd of the learned clause_. Assumes this method is called prior to backtracking and adding the learned clause_ to the database. 
	//updates the moving average data structures that are used to determined if restarts are to take place.
	void UpdateMovingAveragesForRestarts(int learned_clause_lbd); 

	//these two are experimental methods, for now used only as part of the upper bounding algorithm with varying resolution
	//reverting the state will remove all learned clauses and permenant clauses added
	//other parts of the state are kept though
	//should implement a 'Shrink' method (opposite of Grow), but for now we ignore this issue (todo)
	void SetStateResetPoint();
	void PerformStateReset();

//methods related to integer variables----------------

	struct LiteralInformation
	{
		IntegerVariable integer_variable;
		DomainOperation operation;
		int right_hand_side;
	};

	struct IntegerVariableToLiteralInformation
	{
		std::vector<BooleanLiteral> equality_literals, greater_or_equal_literals;
	};

	std::vector<IntegerVariableToLiteralInformation> integer_variable_to_literal_info_;
	int next_domain_update_trail_position_;

	BooleanVariableInternal GetInternalBooleanVariable(int index);

	IntegerVariable CreateNewIntegerVariable(int lower_bound, int upper_bound);
	IntegerVariable CreateNewSimpleBoundedSumVariable(std::vector<BooleanLiteral>& literals, int sum_lower_bound); //the literals are the lower bound variables, lit [i] set to true means that >= i+1. With this in mind the integer variable is created. This is currently used a somewhat-okay-hack in core guided search, todo see if there are better ways.
	IntegerVariable CreateNewEquivalentVariable(BooleanLiteral ref_lit); //create a new binary variable that takes the same value as the given lit -> used in core-guided reformulation, in the long term this will be replaced with views
	IntegerVariable CreateNewThresholdExceedingVariable(IntegerVariable variable, int threshold); //creates a new variable that takes the value max(0, variable - threshold). In the future will be replaced with views.
	BooleanLiteral GetEqualityLiteral(IntegerVariable, int right_hand_side);
	BooleanLiteral GetNotEqualLiteral(IntegerVariable, int right_hand_side);
	BooleanLiteral GetLowerBoundLiteral(IntegerVariable, int right_hand_side);
	BooleanLiteral GetUpperBoundLiteral(IntegerVariable, int right_hand_side);
	BooleanLiteral GetCurrentLowerBoundLiteral(IntegerVariable);
	BooleanLiteral GetCurrentUpperBoundLiteral(IntegerVariable);
	bool AddSimpleSumConstraint(IntegerVariable x, IntegerVariable y, int right_hand_side); //adds the constraint x + y == right_hand_side
	const LiteralInformation& GetLiteralInformation(BooleanLiteral) const;

	bool SetUpperBoundForVariable(IntegerVariable variable, int new_upper_bound); //can only be used at the root level, amounts to adding a unit clause

	void NotifyPropagatorsSubscribedtoInequalityChanges(IntegerVariable);
	void NotifyPropagatorsSubscribedtoLowerBoundChanges(IntegerVariable);
	void NotifyPropagatorsSubscribedtoUpperBoundChanges(IntegerVariable);

	BooleanAssignmentVector ConvertIntegerSolutionToBooleanAssignments(const IntegerAssignmentVector &integer_solution);

	std::vector<LiteralInformation> literal_information_;
	PropagatorQueue propagator_queue_;
	BooleanLiteral true_literal_, false_literal_;

	bool ComputeFlagFromScratch(BooleanVariableInternal) const;
	bool ComputeFlagFromScratch(BooleanLiteral) const;

	bool IsAssigned(IntegerVariable);
	int GetIntegerAssignment(IntegerVariable variable);
		
//public class variables--------------------------todo for now these are kept as public, but at some point most will be moved into the private section
	VariableSelector variable_selector_;
	ValueSelector value_selector_;
	Assignments assignments_;

	//------propagators
	PropagatorClausal propagator_clausal_;
	std::vector<PropagatorGeneric*> additional_propagators_;

	//data structures that control restarts
	SimpleMovingAverage simple_moving_average_block, simple_moving_average_lbd;
	CumulativeMovingAverage cumulative_moving_average_lbd;

//private: 

	//performs an assignment to make the literal true. 
	//Note that the corresponding variable is considered to be assigned a value (0 if the literal was negative, 1 if the literal was positive). 
	//This is a method used internally; usually one would not call this method directly but use 'enqueue' methods instead.
	void MakeAssignment(BooleanLiteral literal, uint64_t code);

	//helper vector when adding binary/ternary clauses
	//std::vector<BooleanLiteral> helper_vector_;

//state restoring variables, for now we only track the permanently added clauses but will do more in the future (maybe)
	int64_t saved_state_num_permanent_clauses_;
	int64_t saved_state_num_learnt_clauses_;
	int64_t saved_state_num_root_literal_assignments_;	

	Counters counters_;

//private class variables--------------------------
	Clause *failure_clause_;
	vec<BooleanLiteral> trail_;
	vec<int> trail_delimiter_; //[i] is the position where the i-th decision level ends (exclusive) on the trail.
	IntegerVariableDomainManager domain_manager_;
	WatchListCP watch_list_CP_;

	int decision_level_;

private:

	bool DebugCheckFixedPointPropagation();

	//creates the next Boolean variable and grows internal data structures appropriately
	//should always be called when creating a new Boolean variable
	BooleanVariableInternal CreateNewInternalBooleanVariable();
	//creates additional variables to support variable indicies from the current index up until the given value (inclusive)
	//executes multiple calls of 'CreateNewVariable'
	void CreateBooleanVariablesUpToIndex(int largest_variable_index);
};

inline void SolverState::IncreaseDecisionLevel()
{
	decision_level_++;
	trail_delimiter_.push(int(trail_.size()));
}

inline void SolverState::EnqueueDecisionLiteral(BooleanLiteral decision_literal)
{
	assert(!assignments_.IsAssigned(decision_literal));	
	MakeAssignment(decision_literal, 0);
}

inline void SolverState::EnqueuePropagatedLiteral(BooleanLiteral propagated_literal, uint32_t reason_code)
{
	pumpkin_assert_advanced(!assignments_.IsAssigned(propagated_literal) && reason_code != 0, "Sanity check.");
	MakeAssignment(propagated_literal, reason_code);
}

/*inline Clause& SolverState::GetReasonClause(BooleanLiteral literal)
{
	uint32_t code = assignments_.GetAssignmentReasonCode(literal.Variable());
	if (code <= next_cp_propagator_id_)
	{
		return clause_manager_.GetClause(ClauseLinearReference(code));
	}
	else
	{
		return *GetPropagatorCP(code)->ExplainLiteralPropagation(literal);
	}	
}*/

inline Clause* SolverState::GetReasonClausePointer(BooleanLiteral literal)
{
	uint32_t code = assignments_.GetAssignmentReasonCode(literal.Variable());
	if (code == 0) 
	{ 
		return NULL; 
	}
	else if (code <= next_cp_propagator_id_)
	{
		return propagator_clausal_.clause_allocator_->GetClausePointer(ClauseLinearReference(code));
	}
	else
	{
		return GetPropagatorCP(code)->ExplainLiteralPropagation(literal);
	}
}

inline Clause& SolverState::GetReasonClausePropagation(BooleanLiteral literal)
{
	assert(assignments_.GetAssignmentReasonCode(literal.Variable()) != 0);

	uint32_t code = assignments_.GetAssignmentReasonCode(literal.Variable());
	if (code <= next_cp_propagator_id_)
	{
		return propagator_clausal_.clause_allocator_->GetClause(ClauseLinearReference(code));
	}
	else
	{
		return *GetPropagatorCP(code)->ExplainLiteralPropagation(literal);
	}

	return propagator_clausal_.clause_allocator_->GetClause(ClauseLinearReference(assignments_.GetAssignmentReasonCode(literal.Variable())));
}

inline Clause* SolverState::GetReasonClausePointerPropagation(BooleanLiteral literal)
{
	assert(assignments_.GetAssignmentReasonCode(literal.Variable()) != 0);

	uint32_t code = assignments_.GetAssignmentReasonCode(literal.Variable());
	if (code <= next_cp_propagator_id_)
	{
		return propagator_clausal_.clause_allocator_->GetClausePointer(ClauseLinearReference(code));
	}
	else
	{
		return GetPropagatorCP(code)->ExplainLiteralPropagation(literal);
	}
}

inline Clause& SolverState::GetReasonClauseFailure()
{
	assert(failure_clause_ != 0);
	return *failure_clause_;
}

inline void SolverState::Reset()
{
	if (GetCurrentDecisionLevel() != 0) Backtrack(0);
}

inline BooleanLiteral SolverState::GetEqualityLiteral(IntegerVariable variable, int right_hand_side)
{
	pumpkin_assert_moderate(!integer_variable_to_literal_info_[variable.id].equality_literals[right_hand_side].IsUndefined(), "Sanity check.");
	return integer_variable_to_literal_info_[variable.id].equality_literals[right_hand_side];
}

inline BooleanLiteral SolverState::GetNotEqualLiteral(IntegerVariable variable, int right_hand_side)
{
	return ~GetEqualityLiteral(variable, right_hand_side);
}

inline BooleanLiteral SolverState::GetLowerBoundLiteral(IntegerVariable variable, int right_hand_side)
{
	if (right_hand_side <= 0) { return true_literal_; } //for now all variables are nonnegative
	return integer_variable_to_literal_info_[variable.id].greater_or_equal_literals[right_hand_side];
}

inline BooleanLiteral SolverState::GetUpperBoundLiteral(IntegerVariable variable, int right_hand_side)
{
	if (right_hand_side + 1 >= integer_variable_to_literal_info_[variable.id].greater_or_equal_literals.size()) { return true_literal_; }
	return ~(GetLowerBoundLiteral(variable, right_hand_side+1));
}

inline BooleanLiteral SolverState::GetCurrentLowerBoundLiteral(IntegerVariable variable)
{
	return GetLowerBoundLiteral(variable, domain_manager_.GetLowerBound(variable));
}

inline BooleanLiteral SolverState::GetCurrentUpperBoundLiteral(IntegerVariable variable)
{
	return GetUpperBoundLiteral(variable, domain_manager_.GetUpperBound(variable));
}

inline bool SolverState::AddSimpleSumConstraint(IntegerVariable x, IntegerVariable y, int right_hand_side)
{
	runtime_assert(GetCurrentDecisionLevel() == 0); //we may only add these constraints at the root for now

	int x_lb = domain_manager_.GetLowerBound(x);
	int x_ub = domain_manager_.GetUpperBound(x);

	int y_lb = domain_manager_.GetLowerBound(y);
	int y_ub = domain_manager_.GetUpperBound(y);

	runtime_assert(y_lb == 0 && y_ub == (x_ub - x_lb) && x_ub == right_hand_side); //for now we only deal with this special case, eventually we could look to explore more

	for (int i = x_lb; i <= x_ub; i++)
	{
		//[x >= i] -> [y <= rhs - i]
		if (i > domain_manager_.GetLowerBound(x))
		{
			BooleanLiteral x_lit = GetLowerBoundLiteral(x, i);
			BooleanLiteral y_lit = GetUpperBoundLiteral(y, right_hand_side - i);
			bool conflict_detected = propagator_clausal_.AddImplication(x_lit, y_lit);
			runtime_assert(!conflict_detected);
			int m = 0;
		}		
		//[x <= i] -> [y >= rhs - i]
		if (i < domain_manager_.GetUpperBound(x))
		{
			BooleanLiteral x_lit = GetUpperBoundLiteral(x, i);
			BooleanLiteral y_lit = GetLowerBoundLiteral(y, right_hand_side - i);
			bool conflict_detected = propagator_clausal_.AddImplication(x_lit, y_lit);
			runtime_assert(!conflict_detected);
		}		
	}
	return false;
}

inline const SolverState::LiteralInformation& SolverState::GetLiteralInformation(BooleanLiteral literal) const
{
	return literal_information_[literal.ToPositiveInteger()];
}

inline bool SolverState::SetUpperBoundForVariable(IntegerVariable variable, int new_upper_bound)
{
	BooleanLiteral ub_literal = GetUpperBoundLiteral(variable, new_upper_bound);
	return propagator_clausal_.AddUnitClause(ub_literal);
}

inline void SolverState::NotifyPropagatorsSubscribedtoInequalityChanges(IntegerVariable variable)
{
	for (PropagatorGenericCP* propagator : watch_list_CP_.watchers[variable.id].neq_watcher)
	{
		bool should_enqueue = propagator->NotifyDomainChange(variable);
		if (should_enqueue) { propagator_queue_.EnqueuePropagator(propagator); }
	}
}

inline void SolverState::NotifyPropagatorsSubscribedtoLowerBoundChanges(IntegerVariable variable)
{
	for (PropagatorGenericCP* propagator : watch_list_CP_.watchers[variable.id].lb_watcher)
	{
		bool should_enqueue = propagator->NotifyDomainChange(variable);
		if (should_enqueue) { propagator_queue_.EnqueuePropagator(propagator); }
	}
}

inline void SolverState::NotifyPropagatorsSubscribedtoUpperBoundChanges(IntegerVariable variable)
{
	for (PropagatorGenericCP* propagator : watch_list_CP_.watchers[variable.id].ub_watcher)
	{
		bool should_enqueue = propagator->NotifyDomainChange(variable);
		if (should_enqueue) { propagator_queue_.EnqueuePropagator(propagator); }
	}
}

inline bool SolverState::ComputeFlagFromScratch(BooleanLiteral literal) const
{
	IntegerVariable variable = GetLiteralInformation(literal).integer_variable;
	return watch_list_CP_.IsVariableWatched(variable);
}

inline bool SolverState::IsAssigned(IntegerVariable variable)
{
	return domain_manager_.GetLowerBound(variable) == domain_manager_.GetUpperBound(variable);
}

inline void SolverState::MakeAssignment(BooleanLiteral literal, uint64_t code)
{
	pumpkin_assert_moderate(!literal.IsUndefined() && !assignments_.IsAssigned(literal), "Sanity check.");
	pumpkin_assert_moderate(ComputeFlagFromScratch(literal) == literal.GetFlag(), "The literal flag is not consistent with the expected value, indicates a problem.");

	assignments_.MakeAssignment(literal, GetCurrentDecisionLevel(), code);
	trail_.push(literal);
}

inline BooleanLiteral SolverState::GetLastDecisionLiteralOnTrail() const
{
	return GetDecisionLiteralForLevel(GetCurrentDecisionLevel());
}

inline BooleanLiteral SolverState::GetDecisionLiteralForLevel(int decision_level) const
{
	runtime_assert(decision_level <= GetCurrentDecisionLevel());

	if (decision_level == 0) { return BooleanLiteral(); } //return undefined literal when there are no decisions on the trail

	BooleanLiteral decision_literal = trail_[trail_delimiter_[decision_level - 1]];

	assert(assignments_.GetAssignmentReasonCode(decision_literal.Variable()) == 0);
	assert(assignments_.GetAssignmentLevel(decision_literal.Variable()) == decision_level);
	return decision_literal;
}

inline bool SolverState::IsRootLevel() const
{
	return GetCurrentDecisionLevel() == 0;
}

inline int SolverState::GetCurrentDecisionLevel() const
{
	return decision_level_;
}

inline size_t SolverState::GetNumberOfAssignedInternalBooleanVariables() const
{
	return int(trail_.size());
}

inline size_t SolverState::GetNumberOfInternalBooleanVariables() const
{
	return int(assignments_.GetNumberOfInternalBooleanVariables());
}

inline int SolverState::NumIntegerVariables() const
{
	return integer_variable_to_literal_info_.size() - 1; //recall that the zero index does not count since it is used for null
}

inline BooleanLiteral SolverState::GetLiteralFromTrailAtPosition(size_t index) const
{
	return trail_[index];
}

inline BooleanLiteral SolverState::GetLiteralFromTheBackOfTheTrail(size_t index) const
{
	return trail_[trail_.size() - index - 1];
}

inline bool SolverState::IsAssignmentBuilt()
{
	return variable_selector_.PeekNextVariable(this) == BooleanVariableInternal(); //no variables are left unassigned, the assignment is built
}

inline int SolverState::AddPropagator(PropagatorGeneric* propagator)
{
	additional_propagators_.push_back(propagator);
	return additional_propagators_.size() - 1;
}

inline PropagatorGeneric* SolverState::GetPropagator(int propagator_id)
{
	return additional_propagators_[propagator_id];
}

inline bool SolverState::HasPropagator(PropagatorGeneric* propagator)
{
	bool found = &propagator_clausal_ == propagator;
	for (PropagatorGeneric* stored_propagator : additional_propagators_) { found |= (stored_propagator == propagator); }
	return found;
}

inline void SolverState::AddPropagatorCP(PropagatorGenericCP* propagator)
{
	pumpkin_assert_simple(next_cp_propagator_id_ > 0, "Indexing of propagators exceeded, which is probably an indicator that something else went wrong as this is highly unlikely to happen.");
	pumpkin_assert_simple(GetCurrentDecisionLevel() == 0, "Adding propagators is only possible at the root node.");
	
	uint32_t new_id = next_cp_propagator_id_;
	next_cp_propagator_id_--;

	cp_propagators_.push_back(propagator);
	propagator_clausal_.clause_allocator_->SetLimit(new_id-1);
	propagator_clausal_.helper_clause_allocator_->SetLimit(new_id-1);

	PropagationStatus propagation_status = propagator->InitialiseAtRoot(new_id, this);
	pumpkin_assert_simple(!propagation_status.conflict_detected, "For now we crash when adding a new propagator that detects a conflict at the root node, even though this is not necessarily an error. Should handle better in the future.");

	propagation_status = PropagateEnqueuedLiterals();
	pumpkin_assert_simple(!propagation_status.conflict_detected, "Root conflict detected after adding propagator, for now we crash the program but this may not necessarily be an error.");
}

inline void SolverState::RemovePropagatorCP(PropagatorGenericCP* propagator)
{
	pumpkin_assert_simple(propagator != NULL && cp_propagators_.size() > 0, "Sanity check.");
	pumpkin_assert_simple(GetCurrentDecisionLevel() == 0, "Removing propagators is only possible at the root node."); //in case we remove during search for some reason, there may be propagated literals that need the propagator for explanations

	//the propagator id counter is not restored in any way when a propagator is removed, todo think about this
	//	for this reason we keep the propagator in the cp_propagators vector, which is a bit hacky but works for now
	//	for now we at least replace the propagator by a null pointer in the vector, and deal with the problem later

	//replace the propagator pointer by null in the list
	auto iter = find(cp_propagators_.begin(), cp_propagators_.end(), propagator);
	pumpkin_assert_permanent(iter != cp_propagators_.end(), "Cannot be that the propagator that needs to be removed is not in the solver.");
	*iter = NULL;

	//define a helper method
	auto remove_propagator_if_present = [](std::vector<PropagatorGenericCP*>& list, PropagatorGenericCP* propagator)
	{
		auto iter = std::find(list.begin(), list.end(), propagator);
		if (iter != list.end()) { list.erase(iter); }
	};

	//not efficient but okay for now
	for (auto& cp_watchers : watch_list_CP_.watchers)
	{
		remove_propagator_if_present(cp_watchers.lb_watcher, propagator);
		remove_propagator_if_present(cp_watchers.ub_watcher, propagator);
		remove_propagator_if_present(cp_watchers.neq_watcher, propagator);
	}
}

inline PropagatorGenericCP* SolverState::GetPropagatorCP(uint32_t id)
{
	return cp_propagators_.at(UINT32_MAX - id);	
}

inline void SolverState::UpdateMovingAveragesForRestarts(int learned_clause_lbd)
{
	simple_moving_average_lbd.AddTerm(learned_clause_lbd);
	cumulative_moving_average_lbd.AddTerm(learned_clause_lbd);
	simple_moving_average_block.AddTerm(GetNumberOfAssignedInternalBooleanVariables());
}

inline bool SolverState::IsPropagationComplete()
{
	bool propagation_complete = propagator_clausal_.IsPropagationComplete();
	for (PropagatorGeneric* propagator : additional_propagators_) { propagation_complete &= propagator->IsPropagationComplete(); }
	return propagation_complete && propagator_queue_.IsEmpty();
}

} //end Pumpkin namespace