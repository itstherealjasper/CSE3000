#pragma once

#include "../Utilities/parameter_handler.h"

namespace Pumpkin
{
class EncoderTester
{
public:
	static void TestTotaliserEncoding(ParameterHandler& parameters);
	static void TestCardinalityNetworkEncoding(ParameterHandler& parameters);

	static void TestGeneralisedTotaliserAsCardinalityConstraintEncoding(ParameterHandler& parameters);
	static void TestGeneralisedTotaliser(ParameterHandler& parameters, int64_t max_weight);

	static void TestGeneralisedTotaliserAsCardinalityConstraintEncodingHard(ParameterHandler& parameters);
	static void TestGeneralisedTotaliserHard(ParameterHandler& parameters, int64_t max_weight);
};
}