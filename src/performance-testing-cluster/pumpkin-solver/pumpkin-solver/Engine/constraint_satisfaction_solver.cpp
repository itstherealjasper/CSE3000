#include "constraint_satisfaction_solver.h"
#include "../Utilities/runtime_assert.h"

#include <assert.h>
#include <algorithm>
#include <iostream>
#include <fstream>

namespace Pumpkin
{
ConstraintSatisfactionSolver::ConstraintSatisfactionSolver(ParameterHandler& parameters):
	state_(0, parameters),
	internal_parameters_(parameters),
	learned_clause_minimiser_(state_), //todo need to ensure that the state object is created before the clause minimiser, rethink the initialisation strategy
	already_processed_(0)
{
}

SolverOutput ConstraintSatisfactionSolver::Solve(std::vector<BooleanLiteral>& assumptions, double time_limit_in_seconds)
{
	SolverExecutionFlag flag = SolveInternal(assumptions, time_limit_in_seconds);

	// std::cout << "c conflicts until restart: " << state_.counters_.num_conflicts_until_restart << "\n";
	// std::cout << "c restart counter: " << state_.counters_.num_restarts << "\n";
	
	double runtime = stopwatch_.TimeElapsedInSeconds();
	bool timeout = (flag == SolverExecutionFlag::TIMEOUT);
	int64_t cost = -1;
	IntegerAssignmentVector solution;
	std::vector<BooleanLiteral> core;

	if (flag == SolverExecutionFlag::SAT)
	{
		cost = 0;
		solution = state_.GetOutputAssignment();

		//debug check to see if assumptions are satisfied
		for (BooleanLiteral assumption_literal : assumptions) 
		{
			auto &literal_info = state_.GetLiteralInformation(assumption_literal);
			bool is_assinged_true = state_.assignments_.IsAssignedTrue(assumption_literal);
			runtime_assert(solution.IsSatisfied(literal_info.integer_variable, literal_info.operation, literal_info.right_hand_side));
		}
	}
	else if (flag == SolverExecutionFlag::UNSAT_UNDER_ASSUMPTIONS)
	{
		BooleanLiteral falsified_assumption = GetNextDecisionLiteral();
		runtime_assert(state_.assignments_.IsAssignedFalse(falsified_assumption));
		core = ExtractCore(falsified_assumption);
	}

	if (state_.GetCurrentDecisionLevel() > 0) { PerformRestartToRoot(); }
	else { state_.failure_clause_ = 0; }

	return SolverOutput(runtime, timeout, solution, cost, core);
}

int ConstraintSatisfactionSolver::ComputeSimpleLowerBound(LinearFunction& function)
{
	int lower_bound = function.GetConstantTerm();
	for (Term term : function)
	{
		if (term.weight > 0)
		{
			lower_bound += term.weight * state_.domain_manager_.GetLowerBound(term.variable);
		}
		else if (term.weight < 0)
		{
			lower_bound += term.weight * state_.domain_manager_.GetUpperBound(term.variable);
		}
	}
	return lower_bound;
}

std::string ConstraintSatisfactionSolver::GetStatisticsAsString()
{
	std::string s;

	// s += "c restarts: " + std::to_string(state_.counters_.num_restarts);
	if (state_.counters_.num_restarts == 0)
	{
		s += "\n";
	}
	else
	{
		// s += " (" + std::to_string(state_.counters_.num_conflicts / state_.counters_.num_restarts) + " conflicts in avg)" + "\n";
	}
	// s += "c blocked restarts: " + std::to_string(state_.counters_.blocked_restarts) + "\n";
	// s += "c nb removed clauses: " + std::to_string(state_.propagator_clausal_.counter_total_removed_clauses_) + "\n";
	// s += "c nb learnts DL2: " + std::to_string(state_.counters_.small_lbd_clauses_learned) + "\n";
	// s += "c nb learnts size 1: " + std::to_string(state_.counters_.unit_clauses_learned) + "\n";
	// s += "c nb learnts size 3: " + std::to_string(state_.counters_.ternary_clauses_learned) + "\n";
	// s += "c nb learnts: " + std::to_string(state_.propagator_clausal_.number_of_learned_clauses_) + "\n";
	// s += "c avg learnt clause size: " + std::to_string(double(state_.propagator_clausal_.number_of_learned_literals_) / state_.propagator_clausal_.number_of_learned_clauses_) + "\n";
	// s += "c current number of learned clauses: " + std::to_string(state_.propagator_clausal_.NumLearnedClauses()) + "\n";
	// s += "c ratio of learned clauses: " + std::to_string(double(state_.propagator_clausal_.NumLearnedClauses()) / state_.propagator_clausal_.NumClausesTotal()) + "\n";
	// s += "c conflicts: " + std::to_string(state_.counters_.num_conflicts) + "\n";
	// s += "c decisions: " + std::to_string(state_.counters_.decisions) + "\n";
	// s += "c propagations: " + std::to_string(state_.counters_.propagations) + "\n";

	return s;
}

ConstraintSatisfactionSolver::SolverExecutionFlag ConstraintSatisfactionSolver::SolveInternal(std::vector<BooleanLiteral>& assumptions, double time_limit_in_seconds)
{
	InitialiseAtRoot(time_limit_in_seconds, assumptions);

	while (stopwatch_.IsWithinTimeLimit())
	{
		PropagationStatus propagation_status = state_.PropagateEnqueuedLiterals();

		if (!propagation_status.conflict_detected)
		{//proceed with variable selection
			if (ShouldRestart()){ PerformRestartDuringSearch(); }

			state_.IncreaseDecisionLevel();
			BooleanLiteral next_decision_literal = GetNextDecisionLiteral();

			//a full assignment has been built, stop the search
			if (next_decision_literal.IsUndefined()) { return SolverExecutionFlag::SAT; }
			//a decision literal may be already assigned only if it is a conflicting assumption, stop the search
			if (state_.assignments_.IsAssigned(next_decision_literal)) { return SolverExecutionFlag::UNSAT_UNDER_ASSUMPTIONS; }

			state_.EnqueueDecisionLiteral(next_decision_literal);
		}		
		else 
		{//a conflict has been detected, resolve the conflict
			runtime_assert(state_.failure_clause_ != 0);			
			
			if (state_.GetCurrentDecisionLevel() == 0) { return SolverExecutionFlag::UNSAT; } //root level conflict - unsat
			
			AnalyseConflict(analysis_result_);
			ProcessConflictAnalysisResult(analysis_result_);
						
			state_.propagator_clausal_.DecayClauseActivities();
			state_.variable_selector_.DecayActivities();
		}		
	}
	return SolverExecutionFlag::TIMEOUT;
}

BooleanLiteral ConstraintSatisfactionSolver::PeekNextAssumption()
{
	assert(state_.GetCurrentDecisionLevel() > 0); //no assumptions can be set at decision level zero; level zero is reserved only for unit clauses

	if (AreAllAssumptionsSet()) { return BooleanLiteral::UndefinedLiteral(); }

	BooleanLiteral next_assumption = assumptions_[state_.GetCurrentDecisionLevel()-1];
	while (AreAllAssumptionsSet() == false && state_.assignments_.IsAssignedTrue(next_assumption))
	{
		state_.IncreaseDecisionLevel();
		if (AreAllAssumptionsSet()) { break; }
		next_assumption = assumptions_[state_.GetCurrentDecisionLevel() - 1];
	}

	//if all assumptions are assigned true, report that there are no assumptions left
	if (AreAllAssumptionsSet()) { return BooleanLiteral::UndefinedLiteral(); }
	
	assert(state_.assignments_.IsAssigned(next_assumption) == false || state_.assignments_.IsAssignedFalse(next_assumption));
	return next_assumption;
}

BooleanLiteral ConstraintSatisfactionSolver::GetNextDecisionLiteral()
{
	//assumption literals have priority over other literals
	//	check if there are any assumption literals that need to be set
	
	BooleanLiteral next_assumption = PeekNextAssumption();
	if (!next_assumption.IsUndefined()) { return next_assumption; }
	
	//at this point, no assumptions need to be set
	//	proceed with standard variable selection	
	BooleanVariableInternal selected_variable = state_.variable_selector_.PeekNextVariable(&state_);
	//variable selector returns an undefined variable if every variable has been assigned
	if (selected_variable.IsUndefined()) { return BooleanLiteral::UndefinedLiteral(); }

	bool selected_value = state_.value_selector_.SelectValue(selected_variable);
	BooleanLiteral decision_literal(selected_variable, selected_value);

	state_.counters_.decisions++;
	assert(!state_.assignments_.IsAssigned(decision_literal));	
	return decision_literal;
}

std::vector<BooleanLiteral> ConstraintSatisfactionSolver::ExtractCore(BooleanLiteral falsified_assumption)
{
	already_processed_.Resize(state_.GetNumberOfInternalBooleanVariables() + 1);
	already_processed_.Clear();

	if (state_.assignments_.IsRootAssignment(falsified_assumption)){ return std::vector<BooleanLiteral>(1, ~falsified_assumption); }

	Clause* reason_clause = state_.GetReasonClausePointer(~falsified_assumption);

	//happens only if the assumptions are inconsistent, e.g., they contain both 'x' and '~x'
	if (reason_clause == NULL)
	{
		std::vector<BooleanLiteral> core;
		core.push_back(~falsified_assumption);
		core.push_back(falsified_assumption);		
		return core;
	}

	//at this point, we know the reason clause contains only assumptions, but some assumptions might be implied 
	//we now expand implied assumptions until all literals are decision assumptions
	std::vector<BooleanLiteral> core_clause;
	std::vector<BooleanLiteral> unconsidered_literals;
	for (int i = 1; i < reason_clause->Size(); i++) //index goes from one since the literal at position zero is the propagating literal
	{ 
		unconsidered_literals.push_back((*reason_clause)[i]);	
		already_processed_.Insert((*reason_clause)[i].VariableIndex());
	}
		
	while (!unconsidered_literals.empty())
	{
		BooleanLiteral current_literal = unconsidered_literals.back();
		unconsidered_literals.pop_back();

		if (state_.assignments_.IsRootAssignment(current_literal))
		{
			//do nothing
			//unit clauses are added to the solver as decision at level zero
			//those need to be ignored
		}
		else if (state_.assignments_.IsDecision(current_literal))
		{
			runtime_assert(already_processed_.IsPresent(current_literal.VariableIndex()));

			if (state_.assignments_.GetAssignmentLevel(current_literal) > 0)
			{
				core_clause.push_back(current_literal);
			}
		}
		else
		{//we now expand the reason for the implied assumption with other assumptions
			reason_clause = state_.GetReasonClausePointerPropagation(~current_literal);
			for (int i = 1; i < reason_clause->Size(); i++) //index goes from one since the literal at position zero is the propagating literal
			{
				BooleanLiteral reason_literal = ~(*reason_clause)[i];
				runtime_assert(state_.assignments_.IsAssignedTrue(reason_literal));
				//ignore literals already processed: this may happen if an assumption was responsible for two or more other implied assumptions
				if (already_processed_.IsPresent(reason_literal.VariableIndex()) || state_.assignments_.GetAssignmentLevel(reason_literal) == 0) { continue; }
				
				unconsidered_literals.push_back(~reason_literal);
				already_processed_.Insert(reason_literal.VariableIndex());
			}
		}
	}
	core_clause.push_back(~falsified_assumption);

	//at this point, all literals in the core are non-implied assumptions
	already_processed_.Clear();

	//debug check that the core only contains assumptions
	//for (BooleanLiteral core_literal : core_clause) 
	for(int i = 0; i < core_clause.size(); i++)
	{ 
		BooleanLiteral core_literal = core_clause[i];
		runtime_assert(state_.assignments_.GetAssignmentLevel(core_literal) <= assumptions_.size());
		runtime_assert(state_.assignments_.IsDecision(core_literal) || ~core_literal == falsified_assumption);
	}

	return core_clause;
}

void ConstraintSatisfactionSolver::AnalyseConflict(ConflictAnalysisResultClausal &analysis_result)
{
	assert(state_.failure_clause_ != 0);

	analysis_result.backtrack_level = 0;
	analysis_result.learned_clause_literals.clear();
	analysis_result.learned_clause_literals.push(); //the convention is to place the asserting literal at index zero, allocate space for it now
	
	int num_current_decision_level_literals = 0;
	int next_trail_index = state_.trail_.size() - 1;
	BooleanLiteral next_literal = BooleanLiteral::UndefinedLiteral();

	do 
	{
		assert(next_literal.IsUndefined() || state_.assignments_.IsPropagated(next_literal.Variable()) && state_.assignments_.GetAssignmentLevel(next_literal) == state_.GetCurrentDecisionLevel());
		
		Clause& reason_clause = (next_literal.IsUndefined() ? *state_.failure_clause_ : state_.GetReasonClausePropagation(next_literal));
		UpdateLBD(reason_clause);
		
		//process the reason literal
		//	effectively perform resolution and update other related internal data structures
		const BooleanLiteral* reason_literal = (next_literal.IsUndefined() ? reason_clause.begin() : reason_clause.begin() + 1);
		const BooleanLiteral* end = reason_clause.end();
		while (reason_literal != end)
		{
			//only consider non-root assignments that have not been considered before
			if (state_.assignments_.GetAssignmentLevel(*reason_literal) > 0 && !seen_[reason_literal->VariableIndex()])
			{
				seen_[reason_literal->VariableIndex()] = 1;

				state_.variable_selector_.BumpActivity(reason_literal->Variable());

				int literal_decision_level = state_.assignments_.GetAssignmentLevel(*reason_literal);
				bool current_level_assignment = (literal_decision_level == state_.GetCurrentDecisionLevel());
				num_current_decision_level_literals += current_level_assignment;
				//literals from previous decision levels are considered for the learnt clause
				if (!current_level_assignment) 
				{ 
					analysis_result.learned_clause_literals.push(*reason_literal);
					//the highest decision level literal must be placed at index 1 to prepare the clause for propagation
					if (literal_decision_level > analysis_result.backtrack_level)
					{
						analysis_result.backtrack_level = literal_decision_level;
						analysis_result.learned_clause_literals.last() = analysis_result.learned_clause_literals[1];
						analysis_result.learned_clause_literals[1] = *reason_literal;
					}
				}			
			}			
			++reason_literal;
		}
		//find the next literal on the trail
			//expand a node of the current decision level
			//find a literal that you have already seen in conflict analysis - recall that each literal may be found on the trail only once
			//literals that have not been seen yet are not important for this conflict so we can skip them
		while (!seen_[state_.trail_[next_trail_index].VariableIndex()])
		{
			--next_trail_index;
			pumpkin_assert_advanced(state_.assignments_.GetAssignmentLevel(state_.trail_[next_trail_index].Variable()) == state_.GetCurrentDecisionLevel(), "Sanity check, often triggers if a propagator is not correctly implemented.");
		}
		//make appropriate adjustments to prepare for the next iteration
		next_literal = state_.trail_[next_trail_index];
		seen_[next_literal.VariableIndex()] = 0; //the same literal cannot be encountered more than once on the trail, so we can clear the flag here
		--num_current_decision_level_literals;
		--next_trail_index;
	} while (num_current_decision_level_literals > 0); //once the counters hits zero we stop, the 1UIP has been found

	analysis_result.learned_clause_literals[0] = ~next_literal;

	for (int i = 0; i < analysis_result.learned_clause_literals.size(); i++) { seen_[analysis_result.learned_clause_literals[i].VariableIndex()] = 0; }

	pumpkin_assert_advanced(analysis_result.CheckCorrectnessAfterConflictAnalysis(state_), "There is an issues with the derived learned clause after analysis!\n");

	if (internal_parameters_.use_clause_minimisation_) { learned_clause_minimiser_.RemoveImplicationGraphDominatedLiteralsBetter(analysis_result); }

	pumpkin_assert_advanced(analysis_result.CheckCorrectnessAfterConflictAnalysis(state_), "After minimisation the learned clause has an issue!\n");
}

void ConstraintSatisfactionSolver::ProcessConflictAnalysisResult(ConflictAnalysisResultClausal& result)
{
	//unit clauses are treated in a special way: they are added as decision literals at decision level 0. This might change in the future if a better idea presents itself
	if (analysis_result_.learned_clause_literals.size() == 1)
	{
		state_.counters_.unit_clauses_learned++;

		state_.Backtrack(0);
		state_.EnqueueDecisionLiteral(analysis_result_.learned_clause_literals[0]);
		state_.UpdateMovingAveragesForRestarts(1);
	}
	else
	{
		//todo lbd could be computed as part of conflict analysis?
		//the zeroth literal is the asserting literal, no need to consider it in lbd computation
		int lbd = state_.ComputeLBD(&analysis_result_.learned_clause_literals[0] + 1, analysis_result_.learned_clause_literals.size() - 1);
		state_.UpdateMovingAveragesForRestarts(lbd);

		state_.Backtrack(analysis_result_.backtrack_level);
		state_.propagator_clausal_.AddLearnedClauseAndEnqueueAssertingLiteral(analysis_result_.learned_clause_literals, lbd);
	}
}

bool ConstraintSatisfactionSolver::ShouldRestart()
{ 
	//todo encapsulate the restart strategies within separate classes?
		
	//if already at the root, do not restart. Note that this affects the cleanup of learned clauses, but for now we ignore this
	if (state_.GetCurrentDecisionLevel() == 0) { return false; }
	//do not consider restarting if the minimum number of conflicts did not occur
	if (state_.counters_.num_conflicts_until_restart > 0) { return false; }

	if (internal_parameters_.restart_strategy_ == InternalParameters::RestartStrategy::LUBY
			|| internal_parameters_.restart_strategy_ == InternalParameters::RestartStrategy::CONSTANT)
	{
		return true;
	}

	//using the glucose restart strategy as described in "Evaluating CDCL Restart Schemes" -> currently using the original version with simple moving averages instead of the exponential decays
	runtime_assert(internal_parameters_.restart_strategy_ == InternalParameters::RestartStrategy::GLUCOSE);

	//should postpone the restart? 
	if (state_.counters_.num_conflicts >= 10000 && state_.GetNumberOfAssignedInternalBooleanVariables() > 1.4*state_.simple_moving_average_block.GetCurrentValue())
	{
		state_.counters_.blocked_restarts++;
		state_.counters_.num_conflicts_until_restart = internal_parameters_.num_min_conflicts_per_restart;
		state_.simple_moving_average_lbd.Reset();
		return false;
	}

	//is the solver learning "bad" clauses?
	if (state_.simple_moving_average_lbd.GetCurrentValue()*0.8 > state_.cumulative_moving_average_lbd.GetCurrentValue())
	{
		state_.simple_moving_average_lbd.Reset();
		state_.counters_.num_conflicts_until_restart = internal_parameters_.num_min_conflicts_per_restart;
		return true;
	}
	else
	{
		return false;
	}
}

void ConstraintSatisfactionSolver::PerformRestartDuringSearch()
{
	if (state_.propagator_clausal_.TemporaryClausesExceedLimit())
	{//currently clause clean up can only be done at the root level, so we use this workaround when using assumptions. Todo fix.
		state_.counters_.num_clause_cleanup++;
		state_.Backtrack(0);

		state_.propagator_clausal_.PromoteAndReduceTemporaryClauses();

		if (state_.propagator_clausal_.ShouldPerformGarbageCollection())
		{
			state_.propagator_clausal_.PerformSimplificationAndGarbageCollection();
		}
	}
	else
	{
		int restart_level = assumptions_.size();
		if (restart_level < state_.GetCurrentDecisionLevel())
		{
			state_.Backtrack(assumptions_.size());
		}		
	}

	state_.counters_.num_restarts++;

	if (internal_parameters_.restart_strategy_ == InternalParameters::RestartStrategy::LUBY)
	{
		state_.counters_.num_conflicts_until_restart = (luby_generator_.GetNextElement() * internal_parameters_.restart_coefficient_);
	}
	else if (internal_parameters_.restart_strategy_ == InternalParameters::RestartStrategy::CONSTANT)
	{
		state_.counters_.num_conflicts_until_restart = internal_parameters_.restart_coefficient_;
	}
	else if (internal_parameters_.restart_strategy_ == InternalParameters::RestartStrategy::GLUCOSE)
	{
		state_.counters_.num_conflicts_until_restart = internal_parameters_.num_min_conflicts_per_restart;
	}
	else
	{
		runtime_assert(1 == 2);
	}	
}

void ConstraintSatisfactionSolver::PerformRestartToRoot()
{//todo - much of this method is copy-pasted from PerformRestartDuringSearch
	state_.Backtrack(0);

	if (state_.propagator_clausal_.TemporaryClausesExceedLimit())
	{
		state_.counters_.num_clause_cleanup++;

		state_.propagator_clausal_.PromoteAndReduceTemporaryClauses();
	}

	state_.counters_.num_restarts++;

	if (internal_parameters_.restart_strategy_ == InternalParameters::RestartStrategy::LUBY)
	{
		state_.counters_.num_conflicts_until_restart = (luby_generator_.GetNextElement() * internal_parameters_.restart_coefficient_);
	}
	else if (internal_parameters_.restart_strategy_ == InternalParameters::RestartStrategy::CONSTANT)
	{
		state_.counters_.num_conflicts_until_restart = internal_parameters_.restart_coefficient_;
	}
	else if (internal_parameters_.restart_strategy_ == InternalParameters::RestartStrategy::GLUCOSE)
	{
		state_.counters_.num_conflicts_until_restart = internal_parameters_.num_min_conflicts_per_restart;
	}
	else
	{
		runtime_assert(1 == 2);
	}
}

void ConstraintSatisfactionSolver::PrintClause(Clause& c)
{
	for (int i = 0; i < c.Size(); i++)
	{
		std::cout << c[i].ToString() << "[";
		if (state_.assignments_.IsAssigned(c[i]) == false)
		{
			std::cout << "X] ";
		}
		else if (state_.assignments_.IsPropagated(c[i].Variable()))
		{
			std::cout << "p" << state_.assignments_.GetAssignmentLevel(c[i]) << "] ";
		}
		else
		{
			std::cout << "c" << state_.assignments_.GetAssignmentLevel(c[i]) << "] ";
		}
	}
	std::cout << "\n";
}

SolverOutput ConstraintSatisfactionSolver::GenerateOutput()
{
	if (!state_.IsAssignmentBuilt())
	{//if no solution has been built but the search completed before the timeout, unsatisfiability has been proven
		return SolverOutput(stopwatch_.TimeElapsedInSeconds(), !stopwatch_.IsWithinTimeLimit(), std::vector<int>(), -1, std::vector<BooleanLiteral>());
	}
	else
	{//note that if a solution has been found, it must have been done within the time limit
		return SolverOutput(stopwatch_.TimeElapsedInSeconds(), false, state_.GetOutputAssignment(), 0, std::vector<BooleanLiteral>());
	}
}

} //end Pumpkin namespace