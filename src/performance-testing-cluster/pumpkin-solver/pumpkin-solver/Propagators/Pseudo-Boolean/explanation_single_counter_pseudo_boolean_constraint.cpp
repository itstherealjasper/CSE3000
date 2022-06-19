#include "explanation_single_counter_pseudo_boolean_constraint.h"

namespace Pumpkin
{
ExplanationSingleCounterPseudoBooleanConstraint::ExplanationSingleCounterPseudoBooleanConstraint():
	p_violating_literals(0),
	end_index(0)
{}

BooleanLiteral ExplanationSingleCounterPseudoBooleanConstraint::operator[](int index) const
{
	return p_violating_literals->at(index);
}

size_t ExplanationSingleCounterPseudoBooleanConstraint::Size() const
{
	return end_index;
}

void ExplanationSingleCounterPseudoBooleanConstraint::InitialiseFailureExplanation(std::vector<BooleanLiteral>* violating_literals)
{
	p_violating_literals = violating_literals;
	end_index = p_violating_literals->size();
}

void ExplanationSingleCounterPseudoBooleanConstraint::InitialisePropagationExplanation(std::vector<BooleanLiteral>* violating_literals, int end_index)
{
	p_violating_literals = violating_literals;
	this->end_index = end_index;
}
}