#include "directly_hashed_integer_set.h"
#include "runtime_assert.h"

namespace Pumpkin
{

DirectlyHashedIntegerSet::DirectlyHashedIntegerSet(int num_values) :
	is_value_present_(num_values, false)
{
}

void DirectlyHashedIntegerSet::Insert(int value)
{
	if (value >= GetCapacity()) { Resize(value + 1); }

	if (is_value_present_[value] == false)
	{
		is_value_present_[value] = true;
		present_values_.push_back(value);
	}
}

void DirectlyHashedIntegerSet::Grow()
{
	is_value_present_.push_back(false);
}

void DirectlyHashedIntegerSet::Resize(int new_size)
{
	runtime_assert(new_size >= GetCapacity());
	while (GetCapacity() < new_size) { Grow(); }
}

void DirectlyHashedIntegerSet::Clear()
{
	for (int value : present_values_) { is_value_present_[value] = false; }
	present_values_.clear();
}

bool DirectlyHashedIntegerSet::IsPresent(int value) const
{
	return value < is_value_present_.size() && is_value_present_[value];
}

int DirectlyHashedIntegerSet::GetNumPresentValues() const
{
	return int(present_values_.size());
}

int DirectlyHashedIntegerSet::GetCapacity() const
{
	return int(is_value_present_.size());
}

typename std::vector<int>::const_iterator DirectlyHashedIntegerSet::begin() const
{
	return present_values_.begin();
}

typename std::vector<int>::const_iterator DirectlyHashedIntegerSet::end() const
{
	return present_values_.end();
}

}
