#pragma once

#include "solver_state.h"
#include "conflict_analysis_result_clausal.h"
#include "learned_clause_minimiser.h"
#include "../Utilities/boolean_variable_internal.h"
#include "../Utilities/boolean_literal.h"
#include "../Utilities/linear_function.h"
#include "../Utilities/problem_specification.h"
#include "../Utilities/solver_parameters.h"
#include "../Utilities/small_helper_structures.h"
#include "../Utilities/simple_moving_average.h"
#include "../Utilities/cumulative_moving_average.h"
#include "../Utilities/stopwatch.h"
#include "../Utilities/solver_output.h"
#include "../Utilities/directly_hashed_integer_set.h"
#include "../Utilities/parameter_handler.h"
#include "../Utilities/luby_sequence_generator.h"

#include <vector>
#include <iostream>
#include <limits>

namespace Pumpkin
{

class ConstraintSatisfactionSolver
{
public:
	ConstraintSatisfactionSolver(ParameterHandler& parameters);
	SolverOutput Solve(double time_limit_in_seconds = std::numeric_limits<double>::max()); //solves the formula currently in the solver and returns a vector where the i-th entry denotes if the literal was true or false (empty vector for unsat formulas)
	SolverOutput Solve(std::vector<BooleanLiteral> &assumptions, double time_limit_in_seconds = std::numeric_limits<double>::max());
	
	//computes a lower bound by assuming the variables take their values that minimise the function (smallest value for positive weighted integers, largest value for negative weighted integers)
	int ComputeSimpleLowerBound(LinearFunction& function);

	std::string GetStatisticsAsString();

	SolverState state_; //todo move to private	

private:

	enum class SolverExecutionFlag { SAT, UNSAT, UNSAT_UNDER_ASSUMPTIONS, TIMEOUT};
	SolverExecutionFlag SolveInternal(std::vector<BooleanLiteral>& assumptions, double time_limit_in_seconds = std::numeric_limits<double>::max());

	void InitialiseAtRoot(double time_limit_in_seconds, std::vector<BooleanLiteral>& assumptions);

//assumption methods------------------------
	bool AreAllAssumptionsSet();
	BooleanLiteral PeekNextAssumption();
	BooleanLiteral GetNextDecisionLiteral();
	
//conflict analysis methods-----------------

	std::vector<BooleanLiteral> ExtractCore(BooleanLiteral falsified_assumption);

	//I think analyse conflict should be in the state and/or a separate conflict-analysing class to make it easier to allow different types of conflict analysis to be interchanged
	//analyses the conflict and returns the information to resolve the conflict. 
	//Does not change the state, apart from bumps activities	
	//ConflictAnalysisResultClausal AnalyseConflict();
	void AnalyseConflict(ConflictAnalysisResultClausal &analysis_result_);

	void UpdateLBD(Clause& clause);

	//changes the state based on the conflict analysis result given as input
	//i.e., adds the learned clause to the database, backtracks, enqueues the propagated literal, and updates internal data structures for simple moving averages
	//note that no propagation is done, this is left to the solver
	void ProcessConflictAnalysisResult(ConflictAnalysisResultClausal& result);
	
//restart methods------------------------

	//true if it is determine that a restart should take place, false otherwise. Does not change the state.
	bool ShouldRestart();
	//Backtracks to the root and updates counters and removes learned clauses. 
	//Note this is different than calling Backtrack(0) since this method updates more information 
	//	and it may not restart to the root level if clause clean up will not happen and there are assumptions
	//	instead it restarts to the assumption level
	void PerformRestartDuringSearch();

	//similar as the previous method (PerformRestartDuringSearch), but always restarts to the root 
	void PerformRestartToRoot(); 
	
	void PrintClause(Clause& c);

	//produces a SolverOutput based on the state
	SolverOutput GenerateOutput();

//variables----------------
	struct InternalParameters
	{
		InternalParameters(ParameterHandler& parameters):
			bump_decision_variables(parameters.GetBooleanParameter("bump-decision-variables")),
			num_min_conflicts_per_restart(parameters.GetIntegerParameter("num-min-conflicts-per-restart")),
			use_clause_minimisation_(parameters.GetBooleanParameter("clause-minimisation"))
		{
			if (parameters.GetStringParameter("restart-strategy") == "glucose") { restart_strategy_ = RestartStrategy::GLUCOSE; }
			else if (parameters.GetStringParameter("restart-strategy") == "luby") { restart_strategy_ = RestartStrategy::LUBY; }
			else if (parameters.GetStringParameter("restart-strategy") == "constant") { restart_strategy_ = RestartStrategy::CONSTANT; }
			else { std::cout << "Error, unknown restart strategy: " << parameters.GetStringParameter("restart-strategy") << "\n"; exit(1); }
		
			restart_coefficient_ = parameters.GetIntegerParameter("restart-multiplication-coefficient");
		}

		bool bump_decision_variables;
		int num_min_conflicts_per_restart;
		enum class RestartStrategy { GLUCOSE, LUBY, CONSTANT } restart_strategy_;
		int restart_coefficient_;
		bool use_clause_minimisation_;
	} internal_parameters_;

	vec<uint8_t> seen_;
	ConflictAnalysisResultClausal analysis_result_;
	Stopwatch stopwatch_;
	LubySequenceGenerator luby_generator_;
	std::vector<BooleanLiteral> assumptions_;
	LearnedClauseMinimiser learned_clause_minimiser_;		
	DirectlyHashedIntegerSet already_processed_;
};

inline SolverOutput ConstraintSatisfactionSolver::Solve(double time_limit_in_seconds)
{
	std::vector<BooleanLiteral> empty_assumptions;
	return Solve(empty_assumptions, time_limit_in_seconds);
}

inline void ConstraintSatisfactionSolver::InitialiseAtRoot(double time_limit_in_seconds, std::vector<BooleanLiteral>& assumptions)
{
	stopwatch_.Initialise(time_limit_in_seconds);
	assumptions_ = assumptions;

	//state_.lbd_counter = 0;
	if (state_.lbd_flag.size() + 1 <= state_.GetNumberOfInternalBooleanVariables()) { state_.lbd_flag.resize(state_.GetNumberOfInternalBooleanVariables() + 1, state_.lbd_counter); }
	if (seen_.size() + 1 <= state_.GetNumberOfInternalBooleanVariables()) { seen_.resize(state_.GetNumberOfInternalBooleanVariables() + 1, 0); }	
}

inline bool ConstraintSatisfactionSolver::AreAllAssumptionsSet()
{
	return state_.GetCurrentDecisionLevel() > assumptions_.size();
}

inline void ConstraintSatisfactionSolver::UpdateLBD(Clause& clause)
{
	if (clause.IsLearned() && clause.GetLBD() > 2)
	{
		state_.propagator_clausal_.BumpClauseActivity(clause);
		uint32_t new_lbd = state_.ComputeLBD(clause.begin(), clause.Size());
		if (new_lbd < clause.GetLBD())
		{
			clause.SetLBD(new_lbd);
			clause.MarkLBDProtection();
		}
	}
}

} //end Pumpkin namespace