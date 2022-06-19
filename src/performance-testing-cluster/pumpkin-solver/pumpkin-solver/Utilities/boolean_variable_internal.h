#pragma once

#include "pumpkin_assert.h"

#include <stdlib.h>
#include <stdint.h>

namespace Pumpkin
{

class BooleanLiteral;
class SolverState;

//A class representing a Boolean variable. This class is mainly introduced for clarity, e.g. to avoid misusing integers, learned_clause_literals, and variables as one another
class BooleanVariableInternal
{
public:
	BooleanVariableInternal(); // create an undefined variable
	BooleanVariableInternal &operator=(BooleanVariableInternal);

	bool IsUndefined() const; //returns true or false if the variable is considered undefined. Internally a special code_ for undefined variables is kept to distinguish it, i.e. the never-used index zero variable

	bool operator==(BooleanVariableInternal); //compares if two variables are identical
	bool operator!=(BooleanVariableInternal);

	uint32_t ToPositiveInteger() const; //used for the VectorObjectIndexed

	static BooleanVariableInternal UndefinedVariable();

	uint32_t has_cp_propagators_attached : 1;
	uint32_t index : 31;

	friend class BooleanLiteral;
	friend class SolverState;

private:
	explicit BooleanVariableInternal(uint32_t index, bool has_cp_propagators_attached); //index > 0 must be, only friend classes can create internal Booleans. This is done to ensure the 'has_cp_propagators' bit is correctly set.
};

inline BooleanVariableInternal::BooleanVariableInternal()
	:has_cp_propagators_attached(0), index(0)
{
}

inline BooleanVariableInternal::BooleanVariableInternal(uint32_t index, bool has_cp_propagators_attached)
	: has_cp_propagators_attached(has_cp_propagators_attached), index(index)
{
	pumpkin_assert_moderate(index > 0, "Sanity check.");
}

inline BooleanVariableInternal& BooleanVariableInternal::operator=(BooleanVariableInternal variable)
{
	this->index = variable.index;
	this->has_cp_propagators_attached = variable.has_cp_propagators_attached;
	return *this;
}

inline bool BooleanVariableInternal::IsUndefined() const
{
	return index == 0;
}

inline bool BooleanVariableInternal::operator==(BooleanVariableInternal variable)
{
	pumpkin_assert_moderate(!(index == variable.index && has_cp_propagators_attached != variable.has_cp_propagators_attached), "Sanity check.");
	return index == variable.index;
}

inline bool BooleanVariableInternal::operator!=(BooleanVariableInternal variable)
{
	pumpkin_assert_moderate(!(index == variable.index && has_cp_propagators_attached != variable.has_cp_propagators_attached), "Sanity check.");
	return index != variable.index;
}

inline uint32_t BooleanVariableInternal::ToPositiveInteger() const
{
	return index;
}

inline BooleanVariableInternal BooleanVariableInternal::UndefinedVariable()
{
	return BooleanVariableInternal();
}

} //end Pumpkin namespace