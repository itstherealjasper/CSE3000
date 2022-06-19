#include "linear_boolean_function_complex.h"
#include "runtime_assert.h"

#include <algorithm>

namespace Pumpkin
{
LinearBooleanFunctionComplex::LinearBooleanFunctionComplex():
    constant_term_(0)
{}

int64_t LinearBooleanFunctionComplex::GetMaximumWeight() const
{
    if (weighted_literals_.Empty()) { return 0; }
    auto iter = std::max_element(
        begin(), 
        end(), 
        [&weights = weights_](auto& var1, auto& var2) { return abs(weights[var1]) < abs(weights[var2]); }
    );
    return abs(weights_[*iter]);
}

int64_t LinearBooleanFunctionComplex::GetMaximumWeightSmallerThanThreshold(int64_t threshold) const
{
    int64_t max_weight = 0;
    for (BooleanVariableInternal weighted_variable : weighted_literals_)
    {
        if (abs(weights_[weighted_variable]) < threshold)
        {
            max_weight = std::max(max_weight, abs(weights_[weighted_variable]));
        }
    }
    return max_weight;
}

std::vector<BooleanLiteral> LinearBooleanFunctionComplex::GetLiteralsWithWeightGreaterOrEqualToThreshold(int64_t threshold) const
{
    runtime_assert(threshold >= 0);

    std::vector<BooleanLiteral> literals;
    for (BooleanVariableInternal weighted_variable : weighted_literals_)
    {
        if (abs(weights_[weighted_variable]) >= threshold) 
        { 
            literals.push_back(BooleanLiteral(weighted_variable, weights_[weighted_variable] >= 0));
        }
    }
    return literals;
}

int64_t LinearBooleanFunctionComplex::GetWeight(BooleanLiteral literal)
{
    Grow(literal);
    return literal.IsPositive() ? weights_[literal.Variable()] : -weights_[literal.Variable()];
}

int64_t LinearBooleanFunctionComplex::GetWeight(BooleanLiteral literal) const
{
    if (literal.Variable().ToPositiveInteger() >= weights_.Size()) { return 0; } //the variable index is too large and has not been seen before
    return literal.IsPositive() ? weights_[literal.Variable()] : -weights_[literal.Variable()];
}

//add literal; remove literals; todo the names are not clear
void LinearBooleanFunctionComplex::IncreaseWeight(BooleanLiteral literal, int64_t weight)
{
    runtime_assert(weight > 0);

    BooleanVariableInternal variable = literal.Variable();

    Grow(literal);

    //update the weight and possibly the constant term
    if (weights_[variable] < 0 && literal.IsPositive())
    {
        int64_t constant_term_update = std::min(abs(weights_[variable]), weight);
        constant_term_ += constant_term_update;
        weights_[variable] += (weight - constant_term_update);
    }
    else if (weights_[variable] >= 0 && literal.IsPositive())
    {
        weights_[variable] += weight;
    }
    else if (weights_[variable] > 0 && literal.IsNegative())
    {
        int64_t constant_term_update = std::min(abs(weights_[variable]), weight);
        constant_term_ += constant_term_update;
        weights_[variable] -= (weight - constant_term_update);
    } 
    else //(weights_[literal.Variable()] <= 0 && literal.IsNegative())
    {
        weights_[variable] -= weight;
    }

    //ensure weighted variables are properly tracked
    if (weights_[variable] == 0)
    {
        runtime_assert(weighted_literals_.IsPresent(variable));
        weighted_literals_.Remove(variable);
    }

    if (abs(weights_[variable]) > 0 && !weighted_literals_.IsPresent(variable))
    {
        weighted_literals_.Insert(variable);
    }
}

void LinearBooleanFunctionComplex::DecreaseWeight(BooleanLiteral literal, int64_t weight)
{
    runtime_assert(weight > 0 && GetWeight(literal) >= weight); //for now this is always the case
    Grow(literal);
    
    if (literal.IsPositive())  { weights_[literal.Variable()] -= weight; }
    else/*is negative literal*/{ weights_[literal.Variable()] += weight; }

    if (weights_[literal.Variable()] == 0) { weighted_literals_.Remove(literal.Variable()); }
}

void LinearBooleanFunctionComplex::IncreaseConstantTerm(int64_t value)
{
    constant_term_ += value;
}

int64_t LinearBooleanFunctionComplex::GetMinimumWeightForLiterals(const std::vector<BooleanLiteral>& literals) const
{
    runtime_assert(!literals.empty());
    int64_t min_weight = GetWeight(literals[0]);
    for (int i = 1; i < literals.size(); i++)
    {
        min_weight = std::min(min_weight, GetWeight(literals[i]));
    }
    return min_weight;
}

int64_t LinearBooleanFunctionComplex::GetConstantTerm() const
{
    return constant_term_;
}

typename std::vector<BooleanVariableInternal>::const_iterator LinearBooleanFunctionComplex::begin() const
{
    return weighted_literals_.begin();
}

typename std::vector<BooleanVariableInternal>::const_iterator LinearBooleanFunctionComplex::end() const
{
    return weighted_literals_.end();
}

int64_t LinearBooleanFunctionComplex::ComputeSolutionCost(const BooleanAssignmentVector& solution)
{
    int64_t cost = constant_term_;
    for (BooleanVariableInternal variable : weighted_literals_)
    {
        if (weights_[variable] > 0 && solution[variable.index]
            || weights_[variable] < 0 && !solution[variable.index])
        {
            cost += weights_[variable];
        }
    }
    return cost;
}

void LinearBooleanFunctionComplex::Grow(BooleanLiteral lit)
{
    BooleanVariableInternal variable = lit.Variable();
    if (variable.index + 1 > weights_.Size()) { weights_.Resize(variable.index + 1, 0); }
}

}