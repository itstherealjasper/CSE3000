#ifndef CUSTOM_VECTOR_H
#define CUSTOM_VECTOR_H

#include "boolean_literal.h"

#include <cassert>
#include <algorithm>
#include <vector>

namespace Pumpkin
{

class LiteralVector
{
public:
	//~LiteralVector(); for now
	
	const BooleanLiteral* begin() const; //should return const pointer? not sure
	const BooleanLiteral* end() const ;

	int Size() const
	{
		return size_;
	}
	
	BooleanLiteral& operator[](int index)
	{
		assert(index < size_);
		return literals_[index];
	}

	BooleanLiteral operator[](int index) const
	{
		assert(index < size_);
		return literals_[index];
	}

	int size_;
	BooleanLiteral literals_[0];
};

} //end Pumpkin namespace

#endif // !CUSTOM_VECTOR_H