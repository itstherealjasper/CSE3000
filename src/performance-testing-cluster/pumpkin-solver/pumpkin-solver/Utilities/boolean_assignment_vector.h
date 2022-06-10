#pragma once

#include "boolean_variable_internal.h"
#include "boolean_literal.h"

#include <vector>

namespace Pumpkin
{
class BooleanAssignmentVector
{
public:
	BooleanAssignmentVector();
	BooleanAssignmentVector(const std::vector<bool>& solution);
	explicit BooleanAssignmentVector(int num_variables, bool default_value = false);

	bool operator[](BooleanLiteral) const;
	void AssignLiteral(BooleanLiteral, bool);
	bool operator[](BooleanVariableInternal) const;
	std::vector<bool>::reference operator[](BooleanVariableInternal);
	bool operator[](int variable_index) const;
	std::vector<bool>::reference operator[](int variable_index);

	void Grow(bool default_value = false);

	int64_t NumVariables() const;
	bool IsEmpty() const;

private:
	std::vector<bool> assignments_;
};
}