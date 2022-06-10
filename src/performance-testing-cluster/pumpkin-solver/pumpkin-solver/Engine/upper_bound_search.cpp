#include "upper_bound_search.h"
#include "preprocessor.h"
#include "../Utilities/stopwatch.h"

#include <fstream>

namespace Pumpkin
{
UpperBoundSearch::UpperBoundSearch(SolverState& state, ParameterHandler& parameters):
	ub_prop_(0)
{
	use_ub_prop_ = parameters.GetBooleanParameter("ub-propagator");

	std::string varying_resolution_parameter = parameters.GetStringParameter("varying-resolution");
	if (varying_resolution_parameter == "off") { varying_resolution_strategy_ = VaryingResolutionStrategy::OFF; }
	else if (varying_resolution_parameter == "basic") { varying_resolution_strategy_ = VaryingResolutionStrategy::BASIC; }
	else if (varying_resolution_parameter == "ratio") { varying_resolution_strategy_ = VaryingResolutionStrategy::RATIO; }
	else { std::cout << "Varying resolution parameter \"" << varying_resolution_parameter << "\" unknown!\n"; exit(1); }

	std::string value_selection_parameter = parameters.GetStringParameter("value-selection");
	if (value_selection_parameter == "phase-saving") { value_selection_strategy_ = ValueSelectionStrategy::PHASE_SAVING; }
	else if (value_selection_parameter == "solution-guided-search") { value_selection_strategy_ = ValueSelectionStrategy::SOLUTION_GUIDED_SEARCH; }
	else if (value_selection_parameter == "optimistic") { value_selection_strategy_ = ValueSelectionStrategy::OPTIMISTIC; }
	else if (value_selection_parameter == "optimistic-aux") { value_selection_strategy_ = ValueSelectionStrategy::OPTIMISTIC_AUX; }
	else { std::cout << "Phase saving parameter \"" << value_selection_parameter << "\" unknown!\n"; exit(1); }
}

bool UpperBoundSearch::Solve(ConstraintSatisfactionSolver& solver, LinearFunction& objective_function, SolutionTracker& solution_tracker, double time_limit_in_seconds)
{
	solution_tracker.ExtendBestSolution(ComputeExtendedSolution(solution_tracker.GetBestSolution(), solver, time_limit_in_seconds));

	//if the best solution stored is already at the lower bound of the given objective function, the solution is optimal and no need to go further
	if (objective_function.Evaluate(solution_tracker.GetBestSolution()) == solver.ComputeSimpleLowerBound(objective_function)) { return true; }

	if (time_limit_in_seconds <= 0.001) { return false; }		

	Stopwatch stopwatch(time_limit_in_seconds);

	solver.state_.SetStateResetPoint();
	int64_t division_coefficient = GetInitialDivisionCoefficient(objective_function, solver.state_); //todo - I think when choosing the coefficient, we should take into account the solution we currently have. If say the coefficient is selected in a way that no literals are violating with weight greater than the coefficient, than there is no point is selecting that weight.
	while (division_coefficient > 0 && stopwatch.IsWithinTimeLimit())
	{
		// std::cout << "c division coefficient: " << division_coefficient << "\n";

		solver.state_.PerformStateReset();
		pseudo_boolean_encoder_.Clear();

		LinearFunction simplified_objective = GetVaryingResolutionObjective(division_coefficient, objective_function, solver.state_);

		LinearSearch(solver, simplified_objective, solution_tracker, stopwatch.TimeLeftInSeconds());

		if (solution_tracker.HasOptimalSolution()) { break; }
		
		division_coefficient = GetNextDivisionCoefficient(division_coefficient, objective_function, solver.state_);
	}
	return stopwatch.IsWithinTimeLimit();
}

void UpperBoundSearch::LinearSearch(ConstraintSatisfactionSolver& solver, LinearFunction& objective_function, SolutionTracker& solution_tracker, double time_limit_in_seconds)
{
	runtime_assert(solution_tracker.HasFeasibleSolution() && objective_function.GetConstantTerm() == 0); //due to varying resolution we always have a zero constant term
	
	Stopwatch stopwatch(time_limit_in_seconds);

	// std::cout << "c linear search time: " << time_limit_in_seconds << "\n";	
	
	if (time_limit_in_seconds <= 0.001) { return; }

	solution_tracker.ExtendBestSolution(ComputeExtendedSolution(solution_tracker.GetBestSolution(), solver, stopwatch.TimeLeftInSeconds())); //we extend the solution to take into account auxiliary variables introduced by the core-guided phase
	int64_t internal_upper_bound = objective_function.Evaluate(solution_tracker.GetBestSolution()); //this is the bound that will be used to update the upper bound. Note that when using varying resolution, the objective function provided might not be the same one used in the solution_tracker, so we need to track the internal upper bound separately

	Preprocessor::RemoveFixedAssignmentsFromObjective(solver, objective_function);
	bool conflict_detected = Preprocessor::PruneDomainValueBasedOnUpperBound(solver, objective_function, internal_upper_bound);
	runtime_assert(!conflict_detected); //I think this never happens but could happen

	int internal_lower_bound = solver.ComputeSimpleLowerBound(objective_function);

	//if the best solution stored is already at the lower bound of the input objective, then there is nothing to be done
	//	todo might be useful to consider solutions of previous iterations that are not optimal with respect to the solution_tracker but may be optimal with respect to the input objective
	if (internal_upper_bound == internal_lower_bound) { return; }

	int initial_fixed_cost = ComputeFixedCost(solver, objective_function);
	EncodingOutput encoding_output = EncodeInitialUpperBound
													(
														solver,
														objective_function,
														internal_upper_bound - 1,
														time_limit_in_seconds
													);

	if (encoding_output.status.conflict_detected) { return; }
		
	std::vector<PairWeightLiteral>& sum_literals = encoding_output.weighted_literals;

	SetValueSelectorValues(solver, objective_function, solution_tracker.GetBestSolution());

	//todo check the stopping condition
	while (stopwatch.IsWithinTimeLimit() && internal_upper_bound > internal_lower_bound && !solution_tracker.HasOptimalSolution())
	{
		SolverOutput output = solver.Solve(stopwatch.TimeLeftInSeconds());
		if (output.HasSolution())
		{
			runtime_assert(pseudo_boolean_encoder_.DebugCheckSatisfactionOfEncodedConstraints(output.solution));

			int64_t new_internal_upper_bound = objective_function.Evaluate(output.solution);
			runtime_assert(new_internal_upper_bound < internal_upper_bound); //the internal upper bound must always decrease
			internal_upper_bound = new_internal_upper_bound;
						
			solution_tracker.UpdateBestSolution(output.solution); //note that this update may fail if we introduced auxiliary variables during the core-guided phase: the aux variables might be set to higher values than they should, and setting them to zero does not change the original solution

			SetValueSelectorValues(solver, objective_function, output.solution);

			int new_upper_bound_on_free_terms = new_internal_upper_bound - initial_fixed_cost - 1; //in case there is a constant term in the objective (e.g., as a result of preprocessing or core-guided search [check whether core-guided search lb gets into the objective?]), then it needs to be taken into account
			if (new_upper_bound_on_free_terms < 0) { break; }
			bool conflict_detected = StrengthenUpperBound
									(
										sum_literals,
										new_upper_bound_on_free_terms,
										objective_function,
										solver
									);
			if (conflict_detected){ break; }
		}
		else if (output.ProvenInfeasible())
		{
			break;
		}
	}
	if (ub_prop_ != NULL) { delete ub_prop_; ub_prop_ = NULL; }
}

EncodingOutput UpperBoundSearch::EncodeInitialUpperBound(ConstraintSatisfactionSolver& solver, LinearFunction& objective_function, int64_t upper_bound, double time_limit_in_seconds)
{
	int64_t fixed_cost = ComputeFixedCost(solver, objective_function);
	runtime_assert(upper_bound >= fixed_cost); //sanity check - todo find a better solution than crashing if this does not hold

	// std::cout << "c linear search initial bound " << upper_bound << "\n";

	if (use_ub_prop_)
	{
		std::cout << "c using propagator for the upper bound constraint\n";

		std::vector<IntegerVariable> vars;
		std::vector<int64_t> coefs;
		int64_t rhs = -upper_bound;
		for (const Term& term : objective_function)
		{
			vars.push_back(term.variable);
			coefs.push_back(-term.weight);
		}
		ub_prop_ = new LinearIntegerInequalityPropagator(vars, coefs, rhs);
		solver.state_.AddPropagatorCP(ub_prop_);
		//bool success = true;// upper_bound_propagator_->NotifyDomainChange(objective_function, upper_bound);
		//runtime_assert(success); //not necessarily needs to hold but if even posting the constraint is unsat, then it is probably due to a bug
		return EncodingOutput();
	}

	int num_free_terms = 0;
	for (Term term : objective_function) { num_free_terms += (!solver.state_.IsAssigned(term.variable)); }

	//the encoder does not properly handle integer variables, which is especially strange when there is only one integer in the objective
	//	so for now we handle this special case manually, and todo hopefully in the future do it properly
	if (num_free_terms == 1)
	{
		std::cout << "c encoding used for the objective but only one free integer in the objective\n";
		Term objective_term;
		for (Term term : objective_function) { if (!solver.state_.IsAssigned(term.variable)) { objective_term = term; break; } }
		runtime_assert(!objective_term.variable.IsNull()); //sanity check
		int fixed_cost = ComputeFixedCost(solver, objective_function);//note that this may not be the same as the lower bound of the objective term in case there are other variables in the objective that are assigned - however I suppose these could be preprocessed...todo check
		if (upper_bound < solver.state_.domain_manager_.GetUpperBound(objective_term.variable))
		{
			bool conflict_detected = solver.state_.SetUpperBoundForVariable(objective_term.variable, upper_bound);
			runtime_assert(!conflict_detected); //does not need to be the case in general but for now we assume it is...todo
		}
		std::vector<PairWeightLiteral> output_literals;
		int obj_var_lb = solver.state_.domain_manager_.GetLowerBound(objective_term.variable);
		for (int val = obj_var_lb+1; val <= upper_bound; val++)
		{
			BooleanLiteral lit = solver.state_.GetLowerBoundLiteral(objective_term.variable, val);
			PairWeightLiteral p(lit, val * objective_term.weight - fixed_cost);
			output_literals.push_back(p);
		}
		EncodingOutput output;
		output.weighted_literals = output_literals;
		return output;
	}
	//for now the encoding time does not care about the stopwatch...need to terminate if we go overtime
	//for the time being we encode the objective using pseudo-Boolean encoders
	//	would be better to use encoders that take into account integer variables natively
	Stopwatch stopwatch(time_limit_in_seconds);
	std::vector<PairWeightLiteral> weighted_literals;
	for (Term term : objective_function) 
	{ 
		int lb = solver.state_.domain_manager_.GetLowerBound(term.variable);
		int ub = solver.state_.domain_manager_.GetUpperBound(term.variable);
		for (int value = lb+1; value <= ub; value++)
		{
			BooleanLiteral literal = solver.state_.GetLowerBoundLiteral(term.variable, value);
			weighted_literals.push_back(PairWeightLiteral(literal, term.weight));
		}
	}

	bool blocks = false;
	if (blocks)
	{
		std::vector<int> literal_weights;
		std::vector<bool> literal_seen;
		for (Term term : objective_function)
		{
			int lb = solver.state_.domain_manager_.GetLowerBound(term.variable);
			int ub = solver.state_.domain_manager_.GetUpperBound(term.variable);
			for (int value = lb + 1; value <= ub; value++)
			{
				BooleanLiteral literal = solver.state_.GetLowerBoundLiteral(term.variable, value);
				int32_t code = literal.ToPositiveInteger();
				if (code >= literal_weights.size())
				{
					literal_weights.resize(code + 1, 0);
					literal_seen.resize(code + 1, false);
				}
				literal_weights[code] = term.weight;
			}
		}

		int counter_num_added = 0;

		std::ifstream input("real-schedule-stripped.txt-integers.txt");
		runtime_assert(input);
		std::vector<std::vector<PairWeightLiteral> > blocks;
		int num_integers;
		input >> num_integers;
		std::vector<PairWeightLiteral> block;
		for (int i = 0; i < num_integers; i++)
		{
			int size;
			input >> size;
			block.clear();
			for (int j = 0; j < size; j++)
			{
				int32_t lit_code;
				input >> lit_code;
				PairWeightLiteral p;
				p.literal = BooleanLiteral::CreateLiteralFromCodeAndFlag(lit_code, false);

				if (lit_code >= literal_weights.size()) { continue; }

				runtime_assert(lit_code < literal_weights.size()); //cannot have an unknown weighted literal
				if (literal_weights[lit_code] == 0)
				{
					runtime_assert(block.size() == 0); //sanity check, only the starting literals can be zero-weighted
					continue;
				}
				runtime_assert(literal_seen[lit_code] == false);

				literal_seen[lit_code] = true;

				p.weight = literal_weights[lit_code];

				/*if (block.size() > 0)
				{
					runtime_assert(literal_weights[lit_code] <= literal_weights[block.back().literal.ToPositiveInteger()]);
					p.weight += block.back().weight;
				}*/
				counter_num_added++;
				block.push_back(p);
			}

			std::reverse(block.begin(), block.end());

			//set the partial sums properly
			for (int m = 1; m < block.size(); m++)
			{
				block[m].weight += block[m-1].weight;
			}

			if (block.size() > 0)
			{
				blocks.push_back(block);
			}				
		}

		std::cout << "c READ AND ADDED INTEGERS from file\n";

		//now add the remaining literals that are not in those blocks
		for (Term term : objective_function)
		{
			int lb = solver.state_.domain_manager_.GetLowerBound(term.variable);
			int ub = solver.state_.domain_manager_.GetUpperBound(term.variable);
			for (int value = lb + 1; value <= ub; value++)
			{
				BooleanLiteral literal = solver.state_.GetLowerBoundLiteral(term.variable, value);
				int32_t code = literal.ToPositiveInteger();
				if (!literal_seen[code] && literal_weights[code] > 0)
				{
					literal_seen[code] = true;
					std::vector<PairWeightLiteral> unit_block(1);
					unit_block[0].literal = literal;
					unit_block[0].weight = literal_weights[code];
					blocks.push_back(unit_block);
				}
			}
		}
					
		std::cout << "c num blocks: " << blocks.size() << "\n";
		runtime_assert(1 == 2); //disabled for now, need to fix with the new version
		//std::vector<PairWeightLiteral> output_literals = pseudo_boolean_encoder_.HardLessOrEqualBlocks(blocks, upper_bound - fixed_cost, solver.state_);
		//std::cout << "c encoding took " << stopwatch.TimeElapsedInSeconds() << " seconds; " << output_literals.size() << " output literals\n";
		//return output_literals;
	}
	
	// std::cout << "c num weighted lits: " << weighted_literals.size() << "\n";
	EncodingOutput encoding_output = pseudo_boolean_encoder_.HardLessOrEqual(weighted_literals, upper_bound - fixed_cost, solver.state_);
	// std::cout << "c encoding took " << stopwatch.TimeElapsedInSeconds() << " seconds; " << encoding_output.weighted_literals.size() << " output literals\n";
	return encoding_output;
}

IntegerAssignmentVector UpperBoundSearch::ComputeExtendedSolution(const IntegerAssignmentVector& reference_solution, ConstraintSatisfactionSolver& solver, double time_limit_in_seconds)
{
	std::vector<BooleanLiteral> assumptions;
	for (int i = 0; i < reference_solution.NumVariables(); i++)
	{
		IntegerVariable variable(i + 1);
		BooleanLiteral lit_lb = solver.state_.GetLowerBoundLiteral(variable, reference_solution[variable]);
		BooleanLiteral lit_ub = solver.state_.GetUpperBoundLiteral(variable, reference_solution[variable]);
		assumptions.push_back(lit_lb);
		assumptions.push_back(lit_ub);
	}
	SolverOutput output = solver.Solve(assumptions, time_limit_in_seconds);
	runtime_assert(output.HasSolution()); //although it could be that the solver timeout, for now we consider this strange
	return output.solution;
}

bool UpperBoundSearch::StrengthenUpperBound(const std::vector<PairWeightLiteral>& sum_literals, int64_t upper_bound, LinearFunction& objective_function, ConstraintSatisfactionSolver& solver)
{
	if (use_ub_prop_)
	{
		//for simplicity we simply destroy the previous propagator and make a new one
		//	todo in the future should be able to update the right hand side in the propagator directly
		pumpkin_assert_simple(ub_prop_, "Sanity check");
		pumpkin_assert_simple(objective_function.GetConstantTerm() == 0, "Fix this, only works if no lower bound.");
		
		solver.state_.RemovePropagatorCP(ub_prop_);

		std::vector<IntegerVariable> vars;
		std::vector<int64_t> coefs;
		int64_t rhs = -upper_bound;
		for (const Term& term : objective_function)
		{
			vars.push_back(term.variable);
			coefs.push_back(-term.weight);
		}
		delete ub_prop_;
		ub_prop_ = 0;
		ub_prop_ = new LinearIntegerInequalityPropagator(vars, coefs, rhs);
		solver.state_.AddPropagatorCP(ub_prop_);
		
		bool conflict_detected = false; //todo fix this, should detect root issues, testing for now
		//bool conflict_detected = upper_bound_propagator_->StrengthenUpperBound(upper_bound);
		return conflict_detected;
	}

	//todo we may be trying to readd unit clauses from previous iterations; fix
	bool added_unit_clause = false;
	for (auto iter = sum_literals.rbegin(); iter != sum_literals.rend(); ++iter)
	{
		if (iter->weight > upper_bound)
		{
			added_unit_clause |= (!solver.state_.assignments_.IsAssigned(iter->literal));

			/*if (!solver.state_.assignments_.IsAssigned(iter->literal))
			{
				std::cout << "c added lit: " << solver.PrettyPrintLiteral(~iter->literal) << "\n";
			}*/

			bool conflict_detected = solver.state_.propagator_clausal_.AddUnitClause(~iter->literal);
			if (conflict_detected) { return true; }
		}
	}
	runtime_assert(added_unit_clause);
	return false; //no conflicts detected
}

int64_t UpperBoundSearch::ComputeFixedCost(ConstraintSatisfactionSolver& solver, LinearFunction& objective_function)
{
	int64_t fixed_cost = objective_function.GetConstantTerm();
	for (Term term : objective_function)
	{
		int lb = (term.weight * solver.state_.domain_manager_.GetLowerBound(term.variable));
		fixed_cost += lb;
	}
	return fixed_cost;
}

void UpperBoundSearch::SetValueSelectorValues(ConstraintSatisfactionSolver& solver, LinearFunction& objective_function, const IntegerAssignmentVector& solution)
{
	switch (value_selection_strategy_)
	{
		case Pumpkin::UpperBoundSearch::ValueSelectionStrategy::PHASE_SAVING:
		{//do nothing
			return;
		}
		case Pumpkin::UpperBoundSearch::ValueSelectionStrategy::SOLUTION_GUIDED_SEARCH:
		{//set the values according to the best solution
			BooleanAssignmentVector boolean_solution = solver.state_.ConvertIntegerSolutionToBooleanAssignments(solution);
			solver.state_.value_selector_.SetPhaseValuesAndFreeze(boolean_solution);
			return;
		}
		case Pumpkin::UpperBoundSearch::ValueSelectionStrategy::OPTIMISTIC:
		{//set the values according to the best solution, apart from the objective literals, which are set to optimistic zero values
			IntegerAssignmentVector modified_solution(solution);
			for (Term term : objective_function)
			{
				int lb = solver.state_.domain_manager_.GetLowerBound(term.variable);
				modified_solution[term.variable] = lb;				
			}
			BooleanAssignmentVector modified_boolean_solution = solver.state_.ConvertIntegerSolutionToBooleanAssignments(modified_solution);
			solver.state_.value_selector_.SetPhaseValuesAndFreeze(modified_boolean_solution);
			return;
		}
		case Pumpkin::UpperBoundSearch::ValueSelectionStrategy::OPTIMISTIC_AUX:
		{//similar as above, but assigns zero to all (auxiliary) variables used in the generalised totaliser encoding for the upper bound
			
			pumpkin_assert_permanent(1 == 2, "The optimistic value selection with auxiliary variables needs to be fixed!"); //todo enable this for the new version

			/*BooleanAssignmentVector modified_solution(solution);
			//in some cases the solution might have less variables than currently in the solver, e.g., after core-guided phase or after adding the upper bound constraint
			while (solver.state_.GetNumberOfVariables() > modified_solution.NumVariables()) { modified_solution.Grow(); }

			for (BooleanLiteral objective_literal : objective_function) //I think these could be removed since these variables are included in the next loop todo;
			{
				modified_solution.AssignLiteral(objective_literal, false);
			}
			for (std::vector<PairWeightLiteral>& literals : pseudo_boolean_encoder_.literals_from_last_hard_call_)
			{
				for (PairWeightLiteral& p : literals)
				{
					modified_solution.AssignLiteral(p.literal, false);
				}
			}		
			solver.state_.value_selector_.SetPhaseValuesAndFreeze(modified_solution);	*/		
			return;
		}
		default:
		{
			runtime_assert(1 == 2);
		}
	}
}

int64_t UpperBoundSearch::GetInitialDivisionCoefficient(LinearFunction& objective_function, SolverState& state)
{
	switch (varying_resolution_strategy_)
	{
		case VaryingResolutionStrategy::OFF:
		{
			return 1;
		}
		case VaryingResolutionStrategy::BASIC:
		{
			return std::max(int64_t(1), objective_function.GetMaximumWeight());
		}
		case VaryingResolutionStrategy::RATIO:
		{

			int64_t large_weight = pow(10, 1 + ceil(log10(objective_function.GetMaximumWeight())));
			return std::max(int64_t(1), GetNextDivisionCoefficientRatioStrategy(large_weight, objective_function, state));
		}
		default:
		{
			runtime_assert(1 == 2); return -1;
		}
	}
}

int64_t UpperBoundSearch::GetNextDivisionCoefficient(int64_t current_division_coefficient, const LinearFunction& objective_function, SolverState& state)
{
	switch (varying_resolution_strategy_)
	{
		case VaryingResolutionStrategy::OFF:
			return 0;

		case VaryingResolutionStrategy::BASIC:
			return GetMaximumWeightSmallerThanThreshold(current_division_coefficient, objective_function, state);

		case VaryingResolutionStrategy::RATIO:
			return GetNextDivisionCoefficientRatioStrategy(current_division_coefficient, objective_function, state);

		default:
			runtime_assert(1 == 2); return -1;
	}
}

LinearFunction UpperBoundSearch::GetVaryingResolutionObjective(int64_t division_coefficient, LinearFunction& original_objective, SolverState& state)
{
	pumpkin_assert_simple(state.GetCurrentDecisionLevel() == 0, "Sanity check");

	LinearFunction simplified_objective;
	for (Term term: original_objective)
	{
		pumpkin_assert_simple(term.weight > 0, "Sanity check"); //all negative weights are preprocessed into positive weights

		int64_t new_weight = term.weight / division_coefficient;	
		if (new_weight > 0 && !state.IsAssigned(term.variable)) 
		{ 
			simplified_objective.AddTerm(term.variable, new_weight); 
		}
	}
	pumpkin_assert_simple(simplified_objective.NumTerms() > 0, "Sanity check");
	return simplified_objective;
}

int64_t UpperBoundSearch::GetNextDivisionCoefficientRatioStrategy(int64_t division_coefficient, const LinearFunction& objective_function, SolverState& state)
{
	pumpkin_assert_simple(division_coefficient == 1 || division_coefficient % 10 == 0, "Sanity check");

	//trivial case, if there are no terms in the objective, return zero to signal nothing needs to be done
	if (objective_function.NumTerms() == 0) { return 0; }
	//if we already tried every weight before, then return zero as the signal to stop
	if (division_coefficient == 1) { return 0; }

	int64_t candidate_coefficient = division_coefficient;
	std::set<int64_t> considered_weights;
	int num_literals_old = GetNumLiteralsWithWeightGreaterOrEqualToThreshold(division_coefficient, objective_function, state);
	int num_literals_new = -1;
	do
	{		
		candidate_coefficient /= 10;

		num_literals_new = GetNumLiteralsWithWeightGreaterOrEqualToThreshold(candidate_coefficient, objective_function, state);
		//if we did not add at least one new literals, continue
		//	todo ideally here we would also check if at least one literal violates the current best solution, if not, then continue since the objective will not change
		if (num_literals_new == num_literals_old) { continue; }
		//collect the unique weights
		for (Term term : objective_function)
		{
			if (term.weight >= candidate_coefficient)
			{
				considered_weights.insert(term.weight);
			}
		}
		//if we hit the ratio, we found the new coefficient, otherwise reiterate
		//	the number 1.25 is a magic number
		if (double(num_literals_new) / considered_weights.size() >= 1.25) 
		{
			return candidate_coefficient;
		}
	} while (candidate_coefficient >= 10 && num_literals_new != GetNumLiteralsWithWeightGreaterOrEqualToThreshold(0, objective_function, state));

	return candidate_coefficient;
}

int UpperBoundSearch::GetNumLiteralsWithWeightGreaterOrEqualToThreshold(int64_t threshold, const LinearFunction& objective_function, SolverState& state)
{
	int num_literals = 0;
	for (Term term : objective_function)
	{
		pumpkin_assert_simple(term.weight > 0, "Sanity check"); //for now all negative weighted terms are converted in preprocessing to positive weights

		if (term.weight >= threshold)
		{
			int lb = state.domain_manager_.GetLowerBound(term.variable);
			int ub = state.domain_manager_.GetUpperBound(term.variable);
			for (int val = lb+1; val <= ub; val++) //plus one since there is effectively no penalty for setting the variable to its lower bound
			{
				BooleanLiteral lit_eq = state.GetEqualityLiteral(term.variable, val);

				//we skip those domain values that cannot be assigned
				if (state.assignments_.IsAssignedFalse(lit_eq) && state.assignments_.IsRootAssignment(lit_eq)) { continue; }

				++num_literals;
			}
		}	
	}
	return num_literals;
}

int64_t UpperBoundSearch::GetMaximumWeightSmallerThanThreshold(int64_t threshold, const LinearFunction& objective_function, SolverState& state)
{
	int64_t target_weight = 0;
	for (Term term : objective_function)
	{
		if (term.weight < threshold && term.weight > target_weight)
		{
			target_weight = term.weight;
		}
	}
	return target_weight;
}

}
