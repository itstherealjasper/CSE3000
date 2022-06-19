#ifndef VECTOR_OBJECT_INDEXED_H_
#define VECTOR_OBJECT_INDEXED_H_

#include <vector>

namespace Pumpkin
{

//a class merely for convenience
//used to have standard vectors that are indexed directly by literals or variables
//ObjectType is the type that will be used to index the vector
//DataType is the type that will be stored inside the vector
template <class ObjectType, class DataType>
class VectorObjectIndexed
{
public:
	VectorObjectIndexed(size_t num_entries = 0, DataType initial_values = DataType()) :data_(num_entries, initial_values) {}
	void push_back(const DataType &val) { data_.push_back(val); }
	size_t Size() const { return data_.size(); }
	void Resize(size_t new_size, const DataType default_value = DataType()) { data_.resize(new_size, default_value); }
	void Clear() { data_.clear(); }
	const DataType& operator[](const ObjectType &obj) const { return data_[obj.ToPositiveInteger()]; }
	DataType& operator[](const ObjectType &obj) { return data_[obj.ToPositiveInteger()]; }
	typename std::vector<DataType>::const_iterator begin() const { return data_.begin(); }
	typename std::vector<DataType>::const_iterator end() const { return data_.end(); }
	
private:
	std::vector<DataType> data_;
};

} //end Pumpkin namespace

#endif // !VECTOR_OBJECT_INDEXED_H_