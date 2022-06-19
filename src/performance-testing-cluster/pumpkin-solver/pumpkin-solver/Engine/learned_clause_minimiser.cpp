#include "learned_clause_minimiser.h"
#include "../Utilities/runtime_assert.h"

#include <iostream>
#include <vector>

#define SEEN 1
#define POISONED 2
#define REMOVABLE2 4

namespace Pumpkin
{
	
const char LearnedClauseMinimiser::PRESENT = 0;
const char LearnedClauseMinimiser::KEEP = 1;
const char LearnedClauseMinimiser::REMOVABLE = 2;
const char LearnedClauseMinimiser::POISON = 3;
const int LearnedClauseMinimiser::MAX_DEPTH = 500;

LearnedClauseMinimiser::LearnedClauseMinimiser(SolverState& state):
	state_(state),
	total_removed_literals_(0),
	num_minimisation_calls_(0),
	labels_(0),
	allowed_decision_levels_(0),
	num_literals_before_minimising_(0),
	root_literals_removed_(0),
	depth_(0),
	mark_(0),
	frames_(0)
{
}

void LearnedClauseMinimiser::RemoveImplicationGraphDominatedLiterals(ConflictAnalysisResultClausal& analysis_result_)
{
	Disjunction& learned_literals = analysis_result_.learned_clause_literals;

	Initialise(learned_literals);	
	size_t end_position(1); //we also keep the propagating literal, which is located in the zeroth index
	for (size_t i = 1; i < learned_literals.size(); i++)
	{
		BooleanLiteral learned_literal = learned_literals[i];
		//note that it does not make sense to remove the propagated literal
		if (IsLiteralDominated(learned_literal) == false)
		{
			learned_literals[end_position] = learned_literal;
			end_position++;
		}
		root_literals_removed_ += (state_.assignments_.GetAssignmentLevel(learned_literal) == 0);
	}
	learned_literals.resize(end_position);
	CleanUp(learned_literals);
	pumpkin_assert_advanced(analysis_result_.CheckCorrectnessAfterConflictAnalysis(state_), "Sanity check.");
}

void LearnedClauseMinimiser::RemoveImplicationGraphDominatedLiteralsBetter(ConflictAnalysisResultClausal& analysis_result_)
{
	Initialise2(analysis_result_);

	size_t end_position(1); //the propagated literal must stay
	for (size_t i = 1; i < analysis_result_.learned_clause_literals.size(); i++)
	{
		BooleanLiteral learned_literal = analysis_result_.learned_clause_literals[i];
		
		if (labels_.IsAssignedSpecificLabel(learned_literal.Variable(), PRESENT))
		{
			ComputeLabel(~learned_literal); //the label is stored in the 'labels_' data structure
		}
			
		char label = labels_.GetLabel(learned_literal.Variable());
		runtime_assert(label == KEEP || label == REMOVABLE || label == POISON);
		
		//poison is only assigned if the recursion went over the depth limit: another label may be assigned in future iterations of the code
		if (label == KEEP || label == POISON) 
		{
			analysis_result_.learned_clause_literals[end_position] = learned_literal;
			end_position++;

			//ensure that the literal at position 1 is at the highest level
			if (state_.assignments_.GetAssignmentLevel(analysis_result_.learned_clause_literals[end_position - 1]) > state_.assignments_.GetAssignmentLevel(analysis_result_.learned_clause_literals[1]))
			{
				std::swap(analysis_result_.learned_clause_literals[end_position - 1], analysis_result_.learned_clause_literals[1]);
			}
		}//otherwise the literal is effectively removed
	}
	analysis_result_.learned_clause_literals.resize(end_position);

	CleanUp(analysis_result_.learned_clause_literals);
	pumpkin_assert_advanced(analysis_result_.CheckCorrectnessAfterConflictAnalysis(state_), "Sanity check.");
}

void LearnedClauseMinimiser::RemoveImplicationsGraphDominatedLiterals2(ConflictAnalysisResultClausal& analysis_result_)
{
	runtime_assert(mark_.GetNumLabelledVariables() == 0);

	while (mark_.GetCapacity() + 1 < state_.GetNumberOfInternalBooleanVariables()) { mark_.Grow(); }
	while (frames_.GetCapacity() + 1 < state_.GetNumberOfInternalBooleanVariables()) { frames_.Grow(); }

	//index goes from one since we skip the asserting literal
	for(int i = 1; i < analysis_result_.learned_clause_literals.size(); i++)
	{
		BooleanLiteral literal = analysis_result_.learned_clause_literals[i];
		mark_.AssignLabel(literal.Variable(), SEEN);
		frames_.Insert(state_.assignments_.GetAssignmentLevel(literal));
	}

	//index goes from one since we always keep the asserting literal
	int end_position = 1;
	for (int i = 1; i < analysis_result_.learned_clause_literals.size(); i++)
	{
		BooleanLiteral literal = analysis_result_.learned_clause_literals[i];
		if (!CanRemoveLiteral(literal, 0))
		{
			analysis_result_.learned_clause_literals[end_position] = literal;
			end_position++;
		}
	}
	analysis_result_.learned_clause_literals.resize(end_position);

	//for (all_elements_on_stack(unsigned, idx, solver->marked))
	//	solver->marks[idx] &= SEEN;

	//CLEAR_STACK(solver->marked);
	mark_.Clear();
	frames_.Clear();
}

bool LearnedClauseMinimiser::CanRemoveLiteral(BooleanLiteral literal, int depth)
{
	//const unsigned idx = literal.VariableIndex();
	signed char mark = mark_.IsLabelled(literal.Variable()) ? mark_.GetLabel(literal.Variable()) : 0; // solver->marks[idx];
	assert(mark >= 0);
	if (mark & POISONED)
		return false;		// Previously shown not to be removable.
	if (mark & REMOVABLE2)
		return true;		// Previously shown to be removable.
	if (depth && (mark & SEEN))
		return true;		// Analyzed thus removable (unless start).
	if (depth > MAX_DEPTH)
		return false;		// Avoids deep recursion (stack overflow).
	//assert(solver->values[lit] < 0);
	Clause* reason_clause = state_.GetReasonClausePointer(literal);
	//struct clause* reason = solver->reasons[idx];
	if (reason_clause == NULL)
		return false;		// Decisions can not be removed.
	const unsigned level = state_.assignments_.GetAssignmentLevel(literal);
	if (!level)
		return true;		// Root-level units can be removed.
	if (!frames_.IsPresent(level))
		return false;		// Decision level not pulled into clause.

	bool res = true;
	//for (all_literals_in_clause(other, reason))
	for(int i = 1; i < reason_clause->Size(); i++)
	{
		BooleanLiteral other = reason_clause->operator[](i);
		if (other == ~literal)
			continue;
		if (CanRemoveLiteral(other, depth + 1))
			continue;
		res = false;
		break;
	}
	if (depth)
	{
		mark |= (res ? REMOVABLE2 : POISONED);
		mark_.AssignLabel(literal.Variable(), mark);
		//solver->marks[idx] = mark;
		//PUSH(solver->marked, idx);
	}
	return res;
}

void LearnedClauseMinimiser::ComputeLabel(BooleanLiteral literal)
{
	runtime_assert(state_.assignments_.IsAssignedTrue(literal) && depth_ < MAX_DEPTH);

	depth_++;
	//for now we assigned KEEP to literals that are too deep into the graph -> todo could avoid assigning poison, in case there is another path to the literal that is not so deep
	if (depth_ == MAX_DEPTH) { labels_.AssignLabel(literal.Variable(), POISON); depth_--; return; }

	//if the literal is already assigned KEEP, REMOVABLE, or POISON, no need to go further.
	if (labels_.IsLabelled(literal.Variable()) && labels_.GetLabel(literal.Variable()) != PRESENT) { depth_--; return; }
	
	//at this point the literal is either PRESENT or unlabelled
	//if the literal is a decision literal, it cannot be one from the original learned clause since those are labelled as part of initialisation (see Initialise2())
	//therefore the decision literal is labelled as poison and then return
	if (state_.assignments_.IsDecision(literal)) { labels_.AssignLabel(literal.Variable(), POISON); depth_--; return; }

	//literals assigned at levels outside those present in the original clause are POISON
	if (allowed_decision_levels_.IsPresent(state_.assignments_.GetAssignmentLevel(literal)) == false)
	{
		labels_.AssignLabel(literal.Variable(), POISON);
		depth_--;
		return;
	}

	Clause& reason_clause = state_.GetReasonClausePropagation(literal);

	/*PropagatorGeneric* propagator = state_.assignments_.GetAssignmentPropagator(literal.Variable());
	ExplanationGeneric* explanation = explanation = propagator->ExplainLiteralPropagation(literal, state_);*/

	for (int i = 1; i < reason_clause.Size(); i++) //go from one since the zeroth literal is the propagated literal
	{
		BooleanLiteral explanation_literal = ~reason_clause[i];
		BooleanVariableInternal explanation_variable = explanation_literal.Variable();
		//root assignments can be ignored
		if (state_.assignments_.GetAssignmentLevel(explanation_literal) == 0) { continue; }

		/*if (!labels_.IsLabelled(explanation_variable) || labels_.IsAssignedSpecificLabel(explanation_variable, PRESENT))
		{
			if (state_.assignments_.IsDecision(explanation_variable)
				|| 
				allowed_decision_levels_.IsPresent(state_.assignments_.GetAssignmentLevel(explanation_variable)) == false
				||
				depth_ + 1 == MAX_DEPTH) 
			{ 
				labels_.AssignLabel(explanation_variable, POISON);
			}
			else
			{
				ComputeLabel(explanation_literal);
			}
		}*/

		ComputeLabel(explanation_literal);
		runtime_assert(labels_.IsAssignedSpecificLabel(explanation_variable, PRESENT) == false); //must have been computed in the previous line

		if (labels_.IsAssignedSpecificLabel(explanation_variable, POISON))
		{
			//this is the case when the antecedent is poison and the literal is in the original learned clause
			if (labels_.IsAssignedSpecificLabel(literal.Variable(), PRESENT))
			{
				labels_.AssignLabel(literal.Variable(), KEEP);
				depth_--;
				return;
			}
			//otherwise the antecedent is POISON and the literal is not from the original clause
			else
			{
				runtime_assert(!labels_.IsLabelled(literal.Variable()));
				labels_.AssignLabel(literal.Variable(), POISON);
				depth_--;
				return;
			}
		}
	}
	//all antecedents of the literal are either KEEP or REMOVABLE, meaning this literal is REMOVABLE
	labels_.AssignLabel(literal.Variable(), REMOVABLE);

	depth_--;
}

void LearnedClauseMinimiser::Initialise(Disjunction& learned_literals)
{
	num_minimisation_calls_++;
	num_literals_before_minimising_ = learned_literals.size();
	depth_ = 0;
	//set the data structures to the appropriate size, e.g., in case new variables have been created since last time
	labels_.Resize(state_.GetNumberOfInternalBooleanVariables()+1); //+1 is needed since we do not use variables with index 0
	allowed_decision_levels_.Resize(state_.GetNumberOfInternalBooleanVariables() + 1);

	for (int i = 0; i < learned_literals.size(); i++)
	{
		BooleanLiteral literal = learned_literals[i];
		labels_.AssignLabel(literal.Variable(), 1);
		allowed_decision_levels_.Insert(state_.assignments_.GetAssignmentLevel(literal));
	}
}

void LearnedClauseMinimiser::Initialise2(ConflictAnalysisResultClausal &analysis_result_)
{
	num_minimisation_calls_++;
	num_literals_before_minimising_ = analysis_result_.learned_clause_literals.size();
	//set the data structures to the appropriate size, e.g., in case new variables have been created since last time
	labels_.Resize(state_.GetNumberOfInternalBooleanVariables() + 1); //+1 is needed since we do not use variables with index 0
	allowed_decision_levels_.Resize(state_.GetNumberOfInternalBooleanVariables() + 1);
	depth_ = 0;

	//mark literals from the initial clause
	labels_.AssignLabel(analysis_result_.learned_clause_literals[0].Variable(), KEEP); //the asserting literal is always kept
	for (int i = 1; i < analysis_result_.learned_clause_literals.size(); i++)
	{
		BooleanLiteral literal = analysis_result_.learned_clause_literals[i];
		runtime_assert(state_.assignments_.GetAssignmentLevel(literal) > 0); //I think root literals are never part of learned clauses, just checking!

		//a decision literal from the clause and the asserting literal cannot be removed; the asserting literal is expected to be in the last position in the array but for simplicity we nevertheless check
		if (state_.assignments_.IsDecision(literal))
		{
			labels_.AssignLabel(literal.Variable(), KEEP); 
		}
		else //the literal is an implied literal from the original learned clause
		{
			labels_.AssignLabel(literal.Variable(), PRESENT);
		}

		allowed_decision_levels_.Insert(state_.assignments_.GetAssignmentLevel(literal));
	}
}

void LearnedClauseMinimiser::CleanUp(Disjunction &learned_literals)
{
	total_removed_literals_ += (num_literals_before_minimising_ - learned_literals.size());
	labels_.Clear();
	allowed_decision_levels_.Clear();
}

bool LearnedClauseMinimiser::IsLiteralDominated(BooleanLiteral candidate)
{
	runtime_assert(state_.assignments_.IsAssigned(candidate));

	if (state_.assignments_.GetAssignmentLevel(candidate) == 0) { return true; } //note that root assignments are considered dominated
	if (state_.assignments_.IsDecision(candidate.Variable())) { return false; }
	if (labels_.IsAssignedSpecificLabel(candidate.Variable(), 0)) { return false; }
	if (!allowed_decision_levels_.IsPresent(state_.assignments_.GetAssignmentLevel(candidate))) { return false; }
	
	Clause& reason_clause = state_.assignments_.IsAssignedTrue(candidate) ? state_.GetReasonClausePropagation(candidate) : state_.GetReasonClausePropagation(~candidate);
	
	//todo clean up after this old code
	//if (state_.assignments_.IsAssignedTrue(candidate)){ explanation = propagator->ExplainLiteralPropagation(candidate, state_); }
	//else { explanation = propagator->ExplainLiteralPropagation(~candidate, state_); }
	
	for (int i = 1; i < reason_clause.Size(); i++) //index goes from one since the literal at position zero  is the propagating literal
	{
		BooleanLiteral reason_literal = ~reason_clause[i];
		BooleanVariableInternal reason_variable = reason_literal.Variable();

		if (labels_.IsAssignedSpecificLabel(reason_variable, 1)) { continue; }

		if (IsLiteralDominated(reason_literal) == false)
		{
			runtime_assert(!labels_.IsLabelled(reason_variable) || labels_.IsAssignedSpecificLabel(reason_variable, 0));
			labels_.AssignLabel(reason_variable, 0);
			return false;
		}
	}
	//if all the checks above passed, the candidate literal is implied by the other literals and can be removed
	labels_.AssignLabel(candidate.Variable(), 1);
	runtime_assert(allowed_decision_levels_.IsPresent(state_.assignments_.GetAssignmentLevel(candidate.Variable())));
	return true;
}

}//end namespace Pumpkin