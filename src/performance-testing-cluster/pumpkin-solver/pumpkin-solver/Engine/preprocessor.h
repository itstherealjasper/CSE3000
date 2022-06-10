#pragma once

#include "constraint_satisfaction_solver.h"
#include "../Utilities/boolean_literal.h"
#include "../Utilities/linear_function.h"
#include "../Utilities/Graph/adjacency_list_graph.h"
#include "../Utilities/union_find_data_structure.h"

#include <vector>

namespace Pumpkin
{
class Preprocessor
{
public:
	//variables that have their values fixed can be safely removed from the objective
	static void RemoveFixedAssignmentsFromObjective(ConstraintSatisfactionSolver& solver, LinearFunction& objective_function);
	//variable assignment that would exceed the upper bound may be removed. Note that the linear inequality propagator would anyway do this. This method is mainly used in core-guided search where the propagator might not be used.
	static bool PruneDomainValueBasedOnUpperBound(ConstraintSatisfactionSolver& solver, LinearFunction& objective_function, int64_t upper_bound);
	
	static bool MergeEquivalentLiterals(ConstraintSatisfactionSolver& solver, LinearFunction& objective_function);

	static void RemoveDuplicateClauses(vec<ClauseLinearReference>& clauses, LinearClauseAllocator& clause_allocator, SolverState &state);

private:
	struct ObjectiveLiteralInfo { int64_t weight; IntegerVariable corresponding_integer_variable; ObjectiveLiteralInfo() :weight(0), corresponding_integer_variable(0) {}; };

	static Graph::AdjacencyListGraph ConstructImplicationGraph(ConstraintSatisfactionSolver& solver, LinearFunction& objective_function);
	static void AddEdgesToImplicationGraphBasedOnClauses(vec<ClauseLinearReference>& clauses, ConstraintSatisfactionSolver& solver, Graph::AdjacencyListGraph& graph);
	static void AddEdgesToImplicationGraphBasedOnPropagation(vec<BooleanLiteral> &literals, ConstraintSatisfactionSolver& solver, Graph::AdjacencyListGraph& graph);

	struct PairRepresentativesObjective 
	{ 
		UnionFindDataStructure lit_to_representative;  
		std::vector<ObjectiveLiteralInfo> obj_lit_info;
		PairRepresentativesObjective(int n) :lit_to_representative(n), obj_lit_info(n) {}
	};

	static PairRepresentativesObjective ComputeRepresentativeLiterals(std::vector<std::vector<int> >& sccs, ConstraintSatisfactionSolver& solver, LinearFunction& objective_function);

	static void ReplaceLiteralsInClausesByRepresentativeLiterals(vec<ClauseLinearReference> &clauses, LinearClauseAllocator &clause_allocator, UnionFindDataStructure& literal_to_representative, ConstraintSatisfactionSolver &solver);
	static void RewriteObjectiveFunctionWithRepresentativeLiterals(ConstraintSatisfactionSolver& solver, PairRepresentativesObjective& rep_info, LinearFunction& objective_function);
	static void ReplaceLiteralsInIntegerRepresentationByRepresentativeLiterals(ConstraintSatisfactionSolver& solver, PairRepresentativesObjective& rep_info);

	static void RemoveEquivalentVariablesFromVariableSelection(ConstraintSatisfactionSolver& solver, PairRepresentativesObjective& rep_info);

	static void UpdateImplicationGraphWithRepresentativeLiterals(ConstraintSatisfactionSolver& solver, Graph::AdjacencyListGraph& implication_graph, PairRepresentativesObjective& rep_info);

	static void RewriteObjectiveBasedOnAtMostOneConstraints(ConstraintSatisfactionSolver& solver, LinearFunction& objective_function, PairRepresentativesObjective& rep_info, Graph::AdjacencyListGraph& implication_graph);
	static Graph::AdjacencyListGraph CreateAtMostOneGraphBasedOnImplicationGraph(Graph::AdjacencyListGraph& implication_graph, PairRepresentativesObjective& rep_info);

	static void RewriteObjectiveBasedOnIntegerDetection(ConstraintSatisfactionSolver& solver, PairRepresentativesObjective& rep_info, Graph::AdjacencyListGraph& implication_graph);

	//check whether for all literals, it holds that rep(x) = ~rep(~x)
	static bool DebugCheckConsistencyOfRepresentativeLiterals(UnionFindDataStructure& lit_to_representative);
	//check whether the graph contains correct implications that can be verified with unit propagation
	//	problem with this test is that it changes the solver state
	static bool DebugCheckCorrectnessOfImplicationGraph(Graph::AdjacencyListGraph &graph, ConstraintSatisfactionSolver &solver);
	static bool DebugCheckCorrectnessOfImplicationGraphForLiteral(BooleanLiteral lit, Graph::AdjacencyListGraph &graph, ConstraintSatisfactionSolver &solver);
};
}