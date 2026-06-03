#ifndef __VECTOR_H__
#define __VECTOR_H__

#include <iostream>
#include <cstddef>      // size_t
#include <string>
#include <sstream>
#include <stdexcept>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <tuple>
#include "general_iterator.h"
#include "util.h"
#include "../types.h"
#include "traits.h"
using namespace std;

// Iterador Forward
template <typename Container>
class vector_forward_iterator : public general_iterator<Container, vector_forward_iterator<Container>>{
public:
    using MySelf = vector_forward_iterator<Container>;
    using Parent = general_iterator<Container, MySelf>;
    using Parent::Parent;

    MySelf operator++() { this->m_pNode++; return *this; }
};

// Iterador Backward
template <typename Container>
class vector_backward_iterator : public general_iterator<Container, vector_backward_iterator<Container>>{
public:
    using MySelf = vector_backward_iterator<Container>;
    using Parent = general_iterator<Container, MySelf>;
    using Parent::Parent;

    MySelf operator++() { this->m_pNode--; return *this; }
};

// Vector Node
template <typename T>
class VectorNode{
public:
    using value_type = T;
    using Node       = VectorNode<T>;
private:
    T   m_data;
    Ref m_ref;
public:
    VectorNode() : m_data(T()), m_ref(Ref()) {}
    VectorNode(T data, Ref ref) : m_data(data), m_ref(ref) {}
    VectorNode(const VectorNode &other) : m_data(other.m_data), m_ref(other.m_ref) {}
    VectorNode(VectorNode &&other) : m_data(move(other.m_data)), m_ref(move(other.m_ref)) {}
    VectorNode& operator=(const VectorNode &other) {
        m_data = other.m_data;
        m_ref  = other.m_ref;
        return *this;
    }
    VectorNode& operator=(VectorNode &&other) {
        m_data = move(other.m_data);
        m_ref  = move(other.m_ref);
        return *this;
    }
    virtual ~VectorNode() {}

    T      getData() const { return m_data; }
    T&     getDataRef()    { return m_data; }
    void   setData(T data) { m_data = data; }
    Ref    getRef() const  { return m_ref; }
    void   setRef(Ref ref) { m_ref = ref; }
};

// Traits de Ordenamiento
template <typename T>
struct AscendingVectorTrait : public BaseTrait<VectorNode<T>, less<T>>{
};

template <typename T>
struct DescendingVectorTrait : public BaseTrait<VectorNode<T>, greater<T>>{
};

// Contenedor Principal Vector
template <typename Trait>
class Vector{
public:
    using value_type = typename Trait::value_type;
    using Node       = typename Trait::Node;
    using Comp       = typename Trait::Comp;
    using MySelf     = Vector<Trait>;

    using forward_iterator  = vector_forward_iterator<MySelf>;
    using backward_iterator = vector_backward_iterator<MySelf>;
    friend forward_iterator;
    friend backward_iterator;

protected:
    size_t  m_capacity;
    size_t  m_size;
    Node   *m_data;
    mutable shared_mutex m_mtx;
    void    resize();

public:
    Vector(size_t capacity = 10) : m_capacity(capacity), m_size(0), m_data(new Node[capacity]) {}

    // Copy Constructor
    Vector(const Vector &other) : m_capacity(0), m_size(0), m_data(nullptr) {
        shared_lock<shared_mutex> lock(other.m_mtx);
        m_capacity = other.m_capacity;
        m_size     = other.m_size;
        m_data     = new Node[m_capacity];
        for(size_t i = 0; i < m_size; ++i)
            m_data[i] = other.m_data[i];
    }

    // Move Constructor
    Vector(Vector &&other) : m_capacity(0), m_size(0), m_data(nullptr) {
        unique_lock<shared_mutex> lock(other.m_mtx);
        m_capacity = std::exchange(other.m_capacity, 0);
        m_size     = std::exchange(other.m_size, 0);
        m_data     = std::exchange(other.m_data, nullptr);
    }

    // Copy Assignment
    Vector& operator=(const Vector &other) {
        if(this != &other){
            unique_lock<shared_mutex> lockMe(m_mtx);
            delete[] m_data;
            shared_lock<shared_mutex> lockOther(other.m_mtx);
            m_capacity = other.m_capacity;
            m_size     = other.m_size;
            m_data     = new Node[m_capacity];
            for(size_t i = 0; i < m_size; ++i)
                m_data[i] = other.m_data[i];
        }
        return *this;
    }

