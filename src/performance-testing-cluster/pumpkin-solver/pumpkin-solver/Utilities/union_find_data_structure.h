#pragma once

#include <vector>

namespace Pumpkin
{
class UnionFindDataStructure
{
public:
	UnionFindDataStructure(int num_ids);

	bool Merge(int id1, int id2); //returns true if a change was needed

	int NumIDs() const;
	bool IsRepresentative(int id);
	int GetRepresentative(int id);	
	int NumEquivalentIDs(int id);	

	void Grow();

private:
	struct ClassInfo { int representative_id; int size; };
	std::vector<ClassInfo> id_to_equivalence_class_;
};
}