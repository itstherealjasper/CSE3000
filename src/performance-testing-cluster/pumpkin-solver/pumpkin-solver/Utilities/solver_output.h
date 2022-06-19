#pragma once

#include "small_helper_structures.h"
#include "integer_assignment_vector.h"

#include <vector>

namespace Pumpkin
{

struct SolverOutput
{
	SolverOutput() :
		runtime_in_seconds(-1),
		timeout_happened(false),
		solution(),
		cost(-1)
	{}

	SolverOutput(double runtime_in_seconds, bool timeout_happened, const IntegerAssignmentVector& solution, int64_t cost, const std::vector<BooleanLiteral>& core) :
		runtime_in_seconds(runtime_in_seconds),
		timeout_happened(timeout_happened),
		solution(solution),
		cost(cost),
		core_clause(core)		
	{}

	bool HasSolution() const { return !solution.IsEmpty(); }
	bool ProvenInfeasible() const { return solution.IsEmpty() && !timeout_happened; }

	double runtime_in_seconds;
	bool timeout_happened;
	IntegerAssignmentVector solution;
	int64_t cost;
	std::vector<BooleanLiteral> core_clause;
};

} //end Pumpkin namespace