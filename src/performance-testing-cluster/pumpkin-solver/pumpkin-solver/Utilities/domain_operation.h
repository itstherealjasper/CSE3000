#pragma once

#include "runtime_assert.h"

#include <string>

namespace Pumpkin
{
class DomainOperation
{
public:
	enum class DomainOperationCodes { LESS_OR_EQUAL, GREATER_OR_EQUAL, NOT_EQUAL, EQUAL, UNKNOWN };

	DomainOperation();
	DomainOperation(DomainOperationCodes operation_code);

	bool IsEquality() const;
	bool IsNotEqual() const;
	bool IsLessOrEqual() const;
	bool IsGreaterOrEqual() const;	

	std::string ToString() const;

private:
	DomainOperationCodes code_;
};

inline DomainOperation::DomainOperation() :code_(DomainOperationCodes::UNKNOWN) { }

inline DomainOperation::DomainOperation(DomainOperationCodes operation_code) : code_(operation_code) { }

inline bool DomainOperation::IsEquality() const
{
	return code_ == DomainOperationCodes::EQUAL;
}

inline bool DomainOperation::IsNotEqual() const
{
	return code_ == DomainOperationCodes::NOT_EQUAL;
}

inline bool DomainOperation::IsLessOrEqual() const
{
	return code_ == DomainOperationCodes::LESS_OR_EQUAL;
}

inline bool DomainOperation::IsGreaterOrEqual() const
{
	return code_ == DomainOperationCodes::GREATER_OR_EQUAL;
}

inline std::string DomainOperation::ToString() const
{
	if (IsEquality()) { return "="; }
	else if (IsNotEqual()) { return "!="; }
	else if (IsLessOrEqual()) { return "<="; }
	else if (IsGreaterOrEqual()) { return ">="; }
	else { runtime_assert(1 == 2); return "problem"; }
}

}