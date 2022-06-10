#pragma once

#include "../propagator_generic_CP.h"
#include "../../Engine/solver_state.h"
#include "../../Utilities/standard_clause_allocator.h"

#include <map>

namespace Pumpkin
{
//propagator for the constraint \sum_i a_i * x_i >= c
class LinearIntegerInequalityPropagator : public PropagatorGenericCP
{
public:

	LinearIntegerInequalityPropagator
	(
		std::vector<IntegerVariable> &variables,  // x_i
		std::vector<int64_t> &coefficients, // a_i
		int64_t right_hand_side  // >= c
	);

	PropagationStatus Propagate();
	PropagationStatus PropagateFromScratch();
	void SynchroniseInternal();
	bool NotifyDomainChange(IntegerVariable);

//private:
	void InitialiseInternalDataStructures(std::vector<Term>& terms, int right_hand_side);

	Clause* ExplainLiteralPropagationInternal(BooleanLiteral);

	bool DebugCheckInfeasibility(const std::vector<IntegerVariable>& relevant_variables, const SimpleBoundTracker& bounds) const;

	PropagationStatus InitialiseAtRootInternal();
	void SubscribeDomainChanges();

	int64_t DivideAndRoundUp(int64_t s, int64_t c);
	int64_t DivideAndRoundDown(int64_t s, int64_t c);

	struct PairTermRootValue { PairTermRootValue() {}; PairTermRootValue(Term t, int64_t rv) :term(t), root_bound(rv) {}; Term term; int64_t root_bound; }; //for positive/negative terms, the root value is the upper/lower bound of term.variable at the root level
	std::vector<PairTermRootValue> positive_terms_, negative_terms_; // a positive/negative term is a term with a positive/negative coefficient
	int64_t root_slack_;

	StandardClauseAllocator clause_allocator_;
	std::map<int64_t, vec<BooleanLiteral> > map_literal_to_eager_explanation_; //lit.ToInt() -> explanation
};

inline int64_t LinearIntegerInequalityPropagator::DivideAndRoundUp(int64_t s, int64_t c)
{
	return (s / c) + ((s % c) != 0);
}

inline int64_t LinearIntegerInequalityPropagator::DivideAndRoundDown(int64_t s, int64_t c)
{
	return (s / c);
}

}