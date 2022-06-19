#pragma once

#include "../../Utilities/custom_vector.h"

namespace Pumpkin
{
class ExplanationSingleCounterPseudoBooleanConstraint
{
public:
	ExplanationSingleCounterPseudoBooleanConstraint();

	BooleanLiteral operator[](int index) const;
	size_t Size() const;

	void InitialiseFailureExplanation(std::vector<BooleanLiteral>* violating_literals);//used to explain a conflict, all literals in p_violating_literals are responsible. Could be improved todo
	void InitialisePropagationExplanation(std::vector<BooleanLiteral>* violating_literals, int end_index); //used for explaining propagations, literals located in indicies [0, 1, 2, ..., end_index) are responsible for the propagation. This could be improved todo.

private:

	std::vector<BooleanLiteral>* p_violating_literals;
	int end_index;
};
} //end Pumpkin namespace