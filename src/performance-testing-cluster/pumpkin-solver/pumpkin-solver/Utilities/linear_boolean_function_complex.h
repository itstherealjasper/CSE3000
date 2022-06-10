#pragma once

#include "boolean_variable_internal.h"
#include "boolean_literal.h"
#include "directly_hashed_boolean_variable_set.h"
#include "vector_object_indexed.h"
#include "boolean_assignment_vector.h"

#include <vector>
#include <map>

namespace Pumpkin
{

class LinearBooleanFunctionComplex
{
public:
	LinearBooleanFunctionComplex();

	int64_t GetMaximumWeight() const;
	int64_t GetMaximumWeightSmallerThanThreshold(int64_t threshold) const;
	std::vector<BooleanLiteral> GetLiteralsWithWeightGreaterOrEqualToThreshold(int64_t threshold) const;

	int64_t GetWeight(BooleanLiteral);
	int64_t GetWeight(BooleanLiteral) const;	
	int64_t GetConstantTerm() const;
	int64_t GetMinimumWeightForLiterals(const std::vector<BooleanLiteral>& literals) const; //returns the minimum weight among the literals. Assumes the input vector is nonempty

	void IncreaseWeight(BooleanLiteral, int64_t);
	void DecreaseWeight(BooleanLiteral, int64_t);
	void IncreaseConstantTerm(int64_t value);
		
	typename std::vector<BooleanVariableInternal>::const_iterator begin() const; //iterators over the objective literals
	typename std::vector<BooleanVariableInternal>::const_iterator end() const;

	int64_t ComputeSolutionCost(const BooleanAssignmentVector& solution);

private:
	void Grow(BooleanLiteral); //grows internal data structures to accommodate the input literal

	int64_t constant_term_;
	VectorObjectIndexed<BooleanVariableInternal, int64_t> weights_;
	DirectlyHashedBooleanVariableSet weighted_literals_;
};

}