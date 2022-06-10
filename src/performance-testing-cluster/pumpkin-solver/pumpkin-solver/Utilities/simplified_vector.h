#pragma once

#include "runtime_assert.h"

#include <assert.h>
#include <string.h>

namespace Pumpkin
{
//todo explain; not the safest class, e.g.,, copying element is done memory-wise without referring to other copies, explain this
/*template <typename Type>
class SimplifiedVector
{
public:
	SimplifiedVector();
	SimplifiedVector(int size);
	SimplifiedVector(int size, const Type &default_value, int capacity);

	Type operator[](int index) const;
	Type& operator[](int index);
	Type& Back();

	int Size() const;
	bool IsEmpty() const;

	void Clear();

	void IncreaseCapacity(int desired_capacity);
	void Resize(int new_size);
	void Grow(const Type &t = Type());
	void PushBack(const Type&);
	void PopBack();

private:
	int size_, capacity_;
	Type* data_;
};

template<typename Type>
inline SimplifiedVector<Type>::SimplifiedVector():
	size_(0), capacity_(16), data_(0)
{
	data_ = new Type[capacity_];
}
template<typename Type>
inline SimplifiedVector<Type>::SimplifiedVector(int size):
	size_(size), capacity_(size), data_(0)
{
	data_ = new Type[size];
}

template<typename Type>
inline SimplifiedVector<Type>::SimplifiedVector(int size, const Type &default_value, int capacity):
	size_(size), capacity_(capacity), data_(0)
{
	data_ = new Type[capacity];
	for (int i = 0; i < size; i++) { data_[i] = default_value; }
}
template<typename Type>
inline Type SimplifiedVector<Type>::operator[](int index) const
{
	assert(index >= 0 && index < size_);
	return data_[index];
}
template<typename Type>
inline Type& SimplifiedVector<Type>::operator[](int index)
{
	assert(index >= 0 && index < size_);
	return data_[index];
}
template<typename Type>
inline Type& SimplifiedVector<Type>::Back()
{
	assert(size_ > 0);
	return data_[size_ - 1];
}
template<typename Type>
inline int SimplifiedVector<Type>::Size() const
{
	return size_;
}
template<typename Type>
inline bool SimplifiedVector<Type>::IsEmpty() const
{
	return size_ == 0;
}

template<typename Type>
inline void SimplifiedVector<Type>::Clear()
{
	size_ = 0;
}

template<typename Type>
inline void SimplifiedVector<Type>::Resize(int new_size)
{
	if (new_size > capacity_) { IncreaseCapacity(new_size); }
	size_ = new_size;
}

template<typename Type>
inline void SimplifiedVector<Type>::Grow(const Type& t)
{
	PushBack(t);
}

template<typename Type>
inline void SimplifiedVector<Type>::PushBack(const Type &element)
{
	if (size_ == capacity_) { IncreaseCapacity(std::max(2*capacity_, 1)); } //the 'max' is used to guarantee the capacity increases even if the previous capacity was zero
	data_[size_] = element;
	++size_;
}

template<typename Type>
inline void SimplifiedVector<Type>::PopBack()
{
	assert(size_ > 0);
	--size_;
}
template<typename Type>
inline void SimplifiedVector<Type>::IncreaseCapacity(int desired_capacity)
{
	//assert(desired_capacity > capacity_);
	int new_capacity = 2 * capacity_;
	while (new_capacity < desired_capacity) { new_capacity *= 2; }

	Type* new_data = new Type[new_capacity];
	runtime_assert(new_data);
	memcpy(new_data, data_, sizeof(Type) * size_);
	capacity_ = new_capacity;
	delete[] data_;
	data_ = new_data;
}*/
}