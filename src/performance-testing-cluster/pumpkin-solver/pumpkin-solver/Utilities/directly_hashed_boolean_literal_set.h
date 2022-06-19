#pragma once

#include "boolean_literal.h"
#include "vector_object_indexed.h"

#include <vector>

namespace Pumpkin
{
//todo description

class DirectlyHashedBooleanLiteralSet
{
public:
	void Insert(BooleanLiteral);
	void Remove(BooleanLiteral);
	void Clear();

	bool IsPresent(BooleanLiteral) const;
	int NumPresentValues() const;
	bool Empty() const;

	typename std::vector<BooleanLiteral>::const_iterator begin() const;
	typename std::vector<BooleanLiteral>::const_iterator end() const;
private:
	void Grow(BooleanLiteral); //grows internal data structures to accommodate for the new literal if necessary
	bool IsLiteralOutOfBounds(BooleanLiteral) const; //returns true if the literal cannot be inserted without growing the internal data structures

	std::vector<BooleanLiteral> present_literals_;
	VectorObjectIndexed<BooleanLiteral, int> literal_location_;
};
}//end namespace Pumpkin