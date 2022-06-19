#include "directly_hashed_boolean_variable_labeller.h"
#include "runtime_assert.h"

#include <limits>

namespace Pumpkin
{
Pumpkin::DirectlyHashedBooleanVariableLabeller::DirectlyHashedBooleanVariableLabeller(int size):
	variable_labels_(size),
	UNLABELLED(std::numeric_limits<char>::max())
{
}

void DirectlyHashedBooleanVariableLabeller::AssignLabel(BooleanVariableInternal variable, char label)
{
	assert(label != UNLABELLED);
	if (!IsLabelled(variable)) { labelled_variables_.push_back(variable); }
	variable_labels_[variable] = label;
}

void DirectlyHashedBooleanVariableLabeller::Grow()
{
	variable_labels_.push_back(UNLABELLED);
}

void DirectlyHashedBooleanVariableLabeller::Resize(int new_size)
{
	runtime_assert(new_size >= GetCapacity()); //for now we only allowing adding new entries (could be extended if needed)
	while (GetCapacity() < new_size) { Grow(); }
}

void DirectlyHashedBooleanVariableLabeller::Clear()
{
	for (BooleanVariableInternal variable : labelled_variables_) { variable_labels_[variable] = UNLABELLED; }
	labelled_variables_.clear();
}

bool DirectlyHashedBooleanVariableLabeller::IsLabelled(BooleanVariableInternal variable) const
{
	return variable_labels_[variable] != UNLABELLED;
}

bool DirectlyHashedBooleanVariableLabeller::IsAssignedSpecificLabel(BooleanVariableInternal variable, char label)
{
	assert(label != UNLABELLED);
	return variable_labels_[variable] == label;
}

char DirectlyHashedBooleanVariableLabeller::GetLabel(BooleanVariableInternal variable) const
{
	runtime_assert(IsLabelled(variable));
	return variable_labels_[variable];
}

int DirectlyHashedBooleanVariableLabeller::GetNumLabelledVariables() const
{
	return int(labelled_variables_.size());
}

int DirectlyHashedBooleanVariableLabeller::GetCapacity() const
{
	return int(variable_labels_.Size());
}

}
