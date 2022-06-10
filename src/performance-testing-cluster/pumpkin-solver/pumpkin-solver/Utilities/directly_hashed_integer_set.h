#pragma once

#include <vector>

namespace Pumpkin
{
//todo description

class DirectlyHashedIntegerSet
{
public:
	DirectlyHashedIntegerSet(int num_values); //values in the set must be within [0,.., num_values)

	void Insert(int value);
	void Grow();
	void Resize(int new_size);
	void Clear();

	bool IsPresent(int value) const;
	int GetNumPresentValues() const;
	int GetCapacity() const;

	typename std::vector<int>::const_iterator begin() const;
	typename std::vector<int>::const_iterator end() const;
private:
	std::vector<int> present_values_;
	std::vector<bool> is_value_present_;
};
}//end namespace Pumpkin