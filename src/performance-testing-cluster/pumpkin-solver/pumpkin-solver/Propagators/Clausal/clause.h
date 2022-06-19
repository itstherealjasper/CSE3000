#pragma once

#include "../../Utilities/boolean_variable_internal.h"
#include "../../Utilities/boolean_literal.h"
#include "../../Utilities/small_helper_structures.h"
#include "../../Utilities/custom_vector.h"
#include "../../Utilities/Vec.h"
#include "../../Utilities/pumpkin_assert.h"

#include <vector>

namespace Pumpkin
{

class SolverState; //circular dependency
class LinearClauseAllocator;
class StandardClauseAllocator;

/*The class represents a clause_ with the two-watch scheme that helps detecting binary clause_ propagation.
todo more explanation on what it does
*/

class Clause
{
public:
	~Clause() { runtime_assert(1 == 2); }

	//getter methods---------------------------------
	uint32_t Size() const;
	bool IsBinaryClause() const;
	uint32_t GetLBD() const;
	float GetActivity(); //This method should only be called for learned clauses. The activity of a clause is used when removing learned clauses. 
	bool HasLBDUpdateProtection() const;
	bool IsLearned() const;
	bool IsDeleted() const;
	bool IsRelocated() const;

	//setter methods---------------------------------
	void MarkDeleted();
	void MarkLBDProtection();
	void ClearLBDProtection();
	void MarkRelocated();
	void SetLBD(uint32_t new_lbd);
	void SetActivity(float new_value); //This method should only be called for learned clauses.
	void ShrinkToSize(int new_size);

	const BooleanLiteral* begin() const; //should return const pointer? not sure
	const BooleanLiteral* end() const;

	BooleanLiteral& operator[](int index);
	BooleanLiteral operator[](int index) const;

	uint32_t NumBytesUsed();
	
private:

	friend class LinearClauseAllocator;
	friend class StandardClauseAllocator;

	//the constructor is private since only clause allocator classes should be able to create clauses
	//	otherwise the inlined literals will not be allocated space
	Clause(vec<BooleanLiteral>& literals, bool is_learned); 
	static uint32_t NumBytesRequiredForClause(uint32_t num_literals, bool is_learned);

	//the activity of the clause is not explicitly declared. The activity is stored only for learned clauses right after the inlined literals. Use GetActivity() to access it.
	uint32_t lbd_ : 28;
	uint32_t lbd_update_protection_ : 1;
	uint32_t is_learned_ : 1;
	uint32_t is_deleted_ : 1;
	uint32_t is_relocated_ : 1;
	uint32_t size_;
	BooleanLiteral literals_[0];
};

inline uint32_t Clause::Size() const
{
	return size_;
}

inline uint32_t Clause::GetLBD() const
{
	pumpkin_assert_moderate(IsLearned(), "Sanity check.");
	return lbd_;
}

inline bool Clause::IsBinaryClause() const
{
	return size_ == 2;
}

inline bool Clause::IsLearned() const
{
	return is_learned_;
}

inline bool Clause::HasLBDUpdateProtection() const
{
	return lbd_update_protection_;
}

inline bool Clause::IsDeleted() const
{
	return is_deleted_;
}

inline bool Clause::IsRelocated() const
{
	return is_relocated_;
}

inline void Clause::MarkDeleted()
{
	pumpkin_assert_moderate(!IsDeleted(), "Sanity check.");
	is_deleted_ = 1;
}

inline void Clause::MarkLBDProtection()
{
	lbd_update_protection_ = 1;
}

inline void Clause::ClearLBDProtection()
{
	lbd_update_protection_ = 0;
}

inline void Clause::MarkRelocated()
{
	is_relocated_ = 1;
}

inline void Clause::SetLBD(uint32_t new_lbd)
{
	pumpkin_assert_moderate(new_lbd < lbd_, "Sanity check."); //usually we only update the lbd if it is better, potentially a bug if this assert triggers
	lbd_ = new_lbd;
}

inline float Clause::GetActivity()
{
	pumpkin_assert_moderate(is_learned_, "Sanity check.");
	return (*reinterpret_cast<float*>(&literals_[size_]));
}

inline void Clause::SetActivity(float new_value)
{
	pumpkin_assert_moderate(is_learned_, "Sanity check.");
	(*reinterpret_cast<float*>(&literals_[size_])) = new_value;
}

inline void Clause::ShrinkToSize(int new_size)
{
	pumpkin_assert_simple(new_size >= 2 && new_size <= size_, "Sanity check.");
	if (new_size < size_)
	{
		//it is important to make the distinction between learnt and non-learnt clauses
		//	this is because learnt clauses have its activity value stored right after the last literal which needs to be taken into account
		if (IsLearned())
		{
			float activity = GetActivity(); //read activity before resizing, after resizing the activity will be lost
			size_ = new_size;
			SetActivity(activity);
		}
		else
		{
			size_ = new_size;
		}
	}
}

inline const BooleanLiteral* Clause::begin() const
{
	return &literals_[0];
}

inline const BooleanLiteral* Clause::end() const
{
	return &literals_[0] + size_;
}

inline BooleanLiteral& Clause::operator[](int index)
{
	return literals_[index];
}

inline BooleanLiteral Clause::operator[](int index) const
{
	return literals_[index];
}

inline uint32_t Clause::NumBytesUsed()
{
	return NumBytesRequiredForClause(size_, is_learned_);
}

inline uint32_t Clause::NumBytesRequiredForClause(uint32_t num_literals, bool is_learned)
{
	return sizeof(BooleanLiteral) * is_learned  //only learned clauses have 'activity'
		+ sizeof(Clause) + (num_literals * sizeof(BooleanLiteral));
}

inline Clause::Clause(vec<BooleanLiteral>& literals, bool is_learned)
{
	pumpkin_assert_simple(literals.size() > 1, "Unit clauses cannot be allocated.");

	lbd_ = literals.size();
	lbd_update_protection_ = 0;
	is_learned_ = is_learned;
	is_deleted_ = 0;
	is_relocated_ = 0;
	size_ = literals.size();
	for (uint32_t i = 0; i < literals.size(); i++) { literals_[i] = literals[i]; }
	if (is_learned) { SetActivity(0.0); } //make sure to make this call only after the size has been properly set in the clause
}

} //end Pumpkin namespace