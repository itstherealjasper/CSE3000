#include "pumpkin-solver.h"

Pumpkin::ConstraintOptimisationSolver* g_solver;
time_t g_start_solve;
clock_t g_start_clock;
bool g_print_sol;

std::string MainGetStatisticsAsString()
{
	time_t total_solve = time(0) - g_start_solve;
	clock_t total_clock = clock() - g_start_clock;

	std::string s = g_solver->GetStatisticsAsString();
	// s += "c wallclock time: " + std::to_string(total_solve) + " s " + "\n";
	// s += "c CPU time: " + std::to_string(double(total_clock) / CLOCKS_PER_SEC) + "\n";
	return s;
}

std::string MainGetSolutionAsString(const Pumpkin::SolverOutput& output)
{
	//unsat
	if (output.ProvenInfeasible())
	{
		return "s UNSATISFIABLE\n";
	}
	//no solution but unknown whether a solution exists
	if (output.timeout_happened && !output.HasSolution())
	{
		return "s UNKNOWN\n";
	}
	//two cases left: solution found and 1) it has been proven optimal, and 2) it has _not_ been proven optimal
	//	is optimal
	std::string s;
	if (!output.timeout_happened && output.HasSolution())
	{
		// s += "s OPTIMUM FOUND\n";
	}
	else
	{
		// s += "s UNKNOWN\n";
	}

	if (!g_print_sol) { return s; }

	s += "v ";
	for (int i = 2; i <= output.solution.NumVariables(); i++) //indices 0 and 1 are reserved for special purposes
	{
		Pumpkin::IntegerVariable var(i - 1); //minus one since user variables start with index 2, where in MaxSAT files the index starts from 1.
		int assignment = output.solution[var];
		s += std::to_string(assignment) + " ";
	}
	s += "\n";
	return s;
}

static void SIGINT_exit(int signum)
{
	std::cout.setstate(std::ios_base::failbit);
	//std::string s = MainGetStatisticsAsString();
	//s += MainGetSolutionAsString(g_solver->GetPreemptiveResult());
	//printf("%s", s.c_str());
	std::cout << "SAT makespan: " << g_solver->GetPreemptiveResult().cost << '\n';
	exit(1);
}

int solve(std::string wncf_filename, bool& optimum_found)
{
	signal(SIGINT, SIGINT_exit);
	signal(SIGTERM, SIGINT_exit);
	g_start_solve = time(0);
	g_start_clock = clock();

	Pumpkin::ParameterHandler parameters = Pumpkin::ConstraintOptimisationSolver::CreateParameterHandler();

	g_print_sol = parameters.GetBooleanParameter("print-solution");

	std::string file = "";

	g_print_sol = false;
	parameters.SetStringParameter("file", wncf_filename);

	//parameters.SetIntegerParameter("time", cpu_time_deadline);

	Pumpkin::ConstraintOptimisationSolver::CheckCorrectnessOfParameterHandler(parameters);

	file = parameters.GetStringParameter("file");

	// std::cout << "c File: " << file << std::endl;

	Pumpkin::ProblemSpecification* problem_specification;

	Pumpkin::ConstraintOptimisationSolver solver(parameters);
	solver.ReadDIMACSFile(parameters.GetStringParameter("file"));
	g_solver = &solver;

	int64_t time_core_guided = parameters.GetIntegerParameter("time-core-guided");
	int64_t time_linear_search = parameters.GetIntegerParameter("time") - time_core_guided;

	Pumpkin::SolverOutput solver_output = solver.SolveBMO(time_linear_search, time_core_guided);

	// if (solver_output.timeout_happened == false) { std::cout << "c optimal\n"; }
	// else { std::cout << "c timeout\n"; }

	std::string file_type = Pumpkin::ProblemSpecification::GetFileType(file);
	if (file_type == "wcnf")
	{
		problem_specification = new Pumpkin::ProblemSpecification(Pumpkin::ProblemSpecification::ReadMaxSATFormula(file));
	}
	else if (file_type == "cnf")
	{
		problem_specification = new Pumpkin::ProblemSpecification(Pumpkin::ProblemSpecification::ReadSATFormula(file));
	}
	else
	{
		std::cout << "Unknown file format! File type was: " << file_type << ", but should be either wcnf or cnf\n";
		std::cout << "I think the problem specification reader does not work with compressed files, so that may be the issue.\n";
		std::cout << "File: " << file << "\n";
		return 1;
	}
	runtime_assert(Pumpkin::SolverOutputChecker::CheckSolutionCorrectness(*problem_specification, solver_output, true));

	if (parameters.GetStringParameter("output-file") != "")
	{
		std::ofstream output(parameters.GetStringParameter("output-file"));
		if (solver_output.ProvenInfeasible())
		{
			output << "UNSAT";
		}
		else if (solver_output.HasSolution())
		{
			std::cout << solver_output.cost << "\n";
			for (int i = 1; i <= problem_specification->NumBooleanVariables(); i++)
			{
				int truth_value = solver_output.solution[Pumpkin::IntegerVariable(i + 1)]; //+1 since the solver reserves indicies 0 and 1 for special purpose
				pumpkin_assert_permanent(truth_value == 0 || truth_value == 1, "Assignment to Boolean variable must be binary.");

				if (truth_value)
				{
					output << i << " ";
				}
				else
				{
					output << -i << " ";
				}
			}

		}
		else
		{
			output << "UNKNOWN";
		}
	}

	std::string s = MainGetStatisticsAsString();
	s += MainGetSolutionAsString(solver_output);
	printf("%s", s.c_str());

	delete problem_specification;

	if (!solver_output.timeout_happened && solver_output.HasSolution())
	{
		optimum_found = true;
	}
	else {
		optimum_found = false;
	}

	return solver_output.cost;
}
