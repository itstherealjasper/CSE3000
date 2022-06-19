#pragma once

#include "problem_specification.h"
#include "solver_output.h"

#include <iostream>

namespace Pumpkin
{
class SolverOutputChecker
{
public:
	static bool CheckSolutionCorrectness(ProblemSpecification& problem_specification, SolverOutput& output, bool verbose);
};

}