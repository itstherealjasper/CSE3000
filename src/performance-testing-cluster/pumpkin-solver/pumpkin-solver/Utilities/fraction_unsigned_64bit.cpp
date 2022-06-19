#include "fraction_unsigned_64bit.h"

#include <assert.h>
#include <iostream>

namespace Pumpkin
{

FractionUnsigned64Bit::FractionUnsigned64Bit(uint64_t numerator, uint64_t denominator):
	numerator_(numerator), denominator_(denominator)
{
	//normalise
	if (numerator_ % denominator_ == 0)
	{
		numerator_ /= denominator_;
		denominator = 1;
	}
}

uint64_t FractionUnsigned64Bit::RoundDownToInteger()
{
	return numerator_ / denominator_;
}

bool FractionUnsigned64Bit::IsInteger() const
{
	return denominator_ == 1;
}

FractionUnsigned64Bit operator*(uint64_t integer64, const FractionUnsigned64Bit & fraction)
{
	return fraction * integer64;
}

FractionUnsigned64Bit operator*(const FractionUnsigned64Bit & fraction, uint64_t integer64)
{
	/*
	Special consideration is taken to avoid overflows.

	num -> numerator_
	den -> denominator_
	integer64 -> int64

	this method computes (num*int64)/den as follows:

	num = k_a * den + r_a, where r_a = num % den
	int64 = k_b * den + r_b, where r_b = int64 % den 

	(num*int64)/den = (k_a * den + r_a)*(k_b * den + r_b)/den = k_a * k_b * den + k_a * r_b + k_b * r_a + (r_a*r_b)/den 
	*/

	uint64_t k_a = fraction.numerator_ / fraction.denominator_;
	uint64_t r_a = fraction.numerator_ % fraction.denominator_;
	
	uint64_t k_b = integer64 / fraction.denominator_;
	uint64_t r_b = integer64 % fraction.denominator_;

	assert(FractionUnsigned64Bit::WillMultiplicationOverflow(k_a, k_b, fraction.denominator_) == false);
	assert(FractionUnsigned64Bit::WillMultiplicationOverflow(k_a, r_b) == false);
	assert(FractionUnsigned64Bit::WillMultiplicationOverflow(k_b, r_a) == false);
	assert(FractionUnsigned64Bit::WillMultiplicationOverflow(r_a, r_b) == false);

	uint64_t term1 = k_a * k_b * fraction.denominator_;
	uint64_t term2 = k_a * r_b;
	uint64_t term3 = k_b * r_a;
	uint64_t term4_numerator = (r_a * r_b);
	uint64_t term4_denominator = fraction.denominator_;

	if (term4_numerator % term4_denominator == 0)
	{
		term4_numerator /= term4_denominator;
		term4_denominator = 1;
	}

	assert(FractionUnsigned64Bit::WillMultiplicationOverflow(term1, term4_denominator) == false);
	assert(FractionUnsigned64Bit::WillMultiplicationOverflow(term2, term4_denominator) == false);
	assert(FractionUnsigned64Bit::WillMultiplicationOverflow(term3, term4_denominator) == false);

	term1 *= term4_denominator;
	term2 *= term4_denominator;
	term3 *= term4_denominator;

	assert(FractionUnsigned64Bit::WillAdditionOverflow(term1, term2, term3, term4_numerator) == false);
	
	return FractionUnsigned64Bit(term1 + term2 + term3 + term4_numerator, term4_denominator);
}

bool FractionUnsigned64Bit::WillAdditionOverflow(uint64_t a, uint64_t b)
{
	bool overflow_will_happen = (a > UINT64_MAX - b);
	if (overflow_will_happen) { assert(1 == 2); std::cout << "c Overflow happened!\n"; }
	return overflow_will_happen;
}

bool FractionUnsigned64Bit::WillAdditionOverflow(uint64_t a, uint64_t b, uint64_t c, uint64_t d)
{
	if (WillAdditionOverflow(a, b)) { return true; }
	uint64_t t = a + b;
	if (WillAdditionOverflow(t, c)) { return true; }
	t += c;
	if (WillAdditionOverflow(t, d)) { return true; }
}

bool FractionUnsigned64Bit::WillMultiplicationOverflow(uint64_t a, uint64_t b)
{
	if (a == 0 || b == 0) { return false; }
	bool overflow_will_happen = (a > UINT64_MAX / b);
	if (overflow_will_happen) { assert(1 == 2); std::cout << "c Overflow happened!\n"; }
	return overflow_will_happen;
}

bool FractionUnsigned64Bit::WillMultiplicationOverflow(uint64_t a, uint64_t b, uint64_t c)
{
	if (WillMultiplicationOverflow(a, b)) { return true; }
	uint64_t ab = a * b;
	return WillMultiplicationOverflow(ab, c);
}

} //end Pumpkin namespace