#pragma once

#include "simple_bound_tracker.h"
#include "../Propagators/Clausal/clause.h"
#include "../Utilities/integer_variable.h"
#include "../Utilities/propagation_status.h"
#include "../Utilities/pumpkin_assert.h"

#include <vector>

//This is the main propagator abstract class
//	new propagator classes should inherent this class and implement only the abstract methods explained below

//A few notes on implementing new propagators:

//	some of the methods are needed for functionality reasons as expected, but some are required only to help with debugging
//	to enable debug mode, change the PUMPKIN_ASSERT_LEVEL_DEFINITION in "Utilities/pumpkin_assert.h" to 3 ('advanced') or 4 ('extreme')
//	after implementing a new propagator, use the debugging functionality to detect common issues with incorrect propagators

//	propagators are expected to work on a 'snapshot' of the solver state, i.e.,
//		at the start of propagation the variables have certain domains -> this is the snapshot
//		all propagation should be done considering those domains
//		in case a propagation happens and for example a lower bound of a variable is increased
//			the effect of increasing the lower bound is only applied afterwards outside the propagation method
//				 (propagating the bound will not immediately update the domain of the variable)
//			so further propagation within the same method call should still refer to the snapshot when considering bounds
//	fixed-point propagation is still achieved by calling the propagator possibly multiple times
//	for example, note that within the same decision level, the following is a possible scenario:
//		-the propagator is called
//		-then the variable bounds get updated
//		-then the same propagator is called again with the new state, while still being in the same decision level

//	each propagator should track its own 'snapshot' of the state
//	the snapshot are commonly updated within the Synchronise and NotifyDomainChange methods
//  to help with this, consider using the class 'IntegerVariableBoundTracker' in 'integer_variable_bound_tracker.h'
//		see the cumulative_propagator.h for the usage of the bound tracker

//	when generating explanations, they need to be returned in the form of a pointer to a Clause (Clause*)
//	however clauses cannot be straight-forwardly created as other normal classes
//	instead they are allocated using special classes
//		in this case use 'StandardClauseAllocator' from 'standard_clause_allocator.h'
//	important: allocated clauses need to be deallocated!
//		the recommended way is to call 'clear' on the clause allocator when Synchronise() is called at the root level
//		this is a good way since at the root none of the clauses are needed anymore

//	one way to implement explanations is to do so 'eagerly', i.e.,
//		whenever a literal is propagated, its explanation is recorded in map literal->explanation, and then later if an explanation is needed it can be retrieved from the map
//	another way is to do so 'lazily', construct the explanation only when needed
//		lazy is better but it takes more effort (ironically!) to get it done correctly
//		and depending on the complexity of various things, it may not pay off
//	probably best to go for the eager version, and later consider lazily

//	to construct explanations, you would need access to the Boolean literals that represent [x >= a], [x <= a], or [x == a]
//		to get these, use the the following methods:
//			state_->GetCurrentLowerBoundLiteral(variable)
//			state_->GetCurrentUpperBoundLiteral(variable)
// 
//			or if you need a specific value:
// 
//			state_->GetLowerBoundLiteral(IntegerVariable variable, int right_hand_side)
//			state_->GetUpperBoundLiteral(IntegerVariable variable, int right_hand_side)

//	to propagate a lower/upper bound, get the corresponding literal, and place it in the propagation queue, i.e.,
//		BooleanLiteral lit = state_->GetLowerBoundLiteral(term.variable, new_term_lb);
//		state_->EnqueuePropagatedLiteral(lit, GetPropagatorID());

//	as an example of a non-trivial propagator, see the linear_integer_inequality_propagator.h
//		note that it is not implemented efficiently but it has covers all the methods
//	cumulative_propagator is a better example of a more efficient propagator implementation
//		although it is still a bit messy

//	to use the propagator within a solver, the propagator needs to be added to the solver
//	use state_.AddPropagatorCP(PropagatorGenericCP*) method to add propagators
//	important: 
//		-make sure the pointer provided to the solver is not a pointer to a local variable that may be get destroyed
//		-most likely allocating with the 'new' operator is the way to go, i.e., LinearIntegerInequalityPropagator *p = new LinearIntegerInequalityPropagator(....)

namespace Pumpkin
{
class SolverState;

class PropagatorGenericCP
{
public:
	PropagatorGenericCP(int priority);

	//Propagate method that will be called during search
	//	responsible for propagating variables
	//	reports whether a conflict has been detected through the PropagationStatus return value
	//	most straight-forward inefficient implementation would reply on PropagateFromScratch, see below
	virtual PropagationStatus Propagate() = 0;

	//Another propagation method that is used to help in debugging
	//	it is expected that this method propagates without relying on internal (advanced) data structures
	//	in most cases this would be the simplest but correct implementation of the propagator
	//	the class has mechanisms set in to help debugging and this is one of the methods used
	virtual PropagationStatus PropagateFromScratch() = 0;

	//Called each time the solver backtracks
	//	clears the failure clause and calls 'SynchroniseInternal'
	void Synchronise();

	//Notifies ('awakes') the propagator that a domain change occured with respect to the variable
	//	expected to update internal data structures that help with propagation
	//	returns true if the propagator should be enqueued for propagation
	//	note: the propagator registers which variables are relevant for it using 'SubscribeDomainChanges' (see below)
	virtual bool NotifyDomainChange(IntegerVariable) = 0;

	//Returns the clause that explains unsatisfiability of the propagator
	//	note: the failure clause is set using 'InitialiseFailureClause'
	Clause* ExplainFailure();

	//Returns the explanation clause for the propagation of the input literal.
	//	assumes the input literal is not undefined
	//	its functionality relies on 'ExplainLiteralPropagationInternal'
	//	when debugging, additional checks are done to see if the explanation is correct
	Clause* ExplainLiteralPropagation(BooleanLiteral);

