#pragma once

#include "../Propagators/propagator_generic_CP.h"
#include "../Utilities/integer_variable.h"
#include "../Utilities/pumpkin_assert.h"

#include <vector>

namespace Pumpkin
{
class SolverState;

class WatchListCP
{
public:

	WatchListCP();

	void Grow();
	int NumIntegerVariablesSupported() const;

	void SubscribeToInequalityChanges(PropagatorGenericCP*, IntegerVariable, SolverState&);
	void SubscribeToLowerBoundChanges(PropagatorGenericCP*, IntegerVariable, SolverState&);
	void SubscribeToUpperBoundChanges(PropagatorGenericCP*, IntegerVariable, SolverState&);

	bool IsVariableWatched(IntegerVariable) const;

	void UpdateAllFlags(SolverState&);

	//todo something like GetPropagatorsSubscribedToInequalityChanges...and then have another class actually make use of it
	//	again the problem is that the user can subscribe while going through the list...

	//these are used for waking up propagators
	//void NotifyPropagatorsSubscribedtoInequalityChanges(IntegerVariable);
	//void NotifyPropagatorsSubscribedtoLowerBoundChanges(IntegerVariable);
	//void NotifyPropagatorsSubscribedtoUpperBoundChanges(IntegerVariable);


	//std::vector<PropagatorGenericCP*> enqueued_cp_propagators;

	struct ThreeWatchers
	{
		std::vector<PropagatorGenericCP*> lb_watcher, ub_watcher, neq_watcher;
		bool HasAtLeastOnePropagator() const { return lb_watcher.size() > 0 || ub_watcher.size() > 0 || neq_watcher.size() > 0; }
	};

	std::vector<ThreeWatchers> watchers; //[variable i][operation j] -> list of propagators. For now j=0 is <= (upper bound), j=1 is >= (lower bound), and j=2 is !=
};

inline WatchListCP::WatchListCP() :watchers(1) {}

inline void WatchListCP::Grow()
{
	watchers.emplace_back();
}

inline int WatchListCP::NumIntegerVariablesSupported() const
{
	return watchers.size() - 1;
}

inline void WatchListCP::SubscribeToInequalityChanges(PropagatorGenericCP* propagator, IntegerVariable variable, SolverState &state)
{
	pumpkin_assert_simple(!variable.IsNull(), "Cannot subscribe the null variable to the watch list.");
	watchers[variable.id].neq_watcher.push_back(propagator);
	UpdateAllFlags(state);
}

inline void WatchListCP::SubscribeToLowerBoundChanges(PropagatorGenericCP* propagator, IntegerVariable variable, SolverState& state)
{
	pumpkin_assert_simple(!variable.IsNull(), "Cannot subscribe the null variable to the watch list.");
	watchers[variable.id].lb_watcher.push_back(propagator);
	UpdateAllFlags(state);
}

inline void WatchListCP::SubscribeToUpperBoundChanges(PropagatorGenericCP* propagator, IntegerVariable variable, SolverState& state)
{
	pumpkin_assert_simple(!variable.IsNull(), "Cannot subscribe the null variable to the watch list.");
	watchers[variable.id].ub_watcher.push_back(propagator);
	UpdateAllFlags(state);
}

inline bool WatchListCP::IsVariableWatched(IntegerVariable variable) const
{
	return watchers[variable.id].lb_watcher.size() > 0 || watchers[variable.id].ub_watcher.size() > 0 || watchers[variable.id].neq_watcher.size() > 0;
}
}
