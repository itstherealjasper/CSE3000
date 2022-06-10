#pragma once

#include "integer_variable.h"
#include "integer_assignment_vector.h"
#include "small_helper_structures.h"

#include <algorithm>
#include <set>

namespace Pumpkin
{
class LinearFunction
{
public:
	LinearFunction();

	int64_t Evaluate(const IntegerAssignmentVector& solution) const;
	int64_t NumTerms() const;
	int64_t GetConstantTerm() const;
	int64_t GetMinimumWeight() const;
	int64_t GetMaximumWeight() const;
	int64_t GetWeight(IntegerVariable) const;

	void AddTerm(IntegerVariable, int64_t weight);
	void AddConstantTerm(int64_t value);

	void Clear();
	void RemoveVariable(IntegerVariable);

	typename std::set<Term>::const_iterator begin() const; //iterators over the objective literals. Make sure not to change the weights while iterating, this will lead to undefined behaviour since removing/adding weights may remove/add/change some literals from the list of objective literals and then iterating does not make sense
	typename std::set<Term>::const_iterator end() const;
	
private:
	struct compare 
	{
		bool operator() (const Term &a, const Term &b) const 
		{
			return a.variable.id < b.variable.id;
		}
	};

	std::set<Term, compare> terms_;
	int64_t constant_term_;
};
}