    // Move Assignment
    Vector& operator=(Vector &&other) {
        if(this != &other){
            unique_lock<shared_mutex> lockMe(m_mtx);
            delete[] m_data;
            unique_lock<shared_mutex> lockOther(other.m_mtx);
            m_capacity = std::exchange(other.m_capacity, 0);
            m_size     = std::exchange(other.m_size, 0);
            m_data     = std::exchange(other.m_data, nullptr);
        }
        return *this;
    }

    // Destructor Seguro
    virtual ~Vector() {
        unique_lock<shared_mutex> lock(m_mtx);
        delete[] m_data;
        m_data     = nullptr;
        m_size     = 0;
        m_capacity = 0;
    }

    // Operaciones
    virtual void   push_back(value_type value, Ref ref);
    virtual std::tuple<value_type, Ref> pop_back();

    virtual value_type& operator[](size_t index);
    virtual size_t      size() const;
    virtual bool        empty() const;

    forward_iterator  begin()  { return forward_iterator(this, m_data); }
    forward_iterator  end()    { return forward_iterator(this, m_data + m_size); }
    backward_iterator rbegin() { return backward_iterator(this, m_data + m_size - 1); }
    backward_iterator rend()   { return backward_iterator(this, m_data - 1); }

    // ForEach (concurrente)
    template <typename Func, typename... Args>
    void ForEach(Func func, Args &&... args){
        unique_lock<shared_mutex> lock(m_mtx);
        if(m_size == 0) return;
        for(auto& item : *this){
            func(item, std::forward<Args>(args)...);
        }
    }

    // ReverseForEach (concurrente)
    template <typename Func, typename... Args>
    void ReverseForEach(Func func, Args &&... args){
        unique_lock<shared_mutex> lock(m_mtx);
        if(m_size == 0) return;
        ::ForEach(rbegin(), rend(), func, std::forward<Args>(args)...);
    }

    // ToString
    string toString() const {
        shared_lock<shared_mutex> lock(m_mtx);
        ostringstream oss;
        oss << "[";
        for(size_t i = 0; i < m_size; ++i){
            if(i > 0) oss << ",";
            oss << "(" << m_data[i].getData() << "," << m_data[i].getRef() << ")";
        }
        oss << "]";
        return oss.str();
    }

    // Operadores I/O (mismo formato que LinkedList: [(data,ref),(data,ref),...])
    friend ostream& operator<<(ostream& os, const Vector& v) {
        return os << v.toString();
    }

    friend istream& operator>>(istream& is, Vector& v) {
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
                        v.push_back(val, ref);
                }
            }
        }
        return is;
    }
};

// Implementacion de Metodos de Vector
template <typename Trait>
void Vector<Trait>::resize() {
    m_capacity = (m_capacity < 10) ? m_capacity + 10 : m_capacity * 2;
    Node *new_data = new Node[m_capacity];
    for(size_t i = 0; i < m_size; ++i)
        new_data[i] = m_data[i];
    delete[] m_data;
    m_data = new_data;
}

template <typename Trait>
void Vector<Trait>::push_back(value_type value, Ref ref) {
    unique_lock<shared_mutex> lock(m_mtx);
    if(m_size == m_capacity) resize();
    m_data[m_size++] = Node(value, ref);
}

template <typename Trait>
std::tuple<typename Vector<Trait>::value_type, Ref> Vector<Trait>::pop_back() {
    unique_lock<shared_mutex> lock(m_mtx);
    if(m_size == 0) throw runtime_error("El vector esta vacio");
    auto result = std::make_tuple(m_data[m_size-1].getData(), m_data[m_size-1].getRef());
    m_size--;
    return result;
}

template <typename Trait>
typename Vector<Trait>::value_type& Vector<Trait>::operator[](size_t index) {
    shared_lock<shared_mutex> lock(m_mtx);
    if(index >= m_size) index = m_size - 1;
    return m_data[index].getDataRef();
}

template <typename Trait>
size_t Vector<Trait>::size() const {
    shared_lock<shared_mutex> lock(m_mtx);
    return m_size;
}

template <typename Trait>
bool Vector<Trait>::empty() const {
    shared_lock<shared_mutex> lock(m_mtx);
    return m_size == 0;
}

void DemoVector();
void DemoConcurrentVector();

#endif // __VECTOR_H__
