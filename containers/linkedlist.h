#ifndef __LINKEDLIST_H__
#define __LINKEDLIST_H__

#include <iostream>
#include <cstddef> // size_t
#include <string>
#include <sstream>
#include <shared_mutex> // shared_mutex
#include "general_iterator.h"
#include "util.h"
#include "../types.h"
using namespace std;

// Forward iterator
template <typename Container>
class LinkedListForwardIterator : public general_iterator<Container, LinkedListForwardIterator<Container>>{
    using MySelf = LinkedListForwardIterator<Container>;
    using Parent = general_iterator<Container, MySelf>;
    using Parent::Parent;

    // TODO: Completar el operator++
    MySelf operator++() {
        this->m_pNode = this->m_pNode->getNext();
        return *this;
    }
};

// Linked List Node
template <typename T>
class LLNode{
    using Node = LLNode<T>;
private:
    T   m_data;
    Node *m_next;
public:
    LLNode() : m_data(T()), m_next(nullptr) {}
    LLNode(T data) : m_data(data), m_next(nullptr) {}
    LLNode(T data, Node *next) : m_data(data), m_next(next) {}
    virtual ~LLNode() {}

    T      getData() const { return m_data; }
    T&     getDataRef()    { return m_data; }
    void   setData(T data) { m_data = data; }
    Node*  getNext() const { return m_next; }
    Node*& getNextRef()    { return m_next; }
    void   setNext(Node *next) { m_next = next; }
};

template <typename T>
struct AscendingLinkedListTrait{
    using value_type = T;
    using Node = LLNode<T>;
    using Comp = less<T>;
};

template <typename T>
struct DescendingLinkedListTrait{
    using value_type = T;
    using Node = LLNode<T>;
    using Comp = greater<T>;
};

template <typename Trait>
class LinkedList{
public:
    using value_type = typename Trait::value_type;
    using Node       = typename Trait::Node;
    using Comp       = typename Trait::Comp;
    using MySelf     = LinkedList<Trait>;

    using forward_iterator = LinkedListForwardIterator<MySelf>;
    // friend forward_iterator;

private:
    Node *m_pRoot = nullptr;
    Node *m_tail = nullptr;
    size_t m_size = 0;
    Comp   m_comp;
    mutable shared_mutex m_mtx;
public:
    LinkedList() {}
    LinkedList(const LinkedList &other){ // Copy constructor
        Node *cur = other.m_pRoot;
        while (cur) {
            Node *newNode = new Node(cur->getData());
            if (!m_pRoot) {
                m_pRoot = m_tail = newNode;
            } else {
                m_tail->setNext(newNode);
                m_tail = newNode;
            }
            ++m_size;
            cur = cur->getNext();
        }
    }
    LinkedList(LinkedList &&other) {   // Move constructor
        m_pRoot = other.m_pRoot;
        m_tail  = other.m_tail;
        m_size  = other.m_size;
        other.m_pRoot = nullptr;       // Dejar fuente en estado vacío válido
        other.m_tail  = nullptr;
        other.m_size  = 0;
    }
    
    LinkedList& operator=(const LinkedList &other){ // Copy assignment operator
    }
    LinkedList& operator=(LinkedList &&other){ // Move assignment operator
    }
    
    virtual ~LinkedList() {
        Node *cur = m_pRoot;
        while (cur) {
            Node *next = cur->getNext();
            delete cur;
            cur = next;
        }
        m_pRoot = m_tail = nullptr;
        m_size = 0;
    }

    virtual void    push_front(value_type value, Ref ref);
    virtual void    pop_front();
    virtual void    push_back(value_type value, Ref ref);
    virtual void    pop_back();
private:
            void    internal_insert(Node* &pParent, const value_type &value, Ref ref);
public:
    virtual void    insert(const value_type &value, Ref ref);
    
    virtual value_type& operator[](size_t index);
    virtual size_t  size() const;
    virtual string  toString() const;

    forward_iterator begin() { return forward_iterator(this, m_pRoot); }
    forward_iterator end()   { return forward_iterator(this, nullptr); }

    // Agregar Foreach
    template <typename Func, typename... Args>
    void ForEach(Func func, Args &&...  args){
        unique_lock<shared_mutex> lock(m_mtx);
        ::ForEach(begin(), end(), func, std::forward<Args>(args)... );
    }
};

template <typename Trait>
void LinkedList<Trait>::push_front(value_type value, Ref ref) {
    unique_lock<shared_mutex> lock(m_mtx);
    Node *newNode = new Node(value, ref, m_pRoot);
    m_pRoot = newNode;
    if (!m_tail)
        m_tail = m_pRoot;
    ++m_size;
}

template <typename Trait>
void LinkedList<Trait>::pop_front() {
    unique_lock<shared_mutex> lock(m_mtx);
    if (!m_pRoot) return;
    Node *toDelete = m_pRoot;
    m_pRoot = m_pRoot->getNext();
    if (!m_pRoot)
        m_tail = nullptr;
    delete toDelete;
    --m_size;
}

template <typename Trait>
void LinkedList<Trait>::push_back(value_type value, Ref ref) {
    unique_lock<shared_mutex> lock(m_mtx);
    Node *newNode = new Node(value, ref);
    if (!m_tail) {
        m_pRoot = m_tail = newNode;
    } else {
        m_tail->setNext(newNode);
        m_tail = newNode;
    }
    ++m_size;
}

template <typename Trait>
void LinkedList<Trait>::pop_back() {
    unique_lock<shared_mutex> lock(m_mtx);
    if (!m_pRoot) return;
    if (m_pRoot == m_tail) {
        delete m_pRoot;
        m_pRoot = m_tail = nullptr;
    } else {
        Node *prev = m_pRoot;
        while (prev->getNext() != m_tail)
            prev = prev->getNext();
        delete m_tail;
        m_tail = prev;
        m_tail->setNext(nullptr);
    }
    --m_size;
}

template <typename Trait>
typename LinkedList<Trait>::value_type& LinkedList<Trait>::operator[](size_t index) {
    shared_lock<shared_mutex> lock(m_mtx);
    Node *cur = m_pRoot;
    for (size_t i = 0; i < index && cur; ++i)
        cur = cur->getNext();
    return cur->getDataRef();
}

template <typename Trait>
string LinkedList<Trait>::toString() const {
    shared_lock<shared_mutex> lock(m_mtx);
    ostringstream oss;
    oss << "[";
    Node *cur = m_pRoot;
    bool first = true;
    while (cur) {
        if (!first) oss << ",";
        oss << cur->getData();
        first = false;
        cur = cur->getNext();
    }
    oss << "]";
    return oss.str();
}

// ─── operator<< / operator>> ────────────────────────────────────────
template <typename Trait>
ostream& operator<<(ostream &os, const LinkedList<Trait> &list) {
    return os << list.toString();
}

template <typename T>
void LinkedList<T>::internal_insert(Node* &pPr  ev, const value_type &value, Ref ref){
    if(!pPrev || m_comp(value, pPrev->getDataRef())){
        pPrev = new Node(value, ref, pPrev);
        m_size++;
        if(pPrev == m_pRoot)
            m_tail = pPrev;
        return;
    }
    internal_insert(pPrev->getNextRef(), value, ref);
}



template <typename T>
void LinkedList<T>::insert(const value_type &value, Ref ref){
    internal_insert(m_pRoot, value, ref);
}



#endif // __LINKEDLIST_H__