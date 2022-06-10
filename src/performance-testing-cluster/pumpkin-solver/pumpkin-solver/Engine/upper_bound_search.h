#pragma once

#include "constraint_satisfaction_solver.h"
#include "../Pseudo-Boolean Encoders/encoder_generalised_totaliser.h"
#include "../Utilities/linear_function.h"
#include "../Utilities/solution_tracker.h"
#include "../Utilities/parameter_handler.h"
#include "../Propagators/Pseudo-Boolean/counter_single_pseudo_boolean_propagator.h"
#include "../Propagators/Linear Integer Inequality/linear_integer_inequality_propagator.h"
namespace Pumpkin
{
class UpperBoundSearch
{
public:
	UpperBoundSearch(SolverState &state, ParameterHandler& parameters);

	bool Solve
	(
		ConstraintSatisfactionSolver& solver,
		LinearFunction& objective_function,
		SolutionTracker& solution_tracker,
		double time_limit_in_seconds
	);

private:
	void LinearSearch
	(
		ConstraintSatisfactionSolver& solver,
		LinearFunction& objective_function,
		SolutionTracker& solution_tracker,
		double time_limit_in_seconds
	);

	//encodes a pseudo-Boolean constraint is encodes that the assignment must be less or equal to the upper bound
	//todo think about moving everything related to linear constraints into the encoder class rather than keep it here
	EncodingOutput EncodeInitialUpperBound(ConstraintSatisfactionSolver& solver, LinearFunction& objective_function, int64_t upper_bound, double time_limit_in_seconds);
	bool StrengthenUpperBound(const std::vector<PairWeightLiteral>& sum_literals, int64_t upper_bound, LinearFunction& objective_function, ConstraintSatisfactionSolver& solver);
	int64_t ComputeFixedCost(ConstraintSatisfactionSolver& solver, LinearFunction& objective_function); //the fixed cost is the constant term plus the value we get by setting all variables to their lower bound
	void SetValueSelectorValues(ConstraintSatisfactionSolver& solver, LinearFunction& objective_function, const IntegerAssignmentVector& solution);

	IntegerAssignmentVector ComputeExtendedSolution(const IntegerAssignmentVector& reference_solution, ConstraintSatisfactionSolver& solver, double time_limit_in_seconds); //extends the input solution to assign values to the auxiliary variables that are not present in the solution but are part of the solver

	int64_t GetInitialDivisionCoefficient(LinearFunction& objective_function, SolverState &state);
	int64_t GetNextDivisionCoefficient(int64_t current_division_coefficient, const LinearFunction& objective_function, SolverState& state);
	LinearFunction GetVaryingResolutionObjective(int64_t division_coefficient, LinearFunction& original_objective, SolverState& state);
	int64_t GetNextDivisionCoefficientRatioStrategy(int64_t division_coefficient, const LinearFunction& objective_function, SolverState& state);

	static int GetNumLiteralsWithWeightGreaterOrEqualToThreshold(int64_t threshold, const LinearFunction& objective_function, SolverState& state);
	static int64_t GetMaximumWeightSmallerThanThreshold(int64_t threshold, const LinearFunction& objective_function, SolverState& state);

	EncoderGeneralisedTotaliser pseudo_boolean_encoder_;
	//PropagatorCounterSinglePseudoBoolean *upper_bound_propagator_; //should not be deleted by the class if passed to SolverState
	LinearIntegerInequalityPropagator *ub_prop_;
	bool use_ub_prop_;
	std::vector<BooleanLiteral> helper_;
	enum class VaryingResolutionStrategy { OFF, BASIC, RATIO } varying_resolution_strategy_;
	enum class ValueSelectionStrategy { PHASE_SAVING, SOLUTION_GUIDED_SEARCH, OPTIMISTIC, OPTIMISTIC_AUX } value_selection_strategy_;
};
}