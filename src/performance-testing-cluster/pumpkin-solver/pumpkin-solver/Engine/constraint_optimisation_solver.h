#pragma once

#include "constraint_satisfaction_solver.h"
#include "lower_bound_search.h"
#include "upper_bound_search.h"
#include "../Utilities/linear_function.h"
#include "../Utilities/solver_output.h"
#include "../Utilities/parameter_handler.h"
#include "../Utilities/solution_tracker.h"
#include "../Utilities/gz_file_reader.h"

#include <limits>
#include <vector>

namespace Pumpkin 
{
class ConstraintOptimisationSolver
{
public:
	ConstraintOptimisationSolver(ParameterHandler& parameters);

	void ReadDIMACSFile(std::string file_location);
	//ConstraintOptimisationSolver(ProblemSpecification& problem_specification, ParameterHandler& parameters);
	SolverOutput Solve(int64_t time_limit_in_seconds_linear_search, int64_t time_limit_in_seconds_core_guided);
	SolverOutput SolveBMO(int64_t time_limit_in_seconds_linear_search, int64_t time_limit_in_seconds_core_guided);

	std::string GetStatisticsAsString();
	SolverOutput GetPreemptiveResult(); //used to get the solution on Starexec

	static ParameterHandler CreateParameterHandler();
	static void CheckCorrectnessOfParameterHandler(const ParameterHandler& parameters);

//private:
	//converts the function into an equivalent function that does not have negative terms
	LinearFunction ConvertToCanonicalForm(LinearFunction& input_function);
	struct WeightInterval { int64_t min_weight, max_weight; WeightInterval(int64_t min, int64_t max) :min_weight(min), max_weight(max) {}; };
	std::vector<WeightInterval> ComputeLexicographicalObjectiveWeightRanges(LinearFunction& objective_function);
	SolverOutput ComputeInitialSolution(LinearFunction& objective_function, Stopwatch &stopwatch);
	bool use_lexicographical_objectives_, optimistic_initial_solution_;

	void ConvertDIMASIntegersToClause(std::vector<int64_t>& dimacs_integers, std::vector<BooleanLiteral> &output_clause, bool ignore_first_integer = false);
	void ReadSATFileInternal(GzFileReader &file_reader);
	void ReadMaxSATFileInternal(GzFileReader& file_reader);

	ParameterHandler parameters_;
	ConstraintSatisfactionSolver constrained_satisfaction_solver_;
	LowerBoundSearch core_guided_searcher_;
	UpperBoundSearch linear_searcher_;
	SolutionTracker solution_tracker_;
	LinearFunction original_objective_function_;
};

}//end Pumpkin namespace