#pragma once

#include "../Utilities/boolean_variable_internal.h"
#include "vector_object_indexed.h"

#include <vector>

namespace Pumpkin{

class DirectlyHashedBooleanVariableLabeller
{
public:
	DirectlyHashedBooleanVariableLabeller(int size); //values in the set must be within [0,.., size). Remember that usually the zero-th index will not be used.

	void AssignLabel(BooleanVariableInternal variable, char label);

	void Grow();
	void Resize(int new_size);
	void Clear();

	char GetLabel(BooleanVariableInternal) const;

	bool IsLabelled(BooleanVariableInternal) const;
	bool IsAssignedSpecificLabel(BooleanVariableInternal variable, char label); //returns true if the variable has been assigned 'label', and false otherwise. Note that the method returns false for all unlabelled variables.
	
	int GetNumLabelledVariables() const;
	int GetCapacity() const;

private:
	std::vector<BooleanVariableInternal> labelled_variables_;
	VectorObjectIndexed<BooleanVariableInternal, char> variable_labels_;

	const char UNLABELLED;
};
}