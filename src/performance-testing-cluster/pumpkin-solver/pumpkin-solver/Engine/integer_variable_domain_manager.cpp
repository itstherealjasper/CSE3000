#include "integer_variable_domain_manager.h"
#include "solver_state.h"

namespace Pumpkin
{
IntegerVariableDomainManager::IntegerVariableDomainManager(SolverState &state):
	state_(state),
	domains_(1) //zeroth index not used
{}

void IntegerVariableDomainManager::UpdateDomainsFromScratch()
{
	pumpkin_assert_permanent(state_.GetCurrentDecisionLevel() == 0, "Updating the bounds from scratch can only be done at the root for now.");
	
	for (int var_id = 1; var_id < domains_.size(); var_id++)
	{
		IntegerVariable var(var_id);
		domains_[var_id].lower_bound = GetRootLowerBound(var);
		domains_[var_id].upper_bound = GetRootUpperBound(var);

		for (int i = 0; i < domains_[var_id].is_value_in_domain.Size(); i++)
		{
			BooleanLiteral lit = state_.GetNotEqualLiteral(var, i);
			if (state_.assignments_.IsAssignedTrue(lit))
			{
				domains_[var_id].is_value_in_domain.ClearBit(i);
			}
		}
	}
}

void IntegerVariableDomainManager::UpdateDomain(IntegerVariable variable, DomainOperation operation, int right_hand_side)
{
	DomainInfo& domain = domains_[variable.id];
	if (operation.IsEquality())
	{
		pumpkin_assert_moderate(domain.is_value_in_domain.ReadBit(right_hand_side), "Cannot do an assignment that is not in the domain.");
		//remove all values from the domain apart from the given right hand side
		int old_lower_bound = GetLowerBound(variable);
		int old_upper_bound = GetUpperBound(variable);
		for (int i = old_lower_bound; i <= old_upper_bound; i++) { domain.is_value_in_domain.AssignBit(i, (i == right_hand_side)); }
		//set the lower and upper bound to the assigned value
		domain.lower_bound = right_hand_side;
		domain.upper_bound = right_hand_side;

		//activate propagators subscribed to domain changes

		//...if at least one value was removed
		if (old_lower_bound != old_upper_bound)
		{
			state_.NotifyPropagatorsSubscribedtoInequalityChanges(variable);
		}
		//...if the lower bound has been raised
		if (old_lower_bound != right_hand_side)
		{
			pumpkin_assert_moderate(old_lower_bound < right_hand_side, "Sanity check.");
			state_.NotifyPropagatorsSubscribedtoLowerBoundChanges(variable);
		}
		//...if the upper bound has been lowered
		if (old_upper_bound != right_hand_side)
		{
			pumpkin_assert_moderate(old_upper_bound > right_hand_side, "Sanity check.");
			state_.NotifyPropagatorsSubscribedtoUpperBoundChanges(variable);
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
			state_.NotifyPropagatorsSubscribedtoInequalityChanges(variable);

			if (old_lower_bound == right_hand_side)
			{
				//set the lower bound to the next value (note that the lb might increase by more than one, if the value after 'right_hand_side' are also removed)
				while (!domain.is_value_in_domain.ReadBit(domain.lower_bound)) { domain.lower_bound++;	}

				state_.NotifyPropagatorsSubscribedtoLowerBoundChanges(variable);
			}

			if (old_upper_bound == right_hand_side)
			{
				//set the upper bound to the next value (note that the ub might decrease by more than one, e.g., if the value right below the upper bound is also not in the domain)
				while (!domain.is_value_in_domain.ReadBit(domain.upper_bound)) { domain.upper_bound--; }

				state_.NotifyPropagatorsSubscribedtoUpperBoundChanges(variable);
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
			pumpkin_assert_moderate(domain.lower_bound <= domain.upper_bound, "Sanity check.");

			state_.NotifyPropagatorsSubscribedtoUpperBoundChanges(variable);
			state_.NotifyPropagatorsSubscribedtoInequalityChanges(variable);			
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
			pumpkin_assert_moderate(domain.lower_bound <= domain.upper_bound, "Sanity check.");

			state_.NotifyPropagatorsSubscribedtoLowerBoundChanges(variable);
			state_.NotifyPropagatorsSubscribedtoInequalityChanges(variable);
		}
	}
	else
	{
		std::cout << "var: " << variable.id << "\n";
		std::cout << "rhs: " << right_hand_side << "\n";
		pumpkin_assert_permanent(1 == 2, "Unknown domain operation."); //operation not recognised
	}

	//assert(!DebugIsDomainEmpty(variable));	
}

void IntegerVariableDomainManager::DebugUpdateDomainNoNotification(IntegerVariable variable, DomainOperation operation, int right_hand_side)
{
	DomainInfo& domain = domains_[variable.id];
	if (operation.IsEquality())
	{
		runtime_assert(domain.is_value_in_domain.ReadBit(right_hand_side));
		//remove all values from the domain apart from the given right hand side
		int old_lower_bound = GetLowerBound(variable);
		int old_upper_bound = GetUpperBound(variable);
		for (int i = old_lower_bound; i <= old_upper_bound; i++) { domain.is_value_in_domain.AssignBit(i, (i == right_hand_side)); }
		//set the lower and upper bound to the assigned value
		domain.lower_bound = right_hand_side;
		domain.upper_bound = right_hand_side;

		//activate propagators subscribed to domain changes

		//...if at least one value was removed
		if (old_lower_bound != old_upper_bound)
		{
			//state_.NotifyPropagatorsSubscribedtoInequalityChanges(variable);
		}
		//...if the lower bound has been raised
		if (old_lower_bound != right_hand_side)
		{
			runtime_assert(old_lower_bound < right_hand_side);
			//state_.NotifyPropagatorsSubscribedtoLowerBoundChanges(variable);
		}
		//...if the upper bound has been lowered
		if (old_upper_bound != right_hand_side)
		{
			runtime_assert(old_upper_bound > right_hand_side);
			//state_.NotifyPropagatorsSubscribedtoUpperBoundChanges(variable);
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
			//state_.NotifyPropagatorsSubscribedtoInequalityChanges(variable);

			if (old_lower_bound == right_hand_side)
			{
				//set the lower bound to the next value (note that the lb might increase by more than one, if the value after 'right_hand_side' are also removed)
				while (!domain.is_value_in_domain.ReadBit(domain.lower_bound)) { domain.lower_bound++; }

				//state_.NotifyPropagatorsSubscribedtoLowerBoundChanges(variable);
			}

			if (old_upper_bound == right_hand_side)
			{
				//set the upper bound to the next value (note that the ub might decrease by more than one, e.g., if the value right below the upper bound is also not in the domain)
				while (!domain.is_value_in_domain.ReadBit(domain.upper_bound)) { domain.upper_bound--; }

				//state_.NotifyPropagatorsSubscribedtoUpperBoundChanges(variable);
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
			runtime_assert(domain.lower_bound <= domain.upper_bound);

			//state_.NotifyPropagatorsSubscribedtoUpperBoundChanges(variable);
			//state_.NotifyPropagatorsSubscribedtoInequalityChanges(variable);
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
			runtime_assert(domain.lower_bound <= domain.upper_bound);

			//state_.NotifyPropagatorsSubscribedtoLowerBoundChanges(variable);
			//state_.NotifyPropagatorsSubscribedtoInequalityChanges(variable);
		}
	}
	else
	{
		runtime_assert(1 == 2); //operation not recognised
	}
}

bool IntegerVariableDomainManager::DebugCheckVariableDomain(IntegerVariable variable)
{
	runtime_assert(domains_[variable.id].lower_bound <= domains_[variable.id].upper_bound);

	auto& domain = domains_[variable.id].is_value_in_domain;
	for (int i = 0; i < domain.Size(); i++)
	{
		if (!domain.ReadBit(i))
		{
			BooleanLiteral lit_neq = state_.GetNotEqualLiteral(variable, i);
			runtime_assert(state_.assignments_.IsAssignedTrue(lit_neq));			
		}
		else
		{
			pumpkin_assert_simple(domains_[variable.id].lower_bound <= i && i <= domains_[variable.id].upper_bound, "The domain has the value in its domain, but the value is outside the bounds.");

			if (i > 0)
			{
				BooleanLiteral lit_lb = state_.GetLowerBoundLiteral(variable, i);
				auto& c = state_.GetLiteralInformation(lit_lb);
				runtime_assert(!state_.assignments_.IsAssignedFalse(lit_lb));
				int m = 0;
			}

			if (i + 1 < domain.Size())
			{
				BooleanLiteral lit_ub = state_.GetUpperBoundLiteral(variable, i);
				runtime_assert(!state_.assignments_.IsAssignedFalse(lit_ub));
			}					
		}
	}
	//check if the lower bound is correct
	for (int i = 0; i < domain.Size(); i++){ if (domain.ReadBit(i)) { runtime_assert(domains_[variable.id].lower_bound == i); break; } }
	//check if the upper bound is correct
	for (int i = domain.Size() - 1; i >= 0; i--) { if (domain.ReadBit(i)) { runtime_assert(domains_[variable.id].upper_bound == i); break; } }

	return true;
}

int IntegerVariableDomainManager::GetRootLowerBound(IntegerVariable variable)
{
	int lb = 0;
	for (int i = 1; i < domains_[variable.id].is_value_in_domain.Size(); i++)
	{
		BooleanLiteral literal = state_.GetLowerBoundLiteral(variable, i);

		//if the literal is unassigned at the root, then the previous lower bound is the lower bound at the root
		if (!state_.assignments_.IsRootAssignment(literal)) { break; }

		//at this point, the literal is a root assignment
		//if the literal is assigned false, then the previous lower bound is the lower bound at the root
		if (state_.assignments_.IsAssignedFalse(literal)) { break; }

		pumpkin_assert_simple(state_.assignments_.IsAssignedTrue(literal), "Literal must be set to true.");
		//otherwise, register that the lower is at least 'i', and continue searching
		lb = i;
	}
	return lb;
}

int IntegerVariableDomainManager::GetRootUpperBound(IntegerVariable variable)
{
	int ub = domains_[variable.id].is_value_in_domain.Size() - 1;
	for (int i = ub - 1; i >= 0; i--)
	{
		BooleanLiteral literal = state_.GetUpperBoundLiteral(variable, i);

		//if the literal is unassigned at the root, then the previous upper bound is the upper bound at the root
		if (!state_.assignments_.IsRootAssignment(literal)) { break; }

		//at this point, the literal is a root assignment
		//if the literal is assigned false, then the previous upper bound is the upper bound at the root
		if (state_.assignments_.IsAssignedFalse(literal)) { break; }

		pumpkin_assert_simple(state_.assignments_.IsAssignedTrue(literal), "Literal must be set to true.");
		//otherwise, register that the lower is at least 'i', and continue searching
		ub = i;
	}
	return ub;
}

int IntegerVariableDomainManager::GetLowerBound(IntegerVariable variable)
{
	pumpkin_assert_moderate(!variable.IsNull() && domains_[variable.id].lower_bound <= domains_[variable.id].upper_bound, "Sanity check.");

	//variables that have cp propagators associated with it have their domains tracked correctly
	//	however variable that are _not_ used in cp propagators, the domains are not properly tracked
	//	this is done for efficiency reasons
	
	if (state_.watch_list_CP_.IsVariableWatched(variable))
	{
		return domains_[variable.id].lower_bound;
	}
	else
	{
		return ComputeLowerBoundFromState(variable);
	}
}

int IntegerVariableDomainManager::GetUpperBound(IntegerVariable variable)
{
	pumpkin_assert_moderate(!variable.IsNull() && domains_[variable.id].lower_bound <= domains_[variable.id].upper_bound, "Sanity check.");
	
	//variables that have cp propagators associated with it have their domains tracked correctly
	//	however variable that are _not_ used in cp propagators, the domains are not properly tracked
	//	this is done for efficiency reasons

	if (state_.watch_list_CP_.IsVariableWatched(variable))
	{
		return domains_[variable.id].upper_bound;
	}
	else
	{
		return ComputeUpperBoundFromState(variable);
	}	
}

bool IntegerVariableDomainManager::IsInDomain(IntegerVariable variable, int value)
{
	//variables that have cp propagators associated with it have their domains tracked correctly
	//	however variable that are _not_ used in cp propagators, the domains are not properly tracked
	//	this is done for efficiency reasons

	if (state_.watch_list_CP_.IsVariableWatched(variable))
	{
		return domains_[variable.id].is_value_in_domain.ReadBit(value);
	}
	else
	{
		//important to use lower and upper bound literals, since the equality literal may not be created yet
		BooleanLiteral lit_lb = state_.GetLowerBoundLiteral(variable, value);
		BooleanLiteral lit_ub = state_.GetUpperBoundLiteral(variable, value);
		bool is_assigned_false = state_.assignments_.IsAssignedFalse(lit_lb) || state_.assignments_.IsAssignedFalse(lit_ub);
		return !is_assigned_false;
	}	
}

bool IntegerVariableDomainManager::DebugIsDomainEmpty(IntegerVariable variable)
{
	for (int i = 0; i < domains_[variable.id].is_value_in_domain.Size(); i++)
	{
		bool b = domains_[variable.id].is_value_in_domain.ReadBit(i);
		if (b) { return false; }
	}	
	return true;
}

int IntegerVariableDomainManager::ComputeLowerBoundFromState(IntegerVariable variable)
{
	for (int val = 1; val < domains_[variable.id].is_value_in_domain.Size(); val++)
	{
		BooleanLiteral lb_lit = state_.GetLowerBoundLiteral(variable, val);
		if (!state_.assignments_.IsAssignedTrue(lb_lit)){ return val - 1; }
	}	
	return domains_[variable.id].is_value_in_domain.Size()-1; //otherwise all lb literals are true, so return the maximum value
}

int IntegerVariableDomainManager::ComputeUpperBoundFromState(IntegerVariable variable)
{
	for (int val = domains_[variable.id].is_value_in_domain.Size() - 1 - 1; val >= 0; --val) //-1 since the size exceed the ub by one, and another minus one since the maximum value is trivially true
	{
		BooleanLiteral ub_lit = state_.GetUpperBoundLiteral(variable, val);
		bool is_assigned = state_.assignments_.IsAssignedTrue(ub_lit);
		if (!is_assigned) { return val + 1; }
	}
	return 0; //otherwise all ub literals are set to true, so return 0, which is smallest value for nonnegative integers
}

}