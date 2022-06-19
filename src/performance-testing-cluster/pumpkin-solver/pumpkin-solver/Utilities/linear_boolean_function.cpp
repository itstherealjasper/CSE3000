#include "linear_boolean_function.h"
#include "runtime_assert.h"

namespace Pumpkin
{
int64_t LinearBooleanFunction::GetWeight(BooleanLiteral literal)
{
	Grow(literal);
	return weights_[literal];
}

int64_t LinearBooleanFunction::GetWeight(BooleanLiteral literal) const
{
	if (literal.ToPositiveInteger() >= weights_.Size()) { return 0; } //if literal has not been registered yet
	return weights_[literal];
}

int64_t LinearBooleanFunction::GetConstantTerm() const
{
	return constant_term_;
}

void LinearBooleanFunction::IncreaseWeight(BooleanLiteral literal, int64_t weight)
{
	runtime_assert(weight != 0);

	Grow(literal);
	weights_[literal] += weight;
	if (weights_[literal] == 0) { weighted_literals_.Remove(literal); }
	else if (weights_[literal] > 0 && !weighted_literals_.IsPresent(literal)) { weighted_literals_.Insert(literal); }
	runtime_assert(weights_[literal] >= 0);
}

void LinearBooleanFunction::DecreaseWeight(BooleanLiteral literal, int64_t weight)
{
	IncreaseWeight(literal, -weight);
}

void LinearBooleanFunction::AddLiteral(BooleanLiteral literal, int64_t weight)
{
	runtime_assert(GetWeight(literal) == 0);
	IncreaseWeight(literal, weight);
}

void LinearBooleanFunction::RemoveLiteral(BooleanLiteral literal)
{
	int64_t weight = GetWeight(literal);
	DecreaseWeight(literal, weight);
}

void LinearBooleanFunction::IncreaseConstantTerm(int64_t value)
{
	runtime_assert(value >= 0); //for now we never decrease the value
	constant_term_ += value;
}

typename std::vector<BooleanLiteral>::const_iterator LinearBooleanFunction::begin() const
{
	return weighted_literals_.begin();
}

typename std::vector<BooleanLiteral>::const_iterator LinearBooleanFunction::end() const
{
	return weighted_literals_.end();
}

LinearBooleanFunction::LinearBooleanFunction() :
	constant_term_(0)
{
}

LinearBooleanFunction::LinearBooleanFunction(const std::vector<PairWeightLiteral>& weighted_literals)
{
	for (auto wl : weighted_literals) { IncreaseWeight(wl.literal, wl.weight); }
	ConvertIntoCanonicalForm();
}

void LinearBooleanFunction::ConvertIntoCanonicalForm()
{
	//this method is done in two parts: collect noncanonical variables, and then make the form canonical

	//collect noncanonical variables
	std::vector<BooleanVariableInternal> noncanonical_variables;
	for (BooleanLiteral literal: weighted_literals_)
	{
		if (weights_[literal] > 0 && weights_[~literal] > 0) 
		{ 
			noncanonical_variables.push_back(literal.Variable()); 
		}
	}
	//make the canonical form
	for (BooleanVariableInternal variable : noncanonical_variables)
	{
		BooleanLiteral positive_literal(variable, true);
		BooleanLiteral negative_literal(variable, false);

		if (weights_[positive_literal] > 0 && weights_[negative_literal] > 0) //need to make this check since the same variable will appear twice in noncanonical_variables
		{
			int64_t min_value = std::min(weights_[positive_literal], weights_[negative_literal]);
			constant_term_ += min_value;
			DecreaseWeight(positive_literal, min_value);
			DecreaseWeight(negative_literal, min_value);
		}
	}
}

int64_t LinearBooleanFunction::GetMinimumWeight() const
{
	int64_t min_weight = GetMaximumWeight();
	for (BooleanLiteral literal : weighted_literals_) { min_weight = std::min(weights_[literal], min_weight); }
	return min_weight;
}

int64_t LinearBooleanFunction::GetMaximumWeight() const
{
	int64_t max_weight = 0;
	for (BooleanLiteral literal : weighted_literals_) { max_weight = std::max(weights_[literal], max_weight); }
	return max_weight;
}

int64_t LinearBooleanFunction::GetMaximumWeightSmallerThanThreshold(int64_t threshold) const
{
	int64_t max_weight = 0;
	for (BooleanLiteral literal : weighted_literals_)
	{
		if (weights_[literal] < threshold) { max_weight = std::max(weights_[literal], max_weight); }
	}
	return max_weight;
}

std::vector<BooleanLiteral> LinearBooleanFunction::GetLiteralsWithWeightGreaterOrEqualToThreshold(int64_t threshold) const
{
	static std::vector<BooleanLiteral> chosen_literals;
	chosen_literals.clear();
	for (BooleanLiteral literal : weighted_literals_)
	{
		if (weights_[literal] >= threshold) { chosen_literals.push_back(literal); }
	}
	return chosen_literals;
}

int64_t LinearBooleanFunction::GetMinimumWeightForLiterals(const std::vector<BooleanLiteral>& literals) const
{
	runtime_assert(!literals.empty());
	int64_t min_weight = weights_[literals[0]];
	for (size_t i = 1; i < literals.size(); i++) { min_weight = std::min(weights_[literals[i]], min_weight); }
	return min_weight;
}

int LinearBooleanFunction::NumObjectiveLiterals() const
{
	return weighted_literals_.NumPresentValues();
}

int64_t LinearBooleanFunction::ComputeSolutionCost(const BooleanAssignmentVector& solution) const
{
	int64_t cost = constant_term_;
	for (BooleanLiteral literal : weighted_literals_) { cost += (solution[literal] * weights_[literal]); }
	return cost;
}

void LinearBooleanFunction::Grow(BooleanLiteral literal)
{
	size_t required_size = 1 + std::max(literal.ToPositiveInteger(), (~literal).ToPositiveInteger());
	if (required_size >= weights_.Size()) { weights_.Resize(required_size, 0); }
}
}
