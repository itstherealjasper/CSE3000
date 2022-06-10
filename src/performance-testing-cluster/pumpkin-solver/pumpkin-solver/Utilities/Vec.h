/*******************************************************************************************[Vec.h]
Copyright (c) 2003-2007, Niklas Een, Niklas Sorensson
Copyright (c) 2007-2010, Niklas Sorensson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#ifndef Glucose_Vec_h
#define Glucose_Vec_h

#include "runtime_assert.h"
#include "Vec2.h"

#include <assert.h>
#include <new>

#include <string.h>
#include <vector>

template<typename T>
using vec = vec2<T>;

//=================================================================================================
// Automatically resizable arrays
//
// NOTE! Don't use this vector on datatypes that cannot be re-located in memory (with realloc)


/*template<class T>
class vec {
    //T*  data;
    //int sz;
    //int cap;
    std::vector<T> vec_;

    // Don't allow copying (error prone):
    //vec<T>&  operator = (vec<T>& other) { assert(0); return *this; }
    //         vec        (vec<T>& other) { assert(0); }

    // Helpers for calculating next capacity:
    static inline int  imax(int x, int y) { int mask = (y - x) >> (sizeof(int) * 8 - 1); return (x & mask) + (y & (~mask)); }
    //static inline void nextCap(int& cap){ cap += ((cap >> 1) + 2) & ~1; }
    static inline void nextCap(int& cap) { cap += ((cap >> 1) + 2) & ~1; }

public:
    // Constructors:
    vec() : vec_(0) { }
    explicit vec(int size) : vec_(size)  { growTo(size); }
    vec(int size, const T& pad) : vec_(size, pad) { growTo(size, pad); }
    ~vec() { clear(true); }

    // Pointer to first element:
    operator T* (void) { return vec_.size() == 0 ? NULL : &vec_[0];  }

    // Size operations:
    int      size(void) const { return vec_.size(); }
    bool      empty(void) const { return vec_.size() == 0; }
    void     shrink(int nelems) { assert(nelems <= vec_.size()); vec_.resize(vec_.size() - nelems);  }
    void     shrink_(int nelems) { assert(nelems <= vec_.size()); vec_.resize(vec_.size() - nelems);  }
    void resize(int nelems) { assert(nelems <= vec_.size()); vec_.resize(nelems); }
    void resize(int nelems, const T&pad) { assert(nelems >= vec_.size()); vec_.resize(nelems, pad); }
    int      capacity(void) const { return vec_.capacity(); }
    void     capacity(int min_cap);
    void     growTo(int size);
    void     growTo(int size, const T& pad);
    void     clear(bool dealloc = false);

    auto begin() const { return vec_.begin(); }; //should return const pointer? not sure
    auto end() const {
        return vec_.end();
    }

    // Stack interface:
    void     push(void) { vec_.push_back(T()); }
    void     push(const T& elem) { vec_.push_back(elem);  }
    void     push_(const T& elem) { vec_.push_back(elem); }
    void     pop(void) { vec_.pop_back();  }

    void     remove(const T& elem) {
        int tmp;
        for (tmp = 0; tmp < vec_.size(); tmp++) {
            if (vec_[tmp] == elem)
                break;
        }
        if (tmp < vec_.size()) {
            assert(vec_[tmp] == elem);
            vec_[tmp] = vec_[vec_.size() - 1];
            vec_.pop_back();// sz = sz - 1;
        }

    }

    // NOTE: it seems possible that overflow can happen in the 'sz+1' expression of 'push()', but
    // in fact it can not since it requires that 'cap' is equal to INT_MAX. This in turn can not
    // happen given the way capacities are calculated (below). Essentially, all capacities are
    // even, but INT_MAX is odd.

    const T& last(void) const {
        return vec_.back();
    }// data[sz-1]; }
    T& last(void) { return vec_.back(); }// data[sz - 1]; }

    // Vector interface:
    const T& operator [] (int index) const { return vec_[index]; }
    T& operator [] (int index) { return vec_[index]; }

    // Duplicatation (preferred instead):
    void copyTo(vec<T>& copy) const { copy.vec_ = this->vec_;  }
    void moveTo(vec<T>& dest) { dest.vec_ = this->vec_; this->vec_.clear();  }
    void memCopyTo(vec<T>& copy) const {
        copy.vec_ = this->vec_;
    }

};


template<class T>
void vec<T>::capacity(int min_cap) {
    vec_.reserve(min_cap);
    
}


template<class T>
void vec<T>::growTo(int size, const T& pad) {
    if (vec_.size() >= size) { return; }

    vec_.resize(size, pad);

}


template<class T>
void vec<T>::growTo(int size) {

    if (vec_.size() >= size) { return; }

    vec_.resize(size);


}


template<class T>
void vec<T>::clear(bool dealloc) {
    vec_.clear();

}*/

//=================================================================================================


#endif
