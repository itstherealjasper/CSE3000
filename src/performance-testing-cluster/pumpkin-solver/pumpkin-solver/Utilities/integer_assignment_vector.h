#pragma once

#include "boolean_variable_internal.h"
#include "boolean_literal.h"
#include "integer_variable.h"
#include "domain_operation.h"

#include <vector>

namespace Pumpkin
{
class IntegerAssignmentVector
{
public:
	IntegerAssignmentVector();
	IntegerAssignmentVector(const std::vector<int>& solution);
	explicit IntegerAssignmentVector(int num_variables, int default_value = 0);

	int operator[](IntegerVariable) const;
	int& operator[](IntegerVariable);

	bool IsSatisfied(IntegerVariable, DomainOperation, int right_hand_side) const;

	/*bool operator[](BooleanLiteral) const;
	void AssignLiteral(BooleanLiteral, bool);
	bool operator[](BooleanVariable) const;
	std::vector<bool>::reference operator[](BooleanVariable);
	bool operator[](int variable_index) const;
	std::vector<bool>::reference operator[](int variable_index);*/

	//void Grow(int default_value = 0);

	//bool IsAssigned(IntegerVariable) const;
	//int GetAssignedValue(IntegerVariable) const;
	int64_t NumVariables() const;
	bool IsEmpty() const;

private:
	std::vector<int> assignments_; //[i] is the assignment of the integer variable with id 'i'. Note that [0] is not used.
};
}