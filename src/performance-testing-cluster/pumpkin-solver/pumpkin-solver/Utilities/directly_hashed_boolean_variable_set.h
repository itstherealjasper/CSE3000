#pragma once

#include "boolean_literal.h"
#include "vector_object_indexed.h"

#include <vector>

namespace Pumpkin
{
//todo description

class DirectlyHashedBooleanVariableSet
{
public:
	void Insert(BooleanVariableInternal);
	void Remove(BooleanVariableInternal);
	void Clear();

	bool IsPresent(BooleanVariableInternal) const;
	int NumPresentValues() const;
	bool Empty() const;

	typename std::vector<BooleanVariableInternal>::const_iterator begin() const;
	typename std::vector<BooleanVariableInternal>::const_iterator end() const;
private:
	void Grow(BooleanVariableInternal); //grows internal data structures to accommodate for the new literal if necessary
	bool IsLiteralOutOfBounds(BooleanVariableInternal) const; //returns true if the literal cannot be inserted without growing the internal data structures

	std::vector<BooleanVariableInternal> present_variables_;
	VectorObjectIndexed<BooleanVariableInternal, int> variable_location_;
};
}//end namespace Pumpkin