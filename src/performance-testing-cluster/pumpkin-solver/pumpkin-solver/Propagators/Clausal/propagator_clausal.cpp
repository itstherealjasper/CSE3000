#include "propagator_clausal.h"
#include "../../Engine/solver_state.h"
#include "../../Utilities/runtime_assert.h"

#include "../Linear Integer Inequality/linear_integer_inequality_propagator.h"

namespace Pumpkin
{

PropagatorClausal::PropagatorClausal
(
	SolverState& state,
	int64_t num_variables, 
	double decay_factor,
	int lbd_threshold,
	int target_max_num_temporary_clauses,
	bool use_LBD_for_sorting_temporary_clauses,
	double garbage_tolerance_factor
):
	PropagatorGeneric(state),
	watch_list_(2 * (num_variables + 1)), //recall that the zeros entry is not used for variables, and each variable has two literals
	increment_(1.0),
	max_threshold_(1e20),
	decay_factor_(decay_factor),
	LBD_threshold_(lbd_threshold),
	target_max_num_temporary_clauses_(target_max_num_temporary_clauses),
	use_LBD_for_sorting_temporary_clauses_(use_LBD_for_sorting_temporary_clauses),
	counter_total_removed_clauses_(0),
	number_of_learned_literals_(0),
	number_of_learned_clauses_(0),
	num_garbage_collected_clauses_(0),
	num_garbage_collected_literals_(0),
	clause_allocator_(new LinearClauseAllocator(1 << 4)),
	helper_clause_allocator_(new LinearClauseAllocator(1 << 4)),
	garbage_tolerance_factor_(garbage_tolerance_factor)
{
	pumpkin_assert_permanent(0 <= decay_factor && decay_factor <= 1, "Decay factor must be in the range [0, 1]");
}

PropagationStatus PropagatorClausal::Propagate()
{
	WatcherClause* next_watch_pointer, * old_end_pointer, * new_end_pointer;
	while (next_position_on_trail_to_propagate_ < state_.trail_.size())
	{
		BooleanLiteral true_literal = state_.trail_[next_position_on_trail_to_propagate_];
		pumpkin_assert_advanced(state_.assignments_.IsAssignedTrue(true_literal), "Sanity check.");

		//effectively remove all watches from this true_literal
		//then go through the previous watches one by one and insert them as indicated (some might be placed back in the watch list of this true_literal)
		//if a conflict takes place, put back the remaining clauses into the watch list of this true_literal and report the conflict

		auto &watches = watch_list_[(~true_literal).ToPositiveInteger()];

		//empty watch lists are immediately skipped
		if (watches.size() == 0) { next_position_on_trail_to_propagate_++; continue; }

		//effectively, we are resizing the watch list to zero for this literal, and in the loop we will add some of the old watches
		old_end_pointer = &watches[0] + watches.size();
		new_end_pointer = &watches[0];
		next_watch_pointer = &watches[0];		
		while (next_watch_pointer != old_end_pointer)
		{
			pumpkin_assert_advanced(clause_allocator_->GetClause(next_watch_pointer->clause_reference)[0] == ~true_literal 
					|| clause_allocator_->GetClause(next_watch_pointer->clause_reference)[1] == ~true_literal,
					"One of the watched literals must be the opposite of the input true literal");

			//inspect if the cached literal is already set to true
			//if so, no need to go further in the memory to check the clause
			//often this literal will be true in practice so it is a good heuristic to check
			BooleanLiteral cached_literal = next_watch_pointer->cached_literal;
			if (state_.assignments_.IsAssignedTrue(cached_literal))
			{
				//keep the watcher, the clause is satisfied, no propagation can take place
				*new_end_pointer = *next_watch_pointer; 
				++next_watch_pointer;
				++new_end_pointer;
				continue;
			}

			ClauseLinearReference watched_clause_reference = next_watch_pointer->clause_reference;
			Clause& watched_clause = clause_allocator_->GetClause(watched_clause_reference);

			//clause propagation starts here

			//place the considered literal at position 1 for simplicity
			if (watched_clause[0] == ~true_literal)
			{ 
				watched_clause[0] = watched_clause[1];
				watched_clause[1] = ~true_literal;
			}

			//check the other watched literal to see if the clause is already satisfied
			if (next_watch_pointer->cached_literal != watched_clause[0] && state_.assignments_.IsAssignedTrue(watched_clause[0]) == true)
			{
				next_watch_pointer->cached_literal = watched_clause[0]; //take the true literal as the new cached literal //TODO: mixed results, needs more testing to see if this is worth it
				//keep the watcher, the clause is satisfied, no propagation can take place
				*new_end_pointer = *next_watch_pointer;
				++next_watch_pointer;
				++new_end_pointer;
				continue;
			}

			//look for another nonfalsified literal to replace one of the watched literals
			bool found_new_watch = false;
			for (int i = 2; i < watched_clause.Size(); i++) //i = 2 since we are skipping the two watched literals
			{
				//find a literal that is either true or unassigned, i.e., not assigned false
				if (state_.assignments_.IsAssignedFalse(watched_clause[i]) == false) 
				{//TODO: would it make sense to set the cached literal here if this new literal will be set to true? Needs testing
					//replace the watched literal, add the clause to the watch list of the new watcher literal
					std::swap(watched_clause[1], watched_clause[i]);
					watch_list_[watched_clause[1].ToPositiveInteger()].push(WatcherClause(watched_clause_reference, watched_clause[0]));
					found_new_watch = true;
					break; //no propagation is taking place, go to the next clause. 
				}
			}

			if (found_new_watch) { ++next_watch_pointer; continue; } //note this clause is effectively removed from the watch list of true_literal, since we did not increment new_end_pointer

			//keep the current watch for this literal
			*new_end_pointer = *next_watch_pointer;
			++next_watch_pointer;
			++new_end_pointer;

			//at this point, nonwatched literals and literal[1] are assigned false. There are two scenarios:
			//	watched_clause[0] is unassigned -> propagate the literal to true
			//	watched_clause[0] is assigned false -> conflict

			//can propagate?
			if (!state_.assignments_.IsAssigned(watched_clause[0]))
			{
				state_.EnqueuePropagatedLiteral(watched_clause[0], watched_clause_reference.id);				
			}
			else //conflict detected
			{
				pumpkin_assert_advanced(state_.assignments_.IsAssignedFalse(watched_clause[0]), "Sanity check.");
				//readd the remaining watchers to the watch list
				while (next_watch_pointer != old_end_pointer)
				{
					*new_end_pointer = *next_watch_pointer;
					++next_watch_pointer;
					++new_end_pointer;
				}
				watches.resize(new_end_pointer - &watches[0]);
				//important to set the clause as the failure clause
				state_.failure_clause_ = &watched_clause;
				return true; //conflict detected
			}
		}
		watches.resize(new_end_pointer - &watches[0]);
		next_position_on_trail_to_propagate_++;
	}
	return false; //no conflicts detected
}

void PropagatorClausal::Synchronise()
{
	PropagatorGeneric::Synchronise();
}

Clause * PropagatorClausal::ExplainLiteralPropagation(BooleanLiteral literal)
{
	runtime_assert(1 == 2); //the clausal propagator is a special case -> we never call its explain literal propagation since it already places clauses as the reason
	return NULL; //todo remove the commented code below
	/*runtime_assert(state_.assignments_.GetAssignmentPropagator(literal.Variable()) == this);
	//recover the clause that lead to the propagation
	uint64_t code = state_.assignments_.GetAssignmentReasonCode(literal.Variable());	
	Clause *propagating_clause = reinterpret_cast<Clause*>(code);
	runtime_assert(propagating_clause->literals_[0] == literal); //the convention is that the propagating literal is at the 0th position
	//set an explanation for the propagation based on the propagating clause
	ExplanationClausal* explanation = explanation_generator_.GetAnExplanationInstance();
	explanation->Initialise(propagating_clause->literals_, 0);	
	return explanation;*/
}

void PropagatorClausal::PromoteAndReduceTemporaryClauses()
{
	//for simplicity of implementation, we first promote clauses, and then we see which ones to remove
	//	we could do it in one loop, probably does not make a big difference

	//promote clauses if their LBD is within the threshold
	int current_index = 0, end_index = 0;
	while (current_index < learned_clauses_.temporary_clauses.size())
	{
		ClauseLinearReference clause_reference = learned_clauses_.temporary_clauses[current_index];
		Clause& candidate_clause = clause_allocator_->GetClause(clause_reference);
		if (candidate_clause.GetLBD() <= LBD_threshold_)
		{
			learned_clauses_.low_lbd_clauses.push(clause_reference);
		}
		else
		{
			learned_clauses_.temporary_clauses[end_index] = clause_reference;
			end_index++;
		}
		current_index++;
	}
	learned_clauses_.temporary_clauses.resize(end_index);

	//clauses are sorted in way that 'good' clauses are in the beginning of the array
	if (use_LBD_for_sorting_temporary_clauses_)
	{
		LinearClauseAllocator& temp = *clause_allocator_;
		if (learned_clauses_.temporary_clauses.size() > 0)
		{
			std::sort(
				&learned_clauses_.temporary_clauses[0],
				&learned_clauses_.temporary_clauses[0] + learned_clauses_.temporary_clauses.size(),
				[&temp](const ClauseLinearReference cr1, const ClauseLinearReference cr2)->bool
				{
					Clause& c1 = temp.GetClause(cr1);
					Clause& c2 = temp.GetClause(cr2);
					//binary clauses are prioritised - todo probably not needed given that the lbd is low for binary clauses
					if (c1.IsBinaryClause() == true && c2.IsBinaryClause() == false) return true;
					if (c1.IsBinaryClause() == false && c2.IsBinaryClause() == true) return false;

					//compare based on the lbd score computed during the search (could be different now but its not recomputed!)
					if (c1.GetLBD() < c2.GetLBD()) return true;
					if (c1.GetLBD() > c2.GetLBD()) return false;

					//at this point both or neither clauses are binary and they have the same lbd score
					assert((c1.IsBinaryClause() && c2.IsBinaryClause() || c1.Size() > 2 && c2.Size() > 2) && c1.GetLBD() == c2.GetLBD());

					//the standard minisat comparison based on activity
					return c1.GetActivity() > c2.GetActivity();
				}
			);
		}
	}
	else
	{
		LinearClauseAllocator& temp = *clause_allocator_;
		std::sort(
			&learned_clauses_.temporary_clauses[0],
			&learned_clauses_.temporary_clauses[0] + learned_clauses_.temporary_clauses.size(),
			[&temp](const ClauseLinearReference cr1, const ClauseLinearReference cr2)->bool
			{
				Clause& c1 = temp.GetClause(cr1);
				Clause& c2 = temp.GetClause(cr2);
				return c1.GetActivity() > c2.GetActivity();
			}
		);
	}

	//the clauses at the back of the array are the 'bad' clauses
	//we start removals from the old_end_pointer of the array
	int i = int(learned_clauses_.temporary_clauses.size() - 1); //better use signed over unsigned, since decrementing an unsigned zero will overflow
	while (i >= 0 && learned_clauses_.temporary_clauses.size() > target_max_num_temporary_clauses_ / 2)
	{
		ClauseLinearReference clause_reference = learned_clauses_.temporary_clauses[i];
		Clause& c = clause_allocator_->GetClause(clause_reference);
		//clauses which updated their lbd since last clause clean up are protected from removal for one round
		//	todo not sure if this makes a difference in practice
		if (c.HasLBDUpdateProtection())
		{
			c.ClearLBDProtection();
		}
		//remove
		else
		{
			RemoveClauseFromWatchList(clause_reference);
			learned_clauses_.temporary_clauses[i] = learned_clauses_.temporary_clauses.last(); //we swap places with the clause at the back, and then pop. Normally we could simply pop the clause, but since we may protect clauses in the previous if statement, we need to do it like this
			learned_clauses_.temporary_clauses.pop();
			clause_allocator_->DeleteClause(clause_reference);

			counter_total_removed_clauses_++;
		}
		i--;
	}
}

void PropagatorClausal::RemoveAllLearnedClauses()
{//TODO could be done more efficiently
	for (int i = 0; i < learned_clauses_.low_lbd_clauses.size(); i++)
	{
		ClauseLinearReference clause_reference = learned_clauses_.low_lbd_clauses[i];
		RemoveClauseFromWatchList(clause_reference);
		clause_allocator_->DeleteClause(clause_reference);
		counter_total_removed_clauses_++;
	}

	for (int i = 0; i < learned_clauses_.temporary_clauses.size(); i++)
	{
		ClauseLinearReference clause_reference = learned_clauses_.temporary_clauses[i];
		RemoveClauseFromWatchList(clause_reference);
		clause_allocator_->DeleteClause(clause_reference);
		counter_total_removed_clauses_++;
	}

	learned_clauses_.low_lbd_clauses.clear();
	learned_clauses_.temporary_clauses.clear();
}

bool PropagatorClausal::ShouldPerformGarbageCollection() const
{
	return clause_allocator_->GetDeletedSpaceUsage() > clause_allocator_->GetNumAllocatedIntegersTotal() * garbage_tolerance_factor_;
}

void PropagatorClausal::PerformSimplificationAndGarbageCollection()
{
	pumpkin_assert_permanent(state_.GetCurrentDecisionLevel() == 0, "Error: garbage collection can only be done at the root level.");

	int old_num_garbage_collected_clauses_ = num_garbage_collected_clauses_;
	int old_num_garbage_collected_literals_ = num_garbage_collected_literals_;

	//we initially remove all record of clauses from these data structures
	//the contents are repopulated during the garbage collection process
	//	note that clearing the vectors does not remove these clauses from the solver
	permanent_clauses_.clear();
	learned_clauses_.low_lbd_clauses.clear();
	learned_clauses_.temporary_clauses.clear();
	
	for (int var_index = 1; var_index < state_.GetNumberOfInternalBooleanVariables() + 1; var_index++)
	{
		BooleanVariableInternal var(state_.GetInternalBooleanVariable(var_index));
		BooleanLiteral positive_literal(var, true);
		BooleanLiteral negative_literal(var, false);

		PerformSimplificationAndGarbageCollectionForLiteral(positive_literal);
		PerformSimplificationAndGarbageCollectionForLiteral(negative_literal);
	}

	std::swap(clause_allocator_, helper_clause_allocator_);
	helper_clause_allocator_->Clear();

	if (num_garbage_collected_clauses_ != old_num_garbage_collected_clauses_ || num_garbage_collected_literals_ != old_num_garbage_collected_literals_)
	{
		// std::cout << "c num clauses garbage collected: " << num_garbage_collected_clauses_ - old_num_garbage_collected_clauses_ << "\n";
		// std::cout << "c num literals garbage collected: " << num_garbage_collected_literals_ - old_num_garbage_collected_literals_ << "\n";
	}	
}

void PropagatorClausal::PerformSimplificationAndGarbageCollectionForLiteral(BooleanLiteral watched_literal)
{
	int new_watch_list_size = 0;
	auto& watchers = watch_list_[watched_literal.ToPositiveInteger()];
	for (int current_watch_list_index = 0; current_watch_list_index < watchers.size(); ++current_watch_list_index)
	{
		Clause& clause = clause_allocator_->GetClause(watchers[current_watch_list_index].clause_reference);
		pumpkin_assert_moderate(clause[0] == watched_literal || clause[1] == watched_literal || clause.IsRelocated(), "Watchers not ok.");

		//if the clause has already been deleted, we skip it		
		if (clause.IsDeleted())
		{
			continue;
		}
		//recall that each clause is stored twice in the watch list data structure
		//	if the clause has not yet been relocated, this is the first instance when the clause has been encountered
		else if (!clause.IsRelocated())
		{
			//go through the clause literals and determine:
			//	if the clause is satisfied
			//	if the clause can be simplified, i.e., some literals can be removed
			bool is_satisfied = false;
			int new_clause_size = 0, num_false_literals = 0;
			for (int current_clause_index = 0; current_clause_index < clause.Size(); ++current_clause_index)
			{
				//break;

				BooleanLiteral literal = clause[current_clause_index];
				//we go through the literals and simplify the clause on the fly
				//	if there is a true literal, the clause is satisfied and should not further be considered
				if (state_.assignments_.IsAssignedTrue(literal))
				{
					is_satisfied = true;
					break;
				}
				//	if the literal is unassigned, we should keep the literal
				else if (state_.assignments_.IsUnassigned(literal))
				{
					clause[new_clause_size] = clause[current_clause_index];
					++new_clause_size;
				}
				//note that if the literal is assigned false, it is effectively ignored and removed from the clause				
				num_false_literals += state_.assignments_.IsAssignedFalse(literal);				
			}

			/*is_satisfied = false;
			new_clause_size = clause.Size();
			num_false_literals = 0;*/

			if (is_satisfied)
			{
				clause.MarkDeleted();
				++num_garbage_collected_clauses_;
				continue;
			}
			else
			{
				num_garbage_collected_literals_ += num_false_literals;

				clause.ShrinkToSize(new_clause_size);

				//the clause will now be copied to the new memory allocator
				//the first time we encounter a clause, there is a special procedure:
				//	copy the clause to the helper clause allocated
				//	in the old memory (current clause_allocator), place the position of the new clause reference
				//	this is done as a way to pass information about the new location without needing additional memory
				
				ClauseLinearReference new_clause_reference = helper_clause_allocator_->CreateClauseByCopying(&clause);
				/*vec<BooleanLiteral> lits;
				for (BooleanLiteral lit : clause)
				{
					lits.push(lit);
				}
				ClauseLinearReference new_clause_reference = helper_clause_allocator_->CreateClause(lits, clause.IsLearned());*/

				clause.MarkRelocated();
				clause[0] = BooleanLiteral::CreateLiteralFromCodeAndFlag(new_clause_reference.id, false);

				watchers[current_watch_list_index].clause_reference = new_clause_reference;

				if (!clause.IsLearned())
				{
					permanent_clauses_.push(new_clause_reference);
				}
				else if (clause.GetLBD() <= LBD_threshold_)
				{
					learned_clauses_.low_lbd_clauses.push(new_clause_reference);
				}
				else //learnt clause with high lbd
				{
					learned_clauses_.temporary_clauses.push(new_clause_reference);
				}

				watchers[new_watch_list_size] = watchers[current_watch_list_index];
				++new_watch_list_size;
			}
		}
		//in case the clause has been relocated, then this is the second (and last) time the clause has been encountered
		else
		{
			//the second time the clause is encountered, the new clause reference is located at position 0
			//	set the new clause reference and then restore the approapriate watcher at position 1
			//	note that we do not need to set the other watcher to its appropriate
			watchers[new_watch_list_size] = watchers[current_watch_list_index];
			watchers[new_watch_list_size].clause_reference = ClauseLinearReference(clause[0].ToPositiveInteger());
			++new_watch_list_size;
			//note that in the new memory (helper_clause_allocator), the clause is okay, while in old memory (clause_allocator) it is invalid since we have overwritten one literal, however we do not need to restore the invalid clause
		}
	}
	watchers.resize(new_watch_list_size);
}

void PropagatorClausal::BumpClauseActivityAndUpdateLBD(Clause& clause)
{
	assert(clause.IsLearned());
	BumpClauseActivity(clause);
	uint32_t new_lbd = state_.ComputeLBD(clause.begin(), clause.Size());
	if (new_lbd < clause.GetLBD())
	{
		clause.SetLBD(new_lbd);		
		if (new_lbd <= 30) { clause.MarkLBDProtection(); }
	}		
}

void PropagatorClausal::RescaleClauseActivities()
{
	for (int i = 0; i < learned_clauses_.temporary_clauses.size(); i++)
	{
		ClauseLinearReference c = learned_clauses_.temporary_clauses[i];
		Clause& clause = clause_allocator_->GetClause(c);
		float new_activity = clause.GetActivity() / max_threshold_;
		clause.SetActivity(new_activity);
	}
	increment_ /= max_threshold_;
}

bool PropagatorClausal::AddPermanentClause(std::vector<BooleanLiteral>& literals)
{
	pumpkin_assert_permanent(state_.IsPropagationComplete() && state_.GetCurrentDecisionLevel() == 0, "Adding clauses is currently only possible at the root node once all propagation has been done.");
	pumpkin_assert_simple(!DoesVectorContainUndefinedLiterals(literals), "Sanity check.");
	
	//we use static variables to avoid allocating memory each time
	//	not sure if this is optimal but it works for now TODO
	//	(also below we have another static variable lits, same reasoning)
	static std::vector<BooleanLiteral> literals_preprocessed;
	
	literals_preprocessed = literals;

	PreprocessClause(literals_preprocessed);

	//is unsatisfied at the root? Note that we do not add the original clause to the database in this case
	if (literals_preprocessed.empty()) { return true; }
	//is unit clause? Unit clauses are added as root assignments, rather than as actual clauses
	//	in case the clause is satisfied at the root, the PreprocessClause method will return a unit clause with a literal that is satisfied at the root
	if (literals_preprocessed.size() == 1) { return AddUnitClause(literals_preprocessed[0]); }

	//standard case - the clause has at least two unassigned literals
	pumpkin_assert_simple(literals_preprocessed.size() >= 2, "Sanity check.");

	//static variable is used, same reasoning as at the beginning of the method
	static vec<BooleanLiteral> lits;
	lits.clear();
	for (BooleanLiteral literal : literals_preprocessed) { lits.push(literal); }

	ClauseLinearReference clause_reference = clause_allocator_->CreateClause(lits, false);
	Clause& clause = clause_allocator_->GetClause(clause_reference);

	watch_list_[clause[0].ToPositiveInteger()].push(WatcherClause(clause_reference, clause[1]));
	watch_list_[clause[1].ToPositiveInteger()].push(WatcherClause(clause_reference, clause[0]));

	permanent_clauses_.push(clause_reference);
	return false;
}

void PropagatorClausal::PreprocessClause(std::vector<BooleanLiteral>& literals)
{
	//before the clause is added several simple preprocessing steps are performed, i.e., 
	//	literals that are falsified at the root are removed
	// 	duplicated literals are removed
	// 	clauses that are satisfied on the root are ignored will become a unit clause that contain the true_literal
	
	//the code below is broken down into several parts, could be done more efficiently but probably makes no difference

	//remove literals that are falsified at the root level; 
	//also check if the clause has a true literal at the root level
	bool satisfied_at_root = false;

	int next_location = 0;
	for (int i = 0; i < literals.size(); i++)
	{
		BooleanLiteral literal = literals[i];
		if (state_.assignments_.IsAssignedTrue(literal))
		{
			satisfied_at_root = true;
			break;
		}
		//only add nonfalsified literals, i.e., falsified literals are skipped
		else if (!state_.assignments_.IsAssigned(literal)) 
		{
			literals[next_location] = literal;
			++next_location;			
		}
	}
	literals.resize(next_location);

	//if satisfied at the root, then remove all literals, add a true literal to the clause, and stop
	if (satisfied_at_root)
	{
		literals.resize(1);
		literals[0] = state_.true_literal_;
		return;
	}
	else if (literals.empty())
	{
		return;
	}

	//we now remove duplicated literals and detect if both polarities of a variable are present
	//	the easiest way is to sort the literals and only keep one literals of the same type

	auto compare = [](const BooleanLiteral lit1, const BooleanLiteral lit2)->bool{ return lit1.ToPositiveInteger() < lit2.ToPositiveInteger(); };
	std::sort(literals.begin(), literals.end(), compare);

	//remove duplicate literals
	next_location = 1;
	for (int i = 1; i < literals.size(); i++)
	{
		if (literals[i] != literals[next_location - 1])
		{
			literals[next_location] = literals[i];
			++next_location;
		}
	}
	literals.resize(next_location);

	//check if the clause contains both polarities of the same variable
	//	since we removed duplicates and the literals are sorted, it suffices to check if two neighbouring literals share the same variable
	for (int i = 1; i < literals.size(); i++)
	{
		if (literals[i - 1].VariableIndex() == literals[i].VariableIndex())
		{
			satisfied_at_root = true;
			break;
		}
	}

	if (satisfied_at_root)
	{
		literals.resize(1);
		literals[0] = state_.true_literal_;
		return;
	}	
}

bool PropagatorClausal::AddUnitClause(BooleanLiteral unit_clause)
{//TODO behaviour is not consistent: if the formula becomes unsat, the state may or may not be usable depending on how UNSAT was derived. For example, if Propagate determines unsat, the unit clause will be pushed on the trail. If the unit clause was already unsat at root, then nothing is added. Need to make this consistent. Problem is learned clauses may interfere. For now we say the state is no longer usable, need to clarify everywhere.
	runtime_assert(state_.GetCurrentDecisionLevel() == 0);

	if (!state_.assignments_.IsAssigned(unit_clause))
	{
		state_.EnqueueDecisionLiteral(unit_clause);
		return state_.PropagateEnqueuedLiterals().conflict_detected;
	}
	//the unit clause is already present, no need to do anything
	else if (state_.assignments_.IsAssignedTrue(unit_clause))
	{
		return false;
	}
	//the unit clause is falsified at the root level
	else
	{
		return true;
	}
}

bool PropagatorClausal::AddBinaryClause(BooleanLiteral a, BooleanLiteral b)
{
	static std::vector<BooleanLiteral> lits;
	lits.clear();
	lits.push_back(a);
	lits.push_back(b);
	return AddPermanentClause(lits);
}

bool PropagatorClausal::AddImplication(BooleanLiteral a, BooleanLiteral b)
{
	return AddBinaryClause(~a, b);
}

bool PropagatorClausal::AddTernaryClause(BooleanLiteral a, BooleanLiteral b, BooleanLiteral c)
{
	static std::vector<BooleanLiteral> lits;
	lits.clear();
	lits.push_back(a);
	lits.push_back(b);
	lits.push_back(c);
	return AddPermanentClause(lits);
}

void PropagatorClausal::UpdateFlagInfo()
{
	for (ClauseLinearReference clause_reference : permanent_clauses_)
	{
		UpdateFlagInfoForClause(clause_reference);
	}

	for (ClauseLinearReference clause_reference : learned_clauses_.low_lbd_clauses)
	{
		UpdateFlagInfoForClause(clause_reference);
	}

	for (ClauseLinearReference clause_reference : learned_clauses_.temporary_clauses)
	{
		UpdateFlagInfoForClause(clause_reference);
	}
}

void PropagatorClausal::UpdateFlagInfoForClause(ClauseLinearReference clause_reference)
{
	Clause& clause = clause_allocator_->GetClause(clause_reference);
	for (int i = 0; i < clause.Size(); i++)
	{
		clause[i].SetFlag(state_.ComputeFlagFromScratch(clause[i]));
	}
}

void PropagatorClausal::AddLearnedClauseAndEnqueueAssertingLiteral(vec<BooleanLiteral> &literals, int lbd)
{
	for (int i = 0; i < literals.size(); i++)
	{
		runtime_assert(!literals[i].IsUndefined());
	}

	ClauseLinearReference clause_reference = clause_allocator_->CreateClause(literals, true);
	Clause& learned_clause = clause_allocator_->GetClause(clause_reference);
	learned_clause.SetActivity(increment_);
	learned_clause.SetLBD(lbd);
	//learned clauses are partitioned into two tiers: low-lbd and temporary tiers. See TODO for more details.
	if (lbd <= LBD_threshold_)
	{
		learned_clauses_.low_lbd_clauses.push(clause_reference);
	}
	else
	{
		learned_clauses_.temporary_clauses.push(clause_reference);
	}
	//add clause to watch list
	watch_list_[learned_clause[0].ToPositiveInteger()].push(WatcherClause(clause_reference, learned_clause[1]));
	watch_list_[learned_clause[1].ToPositiveInteger()].push(WatcherClause(clause_reference, learned_clause[0]));

	state_.EnqueuePropagatedLiteral(literals[0], clause_reference.id);	
	
	number_of_learned_literals_ += literals.size();
	number_of_learned_clauses_++;
	assert(lbd == state_.ComputeLBD(learned_clause.begin(), learned_clause.Size()));

	//PrintClauseDetailed(std::cout, clause_reference);
	//std::cout << "BT lvl: " << state_.GetCurrentDecisionLevel() << "\n";
}

void PropagatorClausal::RemoveClauseFromWatchList(ClauseLinearReference clause_reference)
{
	Clause& clause = clause_allocator_->GetClause(clause_reference);
	RemoveClauseFromLiteralWatchers(clause[0], clause_reference);
	RemoveClauseFromLiteralWatchers(clause[1], clause_reference);
}

void PropagatorClausal::RemoveClauseFromLiteralWatchers(BooleanLiteral watched_literal, ClauseLinearReference clause_reference)
{//TODO: could do more efficiently to avoid potential quadratic time of removing watchers in batches, e.g., as in learned clause clean up
	bool found = false;
	auto& watchers = watch_list_[watched_literal.ToPositiveInteger()];
	for (int i = 0; i < watchers.size(); i++)
	{
		if (watchers[i].clause_reference == clause_reference)
		{
			watchers[i] = watchers.last();
			watchers.pop();
			found = true;
			break;
		}
	}
	runtime_assert(found);
}

void PropagatorClausal::DetachAllClauses()
{
	for (int i = 0; i < watch_list_.size(); i++)
	{
		watch_list_[i].clear();
	}
}

void PropagatorClausal::ReattachAllClauses()
{
	ReattachClauses(permanent_clauses_);
	ReattachClauses(learned_clauses_.low_lbd_clauses);
	ReattachClauses(learned_clauses_.temporary_clauses);
}

void PropagatorClausal::ReattachClauses(vec<ClauseLinearReference>& clauses)
{
	//add clauses to watch list
	for (ClauseLinearReference clause_reference : clauses)
	{
		Clause& clause = clause_allocator_->GetClause(clause_reference);
		//need to ensure the watchers are unassigned or assigned true
		int i = 0;
		while (i < clause.Size())
		{
			if (!state_.assignments_.IsAssignedFalse(clause[i])) { break; }
		}
		pumpkin_assert_simple(i < clause.Size(), "Watcher must be found.");
		std::swap(clause[i], clause[0]);

		//now find the second watcher
		++i;
		while (i < clause.Size())
		{
			if (!state_.assignments_.IsAssignedFalse(clause[i])) { break; }
		}
		pumpkin_assert_simple(i < clause.Size(), "Watcher must be found.");
		std::swap(clause[i], clause[1]);

		watch_list_[clause[0].ToPositiveInteger()].push(WatcherClause(clause_reference, clause[1]));
		watch_list_[clause[1].ToPositiveInteger()].push(WatcherClause(clause_reference, clause[0]));
	}
}

bool PropagatorClausal::DoesVectorContainUndefinedLiterals(std::vector<BooleanLiteral>& literals)
{
	for (int i = 0; i < literals.size(); i++)
	{
		if (literals[i].IsUndefined()) { return true; }
	}
	return false;
}

void PropagatorClausal::PrintToFile(std::ostream& out)
{
	out << permanent_clauses_.size() << " " << learned_clauses_.low_lbd_clauses.size() << " " << learned_clauses_.temporary_clauses.size() << "\n";
	PrintClauses(out, permanent_clauses_);
	PrintClauses(out, learned_clauses_.low_lbd_clauses);
	PrintClauses(out, learned_clauses_.temporary_clauses);
}

void PropagatorClausal::PrintClauses(std::ostream& out, vec<ClauseLinearReference>& clauses)
{
	for (ClauseLinearReference clause_reference : clauses)
	{
		PrintClause(out, clause_reference);
	}
}

void PropagatorClausal::PrintClause(std::ostream& out, ClauseLinearReference clause_reference)
{
	Clause& clause = clause_allocator_->GetClause(clause_reference);
	pumpkin_assert_permanent(clause.Size() > 1, "Clause seems to be empty or unit, and we never store such clauses, error.");
	out << clause.Size() << " ";
	for (int i = 0; i < clause.Size(); i++)
	{
		out << clause[i].ToString() << " ";
	}
	out << "\n";
}

void PropagatorClausal::PrintClauseDetailed(std::ostream& out, ClauseLinearReference clause_reference)
{
	Clause& clause = clause_allocator_->GetClause(clause_reference);
	pumpkin_assert_permanent(clause.Size() > 1, "Clause seems to be empty or unit, and we never store such clauses, error.");
	out << clause.Size() << " ";
	for (int i = 0; i < clause.Size(); i++)
	{
		out << clause[i].ToString() << "(";
		if (state_.assignments_.IsUnassigned(clause[i]))
		{
			out << "x) ";
		}
		else
		{
			out << state_.assignments_.IsAssignedTrue(clause[i]) << "[" << state_.assignments_.GetAssignmentLevel(clause[i]) << "]) ";
		}
	}
	out << ";" << clause.IsLearned() << "\n";
}

void PropagatorClausal::CheckClauses(vec<ClauseLinearReference> &clauses)
{
	//for each clause, check if it should propagate something that is not yet propagated -> see if fixed point propagation was indeed okay
	//for each clause, check that it is not failing
	for (ClauseLinearReference clause_reference : clauses)
	{
		Clause& clause = clause_allocator_->GetClause(clause_reference);
		int counter_false_literals = 0, counter_true_literals = 0, counter_unassigned_literals = 0;
		for (int i = 0; i < clause.Size(); i++)
		{
			if (state_.assignments_.IsAssignedFalse(clause[i]))
			{
				counter_false_literals++;
			}
			else if (state_.assignments_.IsAssignedTrue(clause[i]))
			{
				counter_true_literals++;
			}
			else
			{
				assert(state_.assignments_.IsAssigned(clause[i]) == false);
				counter_unassigned_literals++;
			}
		}		

		if ((counter_false_literals == clause.Size() - 1) && (counter_unassigned_literals == 1))
		{
			std::cout << "Curr dl: " << state_.GetCurrentDecisionLevel() << "\n";
			PrintClauseDetailed(std::cout, clause_reference);
			bool b = IsClauseProperlyWatched(clause_allocator_->GetClause(clause_reference));
			pumpkin_assert_permanent(b, "Clause is not properly watched!\n");
			std::cout << "Is ok with watch: " << b << "\n";
		}

		pumpkin_assert_permanent(counter_false_literals != clause.Size(), "No clause is expected to be in a failing state.");
		pumpkin_assert_permanent(!((counter_false_literals == clause.Size() - 1) && (counter_unassigned_literals == 1)), "The clause should propagate but did not propagate for some reason.");
		pumpkin_assert_permanent(counter_true_literals >= 1 || counter_unassigned_literals >= 2, "A different check to determine if the clause is in a correct state failed.");
	}
}

bool PropagatorClausal::IsInGoodState()
{
	//for each clause, check if it should propagate something that is not yet propagated -> see if fixed point propagation was indeed okay
	//for each clause, check that it is not failing
	
	CheckClauses(permanent_clauses_);
	CheckClauses(learned_clauses_.low_lbd_clauses);
	CheckClauses(learned_clauses_.temporary_clauses);

	//check if watches are properly set
	for (int var_id = 1; var_id <= state_.GetNumberOfInternalBooleanVariables(); var_id++)
	{
		BooleanVariableInternal var = state_.GetInternalBooleanVariable(var_id);
		BooleanLiteral positive_literal(var, true);
		BooleanLiteral negative_literal(var, false);
		
		{
			auto& positive_watcher = watch_list_[positive_literal.ToPositiveInteger()];

			for (int current_watch_list_index = 0; current_watch_list_index < positive_watcher.size(); ++current_watch_list_index)
			{
				Clause& clause = clause_allocator_->GetClause(positive_watcher[current_watch_list_index].clause_reference);
				pumpkin_assert_moderate(clause[0] == positive_literal || clause[1] == positive_literal, "Clause is in a watch list, but it is not properly watched.");
			}
		}

		{
			auto& negative_watcher = watch_list_[negative_literal.ToPositiveInteger()];

			for (int current_watch_list_index = 0; current_watch_list_index < negative_watcher.size(); ++current_watch_list_index)
			{
				Clause& clause = clause_allocator_->GetClause(negative_watcher[current_watch_list_index].clause_reference);
				pumpkin_assert_moderate(clause[0] == negative_literal || clause[1] == negative_literal, "Clause is in a watch list, but it is not properly watched.");
			}
		}
	}

	//root assignments are not properly tracked
	if (state_.IsRootLevel()) { true; }

	//for each variable assignment, check if it is correct
	for (int i = 0; i < state_.GetNumberOfInternalBooleanVariables(); i++)
	{
		BooleanVariableInternal variable = state_.GetInternalBooleanVariable(i + 1);
		if (state_.assignments_.IsUnassigned(variable)) { continue; }
		
		int decision_level = state_.assignments_.GetAssignmentLevel(variable);
		if (decision_level == 0) { continue; } //root assignments not properly tracked, especially after garbage collection

		uint32_t code = state_.assignments_.GetAssignmentReasonCode(variable);
		if (code == 0) { continue; }

		if (code <= state_.next_cp_propagator_id_)
		{
			Clause &clause = clause_allocator_->GetClause(ClauseLinearReference(code));
			BooleanLiteral lit = clause[0];
			BooleanVariableInternal var2 = lit.Variable();			

			pumpkin_assert_permanent(clause[0].Variable() == variable && state_.assignments_.IsAssignedTrue(clause[0]), "Propagating literal must be in first position.");
			
			for (int i = 1; i < clause.Size(); i++)
			{
				pumpkin_assert_permanent(state_.assignments_.IsAssignedFalse(clause[i]), "All other literals must be falsified.");
			}
		}
	}

	return true;
}

bool PropagatorClausal::IsClauseProperlyWatched(const Clause &clause)
{
	BooleanLiteral watch_lit1 = clause[0];
	BooleanLiteral watch_lit2 = clause[1];

	auto& list1 = watch_list_[watch_lit1.ToPositiveInteger()];
	auto& list2 = watch_list_[watch_lit2.ToPositiveInteger()];

	bool watch_found1 = false;
	for (int i = 0; i < list1.size(); i++)
	{
		Clause &c = clause_allocator_->GetClause(list1[i].clause_reference);
		if (&c == &clause)
		{
			watch_found1 = true;
			break;
		}
	}

	bool watch_found2 = false;
	for (int i = 0; i < list2.size(); i++)
	{
		Clause& c = clause_allocator_->GetClause(list2[i].clause_reference);
		if (&c == &clause)
		{
			watch_found2 = true;
			break;
		}
	}
	return watch_found1 && watch_found2;
}

bool PropagatorClausal::ShouldClausePropagate(Clause& clause)
{
	int counter_false_literals = 0, counter_true_literals = 0, counter_unassigned_literals = 0;
	for (int i = 0; i < clause.Size(); i++)
	{
		if (state_.assignments_.IsAssignedFalse(clause[i]))
		{
			counter_false_literals++;
		}
		else if (state_.assignments_.IsAssignedTrue(clause[i]))
		{
			counter_true_literals++;
		}
		else
		{
			assert(state_.assignments_.IsAssigned(clause[i]) == false);
			counter_unassigned_literals++;
		}
	}
	return counter_unassigned_literals == 1 && (counter_false_literals + 1 == clause.Size());
}

void PropagatorClausal::RecomputeAndPrintClauseLengthStatsForPermanentClauses(bool print_clauses)
{
	// std::cout << "TODO PRINT STATS\n";
	/*long long number_of_literals = 0;
	long long number_of_clauses = 0;
	long long number_of_binary_clauses = 0;
	long long number_of_ternary_clauses = 0;
	long long number_of_other_clauses = 0;
	long long sum_of_lengths_of_other_clauses = 0;
	std::set<int> variable_indicies;

	for (ClauseLinearReference clause_reference : permanent_clauses_)
	{
		Clause* clause = state
		number_of_literals += clause->literals_.Size();
		number_of_clauses++;
		number_of_binary_clauses += (clause->literals_.Size() == 2);
		number_of_ternary_clauses += (clause->literals_.Size() == 3);
		number_of_other_clauses += (clause->literals_.Size() > 3);
		sum_of_lengths_of_other_clauses += (clause->literals_.Size()>3)*clause->literals_.Size();

		if (print_clauses)
		{
			for (int i = 0; i < clause->literals_.Size(); i++)
			{
				BooleanLiteral lit = clause->literals_.operator[](i);
				if (lit.IsPositive())
				{
					std::cout << lit.VariableIndex() << " ";
				}
				else
				{
					std::cout << "-" << lit.VariableIndex() << " ";
				}
			}
			std::cout << "\n";
		}

		for (BooleanLiteral literal : clause->literals_)
		{
			variable_indicies.insert(literal.VariableIndex());
		}
	}

	std::cout << "c Effective number of variables: " << variable_indicies.size() << "\n";
	std::cout << "c Number of clauses: " << number_of_clauses << "\n";
	std::cout << "c Number of literals: " << number_of_literals << "\n";
	std::cout << "c \tbinary clauses: " << number_of_binary_clauses << "\n";
	std::cout << "c \tternary clauses: " << number_of_ternary_clauses << "\n";
	std::cout << "c \tother clauses: " << number_of_other_clauses << "\n";
	std::cout << "c \tAvg size of other clauses: " << double(sum_of_lengths_of_other_clauses)/number_of_other_clauses << "\n";*/
}

} //old_end_pointer Pumpkin namespace