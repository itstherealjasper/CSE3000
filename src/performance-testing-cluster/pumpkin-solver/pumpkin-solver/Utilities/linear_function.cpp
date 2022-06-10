#include "linear_function.h"
#include "runtime_assert.h"

#include <algorithm>
#include <iostream>

namespace Pumpkin
{

LinearFunction::LinearFunction():constant_term_(0) {}

int64_t LinearFunction::Evaluate(const IntegerAssignmentVector& solution) const
{
	int64_t function_value = constant_term_;
	for (Term term: (*this))
	{
		function_value += (term.weight * solution[term.variable]);
	}
	return function_value;
}

int64_t LinearFunction::NumTerms() const
{
	return int64_t(terms_.size());
}

int64_t LinearFunction::GetConstantTerm() const
{
	return constant_term_;
}

int64_t LinearFunction::GetMinimumWeight() const
{
	if (terms_.empty()) { return 0; }

	auto iter = std::min_element(terms_.begin(), terms_.end(), [](const Term& a, const Term& b) { return a.weight < b.weight; });
	return iter->weight;
}

int64_t LinearFunction::GetMaximumWeight() const
{
	if (terms_.empty()) { return 0; }

	auto iter = std::max_element(terms_.begin(), terms_.end(), [](const Term& a, const Term& b) { return a.weight < b.weight; });
	return iter->weight;
}

int64_t LinearFunction::GetWeight(IntegerVariable variable) const
{
	Term t;
	t.variable = variable;
	auto iter = terms_.find(t);	
	return iter == terms_.end() ? 0 : iter->weight;

}

void LinearFunction::AddTerm(IntegerVariable variable, int64_t weight)
{
	int64_t old_weight = GetWeight(variable);
	Term t;
	t.variable = variable;
	t.weight = weight + old_weight;

	//find the term if it is there
	auto iter = terms_.find(t);
	//remove it if it is present [not ideal implementation but okay for now]
	if (iter != terms_.end()) { terms_.erase(iter); } 
	//and possibly readd if needed
	if (t.weight != 0) { terms_.insert(t); }
}

void LinearFunction::AddConstantTerm(int64_t value)
{
	constant_term_ += value;
}

void LinearFunction::Clear()
{
	terms_.clear();
	constant_term_ = 0;
}

void LinearFunction::RemoveVariable(IntegerVariable variable)
{//inefficient, todo improve
	pumpkin_assert_simple(!variable.IsNull(), "Sanity check.");
	Term t;
	t.variable = variable;
	auto iter = terms_.find(t);
	runtime_assert(iter != terms_.end());
	terms_.erase(iter);
}

typename std::set<Term>::const_iterator Pumpkin::LinearFunction::begin() const
{
	return terms_.begin();
}

typename std::set<Term>::const_iterator LinearFunction::end() const
{
	return terms_.end();
}
}
