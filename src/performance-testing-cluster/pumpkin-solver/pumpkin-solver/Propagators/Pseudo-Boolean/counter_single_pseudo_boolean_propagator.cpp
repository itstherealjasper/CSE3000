#include "counter_single_pseudo_boolean_propagator.h"
#include "../../Engine/solver_state.h"
#include "../../Utilities/runtime_assert.h"

#include <map>

namespace Pumpkin
{
PropagatorCounterSinglePseudoBoolean::PropagatorCounterSinglePseudoBoolean(SolverState& state, bool bump_objective_literals) :
	PropagatorGeneric(state), activated_(false), bump_objective_literals_(bump_objective_literals), upper_bound_(INT64_MAX)
{
}

bool PropagatorCounterSinglePseudoBoolean::NotifyDomainChange(LinearBooleanFunction& objective_function, int64_t upper_bound)
{
	runtime_assert(!activated_ && state_.GetCurrentDecisionLevel() == 0);
	runtime_assert(objective_function.GetConstantTerm() == 0); //todo this is a requirement for now, could easily handle this case, but need to see if I should remove the constant term altogether

	activated_ = true;
	objective_function_ = objective_function;

	//remove root assignments from the upper bound
	for (BooleanLiteral objective_literal : objective_function_)
	{
		if (state_.assignments_.IsAssignedTrue(objective_literal))
		{
			upper_bound -= objective_function_.GetWeight(objective_literal);
		}
	}

	if (upper_bound < 0) { return true; }

	root_slack_ = upper_bound;

	std::map<int64_t, std::vector<BooleanLiteral> > weight_to_literals;
	for (BooleanLiteral objective_literal : objective_function_)
	{
		if (state_.assignments_.IsAssigned(objective_literal)) { continue; }

		int weight = objective_function_.GetWeight(objective_literal);
		weight_to_literals[weight].push_back(objective_literal);
	}

	weighted_literals_.clear();
	for(auto iter_pair = weight_to_literals.rbegin(); iter_pair != weight_to_literals.rend(); ++iter_pair)
	{		
		weighted_literals_.push_back(Entry());
		weighted_literals_.back().weight = iter_pair->first;
		weighted_literals_.back().literals = iter_pair->second;
	}
	runtime_assert(weighted_literals_.size() <= 1 || weighted_literals_[0].weight > weighted_literals_[1].weight); //todo remove this assert
	
	runtime_assert(upper_bound_ > upper_bound); //the old upper bound should be greater than the new one
	upper_bound_ = upper_bound;

	return false;
}

void PropagatorCounterSinglePseudoBoolean::Deactivate()
{
	activated_ = false;
}

bool PropagatorCounterSinglePseudoBoolean::StrengthenUpperBound(int64_t new_upper_bound)
{//todo the method is implemented by reconstructing the propagator from scratch, could be done better
	LinearBooleanFunction copy = objective_function_;
	Deactivate();
	bool conflict_detected = NotifyDomainChange(copy, new_upper_bound);
	return conflict_detected;
}

void PropagatorCounterSinglePseudoBoolean::Synchronise(SolverState& state)
{
	//while (!violating_literals.empty() && state.assignments_.GetAssignmentLevel(violating_literals.back()) > state.GetCurrentDecisionLevel())
	while (!violating_literals.empty() 
		&& !state.assignments_.IsAssigned(violating_literals.back())
		)
	{
		int weight = objective_function_.GetWeight(violating_literals.back());
		root_slack_ += weight;
		violating_literals.pop_back();
	}
	PropagatorGeneric::Synchronise();
}

int PropagatorCounterSinglePseudoBoolean::DebugSum(BooleanAssignmentVector& sol)
{
	int sum = 0;
	for (auto& entry : weighted_literals_)
	{
		for (BooleanLiteral lit : entry.literals)
		{
			sum += entry.weight * sol[lit];
		}
	}
	return sum;
}

Clause* PropagatorCounterSinglePseudoBoolean::ExplainLiteralPropagation(BooleanLiteral literal)
{
	runtime_assert(1 == 2);
	return NULL; //todo implement for the redesigned version
	/*runtime_assert(state.assignments_.GetAssignmentPropagator(literal.Variable()) == this);
	//recover when the literal was propagated - the code stores the size of the violating literals vector when propagation happened
	uint64_t end_index = state.assignments_.GetAssignmentReasonCode(literal.Variable());
	ExplanationSingleCounterPseudoBooleanConstraint* explanation = explanation_generator_.GetAnExplanationInstance();
	explanation->InitialisePropagationExplanation(&violating_literals, end_index);
	BumpVariables(end_index, state);
	return explanation;*/
}

//todo remove dead code
/*ExplanationGeneric* PropagatorCounterSinglePseudoBoolean::ExplainFailure(SolverState& state)
{
	runtime_assert(!violating_literals.empty()); //could be possible but for now it is probably a bug if this triggers
	ExplanationSingleCounterPseudoBooleanConstraint* explanation = explanation_generator_.GetAnExplanationInstance();
	explanation->InitialiseFailureExplanation(&violating_literals);
	BumpVariables(violating_literals.size(), state);
	return explanation;	
}
*/

void PropagatorCounterSinglePseudoBoolean::BumpVariables(int end_index)
{
	if (!bump_objective_literals_) { return; }

	for (int i = 0; i < end_index; i++)
	{
		BooleanVariableInternal variable = violating_literals[i].Variable();
		int64_t weight = objective_function_.GetWeight(violating_literals[i]);
		state_.variable_selector_.BumpActivity(variable, double(weight)/MinWeight()); //todo might be a problem if the weights are large
	}
}

int64_t PropagatorCounterSinglePseudoBoolean::MinWeight() const
{
	runtime_assert(weighted_literals_.size() > 0);
	return weighted_literals_.back().weight;
}

PropagationStatus PropagatorCounterSinglePseudoBoolean::PropagateLiteral(BooleanLiteral true_literal)
{
	runtime_assert(state_.assignments_.IsAssignedTrue(true_literal));

	if (!activated_) { return false; }

	int weight = objective_function_.GetWeight(true_literal);
	if (weight == 0) { return false; }

	violating_literals.push_back(true_literal);
	root_slack_ -= weight;

	if (root_slack_ < 0) { return true; }

	//now propagate literals with weights larger than the slack to zero
	//todo could use binary search but for now we use a linear scan
	for (Entry& e : weighted_literals_)
	{
		if (e.weight > root_slack_)
		{
			for (BooleanLiteral literal : e.literals)
			{
				if (state_.assignments_.IsAssigned(literal)) { continue; }

				runtime_assert(1 == 2);//in the new version this does not make sense anymore, the code is different, change the statement below to add the id of the propagator and store the violating_literals size for internal reference for explanations 
				//bool enqueue_success = state_.EnqueuePropagatedLiteral(~literal, violating_literals.size()); 
				//if (!enqueue_success) { return true; }
			}
		}
		else if (e.weight < root_slack_) { break; } //the entries are kept sorted in descending order, so no need to inspect further
	}
	return false;
}
}