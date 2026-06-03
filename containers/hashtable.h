#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include <iostream>
#include <cstddef>
#include <string>
#include <sstream>
#include <stdexcept>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <functional>
#include "AVL.h"
#include "util.h"
#include "../types.h"
#include "traits.h"
using namespace std;


template <typename K, typename V>
struct HashEntry{
    K first;
    V second;

    HashEntry() : first(K()), second(V()) {}
    HashEntry(const K &k) : first(k), second(V()) {}
    HashEntry(const K &k, const V &v) : first(k), second(v) {}

    bool operator<(const HashEntry &o) const { return first <  o.first; }
    bool operator>(const HashEntry &o) const { return first >  o.first; }
    bool operator==(const HashEntry &o) const { return first == o.first; }
};


// trait
template <typename K, typename V>
struct HashTableTrait : public BaseTrait<BinaryTreeNode<HashEntry<K,V>>, less<HashEntry<K,V>>>{
    using key_type    = K;
    using mapped_type = V;
};


// Iterador Forward: recorre buckets en orden y dentro de cada bucket usa inorder del AVL
template <typename Container>
class hash_forward_iterator : public general_iterator<Container, hash_forward_iterator<Container>>{
public:
    using MySelf = hash_forward_iterator<Container>;
    using Parent = general_iterator<Container, MySelf>;
    using Node   = typename Container::Node;
private:
    size_t m_bucket;
public:
    hash_forward_iterator(Container *t, size_t b, Node *node)
        : Parent(t, node), m_bucket(b) {}

    MySelf operator++() {
        using Bucket = typename Container::Bucket;

        // Reutiliza el inorder del AVL
        typename Bucket::forward_iterator avl_it(&this->m_pContainer->m_buckets[m_bucket],this->m_pNode);
        ++avl_it;
        this->m_pNode = avl_it.getNode();

        // Si terminamos el AVL actual, saltamos al siguiente bucket no vacio
        if(!this->m_pNode){
            ++m_bucket;
            while(m_bucket < this->m_pContainer->m_capacity){
                auto bucket_it = this->m_pContainer->m_buckets[m_bucket].begin();
                if(bucket_it.getNode()){
                    this->m_pNode = bucket_it.getNode();
                    return *this;
                }
                ++m_bucket;
            }
        }
        return *this;
    }
};

// HashTable
template <typename Trait>
class HashTable{
public:
    using key_type    = typename Trait::key_type;
    using mapped_type = typename Trait::mapped_type;
    using value_type  = typename Trait::value_type;   // HashEntry<K,V>
    using Node        = BinaryTreeNode<value_type>;
    using Bucket      = AVL<Trait>;
    using MySelf      = HashTable<Trait>;

    using forward_iterator = hash_forward_iterator<MySelf>;
    friend forward_iterator;

protected:
    Bucket *m_buckets;
    size_t  m_capacity;
    size_t  m_size;
    mutable shared_mutex m_mtx;
    hash<key_type> m_hash;

    // helpers internos
    size_t bucket_index_internal(const key_type &key) const {
        return m_hash(key) % m_capacity;
    }

public:
    // Constructor por defecto
    HashTable(size_t capacity): m_buckets(new Bucket[capacity]), m_capacity(capacity), m_size(0) {}

    // Constructor Copia
    HashTable(const HashTable &other)
        : m_buckets(nullptr), m_capacity(0), m_size(0){
        shared_lock<shared_mutex> lock(other.m_mtx);
        m_capacity = other.m_capacity;
        m_size     = other.m_size;
        m_buckets  = new Bucket[m_capacity];
        for(size_t i = 0; i < m_capacity; ++i)
            m_buckets[i] = other.m_buckets[i];
    }

    // Move Constructor
    HashTable(HashTable &&other)
        : m_buckets(nullptr), m_capacity(0), m_size(0){
        unique_lock<shared_mutex> lock(other.m_mtx);
        m_buckets  = exchange(other.m_buckets, nullptr);
        m_capacity = exchange(other.m_capacity, 0);
        m_size     = exchange(other.m_size, 0);
    }

