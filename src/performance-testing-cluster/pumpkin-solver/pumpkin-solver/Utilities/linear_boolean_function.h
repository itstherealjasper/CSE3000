#pragma once

#include "boolean_variable_internal.h"
#include "boolean_literal.h"
#include "directly_hashed_boolean_literal_set.h"
#include "vector_object_indexed.h"
#include "boolean_assignment_vector.h"
#include "pair_weight_literal.h"

namespace Pumpkin
{
class LinearBooleanFunction
{
public:
	LinearBooleanFunction();
	LinearBooleanFunction(const std::vector<PairWeightLiteral>& weighted_literals);

	int64_t GetWeight(BooleanLiteral);
	int64_t GetWeight(BooleanLiteral) const;
	int64_t GetConstantTerm() const;

	void IncreaseWeight(BooleanLiteral, int64_t weight);
	void DecreaseWeight(BooleanLiteral, int64_t weight);
	void AddLiteral(BooleanLiteral, int64_t weight);
	void RemoveLiteral(BooleanLiteral);
	void IncreaseConstantTerm(int64_t value);
	void ConvertIntoCanonicalForm();

	typename std::vector<BooleanLiteral>::const_iterator begin() const; //iterators over the objective literals. Make sure not to change the weights while iterating, this will lead to undefined behaviour since removing/adding weights may remove/add/change some literals from the list of objective literals and then iterating does not make sense
	typename std::vector<BooleanLiteral>::const_iterator end() const;

	int64_t GetMinimumWeight() const;
	int64_t GetMaximumWeight() const;
	int64_t GetMaximumWeightSmallerThanThreshold(int64_t threshold) const;
	std::vector<BooleanLiteral> GetLiteralsWithWeightGreaterOrEqualToThreshold(int64_t threshold) const;
	int64_t GetMinimumWeightForLiterals(const std::vector<BooleanLiteral>& literals) const; //returns the minimum weight among the literals. Assumes the input vector is nonempty
	int NumObjectiveLiterals() const;

	int64_t ComputeSolutionCost(const BooleanAssignmentVector& solution) const;

private:
	void Grow(BooleanLiteral); //grows internal data structures to accommodate the input literal

	int64_t constant_term_; //the constant term in the linear function, may appear because of conflicting literals or due to core-guided search
	VectorObjectIndexed<BooleanLiteral, int64_t> weights_; //maps literal -> weight; maybe literals have zero weight
	DirectlyHashedBooleanLiteralSet weighted_literals_; //keeps track of literals that have nonzero weight
};
}