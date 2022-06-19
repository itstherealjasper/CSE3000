#pragma once

namespace Pumpkin
{
class IntegerVariable
{
public:
	IntegerVariable() :id(0) {} //null variable

	explicit IntegerVariable(int index):id(index){}

	bool operator==(IntegerVariable v) const { return this->id == v.id; }
	bool operator!=(IntegerVariable v) const { return this->id != v.id; }
	bool IsNull() const { return id == 0; }

	static IntegerVariable UndefinedInteger() { return IntegerVariable(0); }

	int id;
};
}