#ifndef SOLVER_PARAMETERS_H
#define SOLVER_PARAMETERS_H

namespace Pumpkin
{

struct SolverParameters
{
	//Restart constants
		//See the paper "Evaluating CDCL Restart Scheme" on page 14
	/*int glucose_queue_lbd_limit;
	int glucose_queue_reset_limit;

	int num_min_conflicts_per_restart;
	int num_min_conflicts_per_clause_cleanup;

	bool bump_decision_variables;
	bool use_clause_minimisation;
	bool use_binary_clause_propagator;

	double learned_clause_decay_factor;	

	SolverParameters():
		glucose_queue_lbd_limit(50),
		glucose_queue_reset_limit(5000),
		num_min_conflicts_per_restart(50),
		num_min_conflicts_per_clause_cleanup(2000),
		bump_decision_variables(true),
		use_binary_clause_propagator(true),
		use_clause_minimisation(true),
		learned_clause_decay_factor(0.99)
	{
	}*/

};

} //end Pumpkin namespace

#endif // !SOLVER_PARAMETERS_H