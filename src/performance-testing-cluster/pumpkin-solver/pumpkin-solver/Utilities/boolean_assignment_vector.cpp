#include "boolean_assignment_vector.h"
#include "runtime_assert.h"

namespace Pumpkin
{
BooleanAssignmentVector::BooleanAssignmentVector():
    assignments_(1, false) //empty since the 0th entry is not used
{
}

BooleanAssignmentVector::BooleanAssignmentVector(const std::vector<bool>& solution):
assignments_(solution)
{
    runtime_assert(assignments_.size() > 0); //index zero is not used, so the vector has to have size at least one
}

BooleanAssignmentVector::BooleanAssignmentVector(int num_variables, bool default_value):
    assignments_(size_t(num_variables)+1, default_value)
{
}

bool BooleanAssignmentVector::operator[](BooleanLiteral literal) const
{
    runtime_assert(!literal.IsUndefined());
    return (literal.IsPositive() ? assignments_.at(literal.VariableIndex()) : !assignments_.at(literal.VariableIndex()));
}

void BooleanAssignmentVector::AssignLiteral(BooleanLiteral literal, bool value)
{
    runtime_assert(!literal.IsUndefined());
    literal.IsPositive() ? assignments_.at(literal.VariableIndex()) = value : assignments_.at(literal.VariableIndex()) = !value;
}

bool BooleanAssignmentVector::operator[](BooleanVariableInternal variable) const
{
    runtime_assert(!variable.IsUndefined());
    return assignments_.at(variable.index);
}

std::vector<bool>::reference BooleanAssignmentVector::operator[](BooleanVariableInternal variable)
{
    runtime_assert(!variable.IsUndefined());
    return assignments_.at(variable.index);
}

bool BooleanAssignmentVector::operator[](int variable_index) const
{
    runtime_assert(variable_index > 0); //remember the 0th index is not used, variable indicies start at one
    return assignments_.at(variable_index);
}

std::vector<bool>::reference BooleanAssignmentVector::operator[](int variable_index)
{
    runtime_assert(variable_index > 0); //remember the 0th index is not used, variable indicies start at one
    return assignments_.at(variable_index);
}

void BooleanAssignmentVector::Grow(bool default_value)
{
    assignments_.push_back(default_value);
}

int64_t BooleanAssignmentVector::NumVariables() const
{
    return assignments_.size() - 1;
}

bool BooleanAssignmentVector::IsEmpty() const
{
    return NumVariables() == 0;
}
}
