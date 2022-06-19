#pragma once

#include "../Engine/solver_state.h"
#include "../Utilities/boolean_literal.h"
#include "../Utilities/boolean_assignment_vector.h"

#include <vector>

namespace Pumpkin
{
class EncoderCardinalityNetwork
{
public:
	EncoderCardinalityNetwork();

	//encodes the soft constraint \sum x_i <= right_hand_side
	//the output literals indicate the degree of violation of the constraint, i.e., output[i] = true if the sum is 'i+1' units over right_hand_side 
	std::vector<BooleanLiteral> SoftLessOrEqual(std::vector<BooleanLiteral>& input_literals, int right_hand_side, SolverState& state);

	int64_t NumClausesAddedLastTime() const;
	int64_t TotalNumClausesAdded() const;
	int64_t TotalTimeSpent() const;

	bool DebugCheckSatisfactionOfEncodedConstraints(const BooleanAssignmentVector& solution);

private:
	int64_t num_clauses_added_last_time_;
	int64_t total_num_clauses_added_;
	int64_t total_time_spent_;

	std::vector<BooleanLiteral> helper1_, helper2_;

	std::vector<BooleanLiteral> HSort(const std::vector<BooleanLiteral>& literals, SolverState& state);
	std::vector<BooleanLiteral> HMerge(const std::vector<BooleanLiteral>& a, const std::vector<BooleanLiteral>& b, SolverState& state);
	std::vector<BooleanLiteral> HMerge(BooleanLiteral a, BooleanLiteral b, SolverState& state);

	struct EncodedConstraint { std::vector<BooleanLiteral> input_literals, output_literals; int64_t right_hand_side; };
	std::vector<EncodedConstraint> encoded_constraints_;
};
}