#include "cumulative_propagator.h"
#include "../../Utilities/pumpkin_assert.h"

#include <algorithm>

namespace Pumpkin
{
CumulativePropagator::CumulativePropagator(
	std::vector<IntegerVariable>& start_times,
	std::vector<int>& resource_consumptions,
	std::vector<int>& durations,
	int max_capacity,
	int max_time,
	TaskOrderingStrategy ordering_strategy,
	IncrementalStrategy incremental_strategy,
	LiftingStrategy lifting_strategy
):
	max_capacity_(max_capacity),
	max_time_(max_time),
	PropagatorGenericCP(1),
	failure_detected_(false),
	incremental_strategy_(incremental_strategy),
	task_ordering_strategy_(ordering_strategy),
	lifting_strategy_(lifting_strategy)
{
	pumpkin_assert_simple(start_times.size() == durations.size(), "Sanity check.");

	for (int i = 0; i < start_times.size(); i++)
	{
		pumpkin_assert_simple(durations[0] > 0 && resource_consumptions[i] <= max_capacity_, "Sanity check.");
	
		if (resource_consumptions[i] == 0) { continue; }

		Task task;
		task.start_time = start_times[i];
		task.resource_consumption = resource_consumptions[i];
		task.duration = durations[i];
		tasks_.push_back(task);
	}

	compulsory_part_ = std::vector<int>(max_time_, 0);
	is_task_active_ = std::vector<std::vector<bool> >(tasks_.size(), std::vector<bool>(max_time_, false));

	if (ordering_strategy == TaskOrderingStrategy::ASCENDING_DURATION)
	{
		std::sort(tasks_.begin(), tasks_.end(), [](const Task& t1, const Task& t2)->bool { return t1.duration < t2.duration; });
	}
	else if (ordering_strategy == TaskOrderingStrategy::DESCENDING_DURATION)
	{
		std::sort(tasks_.begin(), tasks_.end(), [](const Task& t1, const Task& t2)->bool { return t1.duration > t2.duration; });
	}
	else if (ordering_strategy == TaskOrderingStrategy::ASCENDING_CONSUMPTION)
	{
		std::sort(tasks_.begin(), tasks_.end(), [](const Task& t1, const Task& t2)->bool { return t1.resource_consumption < t2.resource_consumption; });
	}
	else if (ordering_strategy == TaskOrderingStrategy::DESCENDING_CONSUMPTION || ordering_strategy == TaskOrderingStrategy::DESCENDING_CONSUMPTION_SPECIAL)
	{
		std::sort(tasks_.begin(), tasks_.end(), [](const Task& t1, const Task& t2)->bool { return t1.resource_consumption > t2.resource_consumption; });
	}
	else if (ordering_strategy == TaskOrderingStrategy::IN_ORDER)
	{
		//do nothing
	}
	else
	{
		std::cout << "Unsupported strategy in cumulative?\n";
		abort();
	}

	for (int i = 0; i < tasks_.size(); i++)
	{
		int var_id = tasks_[i].start_time.id;
		if (integer_variable_to_task_id_.size() <= var_id)
		{
			integer_variable_to_task_id_.resize(var_id+1, -1);
		}
		integer_variable_to_task_id_[var_id] = i;
	}
}

PropagationStatus CumulativePropagator::Propagate()
{
	if (incremental_strategy_ == IncrementalStrategy::OFF) { return PropagateFromScratch(); }

	if (task_ordering_strategy_ == TaskOrderingStrategy::DESCENDING_CONSUMPTION_SPECIAL) { return PropagateSpecial(); }
		
	if (failure_detected_)
	{
		//extract all intervals where the the capacity of overused
		struct Interval { int start_time; int end_time; };
		std::vector<Interval> overused_intervals;
		int t = 0;
		while (t < max_time_)
		{
			if (compulsory_part_[t] > max_capacity_)
			{
				Interval failure_interval;
				failure_interval.start_time = t;
				do { t++; } while (t < max_time_ && compulsory_part_[t] > max_capacity_);
				failure_interval.end_time = t;
				overused_intervals.push_back(failure_interval);
			}
			t++;
		}
		pumpkin_assert_simple(!overused_intervals.empty(), "Sanity check.");
		//failure - there is at least one interval where the resource consumption goes beyond the maximum capacity
		if (!overused_intervals.empty())
		{
			//for now we select the middle point of the first interval; could play around with this later
			//generate explanation; need to get tasks related to this failure; also need to choose the explanation type...
			//could possibily remove some tasks...lift explanations...
			//for now we do pointwise explanations
			//set the failure clause
			int failure_time = overused_intervals[0].start_time + (overused_intervals[0].end_time - overused_intervals[0].start_time) / 2;
			vec<BooleanLiteral> failure_literals;
			int total_resource_consumption = 0; //we collect only a subset of the failure tasks in order of id, possibly there are better ways here
			for (int task_id = 0; task_id < tasks_.size() && total_resource_consumption <= max_capacity_; task_id++)
			{
				if (is_task_active_[task_id][failure_time])
				{
					Task& task = tasks_[task_id];

					total_resource_consumption += task.resource_consumption;

					BooleanLiteral lb_lit = state_->GetCurrentLowerBoundLiteral(task.start_time);
					failure_literals.push(~lb_lit);

					BooleanLiteral ub_lit = state_->GetCurrentUpperBoundLiteral(task.start_time);
					failure_literals.push(~ub_lit);
				}
			}
			Clause* failure_clause = clause_allocator_.CreateClause(failure_literals);
			InitialiseFailureClause(failure_clause);
			return true; //report conflict detected
		}
	}

	pumpkin_assert_simple(!updated_times_.empty(), "Sanity check."); //in the current version there must be a change

	//only look at the times that changed
	//no failure, proceed with propagation
	//	remove infeasible times from starting times of tasks
	for (int t: updated_times_)
	{
		int num_tasks_for_which_we_generate_explanations = 0;
		for (int task_id = 0; task_id < tasks_.size(); task_id++)
		{
			Task& task = tasks_[task_id];

			//if the tasks are sorted by duration, then there is no point in further continuing this loop since all the other tasks may have at most this duration
			if (task_ordering_strategy_ == TaskOrderingStrategy::DESCENDING_CONSUMPTION && compulsory_part_[t] + task.resource_consumption <= max_capacity_)
			{
				break;				
			}

			//if the explanation_task cannot be scheduled at this time
			if (!is_task_active_[task_id][t] && (compulsory_part_[t] + task.resource_consumption > max_capacity_))
			{
				//skip if surely no propagation will be done
				if (state_->domain_manager_.GetLowerBound(task.start_time) > t) { continue; }
				
				num_tasks_for_which_we_generate_explanations++;
				
				static vec<BooleanLiteral> explanation_literals;
				explanation_literals.clear();

				int total_resource_consumption = 0; //we collect only a subset of the tasks in order of id, possibly there are better ways here
				for (int explanation_task_id = 0; explanation_task_id < tasks_.size() && total_resource_consumption + task.resource_consumption <= max_capacity_; explanation_task_id++)
				{
					if (is_task_active_[explanation_task_id][t])
					{
						pumpkin_assert_moderate(task_id != explanation_task_id, "Sanity check.");
						Task& explanation_task = tasks_[explanation_task_id];						
						total_resource_consumption += explanation_task.resource_consumption;

						BooleanLiteral lb_lit = state_->GetCurrentLowerBoundLiteral(explanation_task.start_time);
						explanation_literals.push(~lb_lit);
		
						BooleanLiteral ub_lit = state_->GetCurrentUpperBoundLiteral(explanation_task.start_time);
						explanation_literals.push(~ub_lit);			
					}
				}
				//t is a forbidden time, meaning that any start time that would require the explanation_task to be active at t is also forbidden
				int forbidden_interval_start = std::max(t - task.duration + 1, 0);				
				//for (int forbidden_t = forbidden_interval_start; forbidden_t < forbidden_interval_start + task.duration; forbidden_t++)
				for (int forbidden_t = forbidden_interval_start; forbidden_t <= t; forbidden_t++)
				{
					BooleanLiteral propagated_literal = ~(state_->GetEqualityLiteral(task.start_time, forbidden_t));
					pumpkin_assert_simple(!state_->assignments_.IsAssignedFalse(propagated_literal), "Sanity check.");//I think if it was assigned false, a conflict would have been detected, and if it was true, the lower_bound would not be propagated now

					//only provide explanations if the value is in the domain
					if (!state_->assignments_.IsAssigned(propagated_literal))
					//if (state_.domain_manager_.IsInDomain(task.start_time, forbidden_t))
					{
						//set the explanation eagerly
						//we respect the convention that the propagated literal is at the zeroth position
						static vec<BooleanLiteral> explanation_propagation;
						explanation_propagation.resize(explanation_literals.size() + 1);

						explanation_propagation[0] = propagated_literal;
						for (int i = 1; i < explanation_propagation.size(); i++) { explanation_propagation[i] = explanation_literals[i - 1]; }

						state_->EnqueuePropagatedLiteral(propagated_literal, GetPropagatorID());

						pumpkin_assert_simple(explanation_propagation.size() > 1 || state_->GetCurrentDecisionLevel() == 0, "Sanity check.");
						map_literal_to_eager_explanation_[propagated_literal.ToPositiveInteger()] = explanation_propagation;
					}					
				}				
			}
		}
	}
	updated_times_.clear();
	return false; //no conflict
}

PropagationStatus CumulativePropagator::PropagateSpecial()
{
	if (failure_detected_)
	{
		//extract all intervals where the the capacity of overused
		struct Interval { int start_time; int end_time; };
		std::vector<Interval> overused_intervals;
		int t = 0;
		while (t < max_time_)
		{
			if (compulsory_part_[t] > max_capacity_)
			{
				Interval failure_interval;
				failure_interval.start_time = t;
				do { t++; } while (t < max_time_ && compulsory_part_[t] > max_capacity_);
				failure_interval.end_time = t;
				overused_intervals.push_back(failure_interval);
			}
			t++;
		}
		pumpkin_assert_simple(!overused_intervals.empty(), "Sanity check.");
		//failure - there is at least one interval where the resource consumption goes beyond the maximum capacity
		if (!overused_intervals.empty())
		{
			//for now we select the middle point of the first interval; could play around with this later
			//generate explanation; need to get tasks related to this failure; also need to choose the explanation type...
			//could possibily remove some tasks...lift explanations...
			//for now we do pointwise explanations
			//set the failure clause
			int failure_time = overused_intervals[0].start_time + (overused_intervals[0].end_time - overused_intervals[0].start_time) / 2;
			vec<BooleanLiteral> failure_literals;
			int total_resource_consumption = 0; //we collect only a subset of the failure tasks in order of id, possibly there are better ways here
			for (int task_id = 0; task_id < tasks_.size() && total_resource_consumption <= max_capacity_; task_id++)
			{
				if (is_task_active_[task_id][failure_time])
				{
					Task& task = tasks_[task_id];
					total_resource_consumption += task.resource_consumption;
					AddTaskExplanationForTime(failure_literals, task, failure_time);
				}
			}
			Clause *failure_clause = clause_allocator_.CreateClause(failure_literals);
			InitialiseFailureClause(failure_clause);
			return true; //report conflict detected
		}
	}

	pumpkin_assert_simple(!updated_times_.empty(), "Sanity check."); //in the current version there must be a change

	//only look at the times that changed
	//no failure, proceed with propagation
	//	remove infeasible times from starting times of tasks
	for (int t : updated_times_)
	{
		static std::vector<int> violating_tasks;
		violating_tasks.clear();

		//collect all tasks that cannot be scheduled at the given time, that have the time current in their domain
		//recall that in this version of the algorithm, the tasks are sorted according to their resource consumption in descending order
		for (int task_id = 0; task_id < tasks_.size(); task_id++)
		{
			Task& task = tasks_[task_id];

			if (compulsory_part_[t] + task.resource_consumption <= max_capacity_) { break; }

			//if the explanation_task cannot be scheduled at this time
			if (!is_task_active_[task_id][t] && (compulsory_part_[t] + task.resource_consumption > max_capacity_))				
			{
				//skip if surely no propagation will be done
				if (state_->domain_manager_.GetLowerBound(task.start_time) > t) { continue; }

				violating_tasks.push_back(task_id);
			}
		}
		
		//note that the tasks in violating tasks are sorted in descending order as well

		static vec<BooleanLiteral> explanation_literals;
		explanation_literals.clear();
		explanation_literals.push(BooleanLiteral()); //dummy for the propagated literal
		int consumption_by_explanation_tasks = 0;
		int next_explanation_task_candidate_index = 0;

		for (int violating_task_id : violating_tasks)
		{
			Task& violating_task = tasks_[violating_task_id];
			//need to add more tasks to the explanation
			while (consumption_by_explanation_tasks + violating_task.resource_consumption <= max_capacity_)
			{
				//find next active task
				while (!is_task_active_[next_explanation_task_candidate_index][t])
				{
					pumpkin_assert_moderate(next_explanation_task_candidate_index < tasks_.size(), "Sanity check.");
					next_explanation_task_candidate_index++;
				}
				pumpkin_assert_simple(next_explanation_task_candidate_index < tasks_.size(), "Sanity check.");

				Task& explanation_task = tasks_[next_explanation_task_candidate_index];
				next_explanation_task_candidate_index++;

				consumption_by_explanation_tasks += explanation_task.resource_consumption;

				AddTaskExplanationForTime(explanation_literals, explanation_task, t);
			}

			//generate explanation for the task....

			//t is a forbidden time, meaning that any start time that would require the explanation_task to be active at t is also forbidden
			int forbidden_interval_start = std::max(t - violating_task.duration + 1, 0);
			//for (int forbidden_t = forbidden_interval_start; forbidden_t < forbidden_interval_start + task.duration; forbidden_t++)
			for (int forbidden_t = forbidden_interval_start; forbidden_t <= t; forbidden_t++)
			{
				BooleanLiteral propagated_literal = ~(state_->GetEqualityLiteral(violating_task.start_time, forbidden_t));
				pumpkin_assert_simple(!state_->assignments_.IsAssignedFalse(propagated_literal), "Sanity check.");//I think if it was assigned false, a conflict would have been detected, and if it was true, the lower_bound would not be propagated now

				//only provide explanations if the value is in the domain
				if (!state_->assignments_.IsAssigned(propagated_literal))
					//if (state_.domain_manager_.IsInDomain(task.start_time, forbidden_t))
				{
					//set the explanation eagerly
					//we respect the convention that the propagated literal is at the zeroth position
					explanation_literals[0] = propagated_literal;					

					state_->EnqueuePropagatedLiteral(propagated_literal, GetPropagatorID());

					pumpkin_assert_simple(explanation_literals.size() > 1 || state_->GetCurrentDecisionLevel() == 0, "Sanity check.");
					map_literal_to_eager_explanation_[propagated_literal.ToPositiveInteger()] = explanation_literals;
				}
			}
		}
	}
	updated_times_.clear();
	return false; //no conflict
}

PropagationStatus CumulativePropagator::PropagateFromScratch()
{
	for (auto p : bound_tracker_.DebugGetCurrentBounds())
	{
		int lb = state_->domain_manager_.GetLowerBound(p.first);
		int ub = state_->domain_manager_.GetUpperBound(p.first);
		pumpkin_assert_simple(lb == p.second.lower_bound, "Sanity check.");
		pumpkin_assert_simple(ub == p.second.upper_bound, "Sanity check.");
		int m = 0;
	}

	std::vector<int> compulsory_part(max_time_, 0);
	std::vector<std::vector<bool> > is_task_active(tasks_.size(), std::vector<bool>(max_time_, false));

	//compute the current resource consumption
	for (int task_id = 0; task_id < tasks_.size(); task_id++)
	{
		Task& task = tasks_[task_id];
		int lower_bound = state_->domain_manager_.GetLowerBound(task.start_time);
		int upper_bound = state_->domain_manager_.GetUpperBound(task.start_time);
		int length_of_activity = std::max(lower_bound + task.duration - upper_bound, 0);
		for (int t = lower_bound + task.duration - length_of_activity; t < lower_bound + task.duration; t++)
		{
			compulsory_part[t] += task.resource_consumption;
			is_task_active[task_id][t] = true;
		}
	}
	//extract all intervals where the the capacity of overused
	struct Interval { int start_time; int end_time; };
	std::vector<Interval> overused_intervals;
	int t = 0;
	while (t < max_time_)
	{
		if (compulsory_part[t] > max_capacity_)
		{
			Interval failure_interval;
			failure_interval.start_time = t;
			do { t++; } while (t < max_time_ && compulsory_part[t] > max_capacity_);
			failure_interval.end_time = t;
			overused_intervals.push_back(failure_interval);
		}
		t++;
	}
	//failure - there is at least one interval where the resource consumption goes beyond the maximum capacity
	if (!overused_intervals.empty())
	{
		//for now we select the middle point of the first interval; could play around with this later
		//generate explanation; need to get tasks related to this failure; also need to choose the explanation type...
		//could possibily remove some tasks...lift explanations...
		//for now we do pointwise explanations
		//set the failure clause
		int failure_time = overused_intervals[0].start_time + (overused_intervals[0].end_time - overused_intervals[0].start_time) / 2;
		vec<BooleanLiteral> failure_literals;
		int total_resource_consumption = 0; //we collect only a subset of the failure tasks in order of id, possibly there are better ways here
		for (int task_id = 0; task_id < tasks_.size() && total_resource_consumption <= max_capacity_; task_id++)
		{
			if (is_task_active[task_id][failure_time])
			{
				Task& task = tasks_[task_id];

				total_resource_consumption += task.resource_consumption;

				BooleanLiteral lb_lit = state_->GetCurrentLowerBoundLiteral(task.start_time);
				failure_literals.push(~lb_lit);

				BooleanLiteral ub_lit = state_->GetCurrentUpperBoundLiteral(task.start_time);
				failure_literals.push(~ub_lit);
			}
		}
		//runtime_assert(DebugCheckOptimalSolution(failure_literals));
		Clause *failure_clause = clause_allocator_.CreateClause(failure_literals);
		InitialiseFailureClause(failure_clause);
		return true; //report conflict detected
	}
	//no failure, proceed with propagation
	//	remove infeasible times from starting times of tasks
	for (int t = 0; t < max_time_; t++)
	{
		for (int task_id = 0; task_id < tasks_.size(); task_id++)
		{
			Task& task = tasks_[task_id];
			//if the explanation_task cannot be scheduled at this time
			if (!is_task_active[task_id][t]
				&& (compulsory_part[t] + task.resource_consumption > max_capacity_ /*|| !state_.domain_manager_.IsInDomain(task.start_time, t)*/))
				//is the !not_in_domain standard? I am not sure, probably
			{
				//skip if surely no propagation will be done
				if (state_->domain_manager_.GetLowerBound(task.start_time) > t) { continue; }

				vec<BooleanLiteral> explanation_literals;
				int total_resource_consumption = 0; //we collect only a subset of the tasks in order of id, possibly there are better ways here
				for (int explanation_task_id = 0; explanation_task_id < tasks_.size() && total_resource_consumption + task.resource_consumption <= max_capacity_; explanation_task_id++)
				{
					if (is_task_active[explanation_task_id][t])
					{
						pumpkin_assert_moderate(task_id != explanation_task_id, "Sanity check.");
						Task& explanation_task = tasks_[explanation_task_id];

						total_resource_consumption += explanation_task.resource_consumption;

						BooleanLiteral lb_lit = state_->GetCurrentLowerBoundLiteral(explanation_task.start_time);
						explanation_literals.push(~lb_lit);

						BooleanLiteral ub_lit = state_->GetCurrentUpperBoundLiteral(explanation_task.start_time);
						explanation_literals.push(~ub_lit);
					}
				}
				//t is a forbidden time, meaning that any start time that would require the explanation_task to be active at t is also forbidden
				int forbidden_interval_start = std::max(t - task.duration + 1, 0);
				//for (int forbidden_t = forbidden_interval_start; forbidden_t < forbidden_interval_start + task.duration; forbidden_t++)
				for (int forbidden_t = forbidden_interval_start; forbidden_t <= t; forbidden_t++)
				{
					BooleanLiteral propagated_literal = ~(state_->GetEqualityLiteral(task.start_time, forbidden_t));
					pumpkin_assert_simple(!state_->assignments_.IsAssignedFalse(propagated_literal), "Sanity check.");//I think if it was assigned false, a conflict would have been detected, and if it was true, the lower_bound would not be propagated now

					//only provide explanations if the value is in the domain
					if (!state_->assignments_.IsAssigned(propagated_literal))
					{
						//set the explanation eagerly
						//we respect the convention that the propagated literal is at the zeroth position
						vec<BooleanLiteral> explanation_propagation(explanation_literals.size() + 1);
						explanation_propagation[0] = propagated_literal;
						for (int i = 1; i < explanation_propagation.size(); i++) { explanation_propagation[i] = explanation_literals[i - 1]; }

						state_->EnqueuePropagatedLiteral(propagated_literal, GetPropagatorID());

						pumpkin_assert_simple(explanation_propagation.size() > 1 || state_->GetCurrentDecisionLevel() == 0, "Sanity check.");
						//runtime_assert(DebugCheckOptimalSolution(explanation_propagation));
						map_literal_to_eager_explanation_[propagated_literal.ToPositiveInteger()] = explanation_propagation;
					}
				}
			}
		}
	}
	return false; //no conflict
}

void CumulativePropagator::SynchroniseInternal()
{
	if (state_->GetCurrentDecisionLevel() == 0){ clause_allocator_.Clear(); }

	if (incremental_strategy_ == IncrementalStrategy::ON)
	{
		updated_times_.clear();
		failure_detected_ = false;

		//make sure the values are properly updated in the bound tracker
		for (const Task& task : tasks_)
		{
			bound_tracker_.UpdateVariableBounds(task.start_time, state_->domain_manager_.GetLowerBound(task.start_time), state_->domain_manager_.GetUpperBound(task.start_time));
		}

		for (const auto& variable_difference : bound_tracker_.GetChanges())
		{
			pumpkin_assert_moderate(variable_difference.bound_info.checkpoint.lower_bound != variable_difference.bound_info.current.lower_bound || variable_difference.bound_info.checkpoint.upper_bound != variable_difference.bound_info.current.upper_bound, "Sanity check.");

			int task_id = integer_variable_to_task_id_[variable_difference.variable.id];
			pumpkin_assert_moderate(task_id != -1, "Sanity check.");
			Task& task = tasks_[task_id];
			pumpkin_assert_moderate(tasks_[task_id].start_time == variable_difference.variable, "Sanity check.");

			int lb_old = variable_difference.bound_info.checkpoint.lower_bound;
			int lb_new = variable_difference.bound_info.current.lower_bound;

			int ub_old = variable_difference.bound_info.checkpoint.upper_bound;
			int ub_new = variable_difference.bound_info.current.upper_bound;

			//update by considering only a lower bound change using the old ub
			for (int t = std::max(lb_new + task.duration, ub_old); t < lb_old + task.duration; t++)
			{
				//remove these times
				pumpkin_assert_moderate(is_task_active_[task_id][t], "Sanity check.");
				is_task_active_[task_id][t] = false;
				compulsory_part_[t] -= task.resource_consumption;
				pumpkin_assert_moderate(compulsory_part_[t] >= 0, "Sanity check.");
			}

			lb_old = lb_new;

			//now update by considering that the lower bound was updated, and now the upper bound changed
			for (int t = ub_old; t < std::min(ub_new, lb_new + task.duration); t++)
			{
				//remove these times
				pumpkin_assert_moderate(is_task_active_[task_id][t], "Sanity check.");
				is_task_active_[task_id][t] = false;
				compulsory_part_[t] -= task.resource_consumption;
				pumpkin_assert_moderate(compulsory_part_[t] >= 0, "Sanity check.");
			}
		}
	}
	
	bound_tracker_.DiscardChanges();
	for (Task& task : tasks_)
	{
		int lb = state_->domain_manager_.GetLowerBound(task.start_time);
		int ub = state_->domain_manager_.GetUpperBound(task.start_time);
		bound_tracker_.UpdateVariableBounds(task.start_time, lb, ub);
	}

	bound_tracker_.SaveCheckpoint(); //necessary to establish this the reference point for future propagation
}

bool CumulativePropagator::DebugCheckInfeasibility(const std::vector<IntegerVariable>& relevant_variables, const SimpleBoundTracker &bounds) const
{
	std::vector<int> compulsory_part(max_time_, 0);
	std::vector<std::vector<bool> > is_task_active(relevant_variables.size(), std::vector<bool>(max_time_, false));

	//compute the current resource consumption considering only the relevant variables
	for (int i = 0; i < relevant_variables.size(); i++)
	{
		const Task& task = tasks_[integer_variable_to_task_id_[relevant_variables[i].id]];
		int lower_bound = bounds.GetLowerBound(task.start_time);
		int upper_bound = bounds.GetUpperBound(task.start_time);
		int length_of_activity = std::max(lower_bound + task.duration - upper_bound, 0);
		for (int t = lower_bound + task.duration - length_of_activity; t < lower_bound + task.duration; t++)
		{
			compulsory_part[t] += task.resource_consumption;
			is_task_active[i][t] = true;
			//if at least one time slot exceeds the capacity, report infeasible
			if (compulsory_part[t] > max_capacity_)
			{
				return true;
			}
		}
	}
	return false; //no infeasibility detected
}

void CumulativePropagator::SubscribeDomainChanges()
{
	for (Task &task: tasks_)
	{
		state_->watch_list_CP_.SubscribeToLowerBoundChanges(this, task.start_time, *state_);
		state_->watch_list_CP_.SubscribeToUpperBoundChanges(this, task.start_time, *state_);
	}
}

bool CumulativePropagator::NotifyDomainChange(IntegerVariable variable)
{
	int task_id = integer_variable_to_task_id_[variable.id];
	pumpkin_assert_moderate(task_id != -1, "Sanity check.");
	Task& task = tasks_[task_id];
	pumpkin_assert_moderate(tasks_[task_id].start_time == variable, "Sanity check.");

	//get activity length before
	int lb_old = bound_tracker_.GetLowerBound(variable);
	int ub_old = bound_tracker_.GetUpperBound(variable);
	int length_of_activity_old = std::max(lb_old + task.duration - ub_old, 0);

	//now update internal bound information
	int lb_new = state_->domain_manager_.GetLowerBound(variable);
	int ub_new = state_->domain_manager_.GetUpperBound(variable);
	bound_tracker_.UpdateVariableBounds(variable, lb_new, ub_new);

	bound_tracker_.SaveCheckpointForVariable(variable);

	//get activity length after the update
	int length_of_activity_new = std::max(lb_new + task.duration - ub_new, 0);

	//based on this difference, update compulsory part and is_task_active, and save the time as new
	if (incremental_strategy_ == IncrementalStrategy::ON && length_of_activity_new > length_of_activity_old)
	{
		if (length_of_activity_old == 0)
		{
			for (int t = lb_new + task.duration - length_of_activity_new; t < lb_new + task.duration; t++)
			{
				is_task_active_[task_id][t] = true;
				compulsory_part_[t] += task.resource_consumption;
				failure_detected_ |= (compulsory_part_[t] > max_capacity_);
				updated_times_.insert(t);
			}
		}
		else
		{
			if (lb_old < lb_new)
			{
				for (int t = lb_old + task.duration; t < lb_new + task.duration; t++)
				{
					is_task_active_[task_id][t] = true;
					compulsory_part_[t] += task.resource_consumption;
					failure_detected_ |= (compulsory_part_[t] > max_capacity_);
					updated_times_.insert(t);
				}
				lb_old = lb_new; //needed so that the next update is done properly
			}

			if (ub_old > ub_new)
			{
				for (int t = ub_new; t < ub_old; t++)
				{
					is_task_active_[task_id][t] = true;
					compulsory_part_[t] += task.resource_consumption;
					failure_detected_ |= (compulsory_part_[t] > max_capacity_);
					updated_times_.insert(t);
				}
			}
		}		
	}

	//runtime_assert(DebugCompulsoryAndTaskActiveAreCorrect());

	pumpkin_assert_simple(length_of_activity_new >= length_of_activity_old, "Sanity check."); //sanity check
	
	return length_of_activity_new > length_of_activity_old;//only activate the propagator if there would be a change in the compulsory part
}

Clause* CumulativePropagator::ExplainLiteralPropagationInternal(BooleanLiteral literal)
{
	pumpkin_assert_moderate(state_->assignments_.IsAssigned(literal) && state_->assignments_.GetAssignmentLevel(literal) > 0, "The propagator does not keep info on root propagations."); 
	vec<BooleanLiteral> explanation = map_literal_to_eager_explanation_[literal.ToPositiveInteger()];
	Clause* clause = clause_allocator_.CreateClause(explanation);
	return clause;
}

void CumulativePropagator::DebugSetOptimalSolution(std::vector<IntegerVariable>& vars, std::vector<int>& sol)
{//this is a bit hacky. We assume that sol is the solution such that sol[i] is the assigned for variable with id i+1
	if (sol.empty()) { return; }

	int max_index = -1;
	for (IntegerVariable var : vars){ max_index = std::max(max_index, var.id); }
	std::vector<int> real_sol(max_index+1, -1);

	for (Task& task : tasks_) { real_sol[task.start_time.id] = sol[task.start_time.id-1]; } //see above about the +-1
	
	optimal_solution_ = real_sol;
}

PropagationStatus CumulativePropagator::InitialiseAtRootInternal()
{
	for (Task& task : tasks_)
	{
		int lb = state_->domain_manager_.GetLowerBound(task.start_time);
		int ub = state_->domain_manager_.GetUpperBound(task.start_time);
		bound_tracker_.RegisterVariable(task.start_time, lb, ub);
	}

	//remove trivially infeasible times from the domains of the variables
	for (Task& task : tasks_)
	{
		for (int t = max_time_ - task.duration + 1; t < max_time_; t++)
		{
			BooleanLiteral lit = ~(state_->GetEqualityLiteral(task.start_time, t));
			bool conflict_detected = state_->propagator_clausal_.AddUnitClause(lit);
			pumpkin_assert_simple(!conflict_detected, "Sanity check."); //we assume this is not possible but in future could look for a better solution instead of crashing; the propagator would need to set the failure clause properly
		}
	}

	RecomputeCompulsoryAndIsTaskActiveFromScratch();

	if (incremental_strategy_ == IncrementalStrategy::ON) { for (int t = 0; t < max_time_; t++) { updated_times_.insert(t); } }

	return Propagate();
}

bool CumulativePropagator::DebugCheckOptimalSolution(vec<BooleanLiteral>& explanation)
{
	if (optimal_solution_.IsEmpty()) { return true; }

	//the feasible solution must satisfy the explanation
	for (BooleanLiteral lit : explanation)
	{
		auto lit_info = state_->GetLiteralInformation(lit);
		pumpkin_assert_simple(optimal_solution_[lit_info.integer_variable] != -1, "Sanity check.");
		bool ok = optimal_solution_.IsSatisfied(lit_info.integer_variable, lit_info.operation, lit_info.right_hand_side);
		if (ok) { return true; }		
	}

	for (BooleanLiteral lit : explanation)
	{
		std::string s;
		auto lit_info = state_->GetLiteralInformation(lit);

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

		
		std::cout << s << " ; ";
	}
	std::cout << "\n";

	return false;
}

bool CumulativePropagator::DebugCompulsoryAndTaskActiveAreCorrect() const
{
	std::vector<int> compulsory_part(compulsory_part_.size(), 0);
	std::vector<std::vector<bool> > is_task_active(is_task_active_.size(), std::vector<bool>(max_time_, false));

	//compute the current resource consumption
	for (int task_id = 0; task_id < tasks_.size(); task_id++)
	{
		const Task& task = tasks_[task_id];
		int lower_bound = state_->domain_manager_.GetLowerBound(task.start_time);
		int upper_bound = state_->domain_manager_.GetUpperBound(task.start_time);
		int length_of_activity = std::max(lower_bound + task.duration - upper_bound, 0);
		for (int t = lower_bound + task.duration - length_of_activity; t < lower_bound + task.duration; t++)
		{
			compulsory_part[t] += task.resource_consumption;
			is_task_active[task_id][t] = true;			
		}
	}

	for (int t = 0; t < max_time_; t++)
	{
		pumpkin_assert_simple(compulsory_part_[t] == compulsory_part[t], "Sanity check.");
		for (int task_id = 0; task_id < tasks_.size(); task_id++)
		{
			pumpkin_assert_simple(is_task_active_[task_id][t] == is_task_active[task_id][t], "Sanity check.");
			int k = 0;
		}
		int m = 0;
	}

	return true;
}

void CumulativePropagator::RecomputeCompulsoryAndIsTaskActiveFromScratch()
{
	failure_detected_ = false;

	for (int t = 0; t < max_time_; t++)
	{
		compulsory_part_[t] = 0;
		for(int task_id = 0; task_id < tasks_.size(); task_id++) { is_task_active_[task_id][t] = false; }
	}

	//compute the current resource consumption
	for (int task_id = 0; task_id < tasks_.size(); task_id++)
	{
		Task& task = tasks_[task_id];
		int lower_bound = state_->domain_manager_.GetLowerBound(task.start_time);
		int upper_bound = state_->domain_manager_.GetUpperBound(task.start_time);
		int length_of_activity = std::max(lower_bound + task.duration - upper_bound, 0);
		for (int t = lower_bound + task.duration - length_of_activity; t < lower_bound + task.duration; t++)
		{
			compulsory_part_[t] += task.resource_consumption;
			is_task_active_[task_id][t] = true;
			failure_detected_ |= (compulsory_part_[t] > max_capacity_);
		}
	}
}

void CumulativePropagator::AddTaskExplanationForTime(vec<BooleanLiteral>& clause, const Task& task, int time) const
{
	if (lifting_strategy_ == LiftingStrategy::OFF)
	{
		BooleanLiteral lb_lit = state_->GetCurrentLowerBoundLiteral(task.start_time);
		clause.push(~lb_lit);

		BooleanLiteral ub_lit = state_->GetCurrentUpperBoundLiteral(task.start_time);
		clause.push(~ub_lit);
	}
	else if (lifting_strategy_ == LiftingStrategy::ON)
	{
		int lower_bound_lifted = std::max(time - task.duration + 1, 0);
		int upper_bound_lifted = time;

		int old_lb = state_->domain_manager_.GetLowerBound(task.start_time);
		int old_ub = state_->domain_manager_.GetUpperBound(task.start_time);
		pumpkin_assert_moderate(lower_bound_lifted <= old_lb, "Sanity check.");
		pumpkin_assert_moderate(upper_bound_lifted >= old_ub, "Sanity check.");

		BooleanLiteral lb_lit = state_->GetLowerBoundLiteral(task.start_time, lower_bound_lifted);
		clause.push(~lb_lit);
		
		BooleanLiteral ub_lit = state_->GetUpperBoundLiteral(task.start_time, upper_bound_lifted);
		clause.push(~ub_lit);
	}
	else
	{
		pumpkin_assert_permanent(1 == 2, "Unknown lifting strategy in the cumulative constraint.\n");
	}
}

}