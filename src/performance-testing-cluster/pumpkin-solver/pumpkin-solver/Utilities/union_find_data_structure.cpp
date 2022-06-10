#include "union_find_data_structure.h"
#include "pumpkin_assert.h"

Pumpkin::UnionFindDataStructure::UnionFindDataStructure(int num_ids):id_to_equivalence_class_(num_ids)
{
	//initially each id is in its own equivalence class
	for (int i = 0; i < num_ids; i++) 
    { 
        id_to_equivalence_class_[i].representative_id = i;
        id_to_equivalence_class_[i].size = 1;
    }
}

bool Pumpkin::UnionFindDataStructure::Merge(int id1, int id2)
{
    int a = GetRepresentative(id1);
    int b = GetRepresentative(id2);

    //if both ids are already in the same equivalence class, nothing to be done
    if (a == b) { return false; }

    //for simplicity let 'a' be the class with more elements
    if (id_to_equivalence_class_[a].size < id_to_equivalence_class_[b].size)
    {
        std::swap(a, b);
    }

    //the class 'b' is merged into class 'a', meaning...
    //  the representative of 'b' is now 'a'
    id_to_equivalence_class_[b].representative_id = a;
    //  the size of 'a' is updated by adding everything from 'b'
    //  note that this is a lazy class, updates of elements that are equivalent to b happens when their representative is queries (see GetRepresentative)
    id_to_equivalence_class_[a].size += id_to_equivalence_class_[b].size;

	return true;
}

int Pumpkin::UnionFindDataStructure::NumIDs() const
{
    return id_to_equivalence_class_.size();
}

bool Pumpkin::UnionFindDataStructure::IsRepresentative(int id)
{
    int rep = GetRepresentative(id);
    return id == rep;
}

int Pumpkin::UnionFindDataStructure::GetRepresentative(int id)
{
    int rep = id_to_equivalence_class_[id].representative_id;
    while (id != rep)
    {
        int rep_rep = id_to_equivalence_class_[rep].representative_id;
        id_to_equivalence_class_[id].representative_id = rep_rep;
        id = rep;
        rep = rep_rep;
    }
    return rep;
}

int Pumpkin::UnionFindDataStructure::NumEquivalentIDs(int id)
{
    int rep = GetRepresentative(id);
    return id_to_equivalence_class_[rep].size;
}

void Pumpkin::UnionFindDataStructure::Grow()
{
    ClassInfo info;
    info.representative_id = id_to_equivalence_class_.size();
    info.size = 1;
    id_to_equivalence_class_.push_back(info);
}
