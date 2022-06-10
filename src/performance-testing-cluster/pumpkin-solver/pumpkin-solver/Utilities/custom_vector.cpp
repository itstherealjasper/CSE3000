#include "custom_vector.h"

namespace Pumpkin
{

const BooleanLiteral * LiteralVector::begin() const
{
	return &literals_[0];
}

const BooleanLiteral * LiteralVector::end() const
{
	return &literals_[0] + Size();
}

} //end Pumpkin namespace