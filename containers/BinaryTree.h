#ifndef __BINARYTREE_H__
#define __BINARYTREE_H__

#include <iostream>
#include <cstddef>
#include <string>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <tuple>
#include "general_iterator.h"
#include "bt_iterators.h"
#include "util.h"
#include "../types.h"
#include "traits.h"
using namespace std;

// Binary Tree Node
template <typename T>
class BinaryTreeNode{
public:
    using value_type = T;
    using Node       = BinaryTreeNode<T>;
private:
    T     m_data;
    Ref   m_ref;
    Node *m_pParent;
    Node *m_pChild[2]; // 0 = left, 1 = right
public:
    BinaryTreeNode() : m_data(T()), m_ref(Ref()), m_pParent(nullptr), m_pChild{nullptr, nullptr} {}
    BinaryTreeNode(T data, Ref ref) : m_data(data), m_ref(ref), m_pParent(nullptr), m_pChild{nullptr, nullptr} {}
    BinaryTreeNode(T data, Ref ref, Node *parent) : m_data(data), m_ref(ref), m_pParent(parent), m_pChild{nullptr, nullptr} {}
    virtual ~BinaryTreeNode() {}

    T      getData() const                  { return m_data; }
    T&     getDataRef()                     { return m_data; }
    void   setData(T data)                  { m_data = data; }
    Ref    getRef() const                   { return m_ref; }
    void   setRef(Ref ref)                  { m_ref = ref; }

    Node*  getChild(size_t branch) const    { return m_pChild[branch]; }
    Node*& getChildRef(size_t branch)       { return m_pChild[branch]; }
    void   setChild(size_t branch, Node *c) { m_pChild[branch] = c; }
    Node*  getLeft()  const                 { return m_pChild[0]; }
    Node*  getRight() const                 { return m_pChild[1]; }

    Node*  getParent() const                { return m_pParent; }
    void   setParent(Node *parent)          { m_pParent = parent; }
};

// Traits de Ordenamiento
template <typename T>
struct AscendingBTTrait : public BaseTrait<BinaryTreeNode<T>, less<T>>{
};

template <typename T>
struct DescendingBTTrait : public BaseTrait<BinaryTreeNode<T>, greater<T>>{
};


// BinaryTree
template <typename Trait>
class BinaryTree{
public:
    using value_type = typename Trait::value_type;
    using Node       = typename Trait::Node;
    using Comp       = typename Trait::Comp;
    using MySelf     = BinaryTree<Trait>;

    using inorder_iterator             = bt_inorder_iterator<MySelf, 1>;
    using inorder_reverse_iterator     = bt_inorder_iterator<MySelf, 0>;
    using preorder_iterator            = bt_descend_iterator<MySelf, 1>;
    using preorder_reverse_iterator    = bt_ascend_iterator <MySelf, 0>;
    using postorder_iterator           = bt_ascend_iterator <MySelf, 1>;
    using postorder_reverse_iterator   = bt_descend_iterator<MySelf, 0>;

    // begin/end por defecto = inorder forward
    using forward_iterator             = inorder_iterator;
    using backward_iterator            = inorder_reverse_iterator;

protected:
    Node   *m_pRoot = nullptr;
    size_t  m_size  = 0;
    Comp    m_comp;
    mutable shared_mutex m_mtx;

    virtual void  internal_insert(Node *&pNode, Node *parent, const value_type &value, Ref ref);
    virtual Node* internal_clone(Node *src, Node *parent);
    void  internal_destroy(Node *node);
    void  internal_dump(ostream &os, Node *node, bool &first) const;

public:
    BinaryTree() {}

    // Copy Constructor
    BinaryTree(const BinaryTree &other) {
        shared_lock<shared_mutex> lock(other.m_mtx);
        m_pRoot = internal_clone(other.m_pRoot, nullptr);
        m_size  = other.m_size;
    }


    // Move Constructor
    BinaryTree(BinaryTree &&other) {
        unique_lock<shared_mutex> lock(other.m_mtx);
        m_pRoot = std::exchange(other.m_pRoot, nullptr);
        m_size  = std::exchange(other.m_size,  0);
    }


    // Copy Assignment
    BinaryTree& operator=(const BinaryTree &other) {
        if (this != &other) {
            unique_lock<shared_mutex> lockMe(m_mtx);
            internal_destroy(m_pRoot);
            m_pRoot = nullptr;
            m_size  = 0;
            shared_lock<shared_mutex> lockOther(other.m_mtx);
            m_pRoot = internal_clone(other.m_pRoot, nullptr);
            m_size  = other.m_size;
        }
        return *this;
    }

    // Move Assignment
    BinaryTree& operator=(BinaryTree &&other) {
        if (this != &other) {
            unique_lock<shared_mutex> lockMe(m_mtx);
            internal_destroy(m_pRoot);
            unique_lock<shared_mutex> lockOther(other.m_mtx);
            m_pRoot = std::exchange(other.m_pRoot, nullptr);
            m_size  = std::exchange(other.m_size,  0);
        }
        return *this;
    }

    // Destructor Seguro
    virtual ~BinaryTree() {
        unique_lock<shared_mutex> lock(m_mtx);
        internal_destroy(m_pRoot);
        m_pRoot = nullptr;
        m_size  = 0;
    }

    // Operaciones
    virtual void   insert(const value_type &value, Ref ref);
    virtual Node*  find(const value_type &value) const;
    virtual size_t size() const;

