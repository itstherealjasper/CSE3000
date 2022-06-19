#include "integer_variable_bound_tracker.h"
#include "../Utilities/runtime_assert.h"

namespace Pumpkin
{
void IntegerVariableBoundTracker::RegisterVariable(IntegerVariable variable, int lower_bound, int upper_bound)
{
	runtime_assert(!IsVariableAlreadyRegistered(variable));

	ResizeInternal(variable);

	BoundsInfo bounds_info;
	bounds_info.checkpoint.lower_bound = lower_bound;
	bounds_info.checkpoint.upper_bound = upper_bound;
	bounds_info.current.lower_bound = lower_bound;
	bounds_info.current.upper_bound = upper_bound;
	
	variable_to_bounds_[variable.id] = bounds_info;
	present_variables_.push_back(variable);
}
void IntegerVariableBoundTracker::UpdateVariableBounds(IntegerVariable variable, int new_lower_bound, int new_upper_bound)
{
	assert(IsVariableAlreadyRegistered(variable));

	variable_to_bounds_[variable.id].current.lower_bound = new_lower_bound;
	variable_to_bounds_[variable.id].current.upper_bound = new_upper_bound;
}

void IntegerVariableBoundTracker::SaveCheckpoint()
{
	for (IntegerVariable var: present_variables_){ variable_to_bounds_[var.id].checkpoint = variable_to_bounds_[var.id].current; }
}

void IntegerVariableBoundTracker::SaveCheckpointForVariable(IntegerVariable variable)
{
	auto& x = variable_to_bounds_[variable.id];
	x.checkpoint = x.current;
}

void IntegerVariableBoundTracker::DiscardChanges()
{
	for (IntegerVariable var : present_variables_) { variable_to_bounds_[var.id].current = variable_to_bounds_[var.id].checkpoint; }
}

int IntegerVariableBoundTracker::GetLowerBound(IntegerVariable var) const
{
	runtime_assert(IsVariableAlreadyRegistered(var));
	return variable_to_bounds_[var.id].current.lower_bound;
}

int IntegerVariableBoundTracker::GetUpperBound(IntegerVariable var) const
{
	runtime_assert(IsVariableAlreadyRegistered(var));
	return variable_to_bounds_[var.id].current.upper_bound;
}

std::vector<IntegerVariableBoundTracker::VariableDifference> IntegerVariableBoundTracker::GetChanges()
{
	std::vector<VariableDifference> diffs;
	for (IntegerVariable var: present_variables_)
	{
		auto& bounds = variable_to_bounds_[var.id];
		if (bounds.checkpoint != bounds.current) 
		{
			VariableDifference diff;
			diff.variable = var;
			diff.bound_info = bounds;
			diffs.push_back(diff);
		}
	}
	return diffs;
}
std::vector<std::pair<IntegerVariable, IntegerVariableBoundTracker::Bounds> > IntegerVariableBoundTracker::DebugGetCurrentBounds()
{
	std::vector<std::pair<IntegerVariable, Bounds> > bounds;
	for (IntegerVariable var: present_variables_)
	{
		std::pair<IntegerVariable, Bounds> pair;
		pair.first = var;
		pair.second.lower_bound = variable_to_bounds_[var.id].current.lower_bound;
		pair.second.upper_bound = variable_to_bounds_[var.id].current.upper_bound;
		bounds.push_back(pair);
	}
	return bounds;
}

bool IntegerVariableBoundTracker::IsVariableAlreadyRegistered(IntegerVariable variable) const
{
	return variable.id < variable_to_bounds_.size() && variable_to_bounds_[variable.id].current.lower_bound != INT32_MAX;
}

void IntegerVariableBoundTracker::ResizeInternal(IntegerVariable var)
{
	if (var.id >= variable_to_bounds_.size())
	{
		Bounds null;
		null.lower_bound = INT32_MAX;
		null.upper_bound = INT32_MIN;
		BoundsInfo b_info;
		b_info.checkpoint = b_info.current = null;
		variable_to_bounds_.resize(var.id + 1, b_info);
	}
}

}