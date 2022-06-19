#pragma once

#include "Vec.h"
#include "../Propagators/Clausal/clause.h"

#include <vector>

namespace Pumpkin
{
class StandardClauseAllocator
{
public:

	Clause* CreateClause(vec<BooleanLiteral>& literals);
	void Clear(); //deletes all allocated clauses

private:
	std::vector<int*> allocated_data_;
};

inline Clause* StandardClauseAllocator::CreateClause(vec<BooleanLiteral>& literals)
{
	runtime_assert(literals.size() > 1);

	int num_ints_for_clause = Clause::NumBytesRequiredForClause(literals.size(), false) / sizeof(uint32_t);
	int* data = new int[num_ints_for_clause];
	Clause* new_clause = new (data) Clause(literals, false);
	allocated_data_.push_back(data);
	return new_clause;
}

inline void Pumpkin::StandardClauseAllocator::Clear()
{
	for (int* data : allocated_data_) { delete data; }
	allocated_data_.clear();
}

}