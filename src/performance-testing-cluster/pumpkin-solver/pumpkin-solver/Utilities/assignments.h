#pragma once

#include "boolean_variable_internal.h"
#include "boolean_literal.h"
#include "small_helper_structures.h"
#include "simplified_vector.h"
#include "Vec.h"

#include <vector>
#include <assert.h>

namespace Pumpkin
{

class Assignments
{
public:
	Assignments(size_t number_of_variables);

	bool IsUnassigned(BooleanVariableInternal) const;
	bool IsUnassigned(BooleanLiteral) const;
	bool IsAssigned(BooleanVariableInternal) const; //reports whether the Boolean variable has been assigned a value (false if undefined, otherwise true)
	bool IsAssigned(BooleanLiteral) const; //report whether the literal has been assigned a value (false if not, other true)
	bool IsInternalBooleanVariableAssigned(int32_t index) const;
	bool IsAssignedTrue(BooleanLiteral) const; //reports whether the literal has been assigned true. If the literal is unassigned or evaluates to false, the method returns false.
	bool IsAssignedTrue(BooleanVariableInternal) const; //reports whether the variable has been assigned true. If the literal is unassigned or evaluates to false, the method returns false.
	bool IsAssignedFalse(BooleanLiteral) const; //reports whether the literal has been assigned false. If the literal is unassigned or evaluates to true, the method returns false 
	bool IsDecision(BooleanVariableInternal) const;//reports whether the variable has been assigned as a result of a decision.
	bool IsDecision(BooleanLiteral) const;
	bool IsPropagated(BooleanVariableInternal) const;//reports if the variable has been assigned as a result of a propagation rather than a decision
	bool IsRootAssignment(BooleanLiteral) const; //reports whether the literal has been set at the root (level 0), either by propagation or as a unit clause

	BooleanLiteral GetAssignment(const BooleanVariableInternal) const; //return the literal of the corresponding variable that has been assigned 'true'. Assumes the variable has been assigned a value, i.e. it is not undefined.
	bool GetAssignment(BooleanLiteral) const; //returns the truth value (true, false) assigned to the literal. It assumes the corresponding Boolean variable has been assigned a value, i.e. it is not undefined.
	int  GetAssignmentLevel(BooleanVariableInternal) const; //return the level when the variable was assigned a value. Assumes the variables has been assigned a value. 
	int  GetAssignmentLevel(BooleanLiteral) const; //return the level when the variable was assigned a value. Assumes the variables has been assigned a value.
	uint32_t GetAssignmentReasonCode(BooleanVariableInternal) const;

	void MakeAssignment(BooleanLiteral true_literal, int decision_level, uint64_t reason_code);
	void UnassignVariable(BooleanVariableInternal variable);

	size_t GetNumberOfInternalBooleanVariables() const;

	void Grow();

private:
	vec<uint8_t> truth_values_; //[i] is the truth value assignment for the i-th variable with the codes: 2 - unassigned; 1 - true; 0 - false
	vec<AuxiliaryAssignmentInfo> info_; //stores additional information about the assignments
};

inline Assignments::Assignments(size_t number_of_variables) :
	truth_values_(number_of_variables + 1, 2),// number_of_variables+1),
	info_(number_of_variables + 1) //remember variable indexing goes from 1 and not 0
{
}

inline bool Assignments::IsUnassigned(BooleanVariableInternal variable) const
{
	return !IsAssigned(variable);
}

inline bool Assignments::IsUnassigned(BooleanLiteral literal) const
{
	return !IsAssigned(literal);
}

inline bool Assignments::IsAssigned(BooleanVariableInternal variable) const
{
	return truth_values_[variable.index] != 2;
}

inline bool Assignments::IsAssigned(BooleanLiteral literal) const
{
	return truth_values_[literal.VariableIndex()] != 2;
}

inline bool Assignments::IsInternalBooleanVariableAssigned(int32_t index) const
{
	return truth_values_[index] != 2;
}

inline bool Assignments::IsAssignedTrue(BooleanLiteral literal) const
{
	return (truth_values_[literal.VariableIndex()] ^ literal.IsPositive()) == 0;
}

inline bool Assignments::IsAssignedTrue(BooleanVariableInternal variable) const
{
	return truth_values_[variable.index] == 1;
}

inline bool Assignments::IsAssignedFalse(BooleanLiteral literal) const
{
	return (truth_values_[literal.VariableIndex()] ^ literal.IsPositive()) == 1;
}

inline bool Assignments::IsRootAssignment(BooleanLiteral literal) const
{
	return IsAssigned(literal) && GetAssignmentLevel(literal) == 0;
}

inline bool Assignments::IsDecision(BooleanVariableInternal variable) const
{
	return IsAssigned(variable) && GetAssignmentReasonCode(variable) == 0 && GetAssignmentLevel(variable) > 0;
}

inline bool Assignments::IsDecision(BooleanLiteral literal) const
{
	return IsDecision(literal.Variable());
}

inline bool Assignments::IsPropagated(BooleanVariableInternal variable) const
{
	return IsAssigned(variable) && GetAssignmentReasonCode(variable) != 0 && GetAssignmentLevel(variable) > 0;
}

inline BooleanLiteral Assignments::GetAssignment(const BooleanVariableInternal variable) const
{
	assert(IsAssigned(variable) && variable.index < truth_values_.size());
	return BooleanLiteral(variable, truth_values_[variable.index]);
}

inline bool Assignments::GetAssignment(BooleanLiteral literal) const
{
	assert(IsAssigned(literal));
	return truth_values_[literal.VariableIndex()] == literal.IsPositive();
}

inline int Assignments::GetAssignmentLevel(BooleanVariableInternal variable) const
{
	assert(IsAssigned(variable));
	return info_[variable.index].decision_level;
}

inline int Assignments::GetAssignmentLevel(BooleanLiteral literal) const
{
	return GetAssignmentLevel(literal.Variable());
}

inline uint32_t Assignments::GetAssignmentReasonCode(BooleanVariableInternal variable) const
{
	pumpkin_assert_moderate(IsAssigned(variable), "Reason code is only available for assigned variables.");
	return info_[variable.index].reason_code;
}

inline void Assignments::MakeAssignment(BooleanLiteral true_literal, int decision_level, uint64_t reason_code)
{
	truth_values_[true_literal.VariableIndex()] = true_literal.IsPositive();
	info_[true_literal.VariableIndex()].decision_level = decision_level;
	info_[true_literal.VariableIndex()].reason_code = reason_code;	
}

inline void Assignments::UnassignVariable(BooleanVariableInternal variable)
{
	truth_values_[variable.index] = 2; //note that other auxiliary information is not updated
}

inline size_t Assignments::GetNumberOfInternalBooleanVariables() const
{
	assert(truth_values_.size() > 0);
	return truth_values_.size() - 1; //remember the 0th index is not used
}

inline void Assignments::Grow()
{
	truth_values_.push(2);
	info_.push(AuxiliaryAssignmentInfo());
}

} //end Pumpkin namespace