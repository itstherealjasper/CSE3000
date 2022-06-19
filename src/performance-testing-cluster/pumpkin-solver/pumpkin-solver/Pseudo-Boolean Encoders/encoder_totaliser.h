#pragma once

#include "../Engine/solver_state.h"
#include "../Utilities/boolean_literal.h"
#include "../Utilities/boolean_assignment_vector.h"

#include <vector>

namespace Pumpkin
{
class EncoderTotaliser
{
public:
	//encodes the soft constraint \sum x_i <= right_hand_side
	//the output literals indicate the degree of violation of the constraint, i.e., output[i] = true if the sum is 'i+1' units over right_hand_side 
	std::vector<BooleanLiteral> SoftLessOrEqual(std::vector<BooleanLiteral>& input_literals, int right_hand_side, SolverState& state);
	
	BooleanLiteral SoftLessOrEqualIncremental(std::vector<BooleanLiteral>& input_literals, int right_hand_side, SolverState& state);
	BooleanLiteral StrengthenConstraint(BooleanLiteral previous_output_literal, SolverState& state);
	bool IsOutputLiteral(BooleanLiteral);

	int64_t NumClausesAddedLastTime() const;
	bool DebugCheckSatisfactionOfEncodedConstraints(const IntegerAssignmentVector &solution, SolverState& state);

private:

	int64_t num_clauses_added_;

	std::vector<BooleanLiteral> helper1_, helper2_;

	struct EncodedConstraint { std::vector<BooleanLiteral> input_literals, output_literals; int64_t right_hand_side; };
	std::vector<EncodedConstraint> encoded_constraints_;
};
}
