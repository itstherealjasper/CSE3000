#pragma once

#include "clause.h"
#include "watcher.h"
#include "../../Engine/linear_clause_allocator.h"
#include "../propagator_generic.h"
#include "../../Utilities/small_helper_structures.h"
#include "../../Utilities/Vec.h"

#include <iostream>
#include <fstream>

namespace Pumpkin
{

class PropagatorClausal : public PropagatorGeneric
{
public:
	PropagatorClausal
	(
		SolverState &state,
		int64_t num_variables, 
		double decay_factor,
		int lbd_threshold,
		int target_max_num_temporary_clauses,
		bool use_LBD_for_sorting_temporary_clauses,
		double garbage_tolerance_factor
	);

	PropagationStatus Propagate();

	void Synchronise();

	Clause* ExplainLiteralPropagation(BooleanLiteral literal);

//methods to add clauses-----------------------------------------

	//assumes that before calling this method, the solver state has completed propagation, i.e., there are no literals waiting in the propagation queue
	//adds a clause to the database and performs propagation 
	//returns true if a conflict has been detected, otherwise false
	//if a conflicted took place, conflict analysis should be performed using the propagator to restore the solver to a non-conflicting state or report unsatisfiability
	//assumes the method is called at the root level
	//TODO better description
	//does some root level simplification too
	bool AddPermanentClause(std::vector<BooleanLiteral>& literals);

	//does simple preprocessing
	// 	the method makes changes directly to the input vector (e.g., the order of literals may be different, some literals may be removed)
	//	removes duplicate literals
	//	removes falsified literals at the root
	//	if the same variable appears with both polarities or there is a literal that is true at the root, removes all literals from the clause and adds a literal that is true at the root
	//	note that clauses that are unsatisfied at the root will become empty
	void PreprocessClause(std::vector<BooleanLiteral> &literals);

	bool AddUnitClause(BooleanLiteral);
	bool AddBinaryClause(BooleanLiteral, BooleanLiteral);
	bool AddImplication(BooleanLiteral, BooleanLiteral);
	bool AddTernaryClause(BooleanLiteral, BooleanLiteral, BooleanLiteral);

	void UpdateFlagInfo();
	void UpdateFlagInfoForClause(ClauseLinearReference clause_reference);

	//creates a clause_ based on the input literals (which are seen as a disjunction), adds it as a learned/temporary clause_ to the database, and returns a pointer to the newly created class. 
	//Note that learned/temporary clauses might later be removed by 'ReduceLearnedClauses' and therefore it is essential to keep this in mind when operating with the returned pointer
	//TODO assumptions...TODO better description
	void AddLearnedClauseAndEnqueueAssertingLiteral(vec<BooleanLiteral>&, int lbd);

	//The unit clauses need to be added before the search starts. 
	//at the moment, for now I need to be careful when adding these unit clauses during the search. This is done in a special way during search, e.g. backtracking to level 0 and then enqueuing the literal.
	//could consider adding a version which takes the state as input

//methods for managing learned clauses-----------------------------------------

	//Temporary clauses that achieved LBD less or equal to the threshold are promoted to the low_lbd_clauses_ category
	//Then half of the least active learned clauses are removed from the clause database.
	// 	   However clauses that improved their LBD since last clean up are protected for one round and not removed
	//For now this should only be done at the root level during the search since there is not record on whether a clause is involved in a propagation.
	void PromoteAndReduceTemporaryClauses();
	void RemoveAllLearnedClauses();

	bool ShouldPerformGarbageCollection() const;
	void PerformSimplificationAndGarbageCollection();
	
	//assumes the clause is a learned clause
	//increase the clause activity and updates its lbd score if it is better than its previous score
	void BumpClauseActivityAndUpdateLBD(Clause& clause);
	void BumpClauseActivity(Clause&);  //bumps the activity of the clause -> used in conflict analysis, so that in ReduceTemporaryClauses() clauses with high activity are prefered over those with low activity 
	void DecayClauseActivities(); //decay the activity of all clauses
	void RescaleClauseActivities(); //divides all activities with a large number when the maximum activity becomes too large

