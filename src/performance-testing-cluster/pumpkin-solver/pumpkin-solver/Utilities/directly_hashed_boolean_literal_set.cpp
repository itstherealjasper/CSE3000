#include "directly_hashed_boolean_literal_set.h"
#include "runtime_assert.h"

namespace Pumpkin
{
void DirectlyHashedBooleanLiteralSet::Insert(BooleanLiteral lit)
{
	runtime_assert(!IsPresent(lit)); //for now we are assuming that we will never add a literal that is already present
	Grow(lit);
	literal_location_[lit] = (present_literals_.size());
	present_literals_.push_back(lit);
}

void DirectlyHashedBooleanLiteralSet::Remove(BooleanLiteral lit)
{
	runtime_assert(IsPresent(lit));
	//we move the last literal in the 'present_literals' vector to the position of the removed literal
	//and then decrease the size of the vector by one
	literal_location_[present_literals_.back()] = literal_location_[lit];
	present_literals_[literal_location_[lit]] = present_literals_.back();
	present_literals_.pop_back();
	literal_location_[lit] = -1;
}

void DirectlyHashedBooleanLiteralSet::Clear()
{
	for (BooleanLiteral literal : present_literals_) { literal_location_[literal] = -1; }
	present_literals_.clear();
}

bool DirectlyHashedBooleanLiteralSet::IsPresent(BooleanLiteral lit) const
{
	return !IsLiteralOutOfBounds(lit) && literal_location_[lit] != -1;
}

int DirectlyHashedBooleanLiteralSet::NumPresentValues() const
{
	return int(present_literals_.size());
}

bool DirectlyHashedBooleanLiteralSet::Empty() const
{
	return NumPresentValues() == 0;
}

typename std::vector<BooleanLiteral>::const_iterator DirectlyHashedBooleanLiteralSet::begin() const
{
	return present_literals_.begin();
}

typename std::vector<BooleanLiteral>::const_iterator DirectlyHashedBooleanLiteralSet::end() const
{
	return present_literals_.end();
}

void DirectlyHashedBooleanLiteralSet::Grow(BooleanLiteral lit)
{
	size_t new_size = std::max(size_t(lit.ToPositiveInteger() + 1), size_t((~lit).ToPositiveInteger() + 1));
	if (new_size > literal_location_.Size()) { literal_location_.Resize(new_size, -1); }
}

bool DirectlyHashedBooleanLiteralSet::IsLiteralOutOfBounds(BooleanLiteral lit) const
{
	return lit.ToPositiveInteger() >= literal_location_.Size();
}
}