	//Initialises the propagator and does root propagation.
	//	called only once
	//	reports whether a root conflict has been detected through the PropagationStatus return value
	//	relies on 'InitialiseAtRootInternal', see below
	PropagationStatus InitialiseAtRoot(uint32_t id, SolverState* state);

	//Returns the assigned propagator id
	//	the id is used for internal purposes of the solver
	//	note that the id is assigned by the solver using the InitialiseAtRoot method
	uint32_t GetPropagatorID() const;

	//Returns the priority of the propagator represented as a integer
	//	lower values mean more priority
	//	the priority is set in the constructor
	//	the priority determines the order in which propagators will be asked to propagate
	//		i.e., after the clausal propagator, propagators with lower priority are called before those with higher priority 
	int GetPriority() const;

protected:
	//simple method that is called each time a conflict is detected
	//	the propagator does not have direct access to the internal failure clause so this must be used
	//	this is mainly done to help with debugging
	//		currently the functionality is limited, will be expanded in the future
	void InitialiseFailureClause(Clause* failure_clause);

	//Called each time the solver backtracks
	//	expected to update internal data structures after backtracking with respect to the new solver state
	virtual void SynchroniseInternal() = 0;

	//Returns the explanation clause for the propagation of the input literal
	//	assumes the input literal is not undefined
	//	the propagated literal _must_ be placed in the [0]th position of the clause	
	virtual Clause* ExplainLiteralPropagationInternal(BooleanLiteral literal) = 0;

	//Called when the propagator is first added to the solver to initialise data structures and propagate at the root node
	//	reports whether a root conflict has been detected through the PropagationStatus return value
	//	called only once when the propagator is added to the solver through the 'InitialiseAtRoot' method
	virtual PropagationStatus InitialiseAtRootInternal() = 0;

	//Called during initialisation at the root
	//	registers which events are relevant for the propagator
	//		possible events:
	//			-the lower bound of a relevant variable changed (state_->watch_list_CP_.SubscribeToLowerBoundChanges)
	//			-the upper bound of a relevant variable changed (state_->watch_list_CP_.SubscribeToUpperBoundChanges)
	//			-a value was removed from the domain of a relevant variable (state_->watch_list_CP_.SubscribeToInequalityChanges)
	//	depending on the propagator, only some of these may be relevant for a subset of the variables
	//	IntegerVariableBoundTracker
	virtual void SubscribeDomainChanges() = 0;

	//Returns whether the input variables and corresponding bounds are enough to determine infeasibility of the propagator
	//	expected to be implemented in a simple way such that the check is performed using only the input and possibly local variables
	//		although is some cases it may still be necessary to read some other values, but the idea is to keep this as simple as possible to avoid errors
	//	the method is used to help debugging
	virtual bool DebugCheckInfeasibility(const std::vector<IntegerVariable>& relevant_variables, const SimpleBoundTracker& bounds) const = 0;

	SolverState* state_;

	//debug methods, called in debugging to detect common issues with incorrect explanations
	//	not relevant for inherenting this class, todo explain
	bool DebugCheckPropagationExplanation(const BooleanLiteral* explanation_clause, int num_literals);
	bool DebugCheckFailureExplanation(const BooleanLiteral* failure_clause, int num_literals);

private:
	//more debug methods
	//	not relevant for inherenting this class, todo explain
	void InitialiseVariablesInvolvedInExplanation(const BooleanLiteral* explanation_clause, int num_literals);
	void InitialiseSimpleBoundTracker(const BooleanLiteral* explanation_clause, int num_literals, bool negate_literals);

	Clause* failure_clause_;
	uint32_t propagator_id_;
	int priority_;
	static SimpleBoundTracker helper_bound_tracker_;
	static std::vector<IntegerVariable> helper_variables_involved_;
};

inline PropagatorGenericCP::PropagatorGenericCP(int priority)
	:state_(0), propagator_id_(0), failure_clause_(0), priority_(priority)
{
}

inline void PropagatorGenericCP::Synchronise()
{
	failure_clause_ = 0;
	SynchroniseInternal();
}

inline Clause* PropagatorGenericCP::ExplainFailure()
{
	pumpkin_assert_advanced(failure_clause_ != 0 && DebugCheckFailureExplanation(failure_clause_->begin(), failure_clause_->Size()), "Sanity check.");
	return failure_clause_;
}

inline Clause* PropagatorGenericCP::ExplainLiteralPropagation(BooleanLiteral literal)
{
	Clause* reason_clause = ExplainLiteralPropagationInternal(literal);
	pumpkin_assert_advanced(DebugCheckPropagationExplanation(reason_clause->begin(), reason_clause->Size()), "Sanity check.");
	return reason_clause;
}

inline PropagationStatus PropagatorGenericCP::InitialiseAtRoot(uint32_t id, SolverState* state)
{
	pumpkin_assert_simple(propagator_id_ == 0 && state_ == 0, "Sanity check.");
	state_ = state;
	propagator_id_ = id;
	SubscribeDomainChanges();
	return InitialiseAtRootInternal();
}

inline uint32_t PropagatorGenericCP::GetPropagatorID() const
{
	pumpkin_assert_moderate(propagator_id_ != 0, "Sanity check.");
	return propagator_id_;
}

inline int PropagatorGenericCP::GetPriority() const
{
	return priority_;
}

inline void PropagatorGenericCP::InitialiseFailureClause(Clause* failure_clause)
{
	pumpkin_assert_moderate(failure_clause != 0 && failure_clause_ == 0, "Sanity check.");
	failure_clause_ = failure_clause;
}

} //end Pumpkin namespace