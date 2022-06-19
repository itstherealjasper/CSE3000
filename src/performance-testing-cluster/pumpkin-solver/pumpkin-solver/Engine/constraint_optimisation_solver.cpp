#include "constraint_optimisation_solver.h"
#include "preprocessor.h"
#include "../Utilities/runtime_assert.h"
#include "../Utilities/solver_output_checker.h"
#include "../Utilities/stopwatch.h"
#include "../Utilities/boolean_assignment_vector.h"
#include "../Utilities/Vec.h"

#include <algorithm>
#include <iostream>

namespace Pumpkin
{
ConstraintOptimisationSolver::ConstraintOptimisationSolver(ParameterHandler& parameters):
	constrained_satisfaction_solver_(parameters),
	core_guided_searcher_(parameters),
	linear_searcher_(constrained_satisfaction_solver_.state_, parameters),
	use_lexicographical_objectives_(parameters.GetBooleanParameter("lexicographical")),
	optimistic_initial_solution_(parameters.GetBooleanParameter("optimistic-initial-solution")),
	parameters_(parameters)
{
}

void ConstraintOptimisationSolver::ReadDIMACSFile(std::string file_location)
{
	if (file_location == "") { std::cout << "No file reading, aborting\n"; exit(1); }

	clock_t start = clock();
	GzFileReader file_reader(file_location);

	char c;
	//skip comments
	file_reader >> c;
	while (c == 'c')
	{
		file_reader.SkipLine();
		file_reader >> c;
	}
	pumpkin_assert_permanent(c == 'p', "Error: Expected 'p' as the first character in the header of the file.");
	std::string file_type;
	file_reader >> file_type;
	//SAT file format
	if (file_type == "cnf")
	{
		// std::cout << "c reading SAT file\n";
		ReadSATFileInternal(file_reader);
	}
	//MaxSAT file format
	else if (file_type == "wcnf")
	{
		// std::cout << "c Reading MaxSAT file\n";
		ReadMaxSATFileInternal(file_reader);
	}
	else
	{
		std::cout << "Error: Something wrong with the input file; expected cnf or wcnf but got \"" << file_type << "\" instead!\n";
		exit(1);
	}
	// std::cout << "c Time spent reading the file: " << double(clock() - start) / CLOCKS_PER_SEC << "\n";

	constrained_satisfaction_solver_.state_.variable_selector_.Reset(parameters_.GetIntegerParameter("seed"));
}

SolverOutput ConstraintOptimisationSolver::Solve(int64_t time_limit_in_seconds_linear_search, int64_t time_limit_in_seconds_core_guided)
{
	runtime_assert(1 == 2); //I think I should delete this method; SolveBMO is a more general version

	/*Stopwatch stopwatch(time_limit_in_seconds_linear_search + time_limit_in_seconds_core_guided);
	LinearFunction objective_function(original_objective_function_);
	solution_tracker_ = SolutionTracker(objective_function, stopwatch);

	bool should_print = constrained_satisfaction_solver_.state_.propagator_clausal_.NumClausesTotal() <= 30;
	constrained_satisfaction_solver_.state_.propagator_clausal_.RecomputeAndPrintClauseLengthStatsForPermanentClauses(should_print);
	
	//find an initial solution by just looking at the hard constraints
	SolverOutput initial_output = constrained_satisfaction_solver_.Solve(stopwatch.TimeLeftInSeconds());
	if (initial_output.ProvenInfeasible()) { return SolverOutput(stopwatch.TimeElapsedInSeconds(), false, BooleanAssignmentVector(), -1, std::vector<BooleanLiteral>()); }
	else if (initial_output.timeout_happened) { return SolverOutput(stopwatch.TimeElapsedInSeconds(), true, BooleanAssignmentVector(), -1, std::vector<BooleanLiteral>()); }
	
	//proceed with the initial solution	
	solution_tracker_.UpdateBestSolution(initial_output.solution);

	core_guided_searcher_.Solve(constrained_satisfaction_solver_, objective_function, solution_tracker_, 0, stopwatch.TimeLeftInSeconds() - time_limit_in_seconds_linear_search);

	linear_searcher_.Solve(constrained_satisfaction_solver_, objective_function, solution_tracker_, 0, stopwatch.TimeLeftInSeconds());

	return SolverOutput(stopwatch.TimeElapsedInSeconds(), !stopwatch.IsWithinTimeLimit(), solution_tracker_.GetBestSolution(), solution_tracker_.UpperBound(), std::vector<BooleanLiteral>());*/
}

SolverOutput ConstraintOptimisationSolver::SolveBMO(int64_t time_limit_in_seconds_linear_search, int64_t time_limit_in_seconds_core_guided)
{
	//basic initialisation
	Stopwatch stopwatch(time_limit_in_seconds_linear_search + time_limit_in_seconds_core_guided);
	LinearFunction objective_function = ConvertToCanonicalForm(original_objective_function_);
	solution_tracker_ = SolutionTracker(objective_function, stopwatch);
	solution_tracker_.UpdateLowerBound(objective_function.GetConstantTerm());

	//printing
	bool should_print = constrained_satisfaction_solver_.state_.propagator_clausal_.NumClausesTotal() <= 30;
	constrained_satisfaction_solver_.state_.propagator_clausal_.RecomputeAndPrintClauseLengthStatsForPermanentClauses(should_print);

	constrained_satisfaction_solver_.state_.propagator_clausal_.PerformSimplificationAndGarbageCollection();

	if (parameters_.GetBooleanParameter("preprocess-equivalent-literals"))
	{
		//simple preprocessing and lower bound update
		Preprocessor::RemoveFixedAssignmentsFromObjective(constrained_satisfaction_solver_, objective_function);
		Preprocessor::MergeEquivalentLiterals(constrained_satisfaction_solver_, objective_function);
		solution_tracker_.UpdateLowerBound(objective_function.GetConstantTerm());
	}

	SolverOutput initial_output = ComputeInitialSolution(objective_function, stopwatch);
	
	//terminate if unsat or if no initial solution could be found within the time limit
	if (initial_output.ProvenInfeasible()) { return SolverOutput(stopwatch.TimeElapsedInSeconds(), false, IntegerAssignmentVector(), -1, std::vector<BooleanLiteral>()); }
	else if (initial_output.timeout_happened) { return SolverOutput(stopwatch.TimeElapsedInSeconds(), true, IntegerAssignmentVector(), -1, std::vector<BooleanLiteral>()); }

	//proceed with the initial solution	
	solution_tracker_.UpdateBestSolution(initial_output.solution);

	//simple preprocessing and lower bound update
	Preprocessor::RemoveFixedAssignmentsFromObjective(constrained_satisfaction_solver_, objective_function);
	bool conflict_detected = Preprocessor::PruneDomainValueBasedOnUpperBound(constrained_satisfaction_solver_, objective_function, solution_tracker_.UpperBound());
	runtime_assert(!conflict_detected);
	solution_tracker_.UpdateLowerBound(constrained_satisfaction_solver_.ComputeSimpleLowerBound(objective_function));
	
	std::vector<WeightInterval> lexicographical_weight_range = ComputeLexicographicalObjectiveWeightRanges(objective_function);
	
	//we optimise one objective at a time
	//	(in most cases there is only one objective function)
	for (auto iter = lexicographical_weight_range.rbegin(); iter != lexicographical_weight_range.rend(); ++iter)
	{
		if (use_lexicographical_objectives_) { std::cout << "c lexicographically processing weights in range [" << iter->min_weight << ", " << iter->max_weight << "]\n"; }

		//set the current lexicographical objective
		LinearFunction current_objective_function;
		for (Term term : objective_function)
		{
			int64_t weight = objective_function.GetWeight(term.variable);
			if (iter->min_weight <= weight && weight <= iter->max_weight)
			{
				current_objective_function.AddTerm(term.variable, weight);
			}
		}

		//core-guided search
		bool optimally_solved = core_guided_searcher_.Solve(constrained_satisfaction_solver_, current_objective_function, solution_tracker_, stopwatch.TimeLeftInSeconds() - time_limit_in_seconds_linear_search);

		if (!stopwatch.IsWithinTimeLimit()) { break; }

		//if core guided search did not solve, then go with linear search
		if (!optimally_solved)
		{
			optimally_solved = linear_searcher_.Solve(constrained_satisfaction_solver_, current_objective_function, solution_tracker_, stopwatch.TimeLeftInSeconds());
			
			if (!stopwatch.IsWithinTimeLimit()) { break; }

			//buggy -> I think linear search will render the current formula unsat if it proves optimality, so next iterations won't work
			//	solution is to add the upper bound as an assumption, and then if it works out, add it as a hard constraint

			// if (optimally_solved) { std::cout << "c\tlinear search proved optimality\n"; }
		}
		else
		{
			runtime_assert(1 == 2); //for now this is disabled until we properly handle it

			//if core guided search solved to optimality, the objective has been rewritten and the solution found satisfies all (rewritten) objective literals
			//fix their values to make sure they also hold in future iterations
			/*for (Term term : current_objective_function)
			{
				//here I need to see...we fix their values to their lower bounds, right? Of what, need to think about this since the assumptions are not variables but expressions ...
				bool conflict_detected = constrained_satisfaction_solver_.state_.propagator_clausal_.AddUnitClause(~literal);
				runtime_assert(!conflict_detected);
			}
			std::cout << "c\tcore-guided proved optimality\n";
			//set the lower bound to this optimal solution
			int64_t lb_increase = current_objective_function.ComputeSolutionCost(solution_tracker_.GetBestSolution()) - current_objective_function.GetConstantTerm();
			current_objective_function.IncreaseConstantTerm(lb_increase);*/
		}
		// std::cout << "c not printing the currently LB and UB; broken\n";
		//std::cout << "c completed LB = " << original_objective_lower_bound << "; UB = " << solution_tracker_.UpperBound() << "\n";
	}
	return SolverOutput(stopwatch.TimeElapsedInSeconds(), !stopwatch.IsWithinTimeLimit(), solution_tracker_.GetBestSolution(), solution_tracker_.UpperBound(), std::vector<BooleanLiteral>());
}

std::string ConstraintOptimisationSolver::GetStatisticsAsString()
{
	std::string stats = constrained_satisfaction_solver_.GetStatisticsAsString();
	// stats += "c Primal integral: " + std::to_string(solution_tracker_.ComputePrimalIntegral()) + "\n";
	return stats;
}

SolverOutput ConstraintOptimisationSolver::GetPreemptiveResult()
{
	return SolverOutput(-1, true, solution_tracker_.GetBestSolution(), solution_tracker_.UpperBound(), std::vector<BooleanLiteral>());
}

ParameterHandler ConstraintOptimisationSolver::CreateParameterHandler()
{
	ParameterHandler parameters;

	parameters.DefineNewCategory("General Parameters");
	parameters.DefineNewCategory("Constraint Satisfaction Solver Parameters");
	parameters.DefineNewCategory("Constraint Satisfaction Solver Parameters - Restarts");
	parameters.DefineNewCategory("Linear Search");
	parameters.DefineNewCategory("Core-Guided Search");
	parameters.DefineNewCategory("Cumulative");

	//GENERAL PARAMETERS----------------------------------------

	parameters.DefineStringParameter
	(
		"file",
		"Location of the instance file.",
		"", //default value
		"General Parameters"
	);

	parameters.DefineIntegerParameter
	(
		"time",
		"Time limit given in seconds. This includes the time of the core-guided search (see -time-core-guided).",
		86400, //default value, one day
		"General Parameters",
		0.0 //min_value
	);

	parameters.DefineBooleanParameter
	(
		"print-solution",
		"Determines if the assignment of variables should be displayed at the end.",
		false, //default value
		"Constraint Satisfaction Solver Parameters"
	);

	parameters.DefineIntegerParameter
	(
		"time-core-guided",
		"Time limit given in seconds for the core-guided phase. Setting this value equal to -time will effective run a pure core-guided algorithm",
		0.0, //default value
		"General Parameters",
		0.0 //min_value
	);

	parameters.DefineIntegerParameter
	(
		"seed",
		"The seed dictates the initial variable ordering. The value -1 means the initial ordering is based on the index of variables, and any nonnegative values triggers a random initial ordering.",
		-1, //default value
		"General Parameters",
		-1 //min_value
	);

	parameters.DefineBooleanParameter
	(	
		"lexicographical",
		"Specifies if lexicographical objectives should be detected. Can make a large difference for instances where the magnitudes of the weights are substantial. Detection lexicographical objectives is computationally cheap and usually there is no downside to keeping this on.",
		false,
		"General Parameters"
	);

	parameters.DefineBooleanParameter
	(
		"optimistic-initial-solution",
		"If set, the solver sets objective literals to zero in value selection. TODO",
		true,
		"General Parameters"
	);

	parameters.DefineStringParameter
	(
		"output-file",
		"Location where to create the file with the output (solution or UNSAT). Empty string means no such outfile is generated.",
		"", //default value
		"General Parameters"
	);

	//CONSTRAINT SATISFACTION SOLVER PARAMETERS-----------------------------------

	parameters.DefineBooleanParameter
	(
		"clause-minimisation",
		"Use clause minimisation for each learned clause based on [todo insert paper].",
		true, //default value
		"Constraint Satisfaction Solver Parameters"
	);

	parameters.DefineBooleanParameter
	(
		"preprocess-equivalent-literals",
		"Perform equivalent literal detection by analysing the implication graph. Removes some clauses as well that become duplicates or redundant after equivalent literals are exchanged by their representative literal. Typically does not take a lot of time to compute.",
		false, //default value
		"Constraint Satisfaction Solver Parameters"
	);

	parameters.DefineBooleanParameter
	(
		"bump-decision-variables",
		"Additionally increase VSIDS activities for decision variables. Experimental parameter, may be removed later.",
		true, //default value
		"Constraint Satisfaction Solver Parameters"
	);

	parameters.DefineFloatParameter
	(
		"garbage-tolerance-factor",
		"Represents the faction of total clausal memory allocated that may be attributed to garbage. A value of 1.0 means garbage collection is never performed, while 0.0 means no garbage is tolerated (too expensive). The default value is suppose to be a compromise but perhaps better values exist.",
		0.2,
		"Constraint Satisfaction Solver Parameters",
		0.0,
		1.0
	);

	parameters.DefineFloatParameter
	(
		"decay-factor-variables",
		"Influences how much weight the solver gives to variable recently involved in conflicts. A high value put emphasise on recent conflicts.",
		0.95, //default
		"Constraint Satisfaction Solver Parameters",
		0.0, //min value
		1.0 //max_value
	);

	parameters.DefineFloatParameter
	(
		"decay-factor-learned-clause",
		"Influences how much weight the solver gives to recent conflicts when deciding on clause removal. A high value put emphasise on recent conflicts.",
		0.99, //default
		"Constraint Satisfaction Solver Parameters",
		0.0, //min value
		1.0 //max_value
	);
	
	parameters.DefineIntegerParameter
	(
		"lbd-threshold",
		"Learned clauses with an LBD value lower or equal to the lbd-threshold are kept forever in the solver. TODO paper",
		2, //default value
		"Constraint Satisfaction Solver Parameters",
		0 //min value
	);

	parameters.DefineIntegerParameter
	(
		"limit-num-temporary-clauses",
		"The solver aims at not storing more temporary clauses than the limit. TODO paper.",
		20000, //default value
		"Constraint Satisfaction Solver Parameters",
		0 //min_value
	);

	parameters.DefineBooleanParameter
	(
		"lbd-sorting-temporary-clauses",
		"When removing temporary clauses, they will be removed in order of LBD if this parameter is set, otherwise according to the clause activities.",
		false, //default value
		"Constraint Satisfaction Solver Parameters"
	);

	//CONSTRAINT SATISFACTION SOLVER PARAMETERS - RESTARTS

	parameters.DefineStringParameter
	(
		"restart-strategy",
		"Specifies the restart strategy used by the solver. Todo description.",
		"constant",
		"Constraint Satisfaction Solver Parameters",
		{ "glucose", "luby", "constant" }
	);

	parameters.DefineIntegerParameter
	(
		"restart-multiplication-coefficient",
		"Multiplies the base number that defines the number of conflicts before restarting. Used with restart-strategy \"luby\" and \"constant\"",
		256,
		"Constraint Satisfaction Solver Parameters",
		1
	);

	parameters.DefineIntegerParameter
	(
		"glucose-queue-lbd-limit",
		"todo",
		50,
		"Constraint Satisfaction Solver Parameters - Restarts",
		0
	);

	parameters.DefineIntegerParameter
	(
		"glucose-queue-reset-limit",
		"todo",
		5000,
		"Constraint Satisfaction Solver Parameters - Restarts",
		0
	);

	parameters.DefineIntegerParameter
	(
		"num-min-conflicts-per-restart",
		"todo",
		50,
		"Constraint Satisfaction Solver Parameters - Restarts",
		0
	);

	//CUMULATIVE GLOBAL CONSTRAINT PARAMETERS------------------------------------------

	parameters.DefineStringParameter
	(
		"cumulative-task-ordering",
		"Indicates if the tasks in the cumulative constraints should be sorted for use in the algorithm. For now this is an experimental idea.",
		"in-order",
		"Cumulative",
		{ "in-order", "ascending-duration", "descending-duration", "ascending-consumption", "descending-consumption", "descending-consumption-special"}
	);

	parameters.DefineBooleanParameter
	(
		"cumulative-incremental",
		"Instructs the solver to use an incremental version of the cumulative, which should be more efficient in most cases over a non-incremental version.",
		true,
		"Cumulative"
	);

	parameters.DefineStringParameter
	(
		"cumulative-lifting",
		"Strategy used when generating explanations. It should always be better to use lifting.",
		"in-order",
		"Cumulative",
		{ "OFF", "ON" }
	);

	//LINEAR SEARCH PARAMETERS---------------------------------------

	parameters.DefineStringParameter
	(
		"varying-resolution",
		"Use the varying resolution heuristic to prioritise large weights [todo insert paper].",
		"ratio",
		"Linear Search",
		{ "off", "basic", "ratio" }
	);

	parameters.DefineStringParameter
	(
		"value-selection",
		"Determines the value that is first assigned to a variable during search.TODO",
		"solution-guided-search",
		"Linear Search",
		{ "phase-saving", "solution-guided-search", "optimistic", "optimistic-aux" }
	);

	parameters.DefineBooleanParameter
	(
		"ub-propagator",
		"If set, the upper bound constraint is handled with a dedicated propagator. Otherwise, the upper bound constraint is encoded using the generalised totaliser encoding.",
		false,
		"Linear Search"
	);

	parameters.DefineBooleanParameter
	(
		"ub-propagator-bump",
		"Instructs the dedicated upper bound propagator to bump the activity of objective literals with respect to their weight (higher-weighted literals get a bigger bump)",
		false,
		"Linear Search"
	);

	//CORE-GUIDED SEARCH PARAMETERS---------------------------------------

	parameters.DefineStringParameter
	(
		"stratification",
		"Use the stratification heuristic to prioritise cores with large weights based on [todo insert paper].",
		"ratio",
		"Core-Guided Search",
		{ "off", "basic", "ratio" }
	);

	parameters.DefineBooleanParameter
	(
		"weight-aware-core-extraction",
		"Extract as many cores as possible before reformulating the instance in core-guided search, see [todo insert paper].",
		true,
		"Core-Guided Search"
	);

	parameters.DefineStringParameter
	(
		"cardinality-encoding",
		"Specifies which cardinality constraint encoding to use when processing cores.",
		"totaliser",
		"Core-Guided Search",
		{"totaliser", "cardinality-network"}
	);

	return parameters;
}

void ConstraintOptimisationSolver::CheckCorrectnessOfParameterHandler(const ParameterHandler& parameters)
{
	//to be expanded...

	if (parameters.GetStringParameter("cumulative-lifting") == "ON" && parameters.GetStringParameter("cumulative-task-ordering") != "descending-consumption-special")
	{
		std::cout << "for now lifting is only implemented in the special propagator for the cumulative, TODO\n";
		exit(1);
	}
}

LinearFunction ConstraintOptimisationSolver::ConvertToCanonicalForm(LinearFunction& input_function)
{
	auto &domain_manager = constrained_satisfaction_solver_.state_.domain_manager_;

	LinearFunction canonical_function;
	canonical_function.AddConstantTerm(input_function.GetConstantTerm());
	for (Term term : input_function)
	{
		if (term.weight > 0) 
		{ 
			canonical_function.AddTerm(term.variable, term.weight); 
		}
		else if (term.weight < 0)
		{
			runtime_assert(domain_manager.GetLowerBound(term.variable) >= 0 && domain_manager.GetUpperBound(term.variable) <= 1); //for now we only support this conversion for Boolean functions but it can easily be done for arbitrary integers too
			
			IntegerVariable inverse_variable = constrained_satisfaction_solver_.state_.CreateNewIntegerVariable(0, 1); //this should be replaced with views, or alternatively, the CreateInverseVariable method todo
			constrained_satisfaction_solver_.state_.AddSimpleSumConstraint(term.variable, inverse_variable, 1);
			canonical_function.AddTerm(inverse_variable, abs(term.weight));
			canonical_function.AddConstantTerm(-abs(term.weight));
		}
	}
	return canonical_function;
}

std::vector<ConstraintOptimisationSolver::WeightInterval> ConstraintOptimisationSolver::ComputeLexicographicalObjectiveWeightRanges(LinearFunction& objective_function)
{
	if (objective_function.NumTerms() == 0) { return std::vector<WeightInterval>(); }

	std::vector<WeightInterval> objectives;

	if (!use_lexicographical_objectives_)
	{
		objectives.push_back(WeightInterval(objective_function.GetMinimumWeight(), objective_function.GetMaximumWeight()));
	}
	else
	{
		//the algorithm tries to identify the number of lexicographical optimisation functions
		//if there is a large differences in the weight values, it could be that the user encoded two or more objectives that need to be optimised lexicographically

		std::vector<int64_t> weights;
		for (const Term &term : objective_function) 
		{ 
			weights.push_back(term.weight);

			if (constrained_satisfaction_solver_.state_.domain_manager_.GetUpperBound(term.variable) > 1)
			{
				// std::cout << "c lexicographical detection for non-pseudo-Boolean objectives not implemented, reverting to the standard objective\n";
				// std::cout << "c num lexicographical objectives: 1\n";

				objectives.clear();
				objectives.push_back(WeightInterval(objective_function.GetMinimumWeight(), objective_function.GetMaximumWeight()));
				
				return objectives;
			}
		}

		std::sort(weights.begin(), weights.end());

		int64_t lower_weight = weights[0];
		int64_t partial_sum = weights[0];
		for (size_t i = 1; i < weights.size(); i++)
		{
			int64_t current_weight = weights[i];

			if (current_weight > partial_sum)
			{
				objectives.push_back(WeightInterval(lower_weight, weights[i - 1]));
				lower_weight = current_weight;
				partial_sum = 0;
			}
			partial_sum += current_weight;
		}
		objectives.push_back(WeightInterval(lower_weight, weights.back()));
	}

	// std::cout << "c num lexicographical objectives: " << objectives.size() << "\n";

	return objectives;
}

SolverOutput ConstraintOptimisationSolver::ComputeInitialSolution(LinearFunction& objective_function, Stopwatch& stopwatch)
{
	if (optimistic_initial_solution_)
	{
		for (Term term : objective_function)
		{
			int lb = constrained_satisfaction_solver_.state_.domain_manager_.GetLowerBound(term.variable);
			int ub = constrained_satisfaction_solver_.state_.domain_manager_.GetUpperBound(term.variable);
			for (int i = lb+1; i <= ub; i++) //note that the plus one is intentional; the variable is always greater or equal to its lower bound so we can skip the corresponding literal
			{
				BooleanLiteral literal = constrained_satisfaction_solver_.state_.GetLowerBoundLiteral(term.variable, i);
				constrained_satisfaction_solver_.state_.value_selector_.SetAndFreezeValue(~literal);
			}
		}
	}
	
	// std::cout << "c computing initial solution...\n";

	//find an initial solution by just looking at the hard constraints
	SolverOutput initial_output = constrained_satisfaction_solver_.Solve(stopwatch.TimeLeftInSeconds());
	
	if (optimistic_initial_solution_) 
	{ 
		constrained_satisfaction_solver_.state_.value_selector_.UnfreezeAll();
	}

	return initial_output;
}

void ConstraintOptimisationSolver::ConvertDIMASIntegersToClause(std::vector<int64_t>& dimacs_integers, std::vector<BooleanLiteral>& output_clause, bool ignore_first_integer)
{
	pumpkin_assert_permanent(!dimacs_integers.empty(), "Error: an empty DIMACS line detected as input?");
	pumpkin_assert_permanent(dimacs_integers.back() == 0, "Error: each DIMACS line must end with a zero, but " + std::to_string(dimacs_integers.back()) + " found instead\n");

	int num_variables = constrained_satisfaction_solver_.state_.NumIntegerVariables();
	output_clause.clear();
	for (int i = (ignore_first_integer ? 1 : 0); i < dimacs_integers.size() - 1; i++) //minus one since the last integer is zero
	{
		//recall that variable indicies in DIMACS start from 1, but in the solver the indicies start from 2
		//	this is because the indicies 0 and 1 are reserved for special purposes (NULL and a true/false literal)
		//as a result, the index in the solver is one greater than in the file

		int64_t number = dimacs_integers[i];
		pumpkin_assert_permanent(number != 0 && abs(number) + 1 <= num_variables, "Error: when converting dimacs file to clause, a variable index of " + to_string(number) + " detected but it goes over the number of variables declared in the file.");
		BooleanLiteral literal = constrained_satisfaction_solver_.state_.GetEqualityLiteral(IntegerVariable(abs(number) + 1), number > 0);
		output_clause.push_back(literal);
	}
}

void ConstraintOptimisationSolver::ReadSATFileInternal(GzFileReader& file_reader)
{
	char c;
	int64_t num_clauses, num_variables, number;
	std::vector<int64_t> integers;
	std::vector<BooleanLiteral> clause;
	
	file_reader >> num_variables >> num_clauses;
	pumpkin_assert_permanent(file_reader.IsOK(), "Error: something went wrong with the input file, the number of variables and number of clauses could not be read properly.");
	file_reader.SkipWhitespacesExceptNewLine();
	pumpkin_assert_permanent(file_reader.PeekNextChar() == '\n', "Error: in the file, after the number of variables and clauses, a new line is expected!\n");
	file_reader.SkipLine();
	
	//eagerly create all variables
	for (int i = 0; i < num_variables; i++) { constrained_satisfaction_solver_.state_.CreateNewIntegerVariable(0, 1); }

	// std::cout << "c num_vars: " << num_variables << "; num_clauses: " << num_clauses << "\n";

	//clauses are read one after the other; each itearation is one clause
	int num_clauses_read = 0;
	while (!file_reader.IsEndOfFile())
	{
		num_clauses_read++;

		file_reader.ReadIntegerLine(integers);
		ConvertDIMASIntegersToClause(integers, clause);
		bool conflict_detected = constrained_satisfaction_solver_.state_.propagator_clausal_.AddPermanentClause(clause);
		pumpkin_assert_permanent(!conflict_detected, "Root conflict detected, in principle this is not an error, but for now we assume something went wrong if the instance is detected unsatisfiable without any search.");
	}
	pumpkin_assert_permanent(num_clauses_read == num_clauses, "Error: input file specified " + std::to_string(num_clauses) + " but only " + std::to_string(num_clauses_read) + " where found.");
}

void ConstraintOptimisationSolver::ReadMaxSATFileInternal(GzFileReader& file_reader)
{
	char c;
	int64_t num_clauses, num_variables, number;
	std::vector<int64_t> integers;
	std::vector<BooleanLiteral> clause;

	file_reader >> num_variables >> num_clauses;
	pumpkin_assert_permanent(file_reader.IsOK(), "Error: something went wrong with the input file, the number of variables and number of clauses could not be read properly.");
	//eagerly create all variables
	for (int i = 0; i < num_variables; i++) { constrained_satisfaction_solver_.state_.CreateNewIntegerVariable(0, 1); }

	int64_t hard_clause_weight;
	file_reader >> hard_clause_weight;
	pumpkin_assert_permanent(file_reader.IsOK(), "Error: something went wrong with the input file, the hard clause weight could not be read properly.");
	file_reader.SkipWhitespacesExceptNewLine();
	pumpkin_assert_permanent(file_reader.PeekNextChar() == '\n', "Error: in the file, after the hard clause weight, a new line is expected!\n");
	file_reader.SkipLine();

	int64_t iter = 0;
	// std::cout << "c num_vars: " << num_variables << "; num_clauses: " << num_clauses << "\n";

	//clauses are read one after the other; each itearation is one clause
	int num_clauses_read = 0;
	while (!file_reader.IsEndOfFile())
	{
		file_reader.ReadIntegerLine(integers);
		num_clauses_read++;
		pumpkin_assert_permanent(!integers.empty(), "Error: While reading the file, a clause line has been detected with no integers in it, unexpected.");

		//if hard clause
		if (integers[0] == hard_clause_weight)
		{
			ConvertDIMASIntegersToClause(integers, clause, true);
			bool conflict_detected = constrained_satisfaction_solver_.state_.propagator_clausal_.AddPermanentClause(clause);
			pumpkin_assert_permanent(!conflict_detected, "Root conflict detected, in principle this is not an error, but for now we assume something went wrong if the instance is detected unsatisfiable without any search.");
		}
		else //the clause is a soft clause
		{
			ConvertDIMASIntegersToClause(integers, clause, true);
			//even though the solver would preprocess the clause, it is important to do this before adding
			//	this is because we need to make several decisions based on the form of the clause (see below)
			constrained_satisfaction_solver_.state_.propagator_clausal_.PreprocessClause(clause);

			//normally for each soft clause, we create a fresh ('selector') variable and add the variable to the soft clause
			//	then we add the clause as a hard clause, and in the objective we add the selector variable
			//	however there are a few exceptions, which will be now checked

			//if the soft clause is satisfied at the root level
			if (clause.size() > 0 && clause[0] == constrained_satisfaction_solver_.state_.true_literal_)
			{
				//do nothing, the clause may be ignored
			}
			//if the soft clause is unsatisfied at the root level
			else if (clause.empty())
			{
				//add the weight of the clause as a constant to the objective, no need to add the clause itself
				int64_t weight = integers[0];
				original_objective_function_.AddConstantTerm(weight);
			}
			//if the clause only has one literal, then we do not need to create a new selector variable
			//	the variable in the clause will be used in the objective function
			else if (clause.size() == 1)
			{
				int64_t weight = integers[0];
				auto lit_info = constrained_satisfaction_solver_.state_.GetLiteralInformation(clause[0]);
				//the added term in the objective depends on the polarity of the literal
				//	note that our objective is a sum of variables, and not a sum of literals (!)
				if (clause[0].IsNegative())
				{
					//add the term weight * variable to the objective
					original_objective_function_.AddTerm(lit_info.integer_variable, weight);
				}
				else
				{
					//add the term weight * (1 - variable) to the objective
					original_objective_function_.AddConstantTerm(weight);
					original_objective_function_.AddTerm(lit_info.integer_variable, -weight);
				}
			}
			//otherwise we arrive at the standard case, i.e., a soft clause with at least two literals
			else
			{
				int64_t weight = integers[0];
				IntegerVariable fresh_selector_variable = constrained_satisfaction_solver_.state_.CreateNewIntegerVariable(0, 1);
				BooleanLiteral soft_literal = constrained_satisfaction_solver_.state_.GetEqualityLiteral(fresh_selector_variable, 1);
				clause.push_back(soft_literal);
				original_objective_function_.AddTerm(fresh_selector_variable, weight);
				bool conflict_detected = constrained_satisfaction_solver_.state_.propagator_clausal_.AddPermanentClause(clause);
				pumpkin_assert_permanent(!conflict_detected, "Error: unsat after adding a soft clause, strange!");
			}
		}
	}
	pumpkin_assert_permanent(num_clauses_read == num_clauses, "Error: input file specified " + std::to_string(num_clauses) + " but only " + std::to_string(num_clauses_read) + " where found.");
	int n = 0;
}

}//end Pumpkin namespace
