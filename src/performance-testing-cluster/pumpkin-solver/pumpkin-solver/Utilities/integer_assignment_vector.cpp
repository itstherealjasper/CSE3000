#include "integer_assignment_vector.h"
#include "runtime_assert.h"

namespace Pumpkin
{
IntegerAssignmentVector::IntegerAssignmentVector():
	assignments_(1) //recall that index zero is reserved for null, indexing starts at 1
{
}
IntegerAssignmentVector::IntegerAssignmentVector(const std::vector<int>& solution) :
	assignments_(solution)
{
	runtime_assert(!solution.empty());
}
IntegerAssignmentVector::IntegerAssignmentVector(int num_variables, int default_value):
	assignments_(num_variables+1, default_value) //recall that index zero is reserved for null, indexing starts at 1
{
}

int IntegerAssignmentVector::operator[](IntegerVariable variable) const
{
	return assignments_[variable.id];
}

int& IntegerAssignmentVector::operator[](IntegerVariable variable)
{
	return assignments_[variable.id];
}

bool IntegerAssignmentVector::IsSatisfied(IntegerVariable variable, DomainOperation operation, int right_hand_side) const
{
	int assigned_value = assignments_[variable.id];

	if (operation.IsEquality())
	{
		return assigned_value == right_hand_side;
	}
	else if (operation.IsNotEqual())
	{
		return assigned_value != right_hand_side;
	}
	else if (operation.IsGreaterOrEqual())
	{
		return assigned_value >= right_hand_side;
	}
	else if (operation.IsLessOrEqual())
	{
		return assigned_value <= right_hand_side;
	}
	else
	{
		runtime_assert(1 == 2); //operation unsupported, error?
		return false;
	}
}

int64_t IntegerAssignmentVector::NumVariables() const
{
	return assignments_.size() - 1; //the zero index is not used
}

bool IntegerAssignmentVector::IsEmpty() const
{
	return assignments_.size() == 1;
}

}