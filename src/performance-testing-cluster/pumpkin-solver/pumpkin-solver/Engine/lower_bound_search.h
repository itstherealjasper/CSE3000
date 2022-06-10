#pragma once

#include "constraint_satisfaction_solver.h"
#include "../Pseudo-Boolean Encoders/encoder_totaliser.h"
#include "../Pseudo-Boolean Encoders/encoder_cardinality_network.h"
#include "../Pseudo-Boolean Encoders/encoder_generalised_totaliser.h"
#include "../Utilities/linear_function.h"
#include "../Utilities/solution_tracker.h"
#include "../Utilities/parameter_handler.h"

#include <map>

namespace Pumpkin
{
class LowerBoundSearch
{
public:

	LowerBoundSearch(ParameterHandler& parameters);

	//The objective function is the objective that the algorithm should optimise
	//Solution tracker keeps track of the globally best solution
	//	Note however that the objective in solution tracker may be different from the one provided here as input
	//	This is because lexicographical optimisation may be used
	bool Solve
	(
		ConstraintSatisfactionSolver& solver,
		LinearFunction& objective_function,
		SolutionTracker& solution_tracker,
		double time_limit_in_seconds
	);

private:
	
	int64_t GetInitialWeightThreshold(SolverState& state);
	int64_t GetNextWeightThreshold(int64_t current_weight_threshold, SolverState& state);
	int64_t GetNextWeightRatioStrategy(int64_t previous_weight_threshold, SolverState &state);

	bool HardenReformulatedObjectiveFunction(SolverState& state);

	void CoreGuidedSearchWithWeightThreshold(
		int64_t weight_threshold,
		Stopwatch& stopwatch,
		ConstraintSatisfactionSolver& constrained_satisfaction_solver_,
		SolutionTracker& solution_tracker
	);

	std::vector<BooleanLiteral> InitialiseAssumptions(int64_t weight_threshold, SolverState &state);
	int64_t GetMinimumCoreWeight(std::vector<BooleanLiteral>& core_clause, SolverState& state);
	void ProcessCores(std::vector<std::vector<BooleanLiteral>>& core_clauses, std::vector<int64_t> core_weights, ConstraintSatisfactionSolver& solver);
	void FilterAssumptions(std::vector<BooleanLiteral> &assumptions, std::vector<BooleanLiteral>& core_clause, int64_t weight_threshold, SolverState &state);
	
	void InitialiseDataStructures(const LinearFunction&objective_function, SolutionTracker& solution_tracker, SolverState& state);
	BooleanLiteral GetOptimisticAssumptionLiteralForVariable(IntegerVariable variable, SolverState &state);
	void PerformSlicingStep(std::vector<BooleanLiteral>& core_clause, int64_t core_weight, SolverState &state);

	int64_t EvaluateReformulatedObjectiveValue(const IntegerAssignmentVector& assignment);

	int GetNumLiteralsWithWeightGreaterOrEqualToThreshold(int64_t threshold, SolverState& state);
	int64_t GetMaximumWeightSmallerThanThreshold(int64_t threshold, SolverState& state);
	
	void RemoveRedundantTermsFromTheReformulatedObjective(SolverState &state);
	int64_t MaxWeightInReformulatedObjective() const;

	IntegerAssignmentVector ComputeExtendedSolution(const IntegerAssignmentVector& reference_solution, ConstraintSatisfactionSolver& solver, double time_limit_in_seconds);
	LinearFunction ConvertReformulatedObjectiveIntoLinearFunction(SolverState &state);

	//other variables
	EncoderGeneralisedTotaliser general_totaliser_encoder_;
	EncoderTotaliser totaliser_encoder_;
	EncoderCardinalityNetwork cardinality_network_encoder_;

	//[var.id] = c, where c is the upper bound value that is used as an assumption. Initially this value is set to the lower bound of the variable, and increased during the search
	//struct PairLowerBoundWeight { int64_t lower_bound, original_weight; };
	//std::vector<PairLowerBoundWeight> variable_to_core_info_;
	//LinearFunction internal_objective_function_;

	struct ReformulatedTerm { int threshold; int64_t residual_weight, full_weight; };
	std::map<int, ReformulatedTerm> reformulated_objective_variables_; //variable -> reformulated_term
	int64_t reformulated_constant_term_, internal_upper_bound_;

	//parameters
	enum class CardinalityConstraintEncoding { TOTALISER, CARDINALITY_NETWORK } cardinality_constraint_encoding_; //todo consider abstract classes instead of this
	enum class StratificationStrategy { OFF, BASIC, RATIO } stratification_strategy_;
	bool use_weight_aware_core_extraction_;
};
}