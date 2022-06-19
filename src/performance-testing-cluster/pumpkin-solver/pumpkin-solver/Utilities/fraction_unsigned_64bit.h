#pragma once

#include <stdint.h>

namespace Pumpkin
{

class FractionUnsigned64Bit
{
public:
	FractionUnsigned64Bit(uint64_t numerator, uint64_t denominator);

	uint64_t RoundDownToInteger();

	friend FractionUnsigned64Bit operator*(uint64_t integer64, const FractionUnsigned64Bit &fraction);
	friend FractionUnsigned64Bit operator*(const FractionUnsigned64Bit &fraction, uint64_t integer64); //special consideration is taken to not overflow. I think it is not perfect, check some time!

	bool IsInteger() const;
private:
	static bool WillAdditionOverflow(uint64_t a, uint64_t b);
	static bool WillAdditionOverflow(uint64_t a, uint64_t b, uint64_t c, uint64_t d);
	static bool WillMultiplicationOverflow(uint64_t a, uint64_t b);
	static bool WillMultiplicationOverflow(uint64_t a, uint64_t b, uint64_t c);

	uint64_t numerator_, denominator_;
};

} //end Pumpkin namespace