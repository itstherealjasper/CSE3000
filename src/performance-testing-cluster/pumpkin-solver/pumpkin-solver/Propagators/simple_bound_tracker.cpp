#include "simple_bound_tracker.h"
#include "../Utilities/pumpkin_assert.h"

namespace Pumpkin
{
SimpleBoundTracker::SimpleBoundTracker():
	registered_ids_(0)
{
}

void SimpleBoundTracker::RegisterVariable(IntegerVariable variable, int lower_bound, int upper_bound)
{
	pumpkin_assert_simple(!IsVariableRegistered(variable) && lower_bound >= 0 && upper_bound >= lower_bound, "Sanity check.");
	if (variable.id >= domains_.size()) { domains_.resize(variable.id + 1); }
	registered_ids_.Insert(variable.id);
	domains_[variable.id].Initialise(lower_bound, upper_bound);	
}

void SimpleBoundTracker::UpdateDomain(IntegerVariable variable, DomainOperation operation, int right_hand_side)
{
	DomainInfo& domain = domains_[variable.id];
	if (operation.IsEquality())
	{
		pumpkin_assert_simple(domain.is_value_in_domain.ReadBit(right_hand_side), "Sanity check.");
		//remove all values from the domain apart from the given right hand side
		int old_lower_bound = GetLowerBound(variable);
		int old_upper_bound = GetUpperBound(variable);
		for (int i = old_lower_bound; i <= old_upper_bound; i++) { domain.is_value_in_domain.AssignBit(i, (i == right_hand_side)); }
		//set the lower and upper bound to the assigned value
		domain.lower_bound = right_hand_side;
		domain.upper_bound = right_hand_side;

		//activate propagators subscribed to domain changes

		//...if the lower bound has been raised
		if (old_lower_bound != right_hand_side)
		{
			pumpkin_assert_simple(old_lower_bound < right_hand_side, "Sanity check.");
		}
		//...if the upper bound has been lowered
		if (old_upper_bound != right_hand_side)
		{
			pumpkin_assert_simple(old_upper_bound > right_hand_side, "Sanity check.");
		}
	}
	else if (operation.IsNotEqual())
	{
		//only do something if the value is in the domain - otherwise the value has already been removed through other means (say a lower bound operation took place right before this inequality)
		if (domain.is_value_in_domain.ReadBit(right_hand_side))
		{
			int old_lower_bound = GetLowerBound(variable);
			int old_upper_bound = GetUpperBound(variable);

			domain.is_value_in_domain.ClearBit(right_hand_side);

			if (old_lower_bound == right_hand_side)
			{
				//set the lower bound to the next value (note that the lb might increase by more than one, if the value after 'right_hand_side' are also removed)
				while (!domain.is_value_in_domain.ReadBit(domain.lower_bound)) { domain.lower_bound++; }
			}

			if (old_upper_bound == right_hand_side)
			{
				//set the upper bound to the next value (note that the ub might decrease by more than one, e.g., if the value right below the upper bound is also not in the domain)
				while (!domain.is_value_in_domain.ReadBit(domain.upper_bound)) { domain.upper_bound--; }
			}
		}
	}
	else if (operation.IsLessOrEqual())
	{
		int old_upper_bound = GetUpperBound(variable);

		//...if at least one change has been done
		if (right_hand_side < old_upper_bound)
		{
			//remove values from the domain
			for (int i = right_hand_side + 1; i <= old_upper_bound; i++) { domain.is_value_in_domain.ClearBit(i); }

			domain.upper_bound = right_hand_side;
			pumpkin_assert_simple(domain.lower_bound <= domain.upper_bound, "Sanity check.");
		}
	}
	else if (operation.IsGreaterOrEqual())
	{
		int old_lower_bound = GetLowerBound(variable);

		//...if at least one change has been done
		if (right_hand_side > old_lower_bound)
		{
			//remove values from the domain
			for (int i = old_lower_bound; i < right_hand_side; i++) { domain.is_value_in_domain.ClearBit(i); }

			domain.lower_bound = right_hand_side;
			pumpkin_assert_simple(domain.lower_bound <= domain.upper_bound, "Sanity check.");
		}
	}
	else
	{
		pumpkin_assert_permanent(1 == 2, "Unknown operation."); //operation not recognised
	}
}

void SimpleBoundTracker::Clear()
{
	registered_ids_.Clear();
}

int SimpleBoundTracker::GetLowerBound(IntegerVariable variable) const
{
	pumpkin_assert_simple(IsVariableRegistered(variable), "Variable not registered.");
	return domains_[variable.id].lower_bound;
}

int SimpleBoundTracker::GetUpperBound(IntegerVariable variable) const
{
	pumpkin_assert_simple(IsVariableRegistered(variable), "Variable not registered.");
	return domains_[variable.id].upper_bound;
}

bool SimpleBoundTracker::IsVariableRegistered(IntegerVariable variable) const
{
	return registered_ids_.IsPresent(variable.id);
}

}