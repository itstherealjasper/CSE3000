#pragma once

#include "boolean_variable_internal.h"
#include "pumpkin_assert.h"

#include <string>
#include <math.h>
#include <stdint.h>

namespace Pumpkin
{

/*
A class representing a Boolean literal. This class is mainly introduced for clarity and simplicity.
*/
class BooleanLiteral 
{
public:
	BooleanLiteral(); //create an undefined literal. Only the (in)equality operators can be used with this literal.
	BooleanLiteral(BooleanVariableInternal, bool is_positive); //create a positive or negative literal of the provided Boolean variable

	/*BooleanLiteral& operator=(const BooleanLiteral& other)
	{
		this->code_ = other.code_;
		this->has_cp_propagators_attached = other.has_cp_propagators_attached;
	}*/

	void SetFlag(bool new_value);

	bool operator==(BooleanLiteral) const; //compares if two literals are equal, i.e. if they belong to the same variable and have the same sign (positive or negative)
	bool operator!=(BooleanLiteral) const; //compares if the two literals are different. This is equivalent to !(operator==)
	bool IsPositive() const; //true if literal is a positive literal of its Boolean variable
	bool IsNegative() const; //true if literal is a negative literal of its Boolean variable
	bool IsUndefined() const; //returns true or false if the literal is considered undefined. Internally a special code_ for undefined literals is kept to distinguish it, i.e. its corresponding Boolean variable is the never-used index zero variable
	bool GetFlag() const;
	BooleanVariableInternal Variable() const; //returns the Boolean variable associated with this literal
	uint32_t VariableIndex() const;
	BooleanLiteral operator~() const; //returns a negated version of the literal, e.g. if 'lit' was positive, ~lit is a negative literal of the same Boolean variable

	uint32_t ToPositiveInteger() const;
	std::string ToString() const;
	//static BooleanLiteral IntToLiteral(uint32_t code); //returns the literal which would return 'code' when calling ToPositiveInteger() on it. todo This seems hacky, and will be considered for removal later on
	static BooleanLiteral UndefinedLiteral();
	static BooleanLiteral CreateLiteralFromCodeAndFlag(uint32_t code, bool flag); //should be used with care, since the flag must be set to its correct value

private:
	uint32_t has_cp_propagators_attached : 1;
	uint32_t code_ : 31;
};

inline BooleanLiteral::BooleanLiteral()
	:has_cp_propagators_attached(0), code_(0)
{
}

inline BooleanLiteral::BooleanLiteral(BooleanVariableInternal variable, bool is_positive)
	: has_cp_propagators_attached(variable.has_cp_propagators_attached), code_(2 * variable.index + is_positive)
{
	pumpkin_assert_moderate(variable.IsUndefined() == false, "Sanity check.");
}

inline void BooleanLiteral::SetFlag(bool new_value)
{
	has_cp_propagators_attached = new_value;
}

inline bool BooleanLiteral::operator==(BooleanLiteral literal) const
{
	pumpkin_assert_moderate(!literal.IsUndefined() && !this->IsUndefined(), "Sanity check.");
	return this->code_ == literal.code_;
}

inline bool BooleanLiteral::operator!=(BooleanLiteral literal) const
{
	return this->code_ != literal.code_;
}

inline bool BooleanLiteral::IsPositive() const
{
	pumpkin_assert_moderate(IsUndefined() == false, "Sanity check.");
	return code_ & 1;
}

inline bool BooleanLiteral::IsNegative() const
{
	pumpkin_assert_moderate(IsUndefined() == false, "Sanity check.");
	return (~code_) & 1;
}

inline bool BooleanLiteral::IsUndefined() const
{
	return code_ == 0;
}

inline bool BooleanLiteral::GetFlag() const
{
	return has_cp_propagators_attached;
}

inline BooleanVariableInternal BooleanLiteral::Variable() const
{
	pumpkin_assert_moderate(IsUndefined() == false, "Sanity check.");
	return BooleanVariableInternal(code_ >> 1, has_cp_propagators_attached);
}

inline uint32_t BooleanLiteral::VariableIndex() const
{
	return code_ >> 1;
}

inline BooleanLiteral BooleanLiteral::operator~() const
{
	return BooleanLiteral(Variable(), !IsPositive());
}

inline uint32_t BooleanLiteral::ToPositiveInteger() const
{
	return code_;
}

inline std::string BooleanLiteral::ToString() const
{
	std::string s;
	if (IsNegative()) { s += '~'; }
	s += std::to_string(VariableIndex());
	return s;
}

/*inline BooleanLiteral BooleanLiteral::IntToLiteral(uint32_t code)
{
	pumpkin_assert_permanent(1 == 2, "temp problem");
	return BooleanLiteral(BooleanVariable(code >> 1), code & 1);
}*/

inline BooleanLiteral BooleanLiteral::UndefinedLiteral()
{
	return BooleanLiteral();
}

inline BooleanLiteral BooleanLiteral::CreateLiteralFromCodeAndFlag(uint32_t code, bool flag)
{
	BooleanLiteral lit = BooleanLiteral();
	lit.code_ = code;
	lit.has_cp_propagators_attached = flag;
	return lit;
}

} //end Pumpkin namespace