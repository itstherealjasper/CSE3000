#pragma once

#include "../integer_variable_bound_tracker.h"
#include "../propagator_generic_CP.h"
#include "../../Engine/solver_state.h"
#include "../../Utilities/standard_clause_allocator.h"
#include "../../Utilities/integer_assignment_vector.h"

#include <map>
#include <set>

namespace Pumpkin
{
//propagator of the cumulative constraint...todo
class CumulativePropagator : public PropagatorGenericCP
{
public:

	enum class TaskOrderingStrategy { IN_ORDER, ASCENDING_DURATION, DESCENDING_DURATION, ASCENDING_CONSUMPTION, DESCENDING_CONSUMPTION, DESCENDING_CONSUMPTION_SPECIAL };
	enum class IncrementalStrategy { ON, OFF };
	enum class LiftingStrategy { ON, OFF };
	
	CumulativePropagator
	(
		std::vector<IntegerVariable>& start_times,
		std::vector<int>& resource_consumptions,
		std::vector<int>& durations,
		int max_capacity,
		int max_time,
		TaskOrderingStrategy ordering_strategy,
		IncrementalStrategy incrementality,
		LiftingStrategy lifting_strategy
	);

	PropagationStatus Propagate();
	PropagationStatus PropagateSpecial();
	PropagationStatus PropagateFromScratch();
	void SynchroniseInternal();
	bool NotifyDomainChange(IntegerVariable);

	void DebugSetOptimalSolution(std::vector<IntegerVariable> &vars, std::vector<int>& sol);

private:
	Clause* ExplainLiteralPropagationInternal(BooleanLiteral);

	/*bool DebugCheckImpliedClause(vec<BooleanLiteral>& literals);
	IntegerVariableDomainManager GenerateBoundsInfeasible(vec<BooleanLiteral> &literals);
	bool DebugCheckInfeasibility(IntegerVariableDomainManager bounds) const;*/
	bool DebugCheckInfeasibility(const std::vector<IntegerVariable>& relevant_variables, const SimpleBoundTracker& bounds) const;

	void SubscribeDomainChanges();
	PropagationStatus InitialiseAtRootInternal();

	bool DebugCheckOptimalSolution(vec<BooleanLiteral> &explanation);
	bool DebugCompulsoryAndTaskActiveAreCorrect() const;
	void RecomputeCompulsoryAndIsTaskActiveFromScratch();

	struct Task { IntegerVariable start_time; int resource_consumption, duration; };
	void AddTaskExplanationForTime(vec<BooleanLiteral>& clause, const Task& task, int time) const;

	TaskOrderingStrategy task_ordering_strategy_;
	IncrementalStrategy incremental_strategy_;
	LiftingStrategy lifting_strategy_;

	std::vector<Task> tasks_;
	int max_capacity_, max_time_;
	IntegerAssignmentVector optimal_solution_;

	//variables used for incremental computation
	std::vector<int> compulsory_part_;
	std::vector<std::vector<bool> > is_task_active_;
	std::set<int> updated_times_;
	bool failure_detected_;
	
	StandardClauseAllocator clause_allocator_;
	std::map<int, vec<BooleanLiteral> > map_literal_to_eager_explanation_; //lit.ToInt() -> explanation
	std::vector<int> integer_variable_to_task_id_;
	IntegerVariableBoundTracker bound_tracker_;
};
}