    // Destructor Seguro
    virtual ~HashTable(){
        unique_lock<shared_mutex> lock(m_mtx);
        delete[] m_buckets;
        m_buckets  = nullptr;
        m_capacity = 0;
        m_size     = 0;
    }

    virtual void          insert(const key_type &key, const mapped_type &value);
    virtual bool          contains(const key_type &key) const;
    virtual mapped_type&  operator[](const key_type &key);
    virtual size_t        size() const;
    virtual bool          empty() const;
    virtual size_t        capacity() const;

    forward_iterator begin(){
        for(size_t i = 0; i < m_capacity; ++i){
            auto bucket_it = m_buckets[i].begin();
            if(bucket_it.getNode()) return forward_iterator(this, i, bucket_it.getNode());
        }
        return end();
    }
    forward_iterator end(){ return forward_iterator(this, m_capacity, nullptr); }

    // ForEach
    template <typename Func, typename... Args>
    void ForEach(Func func, Args &&... args){
        unique_lock<shared_mutex> lock(m_mtx);
        if(m_size == 0) return;
        for(auto& entry : *this)
            func(entry, std::forward<Args>(args)...);
    }

    // Operadores I/O
    friend ostream& operator<<(ostream& os, const HashTable& h){
        shared_lock<shared_mutex> lock(h.m_mtx);
        os << "[";
        bool first = true;
        for(size_t i = 0; i < h.m_capacity; ++i){
            // iteracion in-order del AVL bucket
            for(auto it = h.m_buckets[i].begin(); !(it == h.m_buckets[i].end()); ++it){
                if(!first) os << ",";
                os << "(" << (*it).first << "," << (*it).second << ")";
                first = false;
            }
        }
        os << "]";
        return os;
    }

    friend istream& operator>>(istream& is, HashTable& h){
        Char ch;
        if(!(is >> ch) || ch != '['){
            is.clear(ios_base::failbit);
            return is;
        }
        key_type    key;
        mapped_type val;
        Char comma, parenClose;
        while(is >> ch && ch != ']'){
            if(ch == '('){
                if(is >> key >> comma >> val >> parenClose){
                    if(comma == ',' && parenClose == ')')
                        h.insert(key, val);
                }
            }
        }
        return is;
    }
};


// si la clave ya existe -> actualiza, si no -> nueva entrada en el AVL bucket
template <typename Trait>
void HashTable<Trait>::insert(const key_type &key, const mapped_type &value){
    unique_lock<shared_mutex> lock(m_mtx);

    size_t bucket_idx = bucket_index_internal(key);
    value_type searchEntry(key);
    auto *existing = m_buckets[bucket_idx].find(searchEntry);
    if(existing){
        existing->getDataRef().second = value;   // update
    }else{
        m_buckets[bucket_idx].insert(value_type(key, value), 0);
        m_size++;
    }
}

// contains
template <typename Trait>
bool HashTable<Trait>::contains(const key_type &key) const{
    shared_lock<shared_mutex> lock(m_mtx);
    size_t bucket_idx = bucket_index_internal(key);
    return m_buckets[bucket_idx].find(value_type(key)) != nullptr;
}

// operator[]
template <typename Trait>
typename HashTable<Trait>::mapped_type&
HashTable<Trait>::operator[](const key_type &key){
    unique_lock<shared_mutex> lock(m_mtx);

    size_t bucket_idx = bucket_index_internal(key);
    value_type searchEntry(key);
    auto *existing = m_buckets[bucket_idx].find(searchEntry);
    if(existing)
        return existing->getDataRef().second;

    // No existe
    m_buckets[bucket_idx].insert(value_type(key, mapped_type()), 0);
    m_size++;
    existing = m_buckets[bucket_idx].find(searchEntry);
    return existing->getDataRef().second;
}

template <typename Trait>
size_t HashTable<Trait>::size() const {
    shared_lock<shared_mutex> lock(m_mtx);
    return m_size;
}

template <typename Trait>
bool HashTable<Trait>::empty() const {
    shared_lock<shared_mutex> lock(m_mtx);
    return m_size == 0;
}

template <typename Trait>
size_t HashTable<Trait>::capacity() const {
    shared_lock<shared_mutex> lock(m_mtx);
    return m_capacity;
}

#endif