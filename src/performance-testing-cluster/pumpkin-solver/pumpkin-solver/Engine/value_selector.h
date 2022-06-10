#pragma once

#include "../Utilities/boolean_literal.h"
#include "../Utilities/boolean_assignment_vector.h"
#include "../Utilities/runtime_assert.h"

#include <vector>

namespace Pumpkin
{
class ValueSelector
{
public:
	ValueSelector(int num_variables);
	bool SelectValue(BooleanVariableInternal); //returns the suggested truth value assigned for the given variable
	void UpdatePolarity(BooleanVariableInternal, bool);//sets the polarity of the input variable to the given truth value. As a result, SelectValue will now suggest the input truth value for the variable
	void Grow(); //increases the number of variables considered by one.

	void SetPolaritiesToFalse();
	void FreezeCurrentPhaseValues();
	void UnfreezeAll();

	void SetAndFreezeValue(BooleanLiteral);
	void SetPhaseValuesAndFreeze(const BooleanAssignmentVector& solution);
	void InitialiseValues(const BooleanAssignmentVector& solution);

	//private:
	BooleanAssignmentVector values_, is_frozen_;
};

inline ValueSelector::ValueSelector(int num_variables)
	:values_(num_variables, false), //the 0-th position is not used
	is_frozen_(num_variables, false) //the 0-th position is not used
{
}

inline bool ValueSelector::SelectValue(BooleanVariableInternal variable)
{
	return values_[variable.index];
}

inline void ValueSelector::UpdatePolarity(BooleanVariableInternal variable, bool truth_value)
{
	runtime_assert(variable.IsUndefined() == false);
	if (!is_frozen_[variable.index]) { values_[variable.index] = truth_value; }
}

inline void ValueSelector::Grow()
{
	values_.Grow(false);
	is_frozen_.Grow(false);
}

inline void ValueSelector::SetPolaritiesToFalse()
{
	for (int i = 1; i <= values_.NumVariables(); i++) { values_[i] = false; }
}

inline void ValueSelector::FreezeCurrentPhaseValues()
{
	for (int i = 1; i <= is_frozen_.NumVariables(); i++) { is_frozen_[i] = true; }
}

inline void ValueSelector::UnfreezeAll()
{
	for (int i = 1; i <= is_frozen_.NumVariables(); i++) { is_frozen_[i] = false; }
}

inline void ValueSelector::SetAndFreezeValue(BooleanLiteral frozen_literal)
{
	values_[frozen_literal.Variable()] = frozen_literal.IsPositive();
	is_frozen_[frozen_literal.Variable()] = true;
}

inline void ValueSelector::SetPhaseValuesAndFreeze(const BooleanAssignmentVector& solution)
{
	InitialiseValues(solution);
	FreezeCurrentPhaseValues();
}

inline void ValueSelector::InitialiseValues(const BooleanAssignmentVector& solution)
{
	while (values_.NumVariables() < solution.NumVariables()) { Grow(); }
	for (int i = 1; i <= solution.NumVariables(); i++) { values_[i] = solution[i]; }
	for (int i = solution.NumVariables() + 1; i <= values_.NumVariables(); i++) { values_[i] = false; } //todo, not a great solution but okay for now. The problem is that the input solution may have less variables than the the solver has variables, which happens whenever we add new encodings without updating the previous solution
}

} //end Pumpkin namespace