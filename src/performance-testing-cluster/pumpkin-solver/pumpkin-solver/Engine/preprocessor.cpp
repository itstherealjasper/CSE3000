#include "preprocessor.h"
#include "../Utilities/Graph/strongly_connected_components_computer.h"
#include "../Utilities/Graph/weakly_connected_components_computer.h"
#include "../Utilities/Graph/clique_computer.h"

namespace Pumpkin
{
void Preprocessor::RemoveFixedAssignmentsFromObjective(ConstraintSatisfactionSolver& solver, LinearFunction& objective_function)
{
	runtime_assert(solver.state_.GetCurrentDecisionLevel() == 0);

	static std::vector<IntegerVariable> variables_to_remove;
	variables_to_remove.clear();
	int64_t lb_increase = 0;
	//we first collect the variables that need to be removed from the objective
	//and then in next loop do we remove them
	//	this must be done in two loops since while we are iterating through the terms we should not change their value
	for (Term term : objective_function)
	{
		runtime_assert(term.weight > 0); //for now we do not consider negative terms...
		if (solver.state_.IsAssigned(term.variable))
		{
			lb_increase += solver.state_.GetIntegerAssignment(term.variable) * term.weight;			
			variables_to_remove.push_back(term.variable); //make note of the variable so that it can be removed afterwards			
		}
	}
	//now actually remove the variables
	for (IntegerVariable variable : variables_to_remove) { objective_function.RemoveVariable(variable); }
	objective_function.AddConstantTerm(lb_increase);

	if (lb_increase > 0) { std::cout << "c preprocessor trivial lb increase " << lb_increase << ", removed " << variables_to_remove.size() << " variables\n"; }
}

bool Preprocessor::PruneDomainValueBasedOnUpperBound(ConstraintSatisfactionSolver& solver, LinearFunction& objective_function, int64_t upper_bound)
{//todo should do this until a fixed point, but for now we only do it once
	runtime_assert(solver.state_.GetCurrentDecisionLevel() == 0);

	static std::vector<IntegerVariable> variables_to_remove;
	variables_to_remove.clear();
	int64_t hardening = 0;
	for (Term term : objective_function)
	{
		int new_upper_bound = upper_bound / term.weight;
		int old_upper_bound = solver.state_.domain_manager_.GetUpperBound(term.variable);
		if (new_upper_bound < old_upper_bound)
		{
			hardening += (old_upper_bound - new_upper_bound) * term.weight;
			bool conflict_detected = solver.state_.SetUpperBoundForVariable(term.variable, new_upper_bound);
			runtime_assert(!conflict_detected); //can happen in principle but for now we assume this does not happen
		}
	}

	if (hardening > 0) { std::cout << "c preprocessor harden: " << hardening << "\n"; }

	RemoveFixedAssignmentsFromObjective(solver, objective_function);
	return false;
}

bool Preprocessor::MergeEquivalentLiterals(ConstraintSatisfactionSolver& solver, LinearFunction& objective_function)
{
	//essentially the issue is that I need to make sure that the flags of literals are properly set, for now I am ignoring this issue, which is okay for MaxSAT
	pumpkin_assert_permanent(solver.state_.cp_propagators_.size() == 0, "Error: for now preprocessing should be disabled when using CP propagators...todo enable later."); 
	

	Graph::StronglyConnectedComponentsComputer scc_computer;
	Graph::AdjacencyListGraph implication_graph = ConstructImplicationGraph(solver, objective_function);
	std::vector<std::vector<int> > sccs = scc_computer.Solve(implication_graph);

	std::cout << "c num non-trivial sccs: " << sccs.size() << "\n";

	RemoveFixedAssignmentsFromObjective(solver, objective_function);

	PairRepresentativesObjective rep_info = ComputeRepresentativeLiterals(sccs, solver, objective_function);

	RemoveEquivalentVariablesFromVariableSelection(solver, rep_info);

	//remove all clauses since the clauses will be rewritten with respect to the representative literals
	//	deattaching and reattaching ensure the clauses are properly watched with the two-watched literal scheme
	solver.state_.propagator_clausal_.DetachAllClauses();

	ReplaceLiteralsInClausesByRepresentativeLiterals(solver.state_.propagator_clausal_.permanent_clauses_, *solver.state_.propagator_clausal_.clause_allocator_, rep_info.lit_to_representative, solver);
	ReplaceLiteralsInClausesByRepresentativeLiterals(solver.state_.propagator_clausal_.learned_clauses_.low_lbd_clauses, *solver.state_.propagator_clausal_.clause_allocator_, rep_info.lit_to_representative, solver);
	ReplaceLiteralsInClausesByRepresentativeLiterals(solver.state_.propagator_clausal_.learned_clauses_.temporary_clauses, *solver.state_.propagator_clausal_.clause_allocator_, rep_info.lit_to_representative, solver);

	Stopwatch stopwatch;

	RemoveDuplicateClauses(solver.state_.propagator_clausal_.permanent_clauses_, *solver.state_.propagator_clausal_.clause_allocator_, solver.state_);
	RemoveDuplicateClauses(solver.state_.propagator_clausal_.learned_clauses_.low_lbd_clauses, *solver.state_.propagator_clausal_.clause_allocator_, solver.state_);
	RemoveDuplicateClauses(solver.state_.propagator_clausal_.learned_clauses_.temporary_clauses, *solver.state_.propagator_clausal_.clause_allocator_, solver.state_);

	std::cout << "c duplicate removal took " << stopwatch.TimeElapsedInSeconds() << " seconds\n";

	RewriteObjectiveFunctionWithRepresentativeLiterals(solver, rep_info, objective_function);

	ReplaceLiteralsInIntegerRepresentationByRepresentativeLiterals(solver, rep_info);
	//not sure if and how literal_information should be updated, there could be a flag to say that the literal is no longer used
	//	could be problematic if we add new constraints using the old literals...todo needed for CP

	solver.state_.propagator_clausal_.ReattachAllClauses();

	//	possibly change the objective if we touch objective literals
	//	need to update the flag for each literal...or not, since CP relies on integer variables, not on internal booleans...need to think this through todo

	UpdateImplicationGraphWithRepresentativeLiterals(solver, implication_graph, rep_info);

	RewriteObjectiveBasedOnAtMostOneConstraints(solver, objective_function, rep_info, implication_graph);

	/*{
		std::cout << "\tweird, going again\n";
		implication_graph = ConstructImplicationGraph(solver, objective_function);

		RewriteObjectiveBasedOnAtMostOneConstraints(solver, objective_function, rep_info, implication_graph);

		std::cout << "\tweird, going again\n";
		implication_graph = ConstructImplicationGraph(solver, objective_function);

		RewriteObjectiveBasedOnAtMostOneConstraints(solver, objective_function, rep_info, implication_graph);

		std::cout << "\tweird, going again\n";
		implication_graph = ConstructImplicationGraph(solver, objective_function);

		RewriteObjectiveBasedOnAtMostOneConstraints(solver, objective_function, rep_info, implication_graph);

		system("pause");

	}*/

	//RewriteObjectiveBasedOnIntegerDetection(solver, rep_info, implication_graph);	
	
	bool gen_file = false;
	if (gen_file)
	{
		Graph::AdjacencyListGraph implication_graph = ConstructImplicationGraph(solver, objective_function);
		std::set<int> objective_literals;
		for (Term term : objective_function)
		{
			int lb = solver.state_.domain_manager_.GetLowerBound(term.variable);
			int ub = solver.state_.domain_manager_.GetUpperBound(term.variable);
			for (int value = lb + 1; value <= ub; value++)
			{
				BooleanLiteral literal = solver.state_.GetLowerBoundLiteral(term.variable, value);
				objective_literals.insert(literal.ToPositiveInteger());				
			}
		}
		for (int var_index = 2; var_index <= solver.state_.GetNumberOfInternalBooleanVariables(); var_index++)
		{
			BooleanVariableInternal var = solver.state_.GetInternalBooleanVariable(var_index);
			BooleanLiteral lit_positive(var, true);
			BooleanLiteral lit_negative(var, false);

			IntegerVariable corresponding_integer_variable = rep_info.obj_lit_info[lit_positive.ToPositiveInteger()].corresponding_integer_variable;
			int64_t& weight_positive = rep_info.obj_lit_info[lit_positive.ToPositiveInteger()].weight;
			int64_t& weight_negative = rep_info.obj_lit_info[lit_negative.ToPositiveInteger()].weight;

			runtime_assert(weight_positive == 0 || weight_negative == 0);

			//we only consider nodes that have a weight since we are looking for hidden integers
			if (weight_positive > 0) { objective_literals.insert(lit_positive.ToPositiveInteger()); }

			if (weight_negative > 0) { objective_literals.insert(lit_negative.ToPositiveInteger()); }

			//if no weights were associated with this literal, we can skip it
			//if (weight_positive == 0 && weight_negative == 0) { continue; }
			//objective_literals.insert(lit_positive.ToPositiveInteger());
			//objective_literals.insert(lit_negative.ToPositiveInteger());
		}
		implication_graph.PrintSubgraphToFile(objective_literals, "patras-stripped.txt");

		int free_cores = 0;
		for (Term term : objective_function)
		{
			int lb = solver.state_.domain_manager_.GetLowerBound(term.variable);
			int ub = solver.state_.domain_manager_.GetUpperBound(term.variable);
			for (int value = lb + 1; value <= ub; value++)
			{
				BooleanLiteral literal = solver.state_.GetLowerBoundLiteral(term.variable, value);
				int64_t adv_weight = -1;
				BooleanLiteral best_n;
				for (int neighbour_code : implication_graph.GetNeighbours((~literal).ToPositiveInteger()))
				{
					BooleanLiteral neighbour_literal = BooleanLiteral::CreateLiteralFromCodeAndFlag(neighbour_code, false);
					int64_t neighbour_weight = rep_info.obj_lit_info[neighbour_literal.ToPositiveInteger()].weight;
					if (neighbour_weight > adv_weight && neighbour_weight > 0)
					{
						adv_weight = neighbour_weight;
						best_n = neighbour_literal;						
					}
				}
				if (adv_weight != -1)
				{
					free_cores++;
					//std::cout << term.weight << ", " << adv_weight << "\n";
					//rep_info.obj_lit_info[best_n.ToPositiveInteger()].weight = 0;
					//rep_info.obj_lit_info[literal.ToPositiveInteger()].weight = 0;				
				}
			}
		}
		std::cout << "free cores: " << free_cores << "\n";

		Graph::WeaklyConnectedComponentsComputer wcc_computer;
		auto wccs = wcc_computer.Solve(implication_graph);

		std::cout << "Num weakly connected components: " << wccs.size() << "\n";

		Graph::AdjacencyListGraph new_implication_graph(implication_graph.NumNodes());
		for (int node_index : objective_literals)
		{
			BooleanLiteral node_lit = BooleanLiteral::CreateLiteralFromCodeAndFlag(node_index, false);
			int neg_node_index = (~node_lit).ToPositiveInteger();
			for (int neighbour_index : implication_graph.GetNeighbours(neg_node_index))
			{
				if (objective_literals.count(neighbour_index) > 0)
				{
					new_implication_graph.AddNeighbour(node_index/2, neighbour_index/2);
					new_implication_graph.AddNeighbour(neighbour_index/2, node_index/2);					
				}				
			}
		}
		
		clock_t clique_start = clock();
		Graph::CliqueComputer clique_computer;
		auto cliques = clique_computer.SolveManyCliquesGreedy(new_implication_graph);

		std::cout << "Num cliques: " << cliques.size() << "\n";	
		std::set<int> nodes_in_cliques;
		for (auto& v : cliques) { for (int n : v) { nodes_in_cliques.insert(n); } }
		std::cout << "\tnum nodes in cliques: " << nodes_in_cliques.size() << "\n";
		std::cout << "\t" << double(clock() - clique_start) / CLOCKS_PER_SEC << " seconds\n";

		exit(1);
	}
	return false;
}

void Preprocessor::RemoveDuplicateClauses(vec<ClauseLinearReference>& clauses, LinearClauseAllocator &clause_allocator, SolverState &state)
{
	if (clauses.size() == 0) { return; }

	int num_removed_duplicate_clauses = 0;

	//to remove duplicate clauses, we sort the clauses according to size (primarily) and a hash value (secondarily)
	//	given a pair (size, hash), another duplicate clause can only be in the block that has the same size anad same hash

	std::vector<int64_t> hash(clause_allocator.GetNumCurrentUsedIntegers(), -1);

	for (int i = 0; i < clauses.size(); i++)
	{
		Clause& clause = clause_allocator.GetClause(clauses[i]);
		//sort, and then compute the hash
		//	sorting is needed to speed up clause comparison later
		std::sort(&clause[0], &clause[0] + clause.Size(), [](BooleanLiteral lit1, BooleanLiteral lit2)->bool { return lit1.ToPositiveInteger() < lit2.ToPositiveInteger(); });
		
		//	compute the hash now
		int64_t hash_value = int64_t(clause.Size());
		for (int i = 0; i < clause.Size(); i++)
		{
			int64_t code = clause[i].ToPositiveInteger();
			hash_value ^= code + 0x9e3779b9 + (hash_value << 6) + (hash_value >> 2);
		}

		hash[clauses[i].id] = hash_value;
	}

	//sort clauses, primarily on the size, then by hash
	std::sort(
		clauses.begin(),
		clauses.end(),
		[&clause_allocator, &hash](const ClauseLinearReference cr1, const ClauseLinearReference cr2)->bool
		{
			pumpkin_assert_moderate(hash[cr1.id] != -1 && hash[cr2.id] != -1, "Sanity check.");

			Clause& c1 = clause_allocator.GetClause(cr1);
			Clause& c2 = clause_allocator.GetClause(cr2);

			//compare based on size
			if (c1.Size() < c2.Size()) { return true; }
			if (c1.Size() > c2.Size()) { return false; }

			//the size is the same, so compare based on hash
			if (hash[cr1.id] < hash[cr2.id]) { return true; }
			if (hash[cr1.id] > hash[cr2.id]) { return false; }

			//at this point the clauses have same size and same hash value
			return false;
		}
	);

	//the clauses have been sorted
	//	so now search for duplicate clauses with each unique block of (size, hash)
	int block_start = 0;
	while (block_start + 1 < clauses.size())
	{
		//find the end of the current block
		int block_size = clause_allocator.GetClause(clauses[block_start]).Size();
		int64_t block_hash = hash[clauses[block_start].id];
		int block_end = block_start + 1;
		while (block_end < clauses.size()
				&& block_size == clause_allocator.GetClause(clauses[block_end]).Size()
				&& block_hash == hash[clauses[block_end].id])
		{
			++block_end;
		}

		//if (block_end - block_start > 2) { std::cout << "M: " << (block_end - block_start) << "\n"; }
		//int old = num_removed_duplicate_clauses;

		//now each clause within the index [block_start, block_end) has the same size and same hash
		//	we do a pairwise comparison to determine whether the clauses are identical		
		for (int i = block_start; i < block_end; i++)
		{
			Clause& c1 = clause_allocator.GetClause(clauses[i]);
			if (c1.IsDeleted()) { continue; } //clause can get deleted in this process (see below), so we need to check
			
			for (int j = i + 1; j < block_end; j++)
			{
				Clause& c2 = clause_allocator.GetClause(clauses[j]);
				if (c2.IsDeleted()) { continue; }
				pumpkin_assert_simple(c1.Size() == c2.Size() && hash[clauses[i].id] == hash[clauses[j].id] && hash[clauses[i].id] != -1, "Sanity check"); //this is a redundant check...
				
				//now compare the two clauses
				//	note that previously we sorted the literals of the clause
				//	so comparing two clauses comes down to comparing two sorted arrays
				bool are_identical = true;
				int num_literals = c1.Size();
				for (int m = 0; m < num_literals; m++)
				{
					if (c1[m] != c2[m])
					{
						are_identical = false;
						break;
					}
				}
				//if the clauses are the same, we deleted the second one
				//	we will remove deleted clauses from the array after this loop, see below
				if (are_identical)
				{
					clause_allocator.DeleteClause(clauses[j]);
					++num_removed_duplicate_clauses;
				}
			}
		}

		/*if (block_end - block_start > 2 && num_removed_duplicate_clauses - old > 0) std::cout << "\tremoved: " << num_removed_duplicate_clauses - old << "\n";
		
		if (block_size > 2 && block_end - block_start > 2 && num_removed_duplicate_clauses - old == 0)
		{
			for (int i = block_start; i < block_end; i++)
			{
				Clause& c1 = clause_allocator.GetClause(clauses[i]);
				if (c1.IsDeleted()) { continue; } //clause can get deleted in this process (see below), so we need to check

				vec<BooleanLiteral> disj;
				for (BooleanLiteral lit : c1) { disj.push(lit); }
				std::cout << "\t" << state.PrettyPrintClause(disj) << " : " << hash[clauses[i].id] << "\n";
			}
			system("pause");
		}*/

		block_start = block_end;
	}

	//remove deleted clauses from the clause array
	int new_size = 0;
	for (int i = 0; i < clauses.size(); i++)
	{
		Clause& clause = clause_allocator.GetClause(clauses[i]);
		//if the clause has not been deleted, we keep it
		//	otherwise the clause is effectively removed from the array since we do not copy its reference nor increment new_size
		if (!clause.IsDeleted()) 
		{ 
			clauses[new_size] = clauses[i];
			++new_size;
		}
	}
	clauses.resize(new_size);	

	if (num_removed_duplicate_clauses > 0) { std::cout << "c removed " << num_removed_duplicate_clauses << " duplicate clauses\n"; }
}

Graph::AdjacencyListGraph Preprocessor::ConstructImplicationGraph(ConstraintSatisfactionSolver& solver, LinearFunction& objective_function)
{
	bool use_propagation_implications = true;
	bool propagate_only_objective_literals = true;
		
	solver.state_.propagator_clausal_.PerformSimplificationAndGarbageCollection();

	Graph::AdjacencyListGraph graph(2*(solver.state_.GetNumberOfInternalBooleanVariables()+2)); //we multiply by two since each variable has two literals
	Stopwatch stopwatch;

	AddEdgesToImplicationGraphBasedOnClauses(solver.state_.propagator_clausal_.permanent_clauses_, solver, graph);
	AddEdgesToImplicationGraphBasedOnClauses(solver.state_.propagator_clausal_.learned_clauses_.low_lbd_clauses, solver, graph);
	AddEdgesToImplicationGraphBasedOnClauses(solver.state_.propagator_clausal_.learned_clauses_.temporary_clauses, solver, graph);

	std::cout << "c clausal implications took " << stopwatch.TimeElapsedInSeconds() << " seconds\n";
	stopwatch.Initialise(0);

	if (use_propagation_implications)
	{
		vec<BooleanLiteral> lits;
		if (propagate_only_objective_literals)
		{
			for (const Term& term : objective_function)
			{
				pumpkin_assert_simple(term.weight > 0, "Sanity check"); //we assume the objective function is in canonical form
				int lb = solver.state_.domain_manager_.GetLowerBound(term.variable);
				int ub = solver.state_.domain_manager_.GetUpperBound(term.variable);
				for (int i = lb + 1; i <= ub; i++)
				{
					BooleanLiteral objective_literal = solver.state_.GetLowerBoundLiteral(term.variable, i);
					lits.push(objective_literal);
					lits.push(~objective_literal);
				}
			}
		}
		else
		{
			for (int var_index = 2; var_index <= solver.state_.GetNumberOfInternalBooleanVariables(); var_index++)
			{
				BooleanVariableInternal var = solver.state_.GetInternalBooleanVariable(var_index);
				BooleanLiteral positive_literal(var, true);
				BooleanLiteral negative_literal(var, false);
				lits.push(positive_literal);
				lits.push(negative_literal);
			}
		}

		AddEdgesToImplicationGraphBasedOnPropagation(lits, solver, graph);

		std::cout << "c propagation implications took " << stopwatch.TimeElapsedInSeconds() << " seconds\n";
	}	

	stopwatch.Initialise(0);

	graph.RemoveDuplicateAndLoopEdges();

	std::cout << "c edge duplicate removal took " << stopwatch.TimeElapsedInSeconds() << " seconds\n";

	pumpkin_assert_advanced(DebugCheckCorrectnessOfImplicationGraph(graph, solver), "Sanity check.");

	return graph;
}
/*void Preprocessor::AddEdgeToImplicationGraph(vec<ClauseLinearReference>& clauses, ConstraintSatisfactionSolver& solver, AdjacencyListGraph& graph)
{
	for (ClauseLinearReference clause_reference : clauses)
	{
		Clause& clause = solver.state_.propagator_clausal_.clause_allocator_->GetClause(clause_reference);

		//if the clause is binary, then add it to the graph
		//	since we did garbage collection before, we will not miss out on any clauses
		if (clause.Size() == 2)
		{
			//the clause (a v b) gives rise to two implications:
			//	~a -> b
			graph.AddNeighbour((~clause[0]).ToPositiveInteger(), clause[1].ToPositiveInteger());
			//	~b -> ~a
			graph.AddNeighbour((~clause[1]).ToPositiveInteger(), (~clause[0]).ToPositiveInteger());
		}
	}
}*/

void Preprocessor::AddEdgesToImplicationGraphBasedOnClauses(vec<ClauseLinearReference>& clauses, ConstraintSatisfactionSolver& solver, Graph::AdjacencyListGraph& graph)
{
	int num_edges_added = 0;
	for (ClauseLinearReference clause_reference : clauses)
	{
		Clause& clause = solver.state_.propagator_clausal_.clause_allocator_->GetClause(clause_reference);
		BooleanLiteral lit1, lit2;
		int num_unassigned_literals = 0;
		bool is_satisfied = false;

		//could simplfy the loop if we knew that garbage collection took place, i.e., there may be literals that are true at the root still in clauses
		for (int i = 0; i < clause.Size(); i++)
		{
			if (num_unassigned_literals > 2) { break; }

			if (solver.state_.assignments_.IsAssignedTrue(clause[i]))
			{
				runtime_assert(1 == 2);
				is_satisfied = true;
				break;
			}

			if (solver.state_.assignments_.IsUnassigned(clause[i]))
			{
				if (num_unassigned_literals == 0) { lit1 = clause[i]; }
				else if (num_unassigned_literals == 1) { lit2 = clause[i]; }
				num_unassigned_literals++;
			}
		}
		//if the clause is binary, then add it to the graph
		if (num_unassigned_literals == 2 && !is_satisfied)
		{
			runtime_assert(clause.Size() == 2);

			//lit1 -> lit2
			graph.AddNeighbour((~lit1).ToPositiveInteger(), lit2.ToPositiveInteger());
			//~lit2 -> lit1
			graph.AddNeighbour((~lit2).ToPositiveInteger(), lit1.ToPositiveInteger());
			num_edges_added += 2;
		}
	}
	if (num_edges_added > 0 ) std::cout << "c inserted " << num_edges_added << " edges into implication graph\n";
}

void Preprocessor::AddEdgesToImplicationGraphBasedOnPropagation(vec<BooleanLiteral>& literals, ConstraintSatisfactionSolver& solver, Graph::AdjacencyListGraph& graph)
{
	pumpkin_assert_permanent(solver.state_.GetCurrentDecisionLevel() == 0, "Error: preprocessor expects the solver to be at the root level.");

	std::cout << "c prop implication with " << literals.size() << " literals\n";
	//it seems to expensive to add all implications compare to the benefit
	//	so we limit the implications added to only those that directly involve the input literals
	//	todo investigate more closely
	std::vector<bool> allowed_to_add(literals.size(), false);
	for (BooleanLiteral lit: literals)
	{
		if (lit.ToPositiveInteger() >= allowed_to_add.size()) { allowed_to_add.resize(lit.ToPositiveInteger() + 1, false); }
		allowed_to_add[lit.ToPositiveInteger()] = true;
	};

	int num_unit_clauses_added = 0;
	int num_edges_added = 0;
	auto& assignments = solver.state_.assignments_;
	for (BooleanLiteral lit : literals)
	{
		if (assignments.IsAssigned(lit)) { continue; }

		//propagate the literal
		solver.state_.IncreaseDecisionLevel();
		solver.state_.EnqueueDecisionLiteral(lit);
		PropagationStatus status = solver.state_.PropagateEnqueuedLiterals();
		//if there was no conflict, then collect implications from the trail
		if (!status.conflict_detected)
		{
			pumpkin_assert_simple(solver.state_.trail_[solver.state_.trail_delimiter_[0]] == lit, "Sanity check");

			for(int i = solver.state_.trail_delimiter_[0] + 1; i < solver.state_.trail_.size(); i++) //+1 is needed to skip the current literal 'lit'
			{
				BooleanLiteral trail_literal = solver.state_.trail_[i];

				if (trail_literal.ToPositiveInteger() < allowed_to_add.size() && allowed_to_add[trail_literal.ToPositiveInteger()])
				{
					//a -> b
					graph.AddNeighbour(lit.ToPositiveInteger(), trail_literal.ToPositiveInteger());
					//~b -> ~a
					graph.AddNeighbour((~trail_literal).ToPositiveInteger(), (~lit).ToPositiveInteger());
					num_edges_added += 2;
				}				
			}
			solver.state_.Backtrack(0);
		}	
		//if conflict detected, add the opposite polarity as a unit clause
		else
		{
			++num_unit_clauses_added;
			solver.state_.Backtrack(0);
			bool conflict_detected = solver.state_.propagator_clausal_.AddUnitClause(~lit);
			pumpkin_assert_simple(!conflict_detected, "Error: unsat at the root level - this is not in principle an error, but for now we assume it is for the instances we consider.");
		}		
	}	

	if (num_unit_clauses_added > 0 || num_edges_added > 0)
	{
		std::cout << "c implication construction added " << num_unit_clauses_added << " unit clauses and " << num_edges_added << " edges to the graph\n";
	}

}

Preprocessor::PairRepresentativesObjective Preprocessor::ComputeRepresentativeLiterals(std::vector<std::vector<int>>& sccs, ConstraintSatisfactionSolver& solver, LinearFunction& objective_function)
{
	Stopwatch stopwatch;
	//the strongly connected components form the equivalence classes
	//	note that we need to be careful with combining objective literals since their weights may need to change (see below)

	//construct a direct hash of literals to their weight
	//	used later to test if a literal is an objective literal
	PairRepresentativesObjective return_value(2 * (solver.state_.GetNumberOfInternalBooleanVariables() + 2));
	
	int num_objective_literals_merged = 0;
	//for now we only handle objective literals that are related to 0-1 variables in the objective
	// 	for MaxSAT this is enough, but for CP it does not cover all cases
	//	more general treatment of objective literals not support yet
	std::vector<bool> is_literal_allowed(2 * (solver.state_.GetNumberOfInternalBooleanVariables() + 2), true);
	for (const Term& term : objective_function)
	{
		pumpkin_assert_simple(term.weight > 0, "Sanity check"); //we assume the objective function is in canonical form
		int lb = solver.state_.domain_manager_.GetLowerBound(term.variable);
		int ub = solver.state_.domain_manager_.GetUpperBound(term.variable);
		bool is_binary_objective_variable = (lb == 0 && ub == 1);
		pumpkin_assert_permanent(is_binary_objective_variable, "testing, remove this assert.");
		for (int i = lb + 1; i <= ub; i++)
		{
			BooleanLiteral objective_literal = solver.state_.GetLowerBoundLiteral(term.variable, i);
			
			return_value.obj_lit_info[objective_literal.ToPositiveInteger()].weight = term.weight;
			
			return_value.obj_lit_info[objective_literal.ToPositiveInteger()].corresponding_integer_variable = term.variable;
			return_value.obj_lit_info[(~objective_literal).ToPositiveInteger()].corresponding_integer_variable = term.variable;
			
			is_literal_allowed[objective_literal.ToPositiveInteger()] = is_binary_objective_variable;
			is_literal_allowed[(~objective_literal).ToPositiveInteger()] = is_binary_objective_variable;
		}
	}

	int num_literals_merged = 0, num_ignored_merges = 0, num_pure_objective = 0;
	//merge all the literals in the same component
	for (std::vector<int>& component : sccs)
	{
		//find a literal that is allowed that will be used as the reference point
		BooleanLiteral reference_literal = BooleanLiteral::UndefinedLiteral();
		for (int i = 0; i < component.size(); i++)
		{
			BooleanLiteral component_literal = BooleanLiteral::CreateLiteralFromCodeAndFlag(component[i], false);
			if (is_literal_allowed[component_literal.ToPositiveInteger()])
			{
				pumpkin_assert_simple(is_literal_allowed[(~component_literal).ToPositiveInteger()], "Sanity check"); //the opposite literal must also be allowed, this is probably a redundant assert but keeping it for now
				reference_literal = component_literal;
				break;
			}
		}
		//if none of the literals in the component are allowed, then skip
		if (reference_literal.IsUndefined())
		{
			num_pure_objective += component.size();
			continue;
		}

		auto is_objective_literal = [](BooleanLiteral lit, std::vector<ObjectiveLiteralInfo>& info)->bool
		{
			return info[lit.ToPositiveInteger()].weight > 0 || info[(~lit).ToPositiveInteger()].weight > 0;
		};

		num_objective_literals_merged += is_objective_literal(reference_literal, return_value.obj_lit_info);

		//merge all allowed literals with the reference literal
		for (int i = 0; i < component.size(); i++)
		{
			BooleanLiteral component_literal = BooleanLiteral::CreateLiteralFromCodeAndFlag(component[i], false);

			if (component_literal == reference_literal) { continue; }

			//ignore literals that are not allowed
			if (!is_literal_allowed[component_literal.ToPositiveInteger()])
			{
				pumpkin_assert_simple(is_literal_allowed[(~component_literal).ToPositiveInteger()], "Sanity check"); //the opposite literal must also be allowed, this is probably a redundant assert but keeping it for now
				++num_ignored_merges;
				continue;
			}

			++num_literals_merged;
			num_objective_literals_merged += is_objective_literal(component_literal, return_value.obj_lit_info);

			//I think a and ~a might still have different representatives...uh...think about this...TODO
			return_value.lit_to_representative.Merge(component_literal.ToPositiveInteger(), reference_literal.ToPositiveInteger());
			return_value.lit_to_representative.Merge((~component_literal).ToPositiveInteger(), (~reference_literal).ToPositiveInteger());
						
			int new_rep_id = return_value.lit_to_representative.GetRepresentative(reference_literal.ToPositiveInteger());
			BooleanLiteral new_rep_lit = BooleanLiteral::CreateLiteralFromCodeAndFlag(new_rep_id, false);

			//transfer weights to the new representative
			if (new_rep_id != component_literal.ToPositiveInteger())
			{
				return_value.obj_lit_info[new_rep_lit.ToPositiveInteger()].weight += return_value.obj_lit_info[component_literal.ToPositiveInteger()].weight;
				return_value.obj_lit_info[(~new_rep_lit).ToPositiveInteger()].weight += return_value.obj_lit_info[(~component_literal).ToPositiveInteger()].weight;

				return_value.obj_lit_info[component_literal.ToPositiveInteger()].weight = 0;
				return_value.obj_lit_info[(~component_literal).ToPositiveInteger()].weight = 0;
			}

			if (new_rep_id != reference_literal.ToPositiveInteger())
			{
				return_value.obj_lit_info[new_rep_lit.ToPositiveInteger()].weight += return_value.obj_lit_info[reference_literal.ToPositiveInteger()].weight;
				return_value.obj_lit_info[(~new_rep_lit).ToPositiveInteger()].weight += return_value.obj_lit_info[(~reference_literal).ToPositiveInteger()].weight;

				return_value.obj_lit_info[reference_literal.ToPositiveInteger()].weight = 0;
				return_value.obj_lit_info[(~reference_literal).ToPositiveInteger()].weight = 0;
			}
		}
	}

	std::cout << "c representative literal computation done; " << num_literals_merged << " num literals merged, " << num_objective_literals_merged << " objective merges, " << num_ignored_merges << " ignored objective merges, " << num_pure_objective << " num pure objective merges skipped\n";
	std::cout << "c t = " << stopwatch.TimeElapsedInSeconds() << "\n";

	pumpkin_assert_simple(DebugCheckConsistencyOfRepresentativeLiterals(return_value.lit_to_representative), "Sanity check");

	return return_value;

	/*auto recursive_rep = [](int code, std::vector<BooleanLiteral>& literal_to_representative)
	{
		int rep = literal_to_representative[code].ToPositiveInteger();
		while (rep != literal_to_representative[rep].ToPositiveInteger())
		{
			int old_rep = rep;
			rep = literal_to_representative[rep].ToPositiveInteger();
			literal_to_representative[code] = BooleanLiteral::IntToLiteral(rep);
			code = old_rep;
		}
		return BooleanLiteral::IntToLiteral(rep);
	};

	std::vector<BooleanLiteral> literal_to_representative(2 * (solver.state_.GetNumberOfVariables() + 2));
	
	//need to be careful with replacing literals when I add the 1:31 bit representation...
	std::cout << "c remember to check literal assignments when I add the 1:31 representation...\n";
	//initial each literal is its own representative
	for (int i = 4; i < literal_to_representative.size(); i++)
	{
		literal_to_representative[i] = BooleanLiteral::IntToLiteral(i);
	}

	std::vector<int64_t> literal_weight(2 * (solver.state_.GetNumberOfVariables() + 2), 0);
	for (const Term& term : objective_function)
	{
		pumpkin_assert_simple(term.weight > 0); //we assume the objective function is in canonical form
		int lb = solver.state_.domain_manager_.GetLowerBound(term.variable);
		int ub = solver.state_.domain_manager_.GetUpperBound(term.variable);
		for (int i = lb + 1; i <= ub; i++)
		{
			BooleanLiteral objective_literal = solver.state_.GetLowerBoundLiteral(term.variable, i);
			literal_weight[objective_literal.ToPositiveInteger()] = term.weight;
		}		
	}

	***

	//for now we never merge objective literals for simplicity, todo
	int num_ignored_objective_merges = 0;
	
	//do merging until a fixed point -> may be better ways to do union-find, but this will be good enough probably
	bool change_happened = true;
	int num_iters = 0, num_literals = 0;
	while (change_happened)
	{
		change_happened = false;
		num_literals = 0;
		num_ignored_objective_merges = 0;
		++num_iters;
		for (std::vector<int>& component : sccs)
		{
			//find the reference point as the literal with the smallest representative literal
			//	note that we ignore objective literals, these are currently never merged
			BooleanLiteral reference_point = BooleanLiteral::UndefinedLiteral();
			for (int i = 0; i < component.size(); i++)
			{
				BooleanLiteral component_literal = BooleanLiteral::IntToLiteral(component[i]);
				//ignore objective literals
				if (literal_weight[component_literal.ToPositiveInteger()] > 0 || literal_weight[(~component_literal).ToPositiveInteger()] > 0) 
				{ 
					++num_ignored_objective_merges;
					continue; 
				}

				++num_literals;
				
				BooleanLiteral candidate = recursive_rep(component[i], literal_to_representative);

				if (reference_point.IsUndefined() || candidate.ToPositiveInteger() < reference_point.ToPositiveInteger())
				{
					reference_point = candidate;
				}
			}
			//in case all literals in the component are objective literals, no merge takes place
			if (reference_point.IsUndefined()) { continue; }

			//now replace each literal representative by the new representative literal
			//	note that inverse literals need to also be taken into account
			for (int i = 0; i < component.size(); i++)
			{
				BooleanLiteral component_literal = BooleanLiteral::IntToLiteral(component[i]);
				if (literal_weight[component_literal.ToPositiveInteger()] > 0 || literal_weight[(~component_literal).ToPositiveInteger()] > 0) { continue; }

				BooleanLiteral rep = recursive_rep(component[i], literal_to_representative);
				if (rep == reference_point) { continue; }

				literal_to_representative[component[i]] = reference_point;
				literal_to_representative[rep.ToPositiveInteger()] = reference_point;				
				change_happened = true;
			}
		}

		//since literals a and -a could end up with different representatives, we need to merge these two
		for (std::vector<int>& component : sccs)
		{
			for (int c : component)
			{
				BooleanLiteral component_literal = BooleanLiteral::IntToLiteral(c);
				BooleanLiteral rep_pos = recursive_rep(c, literal_to_representative);
				BooleanLiteral rep_neg = recursive_rep((~component_literal).ToPositiveInteger(), literal_to_representative);

				if (rep_pos.Variable() != rep_neg.Variable())
				{
					change_happened = true;
					if (rep_pos.ToPositiveInteger() < rep_neg.ToPositiveInteger())
					{
						literal_to_representative[rep_neg.ToPositiveInteger()] = ~rep_pos;
					}
					else 
					{
						literal_to_representative[rep_pos.ToPositiveInteger()] = ~rep_neg;
					}
				}
			}
		}
	}

	std::cout << "c representative literal computation done, " << num_iters << " iterations, " << num_literals << " literals involved, " << num_ignored_objective_merges << " ignored merges\n";
	std::cout << "c t = " << stopwatch.TimeElapsedInSeconds() << "\n";
	return literal_to_representative;*/
}
void Preprocessor::ReplaceLiteralsInClausesByRepresentativeLiterals(vec<ClauseLinearReference>& clauses, LinearClauseAllocator& clause_allocator, UnionFindDataStructure& literal_to_representative, ConstraintSatisfactionSolver& solver)
{
	static std::vector<BooleanLiteral> temp;

	//literals will be replaced by their equivalents
	//	but also some clauses may get removed in case they become redundant, e.g., (a v b) can become (a v ~a)
	int old_size = clauses.size();
	int new_size = 0;
	int num_removed_literals = 0;
	int num_unit_clauses = 0;
	for (int i = 0; i < clauses.size(); i++)
	{
		Clause &clause = clause_allocator.GetClause(clauses[i]);
		//first replace all literals with their equivalent literal
		for (int m = 0; m < clause.Size(); m++)
		{
			BooleanLiteral lit = clause[m];
			int code = literal_to_representative.GetRepresentative(lit.ToPositiveInteger());
			BooleanVariableInternal variable = solver.state_.GetInternalBooleanVariable(code / 2);
			BooleanLiteral rep_lit = BooleanLiteral::CreateLiteralFromCodeAndFlag(code, variable.has_cp_propagators_attached);
			clause[m] = rep_lit;
		}
		//now check for redundancy, two scenarios can happen:
		//	the clause now contains duplicate literals
		//	the clause contain both literal 'a' and '~a'
		//	for simplicity we make use of the preprocessing done in the clausal propagator, a bit slower than needed here, but probably does not make a big difference
		temp.clear();
		for (BooleanLiteral lit : clause) { temp.push_back(lit); }
		solver.state_.propagator_clausal_.PreprocessClause(temp);

		pumpkin_assert_simple(temp.size() > 0, "Resulting clause cannot be unsatisfied at root");

		if (temp.size() == 1)
		{
			/*temp.clear();
			for (BooleanLiteral lit : clause) { temp.push_back(lit); }
			solver.state_.propagator_clausal_.PreprocessClause(temp);

			std::cout << "compare:\n";
			for (int m = 0; m < clause.Size(); m++) std::cout << clause[m].ToString() << " ";
			std::cout << "\n";
			for (int m = 0; m < temp.size(); m++) std::cout << temp[m].ToString() << " ";
			std::cout << "---\n";*/

			pumpkin_assert_simple(!solver.state_.assignments_.IsAssignedFalse(temp[0]), "Sanity check.");

			num_unit_clauses += (solver.state_.assignments_.IsUnassigned(temp[0]));
			bool conflict_detected = solver.state_.propagator_clausal_.AddUnitClause(temp[0]);
			pumpkin_assert_simple(!conflict_detected, "Error: conflict during preprocessing, probably a bug in the solver although it could in principle happen.\n");
			clause_allocator.DeleteClause(clauses[i]);
			//do not increment the 'new_size' variable, effectively removing this clause from the input list
			continue;
		}

		//if the clause contained duplicate literals, they were removed, so the new size of the clause should be smaller
		if (temp.size() != clause.Size())
		{
			pumpkin_assert_simple(temp.size() < clause.Size(), "Sanity check.");
			for (int m = 0; m < temp.size(); m++) { clause[m] = temp[m]; }
			num_removed_literals += (clause.Size() - temp.size());
			clause.ShrinkToSize(temp.size()); //todo, this effectively generates wasted space, should update the clause allocator about this, or even better, ask the allocator to shrink the clause
		}

		clauses[new_size] = clauses[i];
		++new_size;
	}	
	clauses.resize(new_size);

	if (old_size != new_size) { std::cout << "c equivalence removed " << (old_size - new_size) << " redundant clauses\n"; }
	if (num_removed_literals > 0) { std::cout << "c equivalence removed " << num_removed_literals << " redundant literals\n"; }
	if (num_unit_clauses > 0) { std::cout << "c equivalence discovered " << num_unit_clauses << " unit clauses\n"; }
}

void Preprocessor::RewriteObjectiveFunctionWithRepresentativeLiterals(ConstraintSatisfactionSolver& solver, PairRepresentativesObjective& rep_info, LinearFunction& objective_function)
{
	//update the objective function, i.e., replace the terms with representative/equivalent literals
	//	a few things need to be taken care of
	//	1. we have the new weights in obj_lit_info, but not necessarily in canonical form, i.e., x and ~x may both have weights, and this needs to be dealt with by increasing the lower bound
	//	2. we could have situations where originally we had 5x + 3y, but now if x and y are equivalent, it should be written as 8x
	//	3. since we did literal merging, it could be that now a regular non-objective literal got weights and previously objective literals lost weight, so we need to update the objective function

	int64_t lb_increase = 0;
	for (int var_index = 2; var_index <= solver.state_.GetNumberOfInternalBooleanVariables(); var_index++)
	{
		BooleanVariableInternal var = solver.state_.GetInternalBooleanVariable(var_index);
		BooleanLiteral lit_positive(var, true);
		BooleanLiteral lit_negative(var, false);

		IntegerVariable corresponding_integer_variable = rep_info.obj_lit_info[lit_positive.ToPositiveInteger()].corresponding_integer_variable;
		int64_t& weight_positive = rep_info.obj_lit_info[lit_positive.ToPositiveInteger()].weight;
		int64_t& weight_negative = rep_info.obj_lit_info[lit_negative.ToPositiveInteger()].weight;

		int rep_pos = rep_info.lit_to_representative.GetRepresentative(lit_positive.ToPositiveInteger());
		int rep_neg = rep_info.lit_to_representative.GetRepresentative(lit_negative.ToPositiveInteger());

		if (lit_positive.ToPositiveInteger() != rep_pos)
		{
			runtime_assert(weight_positive == 0);
		}

		if (lit_negative.ToPositiveInteger() != rep_neg)
		{
			runtime_assert(weight_negative == 0);
		}

		//for simplicity we remove all variables from the objective and replace them with new ones
		//	since the internal Boolean representation does not grow, seems okay, but make things simpler later

		if (!corresponding_integer_variable.IsNull())
		{
			objective_function.RemoveVariable(corresponding_integer_variable);
			//the current corresponding integer will be either removed from the objective or replaced
			rep_info.obj_lit_info[lit_positive.ToPositiveInteger()].corresponding_integer_variable = IntegerVariable::UndefinedInteger();
			rep_info.obj_lit_info[lit_negative.ToPositiveInteger()].corresponding_integer_variable = IntegerVariable::UndefinedInteger();
		}

		//if no weights were associated with this literal, we can skip it
		if (weight_positive == 0 && weight_negative == 0) { continue; }

		runtime_assert(lit_negative.ToPositiveInteger() == rep_neg);
		runtime_assert(lit_positive.ToPositiveInteger() == rep_pos);

		//make the weights canonical, i.e., only one polarity should have weights, and not both
		//	since one of the two polarities will be true, we can assign the smaller weight to the lb, and decrease the larger weight
		if (weight_positive >= weight_negative)
		{
			weight_positive -= weight_negative;
			lb_increase += weight_negative;
			weight_negative = 0;
		}
		else
		{
			pumpkin_assert_simple(weight_negative >= weight_positive, "Sanity check.");
			weight_negative -= weight_positive;
			lb_increase += weight_positive;
			weight_positive = 0;
		}

		//the weights are canonical now
		//what remains is to update the objective function

		int64_t new_weight = std::max(weight_positive, weight_negative);
		//if the weight is zero, then no need to add anything to the objective
		if (new_weight > 0)
		{
			BooleanLiteral new_lit = (weight_positive > weight_negative) ? lit_positive : lit_negative;
			IntegerVariable new_objective_variable = solver.state_.CreateNewEquivalentVariable(new_lit);
			objective_function.AddTerm(new_objective_variable, new_weight);

			rep_info.obj_lit_info[lit_positive.ToPositiveInteger()].corresponding_integer_variable = new_objective_variable;
			rep_info.obj_lit_info[lit_negative.ToPositiveInteger()].corresponding_integer_variable = new_objective_variable;
		}
	}
	objective_function.AddConstantTerm(lb_increase);

	if (lb_increase > 0) { std::cout << "c lb increased by " << lb_increase << " through equivalence merging\n"; }
}

void Preprocessor::ReplaceLiteralsInIntegerRepresentationByRepresentativeLiterals(ConstraintSatisfactionSolver& solver, PairRepresentativesObjective& rep_info)
{
	//update the internal integer representation with respect to the new representatives
//	it would make sense to have this done before replacing literals in clauses
//	but not sure if that is possible when there are cp propagator, todo think about this interaction

	for (int int_id = 0; int_id < solver.state_.integer_variable_to_literal_info_.size(); ++int_id)
	{
		auto& info = solver.state_.integer_variable_to_literal_info_[int_id];

		for (int i = 0; i < info.equality_literals.size(); i++)
		{
			int code = info.equality_literals[i].ToPositiveInteger();
			int rep_id = rep_info.lit_to_representative.GetRepresentative(code);
			BooleanLiteral rep_literal = BooleanLiteral::CreateLiteralFromCodeAndFlag(rep_id, false);
			info.equality_literals[i] = rep_literal;
		}

		for (int i = 0; i < info.greater_or_equal_literals.size(); i++)
		{
			int code = info.greater_or_equal_literals[i].ToPositiveInteger();
			int rep_id = rep_info.lit_to_representative.GetRepresentative(code);
			BooleanLiteral rep_literal = BooleanLiteral::CreateLiteralFromCodeAndFlag(rep_id, false);
			info.greater_or_equal_literals[i] = rep_literal;
		}
	}
}

void Preprocessor::RemoveEquivalentVariablesFromVariableSelection(ConstraintSatisfactionSolver& solver, PairRepresentativesObjective& rep_info)
{
	//remove equivalent literals from variable selection
//	each time a solution is found, the solve will restore their values properly 
	for (int var_index = 2; var_index <= solver.state_.GetNumberOfInternalBooleanVariables(); var_index++)
	{
		BooleanVariableInternal var = solver.state_.GetInternalBooleanVariable(var_index);
		BooleanLiteral lit_positive(var, true);
		int code = rep_info.lit_to_representative.GetRepresentative(lit_positive.ToPositiveInteger());
		BooleanLiteral rep_lit = BooleanLiteral::CreateLiteralFromCodeAndFlag(code, false);

		if (lit_positive != rep_lit)
		{
			solver.state_.variable_selector_.Remove(var);
		}
	}
}

void Preprocessor::UpdateImplicationGraphWithRepresentativeLiterals(ConstraintSatisfactionSolver& solver, Graph::AdjacencyListGraph& implication_graph, PairRepresentativesObjective& rep_info)
{
	//literals should be replaced by their representatives in the graph
	//things that need to be taken into account:
	//	- transfer all edges of non-representative literals to their representatives
	//	- remove all edges from non-representative literals to other literals
	//	- remove duplicate edges from the graph
	//	- remove newly introduced self-loops

	//this is done in two passes for simplicity
	//	in the first pass, for each non-representative literal 'a', all edges (a, x) are removed, and the removed edges are transfered as (representative(a), representative(x))
	//	in the second pass, duplicates and self-loops are removed

	//	first pass
	for (int lit_code = 4; lit_code < implication_graph.NumNodes(); lit_code++)
	{
		if (rep_info.lit_to_representative.IsRepresentative(lit_code) == false)
		{
			int rep_lit = rep_info.lit_to_representative.GetRepresentative(lit_code);
			//transfer all edges first before removing
			for (int neighbour_index : implication_graph.GetNeighbours(lit_code)) //important to use lit_code and not rep_lit 
			{
				int rep_neighbour = rep_info.lit_to_representative.GetRepresentative(neighbour_index);
				implication_graph.AddNeighbour(rep_lit, rep_neighbour);
			}
			implication_graph.RemoveOutgoingEdgesForNode(lit_code);
		}		
	}
	//	second pass
	implication_graph.RemoveDuplicateAndLoopEdges();
}

void Preprocessor::RewriteObjectiveBasedOnAtMostOneConstraints(ConstraintSatisfactionSolver& solver, LinearFunction& objective_function, PairRepresentativesObjective& rep_info, Graph::AdjacencyListGraph& implication_graph)
{
	//the idea is based on the 'Intrinsic AtMost1 constraints' technique used in the MaxSAT solver RC2
	//	"RC2: an Efficient MaxSAT Solver" by A. Ignatiev, A. Morgado, and J. Marques-Silva (see Section 3.2.4)
	//	note that the presentation and implementation used here may differ from the paper cited above

	//the idea is to find a set of objective literals where at most one of the literals may be satisfied
	//	in that case, we can reformulate the instance in a core-guided style:
	//		increase the lower bound by (sum_of_weights(literals) - max_weight(literals))
	//		replace the weight of each literal by (weight(literal) - min_weight(literal))
	//			note this may remove literals from the objective if the weight becomes zero
	//		introduce new variables to capture the cost -> todo explain, see code below for now

	//to do the above, we create a graph, where there is an edge between two objective literals 'a' and 'b' if satisfying 'a' implies 'b' is violated
	//	then a clique in this graph represents a set of literals where at most one literal may be satisifed, and we can preprocess as explained above
	
	Graph::AdjacencyListGraph at_most_one_graph = CreateAtMostOneGraphBasedOnImplicationGraph(implication_graph, rep_info);
	Graph::CliqueComputer clique_computer;
	std::vector<std::vector<int> > set_of_cliques = clique_computer.SolveManyCliquesGreedy(at_most_one_graph);

	std::cout << "c found " << set_of_cliques.size() << " cliques\n";

	//int nonignored = 0;
	int64_t total_lower_bound_increase = 0;
	for (std::vector<int>& clique : set_of_cliques)
	{
		pumpkin_assert_simple(clique.size() > 0, "Sanity check.");
		//compute the sum of the literal weights, the minimum weight, and the maximum weight
		int64_t sum_weights = 0;
		int64_t min_weight = INT64_MAX;
		int64_t max_weight = INT64_MIN;
		for (int lit_code : clique)
		{
			int64_t lit_weight = rep_info.obj_lit_info[lit_code].weight;
			pumpkin_assert_simple(lit_weight > 0, "Sanity check.");
			sum_weights += lit_weight;
			min_weight = std::min(min_weight, lit_weight);
			max_weight = std::max(max_weight, lit_weight);
		}

		/*if (min_weight != max_weight)
		{
			std::cout << "ignored!\n";
			continue;
		}

		nonignored++;*/

		//update the lower bound
		//	optimistically assume that the largest weight will be the one that is satisfied
		//	recall that at most one literal may be satisfied in the clique
		int64_t lower_bound_increase = sum_weights - max_weight; 
		total_lower_bound_increase += lower_bound_increase;
		objective_function.AddConstantTerm(lower_bound_increase);

		//compute 'carry_weights'
		//'carry_weights' are only used in the weighted case, they capture the left over sum for cases when one of the literal is satisfied
		struct PairLiteralCodeWeight { int lit_code; int64_t weight; };
		std::vector<PairLiteralCodeWeight> carry_weights;
		for (int lit_code : clique)
		{
			int64_t lit_weight = rep_info.obj_lit_info[lit_code].weight;
			int64_t new_weight = max_weight - lit_weight;
			if (new_weight > 0)
			{
				PairLiteralCodeWeight p;
				p.lit_code = lit_code;
				p.weight = new_weight;
				carry_weights.push_back(p);
			}
		}

		//determine the 'integer' representation of these carry weights
		struct PairLiteralWeight { BooleanLiteral lit; int64_t weight; };
		std::vector<PairLiteralWeight> new_objective_literals;
		if (carry_weights.size() > 0)
		{
			//sorted in increasing order of the weight
			std::sort(carry_weights.begin(), carry_weights.end(), [](const PairLiteralCodeWeight& p1, const PairLiteralCodeWeight& p2)->bool { return p1.weight < p2.weight; });
			
			//create a list of 'groups'
			//	an objective literal belongs to a group with weight w if the weight of the objective literal is w
			struct Group { int64_t weight; std::vector<int> associated_literals; };
			std::vector<Group> groups;
			Group g;
			g.weight = carry_weights[0].weight;
			groups.push_back(g);
			for (int i = 0; i < carry_weights.size(); i++)
			{	
				int64_t weight = carry_weights[i].weight;
				//if so, add the literal to the group
				if (weight == groups.back().weight)
				{
					groups.back().associated_literals.push_back(carry_weights[i].lit_code);
				}
				//otherwise create new group
				else
				{
					Group g;
					g.weight = weight;
					g.associated_literals.push_back(carry_weights[i].lit_code);
					groups.push_back(g);
				}
			}

			//fix the weights of each group
			//	the weight should be current_weight - previous weight
			//	todo need to explain this better
			//	e.g., if the weights are 2 3 4, the new weights are 2 1 1
			//	the point is that the sum of the new weights is equal to the highest additional violation we may need to pay for the this clique
			int64_t previous_weights = 0;
			for (Group& g : groups)
			{
				g.weight -= previous_weights;
				previous_weights += g.weight;
			}

			//create a new literal for each group
			//	literals that belong to the group all imply this new literal
			for (Group& g : groups)
			{
				IntegerVariable new_int_var = solver.state_.CreateNewIntegerVariable(0, 1);
				BooleanLiteral objective_literal = solver.state_.GetEqualityLiteral(new_int_var, 1);

				ObjectiveLiteralInfo info;
				info.corresponding_integer_variable = IntegerVariable::UndefinedInteger();
				info.weight = 0;

				//pumpkin_assert_simple(rep_info.obj_lit_info.size() + 1 <= objective_literal.ToPositiveInteger());
				rep_info.obj_lit_info.push_back(info);
				rep_info.obj_lit_info.push_back(info);

				rep_info.obj_lit_info[objective_literal.ToPositiveInteger()].weight = g.weight;				
				rep_info.obj_lit_info[objective_literal.ToPositiveInteger()].corresponding_integer_variable = new_int_var;

				PairLiteralWeight p;
				p.lit = objective_literal;
				p.weight = g.weight;
				new_objective_literals.push_back(p);

				//add implications
				for (int lit_code : g.associated_literals)
				{
					BooleanLiteral associated_lit = BooleanLiteral::CreateLiteralFromCodeAndFlag(lit_code, false);
					//~associated_lit -> objective_lit
					bool conflict_detected = solver.state_.propagator_clausal_.AddBinaryClause(associated_lit, objective_literal);
					pumpkin_assert_simple(!conflict_detected, "Sanity check.");
				}				
			}
			//add the implication for group literals
			for (int i = groups.size() - 1; i > 0; i--)
			{
				BooleanLiteral a = new_objective_literals[i].lit;
				BooleanLiteral b = new_objective_literals[i - 1].lit;
				//a -> b
				bool conflict_detected = solver.state_.propagator_clausal_.AddBinaryClause(~a, b);
				pumpkin_assert_simple(!conflict_detected, "Sanity check.");
			}
		}
		//create new literal which is true if all the clique literals have been falsified
		// obj1 AND obj2 AND obj3 AND .... -> new_literal
		IntegerVariable new_int_var = solver.state_.CreateNewIntegerVariable(0, 1);
		BooleanLiteral all_falsified_literal = solver.state_.GetEqualityLiteral(new_int_var, 1);

		ObjectiveLiteralInfo info;
		info.corresponding_integer_variable = IntegerVariable::UndefinedInteger();
		info.weight = 0;
		//pumpkin_assert_simple(rep_info.obj_lit_info.size() + 1 <= all_falsified_literal.ToPositiveInteger());
		rep_info.obj_lit_info.push_back(info);
		rep_info.obj_lit_info.push_back(info); //a bit hacky, need to fix the size of this data structure
		rep_info.obj_lit_info[all_falsified_literal.ToPositiveInteger()].weight = min_weight;
		rep_info.obj_lit_info[all_falsified_literal.ToPositiveInteger()].corresponding_integer_variable = new_int_var;

		PairLiteralWeight p;
		p.lit = all_falsified_literal;
		p.weight = min_weight;
		new_objective_literals.push_back(p);

		if (new_objective_literals.size() >= 2)
		{
			BooleanLiteral a = all_falsified_literal;
			BooleanLiteral b = new_objective_literals[new_objective_literals.size() - 2].lit; // -2 since in the last position we have the all_falsified_literal
			//a -> b
			bool conflict_detected = solver.state_.propagator_clausal_.AddBinaryClause(~a, b);
			pumpkin_assert_simple(!conflict_detected, "Sanity check.");
		}

		std::vector<BooleanLiteral> c;
		c.push_back(all_falsified_literal);
		for (int lit_code : clique)
		{
			BooleanLiteral clique_lit = BooleanLiteral::CreateLiteralFromCodeAndFlag(lit_code, false);
			c.push_back(~clique_lit);
		}
		bool conflict_detected = solver.state_.propagator_clausal_.AddPermanentClause(c);
		pumpkin_assert_simple(!conflict_detected, "Sanity check.");

		//remove clique literals from the objective
		for (int clique_lit : clique)
		{
			pumpkin_assert_simple(rep_info.obj_lit_info[clique_lit].weight > 0, "Sanity check.");
			
			objective_function.RemoveVariable(rep_info.obj_lit_info[clique_lit].corresponding_integer_variable);
			
			rep_info.obj_lit_info[clique_lit].corresponding_integer_variable = IntegerVariable::UndefinedInteger();
			rep_info.obj_lit_info[clique_lit].weight = 0;
		}

		//add the new objective literals to the objective
		for (PairLiteralWeight &p : new_objective_literals)
		{
			IntegerVariable obj_var = rep_info.obj_lit_info[p.lit.ToPositiveInteger()].corresponding_integer_variable;
			int var_id = obj_var.id;
			pumpkin_assert_simple(solver.state_.domain_manager_.GetLowerBound(obj_var) == 0 && solver.state_.domain_manager_.GetUpperBound(obj_var) == 1, "Sanity check.");
			objective_function.AddTerm(obj_var, p.weight);
		}

		//todo record block info...
	}
	//std::cout << "NOT IGNORED " << nonignored << "\n";

	if (total_lower_bound_increase > 0) { std::cout << "c cliques increased lower bound by " << total_lower_bound_increase << "\n"; }
}

Graph::AdjacencyListGraph Preprocessor::CreateAtMostOneGraphBasedOnImplicationGraph(Graph::AdjacencyListGraph& implication_graph, PairRepresentativesObjective& rep_info)
{
	//in the at most one graph, there is an edge between two objective literals 'a' and 'b' if satisfying 'a' implies 'b' is violated
	Graph::AdjacencyListGraph at_most_one_graph(implication_graph.NumNodes());
	for (uint32_t lit_code = 0; lit_code < rep_info.obj_lit_info.size(); lit_code++)
	{
		//check if the literal is an objective literal
		if (rep_info.obj_lit_info[lit_code].weight > 0)
		{
			BooleanLiteral lit = BooleanLiteral::CreateLiteralFromCodeAndFlag(lit_code, false);
			for (int neighbour_index : implication_graph.GetNeighbours((~lit).ToPositiveInteger()))
			{
				if (rep_info.obj_lit_info[neighbour_index].weight > 0)
				{
					at_most_one_graph.AddNeighbour(lit_code, neighbour_index);
					at_most_one_graph.AddNeighbour(neighbour_index, lit_code);
				}
			}
		}
	}
	at_most_one_graph.RemoveDuplicateAndLoopEdges();
	return at_most_one_graph;
}

bool Preprocessor::DebugCheckConsistencyOfRepresentativeLiterals(UnionFindDataStructure& lit_to_representative)
{
	std::cout << "c SOMEWHAT DEBUG CHECK REP ON\n";

	//skip the first two indices since 0 and 1 are reserved for special variables
	for (int i = 2; i < lit_to_representative.NumIDs(); i += 2)
	{
		BooleanLiteral lit_pos = BooleanLiteral::CreateLiteralFromCodeAndFlag(i, false);
		BooleanLiteral lit_neg = ~lit_pos;

		int a = lit_to_representative.GetRepresentative(lit_pos.ToPositiveInteger());
		BooleanLiteral lit_a = BooleanLiteral::CreateLiteralFromCodeAndFlag(a, false);

		int b = lit_to_representative.GetRepresentative(lit_neg.ToPositiveInteger());
		BooleanLiteral lit_b = BooleanLiteral::CreateLiteralFromCodeAndFlag(b, false);

		//this would mean the problem is unsat, in principle not an error but for now we treat it as it is
		if (lit_a == lit_b) { std::cout << "c unsat implication graph? Probably a bug...\n"; runtime_assert(1 == 2); return false; }

		if (lit_a != ~lit_b) { runtime_assert(1 == 2); return false; }
	}
	return true;
}

bool Preprocessor::DebugCheckCorrectnessOfImplicationGraph(Graph::AdjacencyListGraph& graph, ConstraintSatisfactionSolver& solver)
{
	std::cout << "c EXPENSIVE DEBUG FOR IMPLICATION GRAPH ON\n";

	for (int var_index = 2; var_index <= solver.state_.GetNumberOfInternalBooleanVariables(); var_index++)
	{
		BooleanVariableInternal var = solver.state_.GetInternalBooleanVariable(var_index);
		BooleanLiteral lit_positive(var, true);
		BooleanLiteral lit_negative(var, false);

		bool ok = DebugCheckCorrectnessOfImplicationGraphForLiteral(lit_positive, graph, solver);

		if (!ok) { return false; }

		ok = DebugCheckCorrectnessOfImplicationGraphForLiteral(lit_negative, graph, solver);

		if (!ok) { return false; }
	}
	return true;
}

bool Preprocessor::DebugCheckCorrectnessOfImplicationGraphForLiteral(BooleanLiteral lit, Graph::AdjacencyListGraph& graph, ConstraintSatisfactionSolver& solver)
{
	pumpkin_assert_permanent(solver.state_.GetCurrentDecisionLevel() == 0, "Error: preprocessor expects the solver to be at the root level.");

	bool assignment_made = false;
	bool conflict_detected = false;
	//it could be that the literal is already assigned at the root, so we need to be careful
	if (!solver.state_.assignments_.IsAssigned(lit))
	{
		solver.state_.IncreaseDecisionLevel();
		solver.state_.EnqueueDecisionLiteral(lit);
		PropagationStatus status = solver.state_.PropagateEnqueuedLiterals();
		conflict_detected = status.conflict_detected;
		assignment_made = true;
	}	

	if (!conflict_detected)
	{
		for (int m : graph.GetNeighbours(lit.ToPositiveInteger()))
		{
			BooleanLiteral p = BooleanLiteral::CreateLiteralFromCodeAndFlag(m, false);
			if (!solver.state_.assignments_.IsAssignedTrue(p)) { return false; }			
		}
	}

	if (assignment_made)
	{
		solver.state_.Backtrack(0);
	}
	return true;
}

}
