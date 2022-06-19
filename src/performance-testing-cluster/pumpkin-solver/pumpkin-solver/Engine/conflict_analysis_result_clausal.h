#pragma once

#include "solver_state.h"
#include "../Utilities/boolean_literal.h"
#include "../Utilities/runtime_assert.h"
#include "../Utilities/Vec.h"

#include <vector>

namespace Pumpkin
{

struct ConflictAnalysisResultClausal
{
	ConflictAnalysisResultClausal():backtrack_level(-1){}
	
	//debugging method: performs sanity checks to see if the instance has been created correctly
	bool CheckCorrectnessAfterConflictAnalysis(SolverState& state)
	{
		//the propagating literal is the literal at the highest level and the highest level is the current level
		pumpkin_assert_permanent(state.GetCurrentDecisionLevel() == state.assignments_.GetAssignmentLevel(learned_clause_literals[0]), "The propagating literal must be at the highest level.");

		for (int i = 0; i < learned_clause_literals.size(); i++)
		{
			BooleanLiteral current_literal = learned_clause_literals[i];
			//there is only one literal at the highest decision level
			pumpkin_assert_simple(state.assignments_.GetAssignmentLevel(current_literal) < state.GetCurrentDecisionLevel() || current_literal == learned_clause_literals[0], "There can only be one literal at the highest level.");
			//all literals apart from the propagating literal are assigned false
			pumpkin_assert_simple(state.assignments_.IsAssignedFalse(current_literal) || current_literal == learned_clause_literals[0], "All literals apart from the propagating literal must be assigned false.");
		}

		if (learned_clause_literals.size() > 1)
		{
			pumpkin_assert_simple(backtrack_level == state.assignments_.GetAssignmentLevel(learned_clause_literals[1]), "Assertion level seems wrong.");

			//the literal at position 1 is at the second highest level
			int second_highest_level = state.assignments_.GetAssignmentLevel(learned_clause_literals[1]);
			for (int i = 2; i < learned_clause_literals.size(); i++)
			{
				int lvl = state.assignments_.GetAssignmentLevel(learned_clause_literals[i]);

				if (lvl > second_highest_level)
				{
					for (int k = 0; k < learned_clause_literals.size(); k++)
					{
						std::cout << state.assignments_.GetAssignmentLevel(learned_clause_literals[k]) << " ";
					}
					std::cout << "\n";
				}

				pumpkin_assert_simple(lvl <= second_highest_level, "Decision level too high.");
			}
		}		

		return true;
	}

	vec<BooleanLiteral> learned_clause_literals;
	int backtrack_level;
};

}//end namespace Pumpkin