    // Iteradores
    inorder_iterator begin() {
        Node *current = m_pRoot;
        while (current && current->getLeft()) {
            current = current->getLeft();
        }
        return inorder_iterator(this, current);
    }
    inorder_iterator end() { return inorder_iterator(this, nullptr); }

    inorder_reverse_iterator rbegin() {
        Node *current = m_pRoot;
        while (current && current->getRight()) {
            current = current->getRight();
        }
        return inorder_reverse_iterator(this, current);
    }
    inorder_reverse_iterator rend() { return inorder_reverse_iterator(this, nullptr); }

    preorder_iterator pre_begin() { return preorder_iterator(this, m_pRoot); }
    preorder_iterator pre_end()   { return preorder_iterator(this, nullptr); }

    preorder_reverse_iterator pre_rbegin() {
        Node *current = m_pRoot;
        while (current && (current->getLeft() || current->getRight())) {
            current = current->getRight() ? current->getRight() : current->getLeft();
        }
        return preorder_reverse_iterator(this, current);
    }
    preorder_reverse_iterator pre_rend() { return preorder_reverse_iterator(this, nullptr); }

    postorder_iterator post_begin() {
        Node *current = m_pRoot;
        while (current && (current->getLeft() || current->getRight())) {
            current = current->getLeft() ? current->getLeft() : current->getRight();
        }
        return postorder_iterator(this, current);
    }
    postorder_iterator post_end() { return postorder_iterator(this, nullptr); }

    postorder_reverse_iterator post_rbegin() { return postorder_reverse_iterator(this, m_pRoot); }
    postorder_reverse_iterator post_rend()   { return postorder_reverse_iterator(this, nullptr); }

    preorder_view         <MySelf> preorder()          { return {this}; }
    preorder_reverse_view <MySelf> preorder_reverse()  { return {this}; }
    postorder_view        <MySelf> postorder()         { return {this}; }
    postorder_reverse_view<MySelf> postorder_reverse() { return {this}; }
    reverse_view          <MySelf> reverse()           { return {this}; }


    // ForEach  begin()/end()
    template <typename Func, typename... Args>
    void ForEach(Func func, Args &&... args) {
        unique_lock<shared_mutex> lock(m_mtx);
        if (m_size == 0) return;
        for (auto& item : *this) {
            func(item, std::forward<Args>(args)...);
        }
    }

    // Operadores I/O
    friend ostream& operator<<(ostream& os, const BinaryTree& tree){
        shared_lock<shared_mutex> lock(tree.m_mtx);
        os << "[";
        bool first = true;
        tree.internal_dump(os, tree.m_pRoot, first);
        os << "]";
        return os;
    }

    friend istream& operator>>(istream& is, BinaryTree& tree) {
        Char ch;
        if (!(is >> ch) || ch != '['){
            is.clear(ios_base::failbit);
            return is;
        }
        value_type val;
        Ref ref;
        Char comma, parenClose;
        while (is >> ch && ch != ']'){
            if (ch == '(') {
                if (is >> val >> comma >> ref >> parenClose) {
                    if (comma == ',' && parenClose == ')') {
                        tree.insert(val, ref);
                    }
                }
            }
        }
        return is;
    }
};

// Implementacion de Metodos del Arbol
template <typename Trait>
void BinaryTree<Trait>::internal_insert(Node *&pNode, Node *parent, const value_type &value, Ref ref){
    if (pNode == nullptr) {
        pNode = new Node(value, ref, parent);
        m_size++;
        return;
    }
    auto branch = !m_comp(value, pNode->getData());
    internal_insert(pNode->getChildRef(branch), pNode, value, ref);
}

template <typename Trait>
void BinaryTree<Trait>::insert(const value_type &value, Ref ref) {
    unique_lock<shared_mutex> lock(m_mtx);
    internal_insert(m_pRoot, nullptr, value, ref);
}

template <typename Trait>
size_t BinaryTree<Trait>::size() const {
    shared_lock<shared_mutex> lock(m_mtx);
    return m_size;
}

// find: busca un valor usando el comparador del trait
template <typename Trait>
typename BinaryTree<Trait>::Node*
BinaryTree<Trait>::find(const value_type &value) const {
    shared_lock<shared_mutex> lock(m_mtx);
    Node *curr = m_pRoot;
    while(curr){
        if(m_comp(value, curr->getData()))      curr = curr->getLeft();
        else if(m_comp(curr->getData(), value)) curr = curr->getRight();
        else                                    return curr;
    }
    return nullptr;
}


template <typename Trait>
typename BinaryTree<Trait>::Node*
BinaryTree<Trait>::internal_clone(Node *src, Node *parent) {
    if (!src) return nullptr;
    Node *cloned = new Node(src->getData(), src->getRef(), parent);
    cloned->setChild(0, internal_clone(src->getLeft(),  cloned));
    cloned->setChild(1, internal_clone(src->getRight(), cloned));
    return cloned;
}

template <typename Trait>
void BinaryTree<Trait>::internal_destroy(Node *node) {
    if (!node) return;
    internal_destroy(node->getLeft());
    internal_destroy(node->getRight());
    delete node;
}

template <typename Trait>
void BinaryTree<Trait>::internal_dump(ostream &os, Node *node, bool &first) const {
    if (!node) return;
    internal_dump(os, node->getLeft(), first);
    if (!first) os << ",";
    os << "(" << node->getData() << "," << node->getRef() << ")";
    first = false;
    internal_dump(os, node->getRight(), first);
}

#endif

