#ifndef __HEAP_H__
#define __HEAP_H__

#include <iostream>
#include <cstddef>
#include <string>
#include <sstream>
#include <stdexcept>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <tuple>
#include "vector.h"
#include "util.h"
#include "../types.h"
#include "traits.h"
using namespace std;

// Traits de Ordenamiento para Heap
template <typename T>
struct MinHeapTrait : public BaseTrait<VectorNode<T>, less<T>>{
};

template <typename T>
struct MaxHeapTrait : public BaseTrait<VectorNode<T>, greater<T>>{
};

// Contenedor Principal Heap
template <typename Trait>
class Heap{
public:
    using value_type = typename Trait::value_type;
    using Node       = typename Trait::Node;
    using Comp       = typename Trait::Comp;
    using MySelf     = Heap<Trait>;

protected:
    Vector<Trait> m_vec;
    Comp          m_comp;
    mutable shared_mutex m_mtx;

    void heapifyUp(size_t index);
    void heapifyDown(size_t index);

public:
    Heap() : m_vec(), m_comp() {}

    // Copy Constructor
    Heap(const Heap &other) : m_vec(), m_comp() {
        shared_lock<shared_mutex> lock(other.m_mtx);
        m_vec = other.m_vec;
    }

    // Move Constructor
    Heap(Heap &&other) : m_vec(), m_comp() {
        unique_lock<shared_mutex> lock(other.m_mtx);
        m_vec = std::move(other.m_vec);
    }

    // Copy Assignment
    Heap& operator=(const Heap &other) {
        if(this != &other){
            unique_lock<shared_mutex> lockMe(m_mtx);
            shared_lock<shared_mutex> lockOther(other.m_mtx);
            m_vec = other.m_vec;
        }
        return *this;
    }

    // Move Assignment
    Heap& operator=(Heap &&other) {
        if(this != &other){
            unique_lock<shared_mutex> lockMe(m_mtx);
            unique_lock<shared_mutex> lockOther(other.m_mtx);
            m_vec = std::move(other.m_vec);
        }
        return *this;
    }

    // Destructor Seguro
    virtual ~Heap() {
        unique_lock<shared_mutex> lock(m_mtx);
    }

    // Operaciones
    virtual void   insert(value_type value, Ref ref);
    virtual std::tuple<value_type, Ref> extract();
    virtual value_type peek() const;
    virtual size_t size()  const;
    virtual bool   empty() const;

    // ToString
    string toString() const {
        shared_lock<shared_mutex> lock(m_mtx);
        return m_vec.toString();
    }

    // Operadores I/O 
    friend ostream& operator<<(ostream& os, const Heap& h) {
        shared_lock<shared_mutex> lock(h.m_mtx);
        return os << h.m_vec;
    }

    friend istream& operator>>(istream& is, Heap& h) {
        Char ch;
        if(!(is >> ch) || ch != '['){
            is.clear(ios_base::failbit);
            return is;
        }
        value_type val;
        Ref ref;
        Char comma, parenClose;
        while(is >> ch && ch != ']'){
            if(ch == '('){
                if(is >> val >> comma >> ref >> parenClose){
                    if(comma == ',' && parenClose == ')')
                        h.insert(val, ref);   // mantiene la propiedad del heap
                }
            }
        }
        return is;
    }
};


// insert: agrega al final y sube
template <typename Trait>
void Heap<Trait>::insert(value_type value, Ref ref) {
    unique_lock<shared_mutex> lock(m_mtx);
    m_vec.push_back(value, ref);
    heapifyUp(m_vec.size() - 1);
}

// heapifyUp
template <typename Trait>
void Heap<Trait>::heapifyUp(size_t index) {
    while(index > 0){
        size_t parent = (index - 1) / 2;
        if(m_comp(m_vec[index], m_vec[parent])){
            std::swap(m_vec[index], m_vec[parent]);
            index = parent;
        }else{
            break;
        }
    }
}

// extract
template <typename Trait>
std::tuple<typename Heap<Trait>::value_type, Ref> Heap<Trait>::extract() {
    unique_lock<shared_mutex> lock(m_mtx);
    if(m_vec.empty()) return std::make_tuple(value_type(), Ref());

    size_t last = m_vec.size() - 1;
    if(last > 0) std::swap(m_vec[0], m_vec[last]);
    auto result = m_vec.pop_back();           // devuelve la raiz original (data, ref)
    if(m_vec.size() > 0) heapifyDown(0);
    return result;
}

// heapifyDown
template <typename Trait>
void Heap<Trait>::heapifyDown(size_t index) {
    size_t heap_size = m_vec.size();
    while(true){
        size_t left_child  = 2 * index + 1;
        size_t right_child = 2 * index + 2;
        size_t best        = index;
        if(left_child  < heap_size && m_comp(m_vec[left_child],  m_vec[best])) best = left_child;
        if(right_child < heap_size && m_comp(m_vec[right_child], m_vec[best])) best = right_child;
        if(best != index){
            std::swap(m_vec[index], m_vec[best]);
            index = best;
        }else{
            break;
        }
    }
}

// peek
template <typename Trait>
typename Heap<Trait>::value_type Heap<Trait>::peek() const {
    shared_lock<shared_mutex> lock(m_mtx);
    if(m_vec.empty()) throw runtime_error("El heap esta vacio");
    return const_cast<Vector<Trait>&>(m_vec)[0];
}

template <typename Trait>
size_t Heap<Trait>::size() const {
    shared_lock<shared_mutex> lock(m_mtx);
    return m_vec.size();
}

template <typename Trait>
bool Heap<Trait>::empty() const {
    shared_lock<shared_mutex> lock(m_mtx);
    return m_vec.empty();
}

#endif // __HEAP_H__
