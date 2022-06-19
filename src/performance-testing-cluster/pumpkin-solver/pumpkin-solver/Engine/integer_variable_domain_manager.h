#pragma once

#include "../Utilities/integer_variable.h"
#include "../Utilities/domain_operation.h"
#include "../Utilities/domain_info.h"
#include "../Utilities/directly_hashed_integer_set.h"
#include "../Utilities/pumpkin_assert.h"

#include <vector>
#include <assert.h>

namespace Pumpkin
{
class SolverState;

class IntegerVariableDomainManager
{
public:
	IntegerVariableDomainManager(SolverState &state);

	void UpdateDomainsFromScratch();

	void UpdateDomain(IntegerVariable, DomainOperation, int right_hand_side);
	void DebugUpdateDomainNoNotification(IntegerVariable, DomainOperation, int right_hand_side);
	void ReaddToDomain(IntegerVariable, int value);

	int GetLowerBound(IntegerVariable);
	int GetUpperBound(IntegerVariable);
	int GetRootLowerBound(IntegerVariable);
	int GetRootUpperBound(IntegerVariable);
	bool IsInDomain(IntegerVariable, int);
	
	void Grow(int lower_bound, int upper_bound); //increase data structure to take into account one more integer variable with the given domain [lower_bound, upper_bound]
	//void Clear();

	int NumIntegerVariables() const;

//private:
	/*struct DomainChanges 
	{
		std::vector<int> holes; //these are the individual values that have been removed from the domain (not considering changes to the lower and upper bound)
		int old_lower_bound, old_upper_bound;
		int new_lower_bound, new_upper_bound;
	};*/

	bool DebugCheckVariableDomain(IntegerVariable);
	bool DebugIsDomainEmpty(IntegerVariable);	
	int ComputeLowerBoundFromState(IntegerVariable);
	int ComputeUpperBoundFromState(IntegerVariable);

	SolverState& state_;
	std::vector<DomainInfo> domains_; //[i][j] indicates if value j is in the domain of variable i
};

inline void IntegerVariableDomainManager::ReaddToDomain(IntegerVariable variable, int value)
{
	pumpkin_assert_moderate(!domains_[variable.id].is_value_in_domain.ReadBit(value), "Sanity check.");

	domains_[variable.id].is_value_in_domain.SetBit(value);
	domains_[variable.id].lower_bound = std::min(value, domains_[variable.id].lower_bound);
	domains_[variable.id].upper_bound = std::max(value, domains_[variable.id].upper_bound);
}

inline void IntegerVariableDomainManager::Grow(int lower_bound, int upper_bound)
{
	pumpkin_assert_moderate(lower_bound >= 0 && lower_bound <= upper_bound, "Sanity check.");

	DomainInfo domain_info;
	domain_info.lower_bound = lower_bound;
	domain_info.upper_bound = upper_bound;
	domain_info.is_value_in_domain.Resize(upper_bound + 1);
	for (int i = lower_bound; i <= upper_bound; i++) { domain_info.is_value_in_domain.SetBit(i); }
	domains_.push_back(domain_info);
}

/*inline void IntegerVariableDomainManager::Clear()
{
	domains_.resize(1);
}*/

inline int IntegerVariableDomainManager::NumIntegerVariables() const
{
	return domains_.size() - 1; //-1 since the zeroth index is not used
}
}