	int NumLearnedClauses() const; //returns the number of learned clauses currently located in the database. This includes low-lbd clauses and temporary clauses.
	int NumClausesTotal() const; //this will be tricky to maintain with new improvements that implicitly store clauses, TODO
	int NumTemporaryClauses() const;
	bool TemporaryClausesExceedLimit() const;

	//debug methods-----------------------------------------

	bool DoesVectorContainUndefinedLiterals(std::vector<BooleanLiteral>& literals);
	void PrintToFile(std::ostream& out);
	void PrintClauses(std::ostream& out, vec<ClauseLinearReference>& clauses);
	void PrintClause(std::ostream& out, ClauseLinearReference);
	void PrintClauseDetailed(std::ostream& out, ClauseLinearReference);
	void CheckClauses(vec <ClauseLinearReference> &clauses);

	bool IsInGoodState();
	bool ShouldClausePropagate(Clause& clause);
	bool NothingToPropagate() const;
	bool AreWatchesCorrect() const;
	bool AreClausesProperlyWatched(const std::vector<Clause*>&) const;
	bool IsClauseProperlyWatched(const Clause&);
	bool IsLiteralProperlyWatched(BooleanLiteral) const;
	void RecomputeAndPrintClauseLengthStatsForPermanentClauses(bool print_clauses = false);


//a bit hacky to keep as public but okay for now-----------------------------------------
//I guess this will go private since not sure why we would expose it
	vec<ClauseLinearReference> permanent_clauses_;
	struct LearnedClauses { vec<ClauseLinearReference> low_lbd_clauses, temporary_clauses; } learned_clauses_;

	vec<vec<WatcherClause> > watch_list_;
	long long counter_total_removed_clauses_;
	long long number_of_learned_literals_;
	long long number_of_learned_clauses_;
	long long num_garbage_collected_clauses_, num_garbage_collected_literals_;

	float increment_, max_threshold_, decay_factor_;
	double garbage_tolerance_factor_;
	int LBD_threshold_;
	int target_max_num_temporary_clauses_; //note that this target may be breached since we only do database clean ups at the root. This number is used to determine the number of clauses to remove when cleaning up the database.
	bool use_LBD_for_sorting_temporary_clauses_;

	LinearClauseAllocator *clause_allocator_, *helper_clause_allocator_;

//private:

	void RemoveClauseFromWatchList(ClauseLinearReference clause_reference);
	void RemoveClauseFromLiteralWatchers(BooleanLiteral watched_literal, ClauseLinearReference clause_reference);
	void DetachAllClauses();
	void ReattachAllClauses();
	void ReattachClauses(vec<ClauseLinearReference>& clauses);

	PropagationStatus PropagateLiteral(BooleanLiteral true_literal) { runtime_assert(1 == 2); std::cout << "Propagate literal not used for clausal propagator!\n"; exit(1); return false; }

private:
	void PerformSimplificationAndGarbageCollectionForLiteral(BooleanLiteral lit);
};

inline void PropagatorClausal::BumpClauseActivity(Clause& clause)
{
	assert(clause.IsLearned());
	//rescale all activities if needed
	if (clause.GetActivity() + increment_ > max_threshold_) { RescaleClauseActivities(); }
	clause.SetActivity(clause.GetActivity() + increment_);
	assert(clause.GetActivity() < INFINITY);
}

inline void PropagatorClausal::DecayClauseActivities()
{
	increment_ *= (1.0 / decay_factor_);
}

inline int PropagatorClausal::NumLearnedClauses() const
{
	return int(learned_clauses_.low_lbd_clauses.size() + learned_clauses_.temporary_clauses.size());
}

inline int PropagatorClausal::NumClausesTotal() const
{
	return int(permanent_clauses_.size()) + NumLearnedClauses();
}

inline int PropagatorClausal::NumTemporaryClauses() const
{
	return learned_clauses_.temporary_clauses.size();
}

inline bool PropagatorClausal::TemporaryClausesExceedLimit() const
{
	return NumTemporaryClauses() >= target_max_num_temporary_clauses_;
}

} //end Pumpkin namespace