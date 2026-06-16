#ifndef __HASHAVL_H__
#define __HASHAVL_H__

#include <iostream>
#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <functional>
#include <initializer_list>
#include "linkedlist.h"
#include "AVL.h"
#include "util.h"
#include "../types.h"
#include "traits.h"
using namespace std;


// Entrada de la cadena de colisiones
template <typename K, typename V>
struct HashAVLEntry {
    K first;
    V second;

    HashAVLEntry() : first(K()), second(V()) {}
    HashAVLEntry(const K& key) : first(key), second(V()) {}
    HashAVLEntry(const K& key, const V& value) : first(key), second(value) {}

    bool operator<(const HashAVLEntry& other) const { return first <  other.first; }
    bool operator>(const HashAVLEntry& other) const { return first >  other.first; }
    bool operator==(const HashAVLEntry& other) const { return first == other.first; }

    friend ostream& operator<<(ostream& os, const HashAVLEntry& e) {
        return os << "(" << e.first << "," << e.second << ")";
    }
};

// Trait para el LinkedList de colisiones
template <typename K, typename V>
struct HashAVLChainTrait : public BaseTrait<LLNode<HashAVLEntry<K,V>>, less<HashAVLEntry<K,V>>> {};

// Cada nodo del AVL almacena un hash_val y cadena para las colisiones
template <typename K, typename V>
struct HashAVLBucket {
    size_t hash_val;
    LinkedList<HashAVLChainTrait<K,V>> chain;

    HashAVLBucket() : hash_val(0) {}
    HashAVLBucket(size_t h) : hash_val(h) {}

    bool operator<(const HashAVLBucket& other) const { return hash_val <  other.hash_val; }
    bool operator>(const HashAVLBucket& other) const { return hash_val >  other.hash_val; }
    bool operator==(const HashAVLBucket& other) const { return hash_val == other.hash_val; }

    friend ostream& operator<<(ostream& os, HashAVLBucket& b) {
        for (auto it = b.chain.begin(); !(it == b.chain.end()); ++it)
            os << *it;
        return os;
    }
};

// Trait del AVL: ordena nodos por hash_val
template <typename K, typename V>
struct HashAVLTreeTrait : public BaseTrait<BinaryTreeNode<HashAVLBucket<K,V>>, less<HashAVLBucket<K,V>>> {};


// Sucesor inorder en el BST (sin lock, uso interno del iterador)
template <typename NodeType>
NodeType* hashavl_inorder_next(NodeType* node) {
    if(node->getRight()) {
        NodeType* curr = node->getRight();
        while(curr->getLeft()) curr = curr->getLeft();
        return curr;
    }
    NodeType* parent = node->getParent();
    while(parent && node == parent->getRight()) {
        node   = parent;
        parent = parent->getParent();
    }
    return parent;
}


// Iterador: recorre el AVL inorder y dentro de cada nodo la cadena de colisiones
template <typename Container>
class hashavl_forward_iterator : public general_iterator<Container, hashavl_forward_iterator<Container>> {
public:
    using MySelf  = hashavl_forward_iterator<Container>;
    using Parent  = general_iterator<Container, MySelf>;
    using Node    = typename Container::Node;
    using AVLNode = typename Container::AVLNodeType;

private:
    AVLNode* m_pAVLNode;

public:
    hashavl_forward_iterator(Container* cont, AVLNode* avl_node, Node* ll_node)
        : Parent(cont, ll_node), m_pAVLNode(avl_node) {}

    MySelf operator++() {
        if(this->m_pNode)
            this->m_pNode = this->m_pNode->getNext();

        // Si se agoto la cadena, avanzar al siguiente nodo del AVL
        if(!this->m_pNode && m_pAVLNode) {
            m_pAVLNode = hashavl_inorder_next(m_pAVLNode);
            while(m_pAVLNode) {
                Node* ll_node = m_pAVLNode->getDataRef().chain.begin().getNode();
                if(ll_node) {
                    this->m_pNode = ll_node;
                    return *this;
                }
                m_pAVLNode = hashavl_inorder_next(m_pAVLNode);
            }
        }
        return *this;
    }
};


// HashAVL: tabla hash simulada con un AVL como estructura primaria
// El insert del AVL posiciona cada bucket por hash_val
// Las colisiones (mismo hash, distinta clave) se resuelven con LinkedList
template <typename K, typename V>
class HashAVL {
public:
    using key_type    = K;
    using mapped_type = V;
    using value_type  = HashAVLEntry<K, V>;
    using Bucket      = HashAVLBucket<K, V>;
    using AVLTree     = AVL<HashAVLTreeTrait<K, V>>;
    using AVLNodeType = BinaryTreeNode<Bucket>;
    using Node        = LLNode<value_type>;
    using MySelf      = HashAVL<K, V>;

