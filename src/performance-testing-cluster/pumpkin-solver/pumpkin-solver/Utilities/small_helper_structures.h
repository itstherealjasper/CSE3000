#pragma once

#include "boolean_literal.h"
#include "integer_variable.h"
#include "runtime_assert.h"
#include "Vec.h"

#include <vector>
#include <cmath>
#include <assert.h>

namespace Pumpkin
{

class Clause; //circular dependency?
class PropagatorGeneric;

typedef vec<Pumpkin::BooleanLiteral> Disjunction; //mainly to increase readability, i.e. to emphasize what the vector represents //not sure if I need disjunction
typedef vec<Pumpkin::BooleanLiteral> Conjunction; //mainly to increase readability, i.e. to emphasize what the vector represents

struct Term { IntegerVariable variable; int64_t weight; Term():variable(0),weight(0){}; Term(IntegerVariable variable, int64_t weight) :variable(variable), weight(weight) {}; };

struct LiteralCoefficientTerm
{
	LiteralCoefficientTerm() {};
	LiteralCoefficientTerm(const BooleanLiteral lit, uint32_t coef) :literal(lit), coefficient(coef) {}

	BooleanVariableInternal Variable() const { return literal.Variable(); }

	BooleanLiteral literal;
	uint32_t coefficient;
};

struct PropagationResult
{
	PropagationResult(BooleanLiteral propagation_literal, BooleanLiteral new_watched_literal) :propagated_literal(propagation_literal), new_watched_literal(new_watched_literal) {}
	BooleanLiteral propagated_literal, new_watched_literal;
};

struct Assignment_old
{
	Assignment_old(bool truth_value, int decision_level, Clause * reason) :truth_value(truth_value), decision_level(decision_level), reason(reason) {}
	bool truth_value;
	int decision_level;
	Clause *reason;	
};

struct AuxiliaryAssignmentInfo
{
	AuxiliaryAssignmentInfo() :reason_code(0), decision_level(-1) {}
	uint32_t reason_code; //todo explain; this is the reason for the assignment, e.g., normally it is a pointer to the clause. But for other propagators we use special codes and ask the propagators to explain.
	int decision_level;

	void Clear() { reason_code = 0; decision_level = -1; }
};

struct PairTimeCost
{
	PairTimeCost(time_t time, long long cost) :time(time), cost(cost) {}

	time_t time;
	long long cost;
};

class TimeStamps
{
public:
	TimeStamps() {}

	void AddTimePoint(time_t time, long long cost)
	{
		assert(time_points_.empty() || time_points_.back().time <= time && time_points_.back().cost >= cost);
		time_points_.push_back(PairTimeCost(time, cost));
	}

	//computes the integral of the curve defined by the time points (time, cost)
	//there is an undefined case when computing the integral from time zero to the time the first solution is computed
	//we are assuming the initial solution can be computed quickly
	//therefore we will use the score of the first solution found when there is no solution present
	//might be problematic if only one solution was computed or when it takes a long time to compute the initial solution, but this is more of an exception
	double ComputePrimalIntegral(time_t end_time) const
	{
		if (time_points_.size() == 0) { return INFINITY; }

		if (time_points_.size() == 1) { return (double(end_time) * time_points_[0].cost); }

		double integral = 0.0;
		for (unsigned int i = 1; i < time_points_.size(); i++)
		{
			time_t delta_t = time_points_[i].time - time_points_[i - 1].time;
			integral += (delta_t * time_points_[i - 1].cost);
		}
		integral += (end_time - time_points_.back().time)*time_points_.back().cost;
		return integral;
	}

private:
	std::vector<PairTimeCost> time_points_;
};

struct PairUnsatFlagPropagationLiterals
{
	PairUnsatFlagPropagationLiterals(bool unsat, const std::vector<BooleanLiteral> &propagation_lits) :
		is_unsat(unsat), propagation_literals(propagation_lits) {}

	bool is_unsat;
	std::vector<BooleanLiteral> propagation_literals;
};

} //end Pumpkin namespace