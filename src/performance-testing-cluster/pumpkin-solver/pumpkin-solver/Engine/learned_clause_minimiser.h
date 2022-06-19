#pragma once

#include "solver_state.h"
#include "conflict_analysis_result_clausal.h"
#include "../Utilities/directly_hashed_integer_set.h"
#include "../Utilities/directly_hashed_boolean_variable_labeller.h"
#include "../Utilities/simplified_vector.h"
#include "../Utilities/Vec.h"

namespace Pumpkin
{
class LearnedClauseMinimiser
{
public:
	LearnedClauseMinimiser(SolverState& state);

	//assumes the first literal is the propagating literal
	//removes literals that are dominated in the implication graph from the learned clause. A literal is dominated if a subset of the other literals in the learned clause imply that literal, making the dominated literal redundant.
	//the propagating literal cannot be removed
	//the technique used is described in the following paper:
	//Sörensson, Niklas, and Armin Biere. "Minimizing learned clauses." International Conference on Theory and Applications of Satisfiability Testing. Springer, Berlin, Heidelberg, 2009.
	void RemoveImplicationGraphDominatedLiterals(ConflictAnalysisResultClausal& analysis_result_);
	
	//version from Alan Van Gelder, todo cite paper 
	void RemoveImplicationGraphDominatedLiteralsBetter(ConflictAnalysisResultClausal& analysis_result_);


	void RemoveImplicationsGraphDominatedLiterals2(ConflictAnalysisResultClausal& analysis_result_);


		
private:
	void Initialise(Disjunction& learned_literals);
	void Initialise2(ConflictAnalysisResultClausal&); //used with the Van Gelder version, todo clean up code. Probably will remove the other implementation.
	void CleanUp(Disjunction& learned_literals);
	bool IsLiteralDominated(BooleanLiteral candidate);
	bool CanRemoveLiteral(BooleanLiteral literal, int depth);
	
	void ComputeLabel(BooleanLiteral literal); //could add depth parameter to avoid recursing too deep

	SolverState &state_;

	DirectlyHashedBooleanVariableLabeller mark_;
	DirectlyHashedIntegerSet frames_;
	int depth_;
	static const int MAX_DEPTH;
	static const char PRESENT, KEEP, REMOVABLE, POISON;	
	int64_t total_removed_literals_, num_minimisation_calls_, num_literals_before_minimising_, root_literals_removed_; //helper variables used to gather statistics
	DirectlyHashedBooleanVariableLabeller labels_;
	DirectlyHashedIntegerSet allowed_decision_levels_;
	vec<BooleanLiteral> stack_;
	
};
} //end namespace Pumpkin