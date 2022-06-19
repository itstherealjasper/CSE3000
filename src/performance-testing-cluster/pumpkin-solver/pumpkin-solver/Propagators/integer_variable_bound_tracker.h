#pragma once

#include "../Utilities/integer_variable.h"
#include "../Utilities/domain_operation.h"

#include <vector>

namespace Pumpkin
{
class IntegerVariableBoundTracker
{
struct Bounds { int lower_bound, upper_bound; bool operator!=(Bounds& b) { return lower_bound != b.lower_bound || upper_bound != b.upper_bound; }};
struct BoundsInfo { Bounds checkpoint, current; };
struct VariableDifference { IntegerVariable variable; BoundsInfo bound_info; };

public:
	void RegisterVariable(IntegerVariable, int lower_bound, int upper_bound);
	
	void UpdateVariableBounds(IntegerVariable, int new_lower_bound, int new_upper_bound);
	void SaveCheckpoint();
	void SaveCheckpointForVariable(IntegerVariable);
	void DiscardChanges();

	int GetLowerBound(IntegerVariable) const;
	int GetUpperBound(IntegerVariable) const;

	std::vector<VariableDifference> GetChanges();
	std::vector<std::pair<IntegerVariable, Bounds > > DebugGetCurrentBounds();

private:
	bool IsVariableAlreadyRegistered(IntegerVariable) const;
	void ResizeInternal(IntegerVariable);

	std::vector<IntegerVariable> present_variables_;
	std::vector<BoundsInfo> variable_to_bounds_;
};
}