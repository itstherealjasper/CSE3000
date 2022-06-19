#pragma once

#include "../Propagators/propagator_generic_CP.h"
#include "../Utilities/pumpkin_assert.h"

#include <queue>
#include <set>

namespace Pumpkin
{
class PropagatorQueue
{
public:
	PropagatorGenericCP* GetNextPropagator() const;
	bool IsEmpty() const;

	void EnqueuePropagator(PropagatorGenericCP* propagator);
	void PopNextPropagator();

	void Clear();
private:
	bool IsEnqueued(PropagatorGenericCP* propagator);

	std::set<int> present_priorities_; //lower priority goes first
	std::set<int> present_propagator_ids_;
	std::vector<std::queue<PropagatorGenericCP*> > queues_;
};

inline PropagatorGenericCP* PropagatorQueue::GetNextPropagator() const
{
	pumpkin_assert_moderate(present_priorities_.size() > 0 && queues_.at(*present_priorities_.begin()).size() > 0, "Sanity check.");
	int next_priority = *present_priorities_.begin();
	return queues_[next_priority].front();
}

inline bool PropagatorQueue::IsEmpty() const
{
	return present_priorities_.empty();
}

inline void PropagatorQueue::EnqueuePropagator(PropagatorGenericCP* propagator)
{
	if (!IsEnqueued(propagator))
	{
		int priority = propagator->GetPriority();
		if (queues_.size() <= priority) { queues_.resize(priority+1); }
		queues_[priority].push(propagator);
		present_propagator_ids_.insert(propagator->GetPropagatorID());
		present_priorities_.insert(priority);
	}
}

inline void PropagatorQueue::PopNextPropagator()
{
	pumpkin_assert_moderate(!IsEmpty(), "Popping empty propagator priority queue.");
	int next_priority = *present_priorities_.begin();
	int id = queues_[next_priority].front()->GetPropagatorID();
	queues_[next_priority].pop();
	if (queues_[next_priority].empty()) { present_priorities_.erase(next_priority); }
	present_propagator_ids_.erase(id);	
}

inline void PropagatorQueue::Clear()
{
	for (int priority : present_priorities_) 
	{
		while (!queues_[priority].empty()) { queues_[priority].pop(); }
	}	
	present_propagator_ids_.clear();
	present_priorities_.clear();
}

inline bool PropagatorQueue::IsEnqueued(PropagatorGenericCP* propagator)
{
	return present_propagator_ids_.find(propagator->GetPropagatorID()) != present_propagator_ids_.end();
}

}