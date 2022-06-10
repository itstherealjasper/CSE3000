#pragma once

#include "boolean_literal.h"

namespace Pumpkin
{
struct PairWeightLiteral
{
	PairWeightLiteral() :weight(-1) {};
	PairWeightLiteral(BooleanLiteral literal, int64_t weight) :literal(literal), weight(weight) {}

	BooleanLiteral literal;
	int64_t weight;
};
}