#pragma once

#include "../Propagators/Clausal/clause.h"
#include "../Utilities/pumpkin_assert.h"
#include "../Utilities/Vec.h"

#include <vector>
#include <stdint.h>
#include <iostream>

namespace Pumpkin
{
struct ClauseLinearReference
{
	ClauseLinearReference() :id(0) {}
	explicit ClauseLinearReference(uint32_t id) :id(id) {}
	bool operator==(const ClauseLinearReference clause_reference) { return this->id == clause_reference.id; }
	bool IsNull() const { return id == 0; }
	void Nullify() { id = 0; }

	uint32_t id;
};

class LinearClauseAllocator
{
public:
	LinearClauseAllocator(uint32_t num_integers_to_preallocate);

	ClauseLinearReference CreateClause(vec<BooleanLiteral>&literals, bool is_learned);	
	ClauseLinearReference CreateClauseByCopying(Clause* clause);
	void DeleteClause(ClauseLinearReference clause_reference);
	
	Clause* GetClausePointer(ClauseLinearReference clause_reference);	
	Clause& GetClause(ClauseLinearReference clause_reference);

	int32_t GetDeletedSpaceUsage() const;
	int32_t GetNumAllocatedIntegersTotal() const;
	int32_t GetNumCurrentUsedIntegers() const;

	void Clear();

	void SetLimit(uint64_t limit);

private:

	void ResizeToFit(uint64_t new_size);

	uint64_t limit_; //indicates the upper limit on the number of integers that are allowed to be allocated. This artificial limit is used to make sure the clause pointers do not overtake the propagator id counter. TODO explain better.
	uint32_t* data_;
	uint32_t next_location_;
	uint32_t allocated_ints_;
	uint32_t deleted_clause_space_usage_;
};

inline LinearClauseAllocator::LinearClauseAllocator(uint32_t num_integers_to_preallocate)
{
	data_ = new uint32_t[num_integers_to_preallocate];
	next_location_ = 1; //the location starts from one since the pointer to zero is kept to denote the null pointer
	allocated_ints_ = num_integers_to_preallocate;
	deleted_clause_space_usage_ = 0;
	limit_ = UINT32_MAX;
}

inline ClauseLinearReference LinearClauseAllocator::CreateClause(vec<BooleanLiteral>& literals, bool is_learned)
{//assumes the order of the literals is properly set...
	pumpkin_assert_permanent(literals.size() > 1, "Error: creating clause with less than two literals.");

	int num_ints_for_clause = Clause::NumBytesRequiredForClause(literals.size(), is_learned) / sizeof(uint32_t);
	
	ResizeToFit(uint64_t(next_location_) + uint64_t(num_ints_for_clause));

	Clause* new_clause = new (&data_[next_location_]) Clause(literals, is_learned);
	ClauseLinearReference new_clause_reference(next_location_);
	next_location_ += num_ints_for_clause;

	return new_clause_reference;
}

inline ClauseLinearReference LinearClauseAllocator::CreateClauseByCopying(Clause* clause)
{
	pumpkin_assert_permanent(clause->Size() > 1, "Error: creating clause with less than two literals.");
	pumpkin_assert_simple(!clause->IsDeleted(), "Cannot copy a deleted clause.");

	int num_bytes_required = Clause::NumBytesRequiredForClause(clause->Size(), clause->IsLearned());
	int num_ints_for_clause = num_bytes_required / sizeof(uint32_t);
	
	ResizeToFit(uint64_t(next_location_) + uint64_t(num_ints_for_clause));

	memcpy(&data_[next_location_], clause, num_bytes_required);

	ClauseLinearReference new_clause_reference(next_location_);
	next_location_ += num_ints_for_clause;

	return new_clause_reference;
}

inline void LinearClauseAllocator::DeleteClause(ClauseLinearReference clause_reference)
{
	Clause& clause = GetClause(clause_reference);
	runtime_assert(!clause.is_deleted_);
	clause.MarkDeleted();
	deleted_clause_space_usage_ += Clause::NumBytesRequiredForClause(clause.Size(), clause.IsLearned()) / sizeof(uint32_t);
}

inline Clause* LinearClauseAllocator::GetClausePointer(ClauseLinearReference clause_reference)
{
	assert(clause_reference.id < next_location_);
	return (Clause*)(&data_[clause_reference.id]);
}

inline Clause& LinearClauseAllocator::GetClause(ClauseLinearReference clause_reference)
{
	pumpkin_assert_moderate(clause_reference.id < next_location_ && !clause_reference.IsNull(), "Sanity check.");
	return (*(Clause*)(&data_[clause_reference.id]));
}

inline int32_t LinearClauseAllocator::GetDeletedSpaceUsage() const
{
	return deleted_clause_space_usage_;
}

inline int32_t LinearClauseAllocator::GetNumAllocatedIntegersTotal() const
{
	return allocated_ints_;
}

inline int32_t LinearClauseAllocator::GetNumCurrentUsedIntegers() const
{
	return next_location_;
}

inline void LinearClauseAllocator::Clear()
{
	next_location_ = 1;
	deleted_clause_space_usage_ = 0;
}

inline void LinearClauseAllocator::SetLimit(uint64_t limit)
{
	runtime_assert(limit >= allocated_ints_); //this is probably never going to trigger, but nevertheless this should be handled better
	limit_ = limit;
}

inline void LinearClauseAllocator::ResizeToFit(uint64_t new_size)
{
	//uint64_t(next_location_) + uint64_t(num_ints_for_clause)
	pumpkin_assert_permanent(new_size <= limit_, "Error: memory limit for clauses exceeded (quite unusual, probably something else went wrong)."); //make sure the artificial limit is never exceeded, todo think about better ways than just crashing

	//if there is not enough allocated memory, allocate more
	if (new_size > allocated_ints_)
	{		
		uint64_t new_num_allocated_ints = std::max(2 * uint64_t(allocated_ints_), new_size); //doubling could be excessive but ok for now
		new_num_allocated_ints = std::min(new_num_allocated_ints, limit_); //cap the number if it goes overboard; at this point we know we are not exceeding limit by the requirement of the clause (see assert above), but it could be that doubling made this large memory requirement
		uint32_t* new_data = new uint32_t[new_num_allocated_ints];
		memcpy(new_data, data_, sizeof(uint32_t) * allocated_ints_);
		delete[] data_;
		data_ = new_data;
		allocated_ints_ = new_num_allocated_ints;
	}
}

}