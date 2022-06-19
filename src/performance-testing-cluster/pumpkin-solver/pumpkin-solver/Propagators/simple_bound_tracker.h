#pragma once

#include "../Utilities/integer_variable.h"
#include "../Utilities/directly_hashed_integer_set.h"
#include "../Utilities/domain_operation.h"
#include "../Utilities/domain_info.h"

#include <vector>

namespace Pumpkin
{
class SimpleBoundTracker
{
public:
	SimpleBoundTracker();

	void RegisterVariable(IntegerVariable, int lower_bound, int upper_bound);
	void UpdateDomain(IntegerVariable, DomainOperation, int right_hand_side);
	void Clear();

	int GetLowerBound(IntegerVariable) const;
	int GetUpperBound(IntegerVariable) const;

private:
	bool IsVariableRegistered(IntegerVariable) const;

	DirectlyHashedIntegerSet registered_ids_;
	std::vector<DomainInfo> domains_; //[i][j] indicates if value j is in the domain of variable i
};

}
