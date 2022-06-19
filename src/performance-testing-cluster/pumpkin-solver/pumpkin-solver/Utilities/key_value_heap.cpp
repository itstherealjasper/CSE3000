#include "key_value_heap.h"
#include "pumpkin_assert.h"

#include <random>
#include <algorithm>
#include <iostream>
#include <set>
#include <assert.h>

namespace Pumpkin
{

KeyValueHeap::KeyValueHeap(int num_entries, int seed):
	values_(num_entries, 0.0),
	map_key_to_position_(num_entries),
	map_position_to_key_(num_entries),
	end_position_(num_entries)
{
	Reset(seed);
}

void KeyValueHeap::Reset(int seed)
{
	std::vector<size_t> permutation;
	std::vector<bool> was_present(values_.size(), false);
	int old_end_position = end_position_;
	for (size_t i = 0; i < values_.size(); i++) 
	{ 
		was_present[i] = IsKeyPresent(i); 
		values_[i] = 0.0;
		permutation.push_back(i);
	}

	if (seed >= 0)
	{
		std::mt19937 random_number_generator(seed);
		std::shuffle(permutation.begin(), permutation.end(), random_number_generator);
	}

	for (size_t i = 0; i < values_.size(); i++)
	{
		size_t key = permutation[i];
		map_position_to_key_[i] = key;
		map_key_to_position_[key] = i;
	}

	end_position_ = values_.size();

	for (size_t i = 0; i < values_.size(); i++)
	{
		if (was_present[i] == false)
		{
			Remove(i);
		}
	}
	pumpkin_assert_moderate(end_position_ == old_end_position, "Sanity check.");
}

void KeyValueHeap::Increment(int key_id, double increment)
{
	int i = map_key_to_position_[key_id];
	values_[i] += increment;
	if (i < end_position_) //do not sift up values that are not in the heap 
	{
		SiftUp(i); 
	}
}

int KeyValueHeap::PeekMaxKey() const
{
	pumpkin_assert_moderate(end_position_ > 0, "Heap cannot be empty.");
	return map_position_to_key_[0];
}

int KeyValueHeap::PopMax()
{
	pumpkin_assert_moderate(end_position_ > 0, "Heap cannot be empty.");
	pumpkin_assert_advanced(IsHeap(0), "Sanity check.");

	int best_key = map_position_to_key_[0];
	Remove(best_key);
	return best_key;
}

void KeyValueHeap::Remove(int key_id)
{
	pumpkin_assert_moderate(IsKeyPresent(key_id), "Sanity check.");

	//place the key at the end of the heap, decrement the heap, and sift down
	int position = map_key_to_position_[key_id];
	SwapPositions(position, end_position_ - 1);
	end_position_--;
	//if (end_position_ > 1) 
	if (end_position_ > 1 && position < end_position_) //no need to sift for empty or one-node heaps, nor do we do this in case the element removed was anyway the last element
	{ 
		SiftDown(position); 
	} 
	
	pumpkin_assert_extreme(end_position_ == 0 || IsHeap(0), "Sanity check.");
}

void KeyValueHeap::Readd(int key_id)
{
	pumpkin_assert_moderate(end_position_ < values_.size(), "Sanity check.");

	//key_id is somewhere in the position [end_position, max_size-1]
	//place the key at the end of the heap, increase end_position, and sift up
	int key_position = map_key_to_position_[key_id];
	pumpkin_assert_moderate(key_position >= end_position_, "Sanity check.");
	SwapPositions(key_position, end_position_);
	end_position_++;
	SiftUp(end_position_ - 1);
}

void KeyValueHeap::Grow()
{
	pumpkin_assert_moderate(CheckSizeConsistency(), "Sanity check.");
	
	int new_key = int(values_.size());
	values_.push(0);
	map_key_to_position_.push(new_key);//initially played at the very end, will be swapped
	map_position_to_key_.push(new_key);
	SwapPositions(end_position_, new_key);
	end_position_++;
	SiftUp(end_position_-1);
}

void KeyValueHeap::DivideValues(double divisor)
{
	//for (double &val : values_) { val /= divisor; }
	for (int i = 0; i < values_.size(); i++) { values_[i] /= divisor; }
}

double KeyValueHeap::GetKeyValue(int key_id) const
{
	int position = map_key_to_position_[key_id];
	return values_[position];
}

bool KeyValueHeap::IsKeyPresent(int key_id) const
{
	return map_key_to_position_[key_id] < end_position_;
}

double KeyValueHeap::ReadMaxValue() const
{
	pumpkin_assert_advanced(IsHeap(0), "Sanity check.");
	return values_[0];
}

int KeyValueHeap::Size() const
{
	return end_position_;
}

bool KeyValueHeap::Empty() const
{
	return Size() == 0;
}

int KeyValueHeap::MaxSize() const
{
	pumpkin_assert_advanced(AreDataStructuresOfSameSize(), "Sanity check.");
	return values_.size();
}

bool KeyValueHeap::DebugCheckConsistencyDataStructures() const
{
	//each key has exactly one position
	//each position is associated with exactly one key
	std::set<int> m;
	for (int i = 0; i < values_.size(); i++) 
	{ 
		runtime_assert(m.find(map_position_to_key_[i]) == m.end());
		m.insert(map_position_to_key_[i]); 
	}
	runtime_assert(m.size() == values_.size());
	m.clear();
	for (int i = 0; i < values_.size(); i++)
	{
		runtime_assert(m.find(map_key_to_position_[i]) == m.end());
		m.insert(map_key_to_position_[i]);
	}
	runtime_assert(m.size() == values_.size()); //point is these debug asserts will fail if there are duplicate values
	return true;
}

void KeyValueHeap::SwapPositions(int i, int j)
{
	int key_i = map_position_to_key_[i];
	int key_j = map_position_to_key_[j];
	std::swap(values_[i], values_[j]);
	std::swap(map_position_to_key_[i], map_position_to_key_[j]);
	std::swap(map_key_to_position_[key_i], map_key_to_position_[key_j]);
}

void KeyValueHeap::SiftUp(int i)
{
	if (i == 0) { return; } //base case at the root
	
	int parent_i = GetParent(i);
	if (values_[parent_i] >= values_[i]) { return; } //heap property satisfied, done

	SwapPositions(i, parent_i);
	SiftUp(parent_i); //note the new value has been pushed up into parent_id position
}

void KeyValueHeap::SiftDown(int i)
{
	pumpkin_assert_moderate(i < end_position_, "Sanity check.");

	if (IsHeapLocally(i) == true) { return; }

	int largest_child = GetLargestChild(i);
	SwapPositions(i, largest_child);
	SiftDown(largest_child); //note the new value has been pushed down to largest_child position
}

int KeyValueHeap::GetParent(int i) const
{
	pumpkin_assert_moderate(i > 0, "Root has no parent.");
	int parent_id = (i - 1) / 2;
	pumpkin_assert_moderate(GetLeftChild(parent_id) == i || GetRightChild(parent_id) == i, "Sanity check.");
	return parent_id;
}

int KeyValueHeap::GetLeftChild(int i) const
{
	int left_child = 2 * i + 1;
	return left_child;
}

int KeyValueHeap::GetRightChild(int i) const
{
	int right_child = 2 * i + 2;
	if (right_child < end_position_) {
		return right_child;
	}
	else //there is no right child, note the tree is not necessarily a full binary tree
	{ 
		return -1; 
	}
}

int KeyValueHeap::GetLargestChild(int i) const
{
	pumpkin_assert_moderate(IsLeaf(i) == false, "Sanity check.");
	int max_child = GetLeftChild(i); //assume the left child is the largest
	int right_child = GetRightChild(i);
	if (right_child != -1 && values_[right_child] > values_[max_child]) //if there exists a right child, compare to it
	{
		max_child = right_child;
	}
	return max_child;
}

bool KeyValueHeap::IsHeap(int position) const
{
	pumpkin_assert_moderate(position >= 0, "Sanity check."); //root is in position 0
	pumpkin_assert_moderate(position < end_position_, "Cannot exceed the node count.");

	if (IsHeapLocally(position) == false) { return false; }

	if (IsLeaf(position) == true) { return true; }

	int right_child = GetRightChild(position);
	return IsHeap(GetLeftChild(position)) && (right_child == -1 || IsHeap(right_child)); //check the 1) left child and the 2) right child if it exists
}

bool KeyValueHeap::IsHeapLocally(int position) const
{
	if (IsLeaf(position) == true) { return true; }
	else if (IsSmallerThanLeftChild(position) || IsSmallerThanRightChild(position)) { return false; }
	else { return true; }
}

bool KeyValueHeap::IsLeaf(int position) const
{
	return GetLeftChild(position) >= end_position_;
}

bool KeyValueHeap::IsSmallerThanLeftChild(int position) const
{
	pumpkin_assert_moderate(IsLeaf(position) == false, "Sanity check.");
	return values_[position] < values_[GetLeftChild(position)];
}

bool KeyValueHeap::IsSmallerThanRightChild(int position) const
{
	pumpkin_assert_moderate(IsLeaf(position) == false, "Sanity check.");
	int right_child = GetRightChild(position);
	return right_child != -1 && values_[position] < values_[right_child];
}

bool KeyValueHeap::CheckSizeConsistency() const
{
	return values_.size() == map_key_to_position_.size() && values_.size() == map_position_to_key_.size() && end_position_ <= values_.size();
}

bool KeyValueHeap::IsMaxValue(double reference) const
{
	for (int i = 0; i < end_position_; i++)
	{
		double val = values_[i];
		if (reference < val)
		{
			pumpkin_assert_moderate(reference >= val, "Sanity check.");
		}
	}
	return true;
}

bool KeyValueHeap::AreDataStructuresOfSameSize() const
{
	int number_of_entries = values_.size();
	return map_key_to_position_.size() == number_of_entries && map_position_to_key_.size() == number_of_entries && end_position_ <= number_of_entries;
}

} //end Pumpkin namespace