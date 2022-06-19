#include "solver_output_checker.h"
#include "pumpkin_assert.h"

namespace Pumpkin
{

bool SolverOutputChecker::CheckSolutionCorrectness(ProblemSpecification& problem_specification, SolverOutput& output, bool verbose)
{
	if (output.HasSolution())
	{
		BooleanAssignmentVector boolean_assignments(problem_specification.NumBooleanVariables(), false);
		for (int i = 1; i <= problem_specification.NumBooleanVariables(); i++)
		{
			int truth_value = output.solution[IntegerVariable(i + 1)]; //+1 since the solver reserves indicies 0 and 1 for special purpose
			pumpkin_assert_permanent(truth_value == 0 || truth_value == 1, "Assignment to Boolean variable must be binary.");
			boolean_assignments[i] = truth_value;			
		}
		
		if (problem_specification.IsSatisfyingAssignment(boolean_assignments) == false)
		{
			// if (verbose) std::cout << "Solution not OK: does not satisfy hard constraints!\n";
			return false;
		}

		int64_t computed_cost = problem_specification.ComputeCost(boolean_assignments);
		if (computed_cost > output.cost)
		{
			if (verbose)
			{
				// std::cout << "Solution not OK: reported cost is lower than the actual cost!\n";
				// std::cout << "\tReported cost: " << output.cost << "\n";
				// std::cout << "\tRecomputed cost: " << problem_specification.ComputeCost(boolean_assignments) << "\n";

			}
			return false;
		}
		if (computed_cost < output.cost)
		{
			// std::cout << "Solution might be okay: reported cost is greater than actual cost\n";
			// std::cout << "\t...the mismatch in cost could be caused by the fact that the solver sometimes may come up with a solution that is easily fixable, e.g., the soft clause is satisfied but the selector is still penalising. Not a bug per-se but good to keep notice. To be fixed in the future.";
			// std::cout << "\tReported cost: " << output.cost << "\n";
			// std::cout << "\tRecomputed cost: " << problem_specification.ComputeCost(boolean_assignments) << "\n";			
		}
		else
		{
			// std::cout << "c final solution cost: " << computed_cost << "\n";
		}
		// if (verbose) std::cout << "c basic solution checks passed!\n";
	}
	return true;
}
}