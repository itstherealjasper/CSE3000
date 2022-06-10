#ifndef KEY_VALUE_HEAP
#define KEY_VALUE_HEAP

#include <vector>

#include "Vec.h"

namespace Pumpkin
{

/*
A heap class where the keys range from [0, ..., n-1] and the values are nonnegative floating points.
The heap can be queried to return key with the maximum value, and certain keys can be (temporarily) removed/readded as necessary
It allows increasing/decreasing the values of its entries
*/

//should maybe check for numerical stability...
class KeyValueHeap
{
public:
	//create a heap with keys [0, ..., num_entries-1] and values all set to zero.
	//the seed dictates how the initial ordering of the entries
	//	since all values are zero, any ordering is correct
	//	-1 indicates to keep the order in increasing number of keys
	//	a seed >= 0 will then use a random number generator to shuffle the keys
	KeyValueHeap(int num_entries, int seed = -1);

	//sets all values to zeros
	//the seed is used as in the contructor, i.e., it dictates the initial ordering of keys
	void Reset(int seed = -1);

	void Increment(int key_id, double increment); //increments the value of the element of 'key_id' by increment. O(logn) worst case, but average case might be better.
	int PeekMaxKey() const;//Assumes the heap is not empty. Returns the key_id with the highest value. Note that this does not remove the key (see PopMax() to get and remove). O(1).
	int PopMax(); //returns the key_id with the highest value, and removes the key from the heap. O(logn)
	void Remove(int key_id); //removes the entry with key 'key_id' (temporarily) from the heap. Its value remains recorded internally and is available upon readding 'key_id' to the heap. The value can still be subjected to 'DivideValues'. O(logn)
	void Readd(int key_id); //readd the entry with key 'key_id' to the heap. Assumes it was present in heap before and that it is in present in the current state. Its value is the previous value used before Remove(key_id) was called.  O(logn)
	void Grow(); //increases the Size of the heap by one. The key will have zero assigned to its value.
	void DivideValues(double divisor); //divides all the values in the heap by 'divisor'. This will affect the values of keys that have been removed. O(n)
	
	double GetKeyValue(int key_id) const;
	bool IsKeyPresent(int key_id) const;
	double ReadMaxValue() const; //read the key with the highest value without removing it. O(1)
	int Size() const;  //returns the Size of the heap. O(1)
	bool Empty() const; //return if the heap is empty, i.e. Size() == 0.
	int MaxSize() const; //returns the number of elements the heap can support without needing to call Grow(). O(1).

	bool DebugCheckConsistencyDataStructures() const;
	
private:
	vec<double> values_; //contains the values stored as a heap
	vec<int> map_key_to_position_; //[i] shows the location of the value of the key i in the values_ array
	vec<int> map_position_to_key_; //[i] shows which key is associated with values_[i]
	int end_position_; //the index past the last element in the heap

	void SwapPositions(int i, int j);
	void SiftUp(int i);
	void SiftDown(int i);

	int GetParent(int i) const;
	int GetLeftChild(int i) const;
	int GetRightChild(int i) const;
	int GetLargestChild(int i) const;

	bool IsHeap(int position) const;
	bool IsHeapLocally(int position) const;
	bool IsLeaf(int position) const;
	bool IsSmallerThanLeftChild(int position) const;
	bool IsSmallerThanRightChild(int position) const; //note that it returns false if there is no right child
	bool CheckSizeConsistency() const;

	bool IsMaxValue(double) const;

	bool AreDataStructuresOfSameSize() const;
};

} //end Pumpkin namespace

#endif // !KEY_VALUE_HEAP