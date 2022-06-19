#pragma once

#include "pumpkin_assert.h"

#include <cstdint>
#include <iostream>

namespace Pumpkin
{
//A class representing a resizable bitvector
//The advantage of this class is that if the size of the bitvector is small (<= 64), the vector will be inlined
//	saving a memory access that would normally result from double-indirection
//	note: resizing the vector is a linear time operation (as opposed to amortised time like in many data structures from the standard template library)
class SmartBitvector
{
public:
	SmartBitvector();
	SmartBitvector(int num_bits);
	SmartBitvector(const SmartBitvector&);
	~SmartBitvector();

	void SetBit(int i);
	void ClearBit(int i);
	void AssignBit(int i, bool value);
	void Resize(int new_size);

	bool ReadBit(int i) const;	
	int Size() const;

private:
	SmartBitvector& operator=(SmartBitvector&); //for now it is private to forbid copies, could also make it similar to the copy constructor if needed

	union InternalBitStorage
	{
		bool* pointer_to_bitvector; //todo for simplicity the bitvector is represented as an array of bools, but I could consider using a bit-level representation
		uint64_t inlined_bitvector;
	} data_;
	int size_;
};

inline SmartBitvector::SmartBitvector()
{
	data_.pointer_to_bitvector = 0;
	size_ = 0;
}

inline SmartBitvector::SmartBitvector(int num_bits)
{
	size_ = num_bits;
	if (num_bits <= 64)
	{
		data_.inlined_bitvector = 0;
	}
	else
	{
		data_.pointer_to_bitvector = new bool[num_bits];
		for (int i = 0; i < num_bits; i++) { data_.pointer_to_bitvector[i] = false; }
	}	
}

inline SmartBitvector::SmartBitvector(const SmartBitvector &copy)
{
	this->size_ = copy.size_;
	if (copy.size_ <= 64)
	{
		this->data_.inlined_bitvector = copy.data_.inlined_bitvector;
	}
	else
	{
		this->data_.pointer_to_bitvector = new bool[copy.size_];
		for (int i = 0; i < copy.size_; i++)
		{
			this->data_.pointer_to_bitvector[i] = copy.data_.pointer_to_bitvector[i];
		}
	}
}

inline SmartBitvector::~SmartBitvector()
{
	if (size_ > 64) 
	{	
		delete[] data_.pointer_to_bitvector; 
		data_.pointer_to_bitvector = 0; 	
	}
}

inline void SmartBitvector::SetBit(int i)
{
	pumpkin_assert_moderate(i >= 0 && i < size_, "Sanity check.");

	if (size_ <= 64)
	{
		data_.inlined_bitvector |= (uint64_t(1) << i);
	}
	else
	{
		pumpkin_assert_moderate(data_.pointer_to_bitvector != NULL, "Sanity check.");
		data_.pointer_to_bitvector[i] = true;
	}
}

inline void SmartBitvector::ClearBit(int i)
{
	pumpkin_assert_moderate(i >= 0 && i < size_, "Sanity check.");

	if (size_ <= 64)
	{
		data_.inlined_bitvector &= (~(uint64_t(1) << i));
	}
	else
	{
		pumpkin_assert_moderate(data_.pointer_to_bitvector != NULL, "Sanity check.");
		data_.pointer_to_bitvector[i] = false;
	}
}

inline void SmartBitvector::AssignBit(int i, bool value)
{
	if (size_ <= 64)
	{
		if (value) { SetBit(i); }
		else { ClearBit(i); }
	}
	else
	{
		pumpkin_assert_moderate(data_.pointer_to_bitvector != NULL, "Sanity check.");
		data_.pointer_to_bitvector[i] = value;
	}	
}

inline void SmartBitvector::Resize(int new_size)
{
	int old_size = size_;

	if (old_size == new_size) 
	{ 
		return; 
	}
	if (old_size <= 64 && new_size <= 64)
	{
		size_ = new_size;
		return;
	}
	else if (old_size <= 64 && new_size > 64)
	{
		//allocate memory and initialise values
		bool* new_memory = new bool[new_size];
		for (int i = 0; i < old_size; i++) { new_memory[i] = ReadBit(i); }
		for (int i = old_size; i < new_size; i++) { new_memory[i] = false; }
		//set internal data structures
		data_.pointer_to_bitvector = new_memory;
		size_ = new_size;
		return;
	}
	else if (old_size > 64 && new_size > 64)
	{
		pumpkin_assert_moderate(data_.pointer_to_bitvector != NULL, "Sanity check.");

		//allocate memory and initialise values
		bool* new_memory = new bool[new_size];
		for (int i = 0; i < old_size; i++) { new_memory[i] = ReadBit(i); }
		for (int i = old_size; i < new_size; i++) { new_memory[i] = false; }
		//set internal data structures		
		delete[] data_.pointer_to_bitvector; //remember to delete the previously allocated memory
		data_.pointer_to_bitvector = new_memory;
		size_ = new_size;
	}
	else if (old_size > 64 && new_size <= 64)
	{
		pumpkin_assert_moderate(data_.pointer_to_bitvector != NULL, "Sanity check.");
		//need to shrink data now
		uint64_t new_inlined_bitvector = 0;
		for (int i = 0; i < new_size; i++)
		{
			//set the bit if necessary, otherwise do nothing since the default value is zero
			if (data_.pointer_to_bitvector[i])
			{
				new_inlined_bitvector |= (1 << i);
			}			
		}
		//set internal data structures
		delete[] data_.pointer_to_bitvector; //remember to delete the previously allocated memory
		data_.inlined_bitvector = new_inlined_bitvector;
		size_ = new_size;		
	}
	else
	{
		pumpkin_assert_permanent(1 == 2, "Unexpected case...");
	}
}

inline bool SmartBitvector::ReadBit(int i) const
{
	pumpkin_assert_moderate(i >= 0 && i < size_, "Sanity check.");

	if (size_ <= 64)
	{		
		return data_.inlined_bitvector & (uint64_t(1) << i);
	}
	else
	{
		pumpkin_assert_moderate(data_.pointer_to_bitvector != NULL, "Sanity check.");
		return data_.pointer_to_bitvector[i];
	}
}

inline int Pumpkin::SmartBitvector::Size() const
{
	return size_;
}

}