#include "linear_integer_inequality_propagator.h"
#include "../../Utilities/pumpkin_assert.h"

#include <algorithm>

namespace Pumpkin
{
LinearIntegerInequalityPropagator::LinearIntegerInequalityPropagator(
	std::vector<IntegerVariable>& variables, 
	std::vector<int64_t>& coefficients,
	int64_t right_hand_side
):
	PropagatorGenericCP(0)
{
	pumpkin_assert_simple(variables.size() == coefficients.size(), "The number of variables and coefficients do not match in the linear inequality propagator.");

	//do some preprocessing of the input preprocess the input
	//	handle duplicate variables	
	// 	remove terms with zero coefficients

	//collect all terms in an auxiliary vector
	std::vector<Term> terms;
	for (int i = 0; i < variables.size(); i++)
	{		
		if (coefficients[i] == 0) { continue; } //these can be skipped

		terms.push_back(Term(variables[i], coefficients[i]));		
	}
	//remove duplicate variables
	//	sort terms by variable id...
	std::sort(terms.begin(), terms.end(), [](const Term& t1, const Term& t2)->bool { return t1.variable.id < t2.variable.id; });
	//	...then visit variables in order and merge coefficients of identical variables
	int last_position = 0, i = 1;
	while (i < terms.size())
	{
		if (terms[last_position].variable == terms[i].variable)
		{
			terms[last_position].weight += terms[i].weight;
		}
		else
		{
			last_position++;
			terms[last_position] = terms[i];
		}
		i++;
	}
	terms.resize(last_position + 1);

	//now remove variables that have zero coefficients
	//	note that even if initially there were no zero coefficients, in case of duplicates the zero coefficient may appear now (e.g., -x + x + y + z >= 1)
	last_position = 0;
	for (i = 0; i < terms.size(); i++)
	{
		if (terms[i].weight != 0)
		{
			terms[last_position] = terms[i];
			last_position++;
		}
	}	
	terms.resize(last_position);

	//do some initialisation of internal data structures - recall that more will be done once added to the solver based on the state
	root_slack_ = int64_t(-right_hand_side); 
	for (Term& term : terms)
	{
		pumpkin_assert_simple(!term.variable.IsNull() && term.weight != 0, "Sanity check.");

		if (term.weight > 0)
		{
			int ub = INT32_MAX; //dummy value
			positive_terms_.push_back(PairTermRootValue(term, ub));			
		}
		else if (term.weight < 0)
		{
			int lb = INT32_MIN; //dummy value
			negative_terms_.push_back(PairTermRootValue(term, lb));			
		}
	}	
}

PropagationStatus LinearIntegerInequalityPropagator::PropagateFromScratch()
{
	//compute current slack
	int64_t current_slack = root_slack_;
	for (auto& p : positive_terms_)
	{
		int upper_bound = state_->domain_manager_.GetUpperBound(p.term.variable);
		pumpkin_assert_moderate(upper_bound <= p.root_bound, "Sanity check.");
		current_slack -= p.term.weight * (p.root_bound - upper_bound); //possibly decrease the slack	
	}
	for (auto& p : negative_terms_)
	{
		int lower_bound = state_->domain_manager_.GetLowerBound(p.term.variable);
		pumpkin_assert_moderate(lower_bound >= p.root_bound, "Sanity check.");
		current_slack += p.term.weight * (lower_bound - p.root_bound); //possibly decrease the slack - note that the weight is negative, so addition will overall result in a decrease of slack
	}

	//negative slack indicates a constraint violation
	if (current_slack < 0)
	{
		//trivial explanation -> current bounds on all variables are the reason for failure
		vec<BooleanLiteral> failure_lits;

		for (auto& p : positive_terms_)
		{
			IntegerVariable variable = p.term.variable;
			BooleanLiteral ub_lit = state_->GetCurrentUpperBoundLiteral(variable);
			failure_lits.push(~ub_lit);
		}

		for (auto& p : negative_terms_)
		{
			IntegerVariable variable = p.term.variable;
			BooleanLiteral lb_lit = state_->GetCurrentLowerBoundLiteral(variable);
			failure_lits.push(~lb_lit);
		}
		Clause* failure_clause = clause_allocator_.CreateClause(failure_lits);
		InitialiseFailureClause(failure_clause);
		return true;
	}

	//no conflict detected, proceed with propagation

	//	first consider positive terms...
	for (auto& current_term : positive_terms_)
	{
		Term &term = current_term.term;
		int term_lower_bound = state_->domain_manager_.GetLowerBound(term.variable);
		int term_upper_bound = state_->domain_manager_.GetUpperBound(term.variable);
		int new_term_lb = term_upper_bound - DivideAndRoundDown(current_slack, term.weight); // new_lb = UB(X_i) - round_up(slack / a_i)

		if (new_term_lb > term_lower_bound)
		{
			BooleanLiteral lit = state_->GetLowerBoundLiteral(term.variable, new_term_lb);
			pumpkin_assert_simple(!state_->assignments_.IsAssigned(lit), "Sanity check."); //this holds since if the literal was assigned false, a conflict would have been detected, and if it was true, the lower_bound would not be propagated now
			state_->EnqueuePropagatedLiteral(lit, GetPropagatorID());

			//set the explanation eagerly
			vec<BooleanLiteral> explanation;
			explanation.push(lit); //we respect the convention in clauses that the propagated literal is at position 0

			for (auto& other_term : positive_terms_)
			{
				if (current_term.term.variable == other_term.term.variable) { continue; }

				BooleanLiteral ub_lit = state_->GetCurrentUpperBoundLiteral(other_term.term.variable);
				explanation.push(~ub_lit);
			}

			for (auto& other_term : negative_terms_)
			{
				pumpkin_assert_moderate(other_term.term.variable != current_term.term.variable, "Sanity check.");

				BooleanLiteral lb_lit = state_->GetCurrentLowerBoundLiteral(other_term.term.variable);
				explanation.push(~lb_lit);
			}			
			pumpkin_assert_simple(explanation.size() > 1 || state_->GetCurrentDecisionLevel() == 0, "Sanity check.");
			map_literal_to_eager_explanation_[lit.ToPositiveInteger()] = explanation;
		}
	}

	//	...and then consider negative terms
	for (auto& current_term : negative_terms_)
	{
		Term& term = current_term.term;
		int term_lower_bound = state_->domain_manager_.GetLowerBound(term.variable);
		int term_upper_bound = state_->domain_manager_.GetUpperBound(term.variable);
		int new_term_ub = term_lower_bound + DivideAndRoundDown(current_slack, -term.weight);

		if (new_term_ub < term_upper_bound)
		{
			BooleanLiteral lit = state_->GetUpperBoundLiteral(term.variable, new_term_ub);
			pumpkin_assert_simple(!state_->assignments_.IsAssigned(lit), "Sanity check."); //this holds since if the literal was assigned false, a conflict would have been detected, and if it was true, the upper bound would not be propagated now
			state_->EnqueuePropagatedLiteral(lit, GetPropagatorID());

			//set the explanation eagerly
			vec<BooleanLiteral> explanation;
			explanation.push(lit); //we respect the convention in clauses that the propagated literal is at position 0

			for (auto& other_term : positive_terms_)
			{
				pumpkin_assert_moderate(other_term.term.variable != current_term.term.variable, "Sanity check.");

				BooleanLiteral ub_lit = state_->GetCurrentUpperBoundLiteral(other_term.term.variable);
				explanation.push(~ub_lit);
			}

			for (auto& other_term : negative_terms_)
			{
				if (current_term.term.variable == other_term.term.variable) { continue; }

				BooleanLiteral lb_lit = state_->GetCurrentLowerBoundLiteral(other_term.term.variable);
				explanation.push(~lb_lit);
			}
			pumpkin_assert_simple(explanation.size() > 1 || state_->GetCurrentDecisionLevel() == 0, "Sanity check.");
			map_literal_to_eager_explanation_[lit.ToPositiveInteger()] = explanation;
		}
	}
	return false; //no conflicts found
}

PropagationStatus LinearIntegerInequalityPropagator::Propagate()
{
	return PropagateFromScratch();
}

void LinearIntegerInequalityPropagator::SynchroniseInternal()
{
	if (state_->GetCurrentDecisionLevel() == 0)
	{
		clause_allocator_.Clear();		
	}
}

PropagationStatus LinearIntegerInequalityPropagator::InitialiseAtRootInternal()
{
	pumpkin_assert_simple(state_->GetCurrentDecisionLevel() == 0, "Can only add the linear inequality propagator at the root node.");

	//remove variables assigned at the root
	int last_position = 0;
	for (int i = 0; i < positive_terms_.size(); i++)
	{
		IntegerVariable variable = positive_terms_[i].term.variable;
		if (!state_->IsAssigned(variable))
		{
			int ub = state_->domain_manager_.GetUpperBound(positive_terms_[last_position].term.variable);
			positive_terms_[last_position] = positive_terms_[i];
			positive_terms_[last_position].root_bound = ub;
			root_slack_ += positive_terms_[last_position].term.weight * ub;
			last_position++;
		}
	}
	positive_terms_.resize(last_position);

	last_position = 0;
	for (int i = 0; i < negative_terms_.size(); i++)
	{
		IntegerVariable variable = negative_terms_[i].term.variable;
		if (!state_->IsAssigned(variable))
		{
			int lb = state_->domain_manager_.GetLowerBound(negative_terms_[last_position].term.variable);
			negative_terms_[last_position] = negative_terms_[i];
			negative_terms_[last_position].root_bound = lb;
			root_slack_ += negative_terms_[last_position].term.weight * lb;
			last_position++;
		}
	}
	negative_terms_.resize(last_position);	

	pumpkin_assert_permanent(root_slack_ >= 0, "For now we assume that the linear inequality cannot be violating at the root node.");
	pumpkin_assert_simple(positive_terms_.size() + negative_terms_.size() > 1, "For now we expect the linear inequality propagator to have at least two variables, could be that most/all variables were preprocessed?");

	return Propagate();
}

void LinearIntegerInequalityPropagator::SubscribeDomainChanges()
{
	for (auto& p : positive_terms_)
	{
		state_->watch_list_CP_.SubscribeToUpperBoundChanges(this, p.term.variable, *state_);
	}

	for (auto& p : negative_terms_)
	{
		state_->watch_list_CP_.SubscribeToLowerBoundChanges(this, p.term.variable, *state_);
	}	
}

bool LinearIntegerInequalityPropagator::NotifyDomainChange(IntegerVariable)
{
	return true;
}

void LinearIntegerInequalityPropagator::InitialiseInternalDataStructures(std::vector<Term>& terms, int right_hand_side)
{
	//the slack will be computed by optimistically setting the variables to their preferred values, i.e., positive terms to their upper bounds, and negative terms to their lower bounds
	for (Term &term: terms)
	{
		pumpkin_assert_simple(!term.variable.IsNull() && term.weight != 0, "Sanity check.");

		if (term.weight > 0)
		{
			int ub = state_->domain_manager_.GetUpperBound(term.variable);
			positive_terms_.push_back(PairTermRootValue(term, ub));
			root_slack_ += term.weight * ub;
		}
		else if (term.weight < 0)
		{
			int lb = state_->domain_manager_.GetLowerBound(term.variable);
			negative_terms_.push_back(PairTermRootValue(term, lb));
			root_slack_ += term.weight * lb;
		}		
	}
	pumpkin_assert_permanent(root_slack_ >= 0, "For now we assume that the linear inequality cannot be violating at the root node.");
}

Clause* LinearIntegerInequalityPropagator::ExplainLiteralPropagationInternal(BooleanLiteral literal)
{
	runtime_assert(state_->assignments_.IsAssigned(literal) && state_->assignments_.GetAssignmentLevel(literal) > 0); //the propagator does not keep info on root propagations
	vec<BooleanLiteral> explanation = map_literal_to_eager_explanation_[literal.ToPositiveInteger()];
	Clause* clause = clause_allocator_.CreateClause(explanation);
	return clause;
}

bool LinearIntegerInequalityPropagator::DebugCheckInfeasibility(const std::vector<IntegerVariable>& relevant_variables, const SimpleBoundTracker& bounds) const
{
	int64_t slack = root_slack_;
	for (auto& p : positive_terms_)
	{
		bool is_relevant = false;
		for (IntegerVariable variable : relevant_variables)
		{
			if (p.term.variable == variable)
			{
				is_relevant = true;
				break;
			}
		}

		if (is_relevant)
		{
			slack -= p.term.weight * (p.root_bound - bounds.GetUpperBound(p.term.variable)); //will decrease the slack
		}
	}

	for (auto& p : negative_terms_)
	{
		bool is_relevant = false;
		for (IntegerVariable variable : relevant_variables)
		{
			if (p.term.variable == variable)
			{
				is_relevant = true;
				break;
			}
		}

		if (is_relevant)
		{
			slack += p.term.weight * (bounds.GetLowerBound(p.term.variable) - p.root_bound); //will decrease the slack
		}
	}
	
	return slack < 0; //a negative slacks shows that if the variables take their most optimistic values, the constraint can still not be satisfied
}
}