    using forward_iterator = hashavl_forward_iterator<MySelf>;
    friend forward_iterator;

private:
    AVLTree*             m_avl;
    size_t               m_size;
    mutable shared_mutex m_mtx;
    hash<key_type>       m_hash;

    // Inserta sin tomar lock externo (uso en constructores)
    void insert_internal(const K& key, const V& value) {
        size_t hash_val = m_hash(key);
        Bucket search_bucket(hash_val);
        AVLNodeType* avl_node = m_avl->find(search_bucket);
        if(!avl_node) {
            // El insert del AVL ubica el bucket en el arbol segun hash_val
            m_avl->insert(search_bucket, 0);
            avl_node = m_avl->find(search_bucket);
        }
        auto& chain = avl_node->getDataRef().chain;
        // Buscar la clave exacta en la cadena de colisiones
        for(auto chain_it = chain.begin(); !(chain_it == chain.end()); ++chain_it) {
            if((*chain_it).first == key) {
                (*chain_it).second = value;
                return;
            }
        }
        chain.push_back(value_type(key, value), 0);
        ++m_size;
    }

public:
    HashAVL() : m_avl(new AVLTree()), m_size(0) {}

    // Constructor Copia
    HashAVL(const HashAVL& other) : m_avl(new AVLTree()), m_size(0) {
        shared_lock<shared_mutex> lock(other.m_mtx);
        for (const auto& [key, value] : other)
            insert_internal(key, value);
    }

    // Move Constructor
    HashAVL(HashAVL&& other) : m_avl(nullptr), m_size(0) {
        unique_lock<shared_mutex> lock(other.m_mtx);
        m_avl  = exchange(other.m_avl,  nullptr);
        m_size = exchange(other.m_size, 0);
    }

    virtual mapped_type& operator[](const key_type& key);

    forward_iterator begin() {
        AVLNodeType* avl_node = m_avl->begin().getNode();
        while(avl_node) {
            Node* ll_node = avl_node->getDataRef().chain.begin().getNode();
            if(ll_node) return forward_iterator(this, avl_node, ll_node);
            avl_node = hashavl_inorder_next(avl_node);
        }
        return end();
    }
    forward_iterator end() { return forward_iterator(this, nullptr, nullptr); }

    template <typename Func, typename... Args>
    void ForEach(Func func, Args&&... args) {
        unique_lock<shared_mutex> lock(m_mtx);
        if(m_size == 0) return;
        for(auto& entry : *this)
            func(entry, forward<Args>(args)...);
    }

    friend ostream& operator<<(ostream& os, const HashAVL& hash_avl) {
        shared_lock<shared_mutex> lock(hash_avl.m_mtx);
        os << "[";
        for (auto it = hash_avl.m_avl->begin(); !(it == hash_avl.m_avl->end()); ++it)
            os << *it;          
        return os << "]";
    }

    friend istream& operator>>(istream& is, HashAVL& hash_avl) {
        Char ch;
        if(!(is >> ch) || ch != '[') {
            is.clear(ios_base::failbit);
            return is;
        }
        K    key;
        V    value;
        Char comma, parenClose;
        while(is >> ch && ch != ']') {
            if(ch == '(') {
                if(is >> key >> comma >> value >> parenClose) {
                    if(comma == ',' && parenClose == ')')
                        hash_avl[key] = value;
                }
            }   
        }
        return is;
    }
};


// busca el bucket en el AVL por hash_val
template <typename K, typename V>
typename HashAVL<K, V>::mapped_type&
HashAVL<K, V>::operator[](const key_type& key) {
    unique_lock<shared_mutex> lock(m_mtx);
    size_t hash_val = m_hash(key);
    Bucket search_bucket(hash_val);
    AVLNodeType* avl_node = m_avl->find(search_bucket);
    if(!avl_node) {
        m_avl->insert(search_bucket, 0);
        avl_node = m_avl->find(search_bucket);
    }
    auto& chain = avl_node->getDataRef().chain;
    for(auto chain_it = chain.begin(); !(chain_it == chain.end()); ++chain_it) {
        if((*chain_it).first == key)
            return (*chain_it).second;
    }
    chain.push_back(value_type(key, mapped_type()), 0);
    ++m_size;
    for(auto chain_it = chain.begin(); !(chain_it == chain.end()); ++chain_it) {
        if((*chain_it).first == key)
            return (*chain_it).second;
    }
}



#endif
