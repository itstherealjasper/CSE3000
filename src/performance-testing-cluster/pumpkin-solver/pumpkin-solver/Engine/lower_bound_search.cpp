#include "lower_bound_search.h"
#include "preprocessor.h"
#include "../Utilities/stopwatch.h"
#include "../Utilities/runtime_assert.h"

#include <set>

namespace Pumpkin
{
LowerBoundSearch::LowerBoundSearch(ParameterHandler& parameters)
{
	std::string parameter_stratification = parameters.GetStringParameter("stratification");
	
	if (parameter_stratification == "off") { stratification_strategy_ = StratificationStrategy::OFF; }
	else if (parameter_stratification == "basic") { stratification_strategy_ = StratificationStrategy::BASIC; }
	else if (parameter_stratification == "ratio") { stratification_strategy_ = StratificationStrategy::RATIO; }
	else { std::cout << "Stratification strategy \"" << parameter_stratification << "\" unknown!\n"; exit(1); }

	std::string parameter_encoding = parameters.GetStringParameter("cardinality-encoding");
	if (parameter_encoding == "totaliser") { cardinality_constraint_encoding_ = CardinalityConstraintEncoding::TOTALISER; }
	else if (parameter_encoding == "cardinality-network") { cardinality_constraint_encoding_ = CardinalityConstraintEncoding::CARDINALITY_NETWORK; }
	else { std::cout << "Encoding \"" << parameter_encoding << "\" unknown!\n"; exit(1); }

	use_weight_aware_core_extraction_ = parameters.GetBooleanParameter("weight-aware-core-extraction");
}

bool LowerBoundSearch::Solve(ConstraintSatisfactionSolver& solver, LinearFunction& objective_function, SolutionTracker& solution_tracker, double time_limit_in_seconds)
{
	if (time_limit_in_seconds <= 0.001) { return false; }

	Preprocessor::RemoveFixedAssignmentsFromObjective(solver, objective_function);
	bool conflict_detected = Preprocessor::PruneDomainValueBasedOnUpperBound(solver, objective_function, solution_tracker.UpperBound());
	runtime_assert(!conflict_detected); //otherwise too easy, but could be the case. For now we declare an error since we do not expect this on our current instances

	InitialiseDataStructures(objective_function, solution_tracker, solver.state_);
		
	//if the solution is already optimal given the current objective, return
	if (internal_upper_bound_ == solver.ComputeSimpleLowerBound(objective_function)) { return true; }

	std::cout << "c core-guided time: " << time_limit_in_seconds << "\n";

	Stopwatch stopwatch(time_limit_in_seconds);

	int64_t weight_threshold = GetInitialWeightThreshold(solver.state_);
	std::cout << "c \tinitial stratification threshold: " << weight_threshold << "\n";

	while (weight_threshold > 0)
	{
		CoreGuidedSearchWithWeightThreshold(weight_threshold, stopwatch, solver, solution_tracker);
		weight_threshold = GetNextWeightThreshold(weight_threshold, solver.state_);
		std::cout << "c \tnew stratification threshold: " << weight_threshold << "\n";
	}	

	std::cout << "c card network time: " << cardinality_network_encoder_.TotalTimeSpent() << "\n";
	std::cout << "c clauses added: " << cardinality_network_encoder_.TotalNumClausesAdded() << "\n";

	objective_function = ConvertReformulatedObjectiveIntoLinearFunction(solver.state_);
	for (Term term : objective_function)
	{
		runtime_assert(solver.state_.IsAssigned(term.variable) == false);
		int m = 0;
	}
	return stopwatch.IsWithinTimeLimit();
}

int64_t LowerBoundSearch::GetInitialWeightThreshold(SolverState& state)
{
	switch (stratification_strategy_)
	{
		case StratificationStrategy::OFF:
		{
			return 1;
		}
		case StratificationStrategy::BASIC:
		{
			return MaxWeightInReformulatedObjective();
		}
		case StratificationStrategy::RATIO:
		{
			int64_t large_weight = MaxWeightInReformulatedObjective() + 1;
			return std::max(int64_t(1), GetNextWeightRatioStrategy(large_weight, state));
		}
		default:
		{
			runtime_assert(1 == 2); return -1;
		}
	}
}

int64_t LowerBoundSearch::GetNextWeightThreshold(int64_t current_weight_threshold, SolverState& state)
{
	switch (stratification_strategy_)
	{
		case StratificationStrategy::OFF:
			return 0;

		case StratificationStrategy::BASIC:
			return GetMaximumWeightSmallerThanThreshold(current_weight_threshold, state);

		case StratificationStrategy::RATIO:
			return GetNextWeightRatioStrategy(current_weight_threshold, state);

		default:
			runtime_assert(1 == 2); return -1;
	}
}

int64_t LowerBoundSearch::GetNextWeightRatioStrategy(int64_t previous_weight_threshold, SolverState& state)
{
	//trivial case, if there are no literals in the objective, return zero to signal nothing needs to be done
	if (reformulated_objective_variables_.empty()) { return 0; }
	//if we already tried every weight before, then return zero as the signal to stop
	if (previous_weight_threshold == 1) { return 0; }
	
	int64_t candidate_weight = previous_weight_threshold;
	std::set<int64_t> considered_weights;
	int num_literals_total = GetNumLiteralsWithWeightGreaterOrEqualToThreshold(0, state);
	int num_literals_old = GetNumLiteralsWithWeightGreaterOrEqualToThreshold(candidate_weight, state);
	int num_literals_new = -1;
	do
	{
		candidate_weight /= 10;//GetMaximumWeightSmallerThanThreshold(candidate_weight,  state);
		num_literals_new = GetNumLiteralsWithWeightGreaterOrEqualToThreshold(candidate_weight, state);

		/*for (auto &term : reformulated_objective_variables_)
		{
			int64_t weight = term.second.residual_weight;
			if (weight >= candidate_weight)
			{
				considered_weights.insert(weight);
			}
		}
		
		//if we hit the ratio, we found the new weight, otherwise reiterate
		if (double(num_literals_new) / considered_weights.size() >= 1.25)
		{ 
			return candidate_weight; 
		}*/
	} while (num_literals_old == num_literals_new && num_literals_new != num_literals_total); //todo it could be that the final division actually does not introduce anything new, and the optimal solution is found. This is probably rare.
	//if this return statement is reached, it means all literals are being considered
	return candidate_weight;
}

bool LowerBoundSearch::HardenReformulatedObjectiveFunction(SolverState& state)
{
	pumpkin_assert_simple(state.GetCurrentDecisionLevel() == 0, "Hardening only possible at the root level.");

	//hardening is effectively reasoning about the objective
	//	the objective implies the constraint \sum w_i * x_i < UB
	//	so we can compute the upper bound of each variable
	//	note that the reformulated objective does not contain simple variables, but instead each variable is given by \sum w_i min(b_i, max(x_i - a_i, 0)), todo explain better
	
	//todo should do hardening until a fixed point
	//	for now we only do it once since I think it is unlikely a second pass will do anything, but it could be that for instances it makes a difference

	int64_t hardening = 0;
	int64_t diff = internal_upper_bound_ - reformulated_constant_term_;
	for (auto &term : reformulated_objective_variables_)
	{
		IntegerVariable variable(term.first);
		//the goal is to determine the upper bound on the variable given the difference between the upper and lower bound
		//	note that this is not straight forward as in the linear sum objective, since the reformulated objective has those special terms that happen due to slicing
		int new_upper_bound = state.domain_manager_.GetLowerBound(variable);
		//is it possible to assign the next value?
		if (term.second.residual_weight <= diff) //normally we could use '<' here, but since we may be doing lexicographical optimisation, we need to use equality...todo think about how to do this in a better way
		{
			++new_upper_bound;
			//increasing the bound by one will incur only the residual weight (not the full weight)
			int64_t left_over = diff - term.second.residual_weight; 
			//increase the bound further incurs the full weight
			new_upper_bound += (left_over / term.second.full_weight);
			new_upper_bound = std::min(new_upper_bound, state.domain_manager_.GetUpperBound(variable)); //cap the upper bound to its maximum value
		}				
		
		int old_upper_bound = state.domain_manager_.GetUpperBound(variable);
		if (new_upper_bound < old_upper_bound)
		{
			//somewhat complicated way of evaluating the sum of the weights of the literals that are set to false
			if (new_upper_bound == state.domain_manager_.GetLowerBound(variable))
			{
				hardening += term.second.residual_weight;
				hardening += (old_upper_bound - new_upper_bound - 1) * term.second.full_weight;
			}
			else
			{
				hardening += (old_upper_bound - new_upper_bound) * term.second.full_weight;
			}

			bool conflict_detected = state.SetUpperBoundForVariable(variable, new_upper_bound);
			pumpkin_assert_permanent(!conflict_detected, "Error: hardening produced an unsat instances. This could happen (may not be an error in fact) but for now we abort since we do not expect this in our current instances.");
		}
	}

	if (hardening > 0) { std::cout << "c harden: " << hardening << "\n"; }

	RemoveRedundantTermsFromTheReformulatedObjective(state);	
	return false;
}

std::vector<BooleanLiteral> LowerBoundSearch::InitialiseAssumptions(int64_t weight_threshold, SolverState& state)
{
	static std::vector<BooleanLiteral> assumptions;
	assumptions.clear();
	for (auto &p: reformulated_objective_variables_)
	{
		if (p.second.residual_weight >= weight_threshold)
		{
			IntegerVariable var(p.first);
			BooleanLiteral lit = GetOptimisticAssumptionLiteralForVariable(var, state);
			assumptions.push_back(lit);
		}
	}
	return assumptions;
}

int64_t LowerBoundSearch::GetMinimumCoreWeight(std::vector<BooleanLiteral>& core_clause, SolverState& state)
{
	int64_t min_core_weight = INT64_MAX;
	for (BooleanLiteral core_literal : core_clause)
	{
		IntegerVariable var = state.GetLiteralInformation(core_literal).integer_variable;
		pumpkin_assert_simple(reformulated_objective_variables_.count(var.id) > 0, "Variable in core must be in the objective.");
		int64_t weight = reformulated_objective_variables_[var.id].residual_weight;
		
		min_core_weight = std::min(weight, min_core_weight);
	}
	pumpkin_assert_simple(min_core_weight != INT64_MAX, "No core weight found, strange.");
	return min_core_weight;
}

void LowerBoundSearch::ProcessCores(std::vector<std::vector<BooleanLiteral>>& core_clauses, std::vector<int64_t> core_weights, ConstraintSatisfactionSolver& solver)
{
	for (size_t i = 0; i < core_clauses.size(); i++)
	{
		std::vector<BooleanLiteral> soft_literals;
		
		if (cardinality_constraint_encoding_ == CardinalityConstraintEncoding::TOTALISER)
		{
			soft_literals = totaliser_encoder_.SoftLessOrEqual(core_clauses[i], 0, solver.state_);
		}
		else if (cardinality_constraint_encoding_ == CardinalityConstraintEncoding::CARDINALITY_NETWORK)
		{
			runtime_assert(1 == 2); //disabled for now
			//soft_literals = cardinality_network_encoder_.SoftLessOrEqual(core_clauses[i], 0, solver.state_);
		}
		else
		{
			runtime_assert(1 == 2);
		}

		IntegerVariable sum_variable = solver.state_.CreateNewSimpleBoundedSumVariable(soft_literals, 1);
		ReformulatedTerm term;
		term.full_weight = core_weights[i];
		term.residual_weight = core_weights[i];
		//the lower bound for the newly created variable is at least one
		//	but it could be that the bound is higher
		//	todo in the future consider doing additional work to increase the bound, but for now we only look at the trivial increase
		//todo I think the bound will never be greater than one without any additional lower bound computation
		int lb = solver.state_.domain_manager_.GetLowerBound(sum_variable);
		reformulated_constant_term_ += ((lb - 1) * term.full_weight); //minus one since slicing already took care of one unit into the objective function
		term.threshold = lb;

		reformulated_objective_variables_[sum_variable.id] = term;
		//note that we should not try to minimise the core at this point
		//	doing so may remove some literals from the core, which may have an impact on the core weight
		//	so minimisation could be done after the core is generated but not here

		//todo rethink when this bound should be computed -> perhaps initially when the core is generated?
	}
}

void LowerBoundSearch::FilterAssumptions(std::vector<BooleanLiteral>& assumptions, std::vector<BooleanLiteral>& core_clause, int64_t weight_threshold, SolverState& state)
{
	//an assumption literal may need to be filtered out for four reasons:
	// 	it was used as part of a core -> most common reason for filtering
	//	hardening removed the literal from the objective
	//	slicing reduced the weight below the weight threshold
	//	slicing increased the threshold to the upper bound, effectively making the literal redundant

	size_t new_size = 0, index = 0, num_found_assumptions = 0;
	while (index < assumptions.size())
	{
		BooleanLiteral assumption_literal = assumptions[index];
		IntegerVariable var = state.GetLiteralInformation(assumption_literal).integer_variable;
		
		//keep the literal if all criteria is met
		if (std::find(core_clause.begin(), core_clause.end(), ~assumption_literal) == core_clause.end()
			&& !state.assignments_.IsAssigned(assumption_literal)
			&& reformulated_objective_variables_.count(var.id) > 0 
			&& reformulated_objective_variables_[var.id].residual_weight >= weight_threshold)
		{
			assumptions[new_size] = assumptions[index];
			++new_size;
		}
		++index;
	}
	assumptions.resize(new_size);
}

void LowerBoundSearch::InitialiseDataStructures(const LinearFunction& objective_function, SolutionTracker& solution_tracker, SolverState& state)
{
	reformulated_objective_variables_.clear();
	reformulated_constant_term_ = objective_function.GetConstantTerm();
	//if the best solution stored is already at the lower bound of the given objective function, the solution is optimal and no need to go further
	//	here we assume that the objective function provided may be different than the objective in the solution tracker
	internal_upper_bound_ = objective_function.Evaluate(solution_tracker.GetBestSolution());
	for (Term term : objective_function)
	{
		pumpkin_assert_simple(!state.IsAssigned(term.variable), "Error: the objective function should not contain assigned variables?");
		int64_t weight = objective_function.GetWeight(term.variable);
		pumpkin_assert_simple(weight > 0, "Error: the objective function is expected to only contain positive weights?");

		ReformulatedTerm new_term;
		new_term.threshold = state.domain_manager_.GetLowerBound(term.variable);
		new_term.full_weight = weight;
		new_term.residual_weight = weight;
		reformulated_objective_variables_[term.variable.id] = new_term;
	}
}

BooleanLiteral LowerBoundSearch::GetOptimisticAssumptionLiteralForVariable(IntegerVariable variable, SolverState& state)
{
	pumpkin_assert_simple(reformulated_objective_variables_.count(variable.id) > 0, "Optimistic variable must be in the objective.");
	
	auto &iter = reformulated_objective_variables_[variable.id];
	BooleanLiteral assumption_literal = state.GetUpperBoundLiteral(variable, iter.threshold);
	
	int ub = state.domain_manager_.GetUpperBound(variable);
	int lb = state.domain_manager_.GetLowerBound(variable);

	pumpkin_assert_simple(!state.assignments_.IsAssigned(assumption_literal), "Error: assumption literal already assigned?");
	return assumption_literal;
}

void LowerBoundSearch::PerformSlicingStep(std::vector<BooleanLiteral>& core_clause, int64_t core_weight, SolverState& state)
{
	reformulated_constant_term_ += core_weight;

	for (BooleanLiteral core_literal : core_clause)
	{
		IntegerVariable var = state.GetLiteralInformation(core_literal).integer_variable;
		
		auto term = reformulated_objective_variables_.find(var.id);
		pumpkin_assert_simple(term != reformulated_objective_variables_.end(), "Core variable must also be in the objective.");
		
		int64_t current_weight = term->second.residual_weight;
		pumpkin_assert_simple(current_weight >= core_weight, "Individual core weights cannot be smaller than the core weight."); //sanity check
		
		term->second.residual_weight -= core_weight;
		//if the weight is now zero, bump the core lower bound for the variable and reset the weight
		if (term->second.residual_weight == 0)
		{
			do
			{//since the next threshold value may not be in the domain of the variable, we need to iterate until we do find a potential feasible assignment value
				term->second.threshold++;
			}while (term->second.threshold != state.domain_manager_.GetUpperBound(var) &&
				state.assignments_.IsAssigned(state.GetUpperBoundLiteral(var, term->second.threshold)));
			
			//if the variable can still be used as part of a core, reset its weight
			if (term->second.threshold != state.domain_manager_.GetUpperBound(var))
			{
				pumpkin_assert_simple(term->second.threshold < state.domain_manager_.GetUpperBound(var), "Threshold for core-guided variable cannot exceeds its upper bound.");
				term->second.residual_weight = term->second.full_weight;
			}
			//otherwise remove the variable from the objective
			//	note that keeping the variable in the objective with weight zero is not the same as remove it, since we assume that everything in the objective has a nonnegative weight
			else
			{
				reformulated_objective_variables_.erase(term);
			}
		}		
	}
}

int64_t LowerBoundSearch::EvaluateReformulatedObjectiveValue(const IntegerAssignmentVector& assignment)
{
	int64_t cost = reformulated_constant_term_;
	for (auto& term : reformulated_objective_variables_)
	{
		IntegerVariable var(term.first);
		int value = assignment[var];
		if (value > term.second.threshold)
		{
			cost += term.second.residual_weight;
			cost += (term.second.full_weight * (value - term.second.threshold - 1)); //minus one because to avoid counting for the residual weight
		}
	}
	return cost;
}

int LowerBoundSearch::GetNumLiteralsWithWeightGreaterOrEqualToThreshold(int64_t threshold, SolverState& state)
{
	pumpkin_assert_simple(state.GetCurrentDecisionLevel() == 0, "Only possible at the root level");

	int num_literals = 0;
	for (auto &term : reformulated_objective_variables_)
	{
		pumpkin_assert_simple(term.second.residual_weight > 0, "Residual weight cannot be negative."); //for now all negative weighted terms are converted in preprocessing to positive weights

		if (term.second.residual_weight >= threshold)
		{
			IntegerVariable var(term.first);
			int lb = state.domain_manager_.GetLowerBound(var);
			int ub = state.domain_manager_.GetUpperBound(var);
			for (int val = lb + 1; val <= ub; val++) //plus one since there is effectively no penalty for setting the variable to its lower bound
			{
				//only count values in the domain that may be assigned
				num_literals += state.domain_manager_.IsInDomain(var, val);
			}
		}
	}
	return num_literals;
}

int64_t LowerBoundSearch::GetMaximumWeightSmallerThanThreshold(int64_t threshold, SolverState& state)
{
	int64_t target_weight = 0;
	for (auto &term : reformulated_objective_variables_)
	{
		int64_t weight = term.second.residual_weight;
		if (weight < threshold && weight > target_weight)
		{
			target_weight = weight;
		}
	}
	return target_weight;
}

void LowerBoundSearch::RemoveRedundantTermsFromTheReformulatedObjective(SolverState& state)
{
	pumpkin_assert_simple(state.GetCurrentDecisionLevel() == 0, "Only possible at the root level");

	static std::vector<IntegerVariable> variables_to_remove;
	variables_to_remove.clear();
	int64_t lb_increase = 0;
	//we first collect the variables that need to be removed from the objective
	//and then in next loop do we remove them
	//	this must be done in two loops since while we are iterating through the terms we should not change their value
	for (auto &term : reformulated_objective_variables_)
	{
		IntegerVariable variable(term.first);
		//not sure about this assert, seems wrong -> pumpkin_assert_simple(term.second.threshold != state.domain_manager_.GetUpperBound(variable) && term.second.residual_weight > 0); //for now we do not consider negative terms...
		//assigned variables are effectively constants and may be removed
		if (state.IsAssigned(variable))
		{
			int diff = state.GetIntegerAssignment(variable) - term.second.threshold;
			pumpkin_assert_simple(diff >= 0, "Sanity check.");
			if (diff > 0)
			{
				lb_increase += term.second.residual_weight;
				lb_increase += (term.second.full_weight * (diff - 1));
			}
			variables_to_remove.push_back(variable); //make note of the variable so that it can be removed afterwards			
		}
		//variables that are already at their threshold value do not contribute towards the objective anymore
		else if (term.second.threshold == state.domain_manager_.GetUpperBound(variable))
		{
			variables_to_remove.push_back(variable);
		}
	}
	//now actually remove the variables
	for (IntegerVariable variable : variables_to_remove) 
	{ 
		auto iter = reformulated_objective_variables_.find(variable.id);
		pumpkin_assert_simple(iter != reformulated_objective_variables_.end(), "Sanity check");
		reformulated_objective_variables_.erase(iter);
		pumpkin_assert_simple(reformulated_objective_variables_.find(variable.id) == reformulated_objective_variables_.end(), "Sanity check");
	}

	reformulated_constant_term_ += lb_increase;

	if (lb_increase > 0) { std::cout << "c preprocessor trivial lb increase " << lb_increase << ", removed " << variables_to_remove.size() << " variables\n"; }
}

int64_t LowerBoundSearch::MaxWeightInReformulatedObjective() const
{
	int64_t max_weight = 0;
	for (auto& term : reformulated_objective_variables_)
	{
		max_weight = std::max(max_weight, term.second.residual_weight);
	}
	return max_weight;
}

IntegerAssignmentVector LowerBoundSearch::ComputeExtendedSolution(const IntegerAssignmentVector& reference_solution, ConstraintSatisfactionSolver& solver, double time_limit_in_seconds)
{
	std::vector<BooleanLiteral> assumptions;
	for (int i = 0; i < reference_solution.NumVariables(); i++)
	{
		IntegerVariable variable(i + 1);
		int value = reference_solution[variable];

		BooleanLiteral lit_lb = solver.state_.GetLowerBoundLiteral(variable, value);
		BooleanLiteral lit_ub = solver.state_.GetUpperBoundLiteral(variable, value);
		assumptions.push_back(lit_lb);
		assumptions.push_back(lit_ub);
	}
	SolverOutput output = solver.Solve(assumptions, time_limit_in_seconds);
	runtime_assert(output.HasSolution()); //although it could be that the solver timeout, for now we consider this strange
	return output.solution;
}

LinearFunction LowerBoundSearch::ConvertReformulatedObjectiveIntoLinearFunction(SolverState& state)
{
	//need to create those new variables that capture the min/max behaviour
	//	similar to the 'view' variables but not quite
	HardenReformulatedObjectiveFunction(state);

	LinearFunction new_objective;
	new_objective.AddConstantTerm(reformulated_constant_term_);
	for (auto& term : reformulated_objective_variables_)
	{
		IntegerVariable variable(term.first);
		pumpkin_assert_simple(!state.IsAssigned(variable), "Sanity check");

		int lb = state.domain_manager_.GetLowerBound(variable);
		int ub = state.domain_manager_.GetUpperBound(variable);
		int penalty_threshold = term.second.threshold;
		pumpkin_assert_simple(lb <= penalty_threshold, "Sanity check");

		//in case part of the weight was sliced off
		//	then create a new variable to capture the remaining weight
		if (term.second.residual_weight != term.second.full_weight)
		{
			BooleanLiteral ref_lit = state.GetLowerBoundLiteral(variable, penalty_threshold+1);
			IntegerVariable new_variable = state.CreateNewEquivalentVariable(ref_lit);
			pumpkin_assert_simple(!state.IsAssigned(new_variable), "Sanity check");
			new_objective.AddTerm(new_variable, term.second.residual_weight);
			++penalty_threshold;
		}

		//at this point, we only need to consider the full weight starting from the penalty threshold
		//	simplest case: the variable can be directly used in the new objective
		if (lb == penalty_threshold)
		{
			new_objective.AddTerm(variable, term.second.full_weight);
		}
		//	otherwise need to create a new variable to penalise only some assignments
		else if (ub != penalty_threshold)
		{
			IntegerVariable new_variable = state.CreateNewThresholdExceedingVariable(variable, penalty_threshold);
			new_objective.AddTerm(new_variable, term.second.full_weight);
		}
	}
	return new_objective;
}

void LowerBoundSearch::CoreGuidedSearchWithWeightThreshold(int64_t weight_threshold, Stopwatch& stopwatch, ConstraintSatisfactionSolver& solver, SolutionTracker& solution_tracker)
{
	pumpkin_assert_simple(weight_threshold > 0 && solution_tracker.HasFeasibleSolution(), "Sanity check");

	if (stopwatch.TimeLeftInSeconds() <= 0.001) { return; }
	//if the best solution stored is already at the lower bound of the given objective function, the solution is optimal and no need to go further
	//	here we assume that the objective function provided may be different than the objective in the solution tracker
	if (internal_upper_bound_ == reformulated_constant_term_) { return; }

	std::vector<BooleanLiteral> assumptions;
	std::vector<std::vector<BooleanLiteral>> cores;
	std::vector<int64_t> core_weights;
	SolverOutput output;

	while (stopwatch.IsWithinTimeLimit() && reformulated_constant_term_ != internal_upper_bound_ && !solution_tracker.HasOptimalSolution())
	{
		cores.clear();
		core_weights.clear();

		HardenReformulatedObjectiveFunction(solver.state_); //we keep this for now, but in principle could be merged with assumption initialisation
		assumptions = InitialiseAssumptions(weight_threshold, solver.state_);
		int64_t lb_increase_in_iteration = 0;

		do
		{
			output = solver.Solve(assumptions, stopwatch.TimeLeftInSeconds());

			if (!stopwatch.IsWithinTimeLimit()) { std::cout << "c core-guided timeout!\n"; break; } //important to break and not to return since the instances still needs to be reformulated according to the cores

			if (output.HasSolution())
			{
				std::cout << "c core-guided found a solution\n";
				pumpkin_assert_moderate(totaliser_encoder_.DebugCheckSatisfactionOfEncodedConstraints(output.solution, solver.state_), "Sanity check.");
				solution_tracker.UpdateBestSolution(output.solution);
				
				bool conflict_detected = HardenReformulatedObjectiveFunction(solver.state_);
				runtime_assert(!conflict_detected);
				
				assumptions.clear();				
			}
			else if (output.core_clause.empty() && output.ProvenInfeasible()) //this can no longer happen in the current version, see below
			{
				pumpkin_assert_permanent(1==2, "Error: core-guided search proved unsatisfiabiltiy with an empty core, this should not happen?"); //cannot happen anymore, todo fix this part, hardening does not take into account the >= sign, only >, because of lexicographical optimisation. For 'standard' optimisation it should be fine to do >=, todo think about how to resolve this better
				//getting here means that the best solution found is the optimal solution
				/*int64_t lower_bound_update = solution_tracker.UpperBound() - objective_function.GetConstantTerm();
				objective_function.IncreaseConstantTerm(lower_bound_update);

				std::cout << "c unsat during core guided; optimal found\n";
				std::cout << "c LB = " << objective_function.GetConstantTerm() << "\n";
				return; //stop further search*/
			}
			else
			{
				std::cout << "c core size: " << output.core_clause.size() << "\n";
				std::cout << "c t = " << stopwatch.TimeElapsedInSeconds() << "\n";

				//some core post-processing TODO
				//	increase lower bounds of the core
				//	try to remove literals -> 
				//		for example, try to get a core using only the literal that would lead to a worse solution if they are set 
				//		-> in principle this can be handled by hardening but then the solver may get into an UNSAT state which may be a problem with lexicographical optimisation
				
				pumpkin_assert_simple(output.core_clause.size() > 0, "Sanity check");
				int64_t core_weight = GetMinimumCoreWeight(output.core_clause, solver.state_);

				cores.push_back(output.core_clause);
				core_weights.push_back(core_weight);
				
				PerformSlicingStep(output.core_clause, core_weight, solver.state_);
				lb_increase_in_iteration += core_weight;
				
				bool conflict_detected = HardenReformulatedObjectiveFunction(solver.state_);
				runtime_assert(!conflict_detected);			

				FilterAssumptions(assumptions, output.core_clause, weight_threshold, solver.state_);
				std::cout << "c LB increase = " << lb_increase_in_iteration << "; num_assump = " << assumptions.size() << "\n";
				std::cout << "c \tLB = " << solution_tracker.LowerBound() + reformulated_constant_term_ << "\n";
			}
		} while (use_weight_aware_core_extraction_ 
					&& !assumptions.empty() 
					&& reformulated_constant_term_ != internal_upper_bound_
					&& !solution_tracker.HasOptimalSolution());

		if (cores.empty()) { break; }

		ProcessCores(cores, core_weights, solver);	

		//in case a solution has been computed, then we may be able to update the internal upper bound
		if (output.HasSolution()) 
		{	
			auto solution = ComputeExtendedSolution(output.solution, solver, stopwatch.TimeLeftInSeconds()); //need to extend the solution since reformulating with cores introduces new variables
			internal_upper_bound_ = EvaluateReformulatedObjectiveValue(solution);
		}
	}
}

}