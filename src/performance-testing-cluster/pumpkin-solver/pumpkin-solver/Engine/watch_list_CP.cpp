#include "watch_list_CP.h"
#include "solver_state.h"

namespace Pumpkin
{
void WatchListCP::UpdateAllFlags(SolverState &state)
{
	//important to update flag information before calling propagation
	
	//update clauses
	//	in general anything that stores literals directly needs to be updated
	//	however usually we do not store literals directly for this exactly reason, but there is no realistic alternative for the clausal propagator
	state.propagator_clausal_.UpdateFlagInfo();

	state.domain_manager_.UpdateDomainsFromScratch();

	for (int var_id = 1; var_id < state.integer_variable_to_literal_info_.size(); var_id++)
	{
		for (int i = 0; i < state.integer_variable_to_literal_info_[var_id].equality_literals.size(); i++)
		{
			bool flag_value = state.ComputeFlagFromScratch(state.integer_variable_to_literal_info_[var_id].equality_literals[i]);
			state.integer_variable_to_literal_info_[var_id].equality_literals[i].SetFlag(flag_value);
		}

		for (int i = 0; i < state.integer_variable_to_literal_info_[var_id].greater_or_equal_literals.size(); i++)
		{
			bool flag_value = state.ComputeFlagFromScratch(state.integer_variable_to_literal_info_[var_id].greater_or_equal_literals[i]);
			state.integer_variable_to_literal_info_[var_id].greater_or_equal_literals[i].SetFlag(flag_value);
		}
	}
}
}