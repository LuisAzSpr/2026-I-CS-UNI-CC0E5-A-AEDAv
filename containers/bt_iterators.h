#ifndef __BT_ITERATORS_H__
#define __BT_ITERATORS_H__

#include "general_iterator.h"

// D=1 forward / D=0 backward
template <typename Container, size_t D>
class bt_inorder_iterator : public general_iterator<Container, bt_inorder_iterator<Container, D>>{
public:
    using MySelf = bt_inorder_iterator<Container, D>;
    using Parent = general_iterator<Container, MySelf>;
    using Node   = typename Container::Node;
    using Parent::Parent;

    MySelf operator++() {
        Node *current = this->m_pNode;
        if (!current) return *this;

        if (current->getChild(D)) {
            // Bajar a hijo D, luego todo el camino por 1-D
            current = current->getChild(D);
            while (current->getChild(1-D)) {
                current = current->getChild(1-D);
            }
        } else {
            // Subir mientras sea hijo D del padre
            Node *parent = current->getParent();
            while (parent && current == parent->getChild(D)) {
                current = parent;
                parent  = parent->getParent();
            }
            current = parent;
        }
        this->m_pNode = current;
        return *this;
    }
};


//  D=1 preorder forward / D=0 postorder backward
template <typename Container, size_t D>
class bt_descend_iterator : public general_iterator<Container, bt_descend_iterator<Container, D>>{
public:
    using MySelf = bt_descend_iterator<Container, D>;
    using Parent = general_iterator<Container, MySelf>;
    using Node   = typename Container::Node;
    using Parent::Parent;

    MySelf operator++() {
        Node *current = this->m_pNode;
        if (!current) return *this;

        // Bajar al primer hijo no nulo en el orden (1-D, D)
        if (current->getChild(1-D)) {
            this->m_pNode = current->getChild(1-D);
            return *this;
        }
        if (current->getChild(D)) {
            this->m_pNode = current->getChild(D);
            return *this;
        }

        // Subir buscando un hermano D no visitado
        Node *parent = current->getParent();
        while (parent) {
            if (current == parent->getChild(1-D) && parent->getChild(D)) {
                this->m_pNode = parent->getChild(D);
                return *this;
            }
            current = parent;
            parent  = parent->getParent();
        }
        this->m_pNode = nullptr;
        return *this;
    }
};


// D=1 postorder forward / D=0 preorder backward
template <typename Container, size_t D>
class bt_ascend_iterator : public general_iterator<Container, bt_ascend_iterator<Container, D>>{
public:
    using MySelf = bt_ascend_iterator<Container, D>;
    using Parent = general_iterator<Container, MySelf>;
    using Node   = typename Container::Node;
    using Parent::Parent;

    MySelf operator++() {
        Node *current = this->m_pNode;
        if (!current) return *this;

        Node *parent = current->getParent();
        if (!parent) {
            this->m_pNode = nullptr;
            return *this;
        }

        if (current == parent->getChild(1-D) && parent->getChild(D)) {
            // Bajar al nodo mas profundo del subarbol D prefiriendo la rama 1-D
            Node *deepest = parent->getChild(D);
            while (deepest->getChild(0) || deepest->getChild(1)) {
                if (deepest->getChild(1-D)) {
                    deepest = deepest->getChild(1-D);
                } else {
                    deepest = deepest->getChild(D);
                }
            }
            this->m_pNode = deepest;
        } else {
            this->m_pNode = parent;
        }
        return *this;
    }
};

// Views
template <typename Tree>
struct preorder_view {
    Tree *m_tree;
    auto begin() { return m_tree->pre_begin(); }
    auto end()   { return m_tree->pre_end(); }
};

template <typename Tree>
struct preorder_reverse_view {
    Tree *m_tree;
    auto begin() { return m_tree->pre_rbegin(); }
    auto end()   { return m_tree->pre_rend(); }
};

template <typename Tree>
struct postorder_view {
    Tree *m_tree;
    auto begin() { return m_tree->post_begin(); }
    auto end()   { return m_tree->post_end(); }
};

template <typename Tree>
struct postorder_reverse_view {
    Tree *m_tree;
    auto begin() { return m_tree->post_rbegin(); }
    auto end()   { return m_tree->post_rend(); }
};

template <typename Tree>
struct reverse_view {
    Tree *m_tree;
    auto begin() { return m_tree->rbegin(); }
    auto end()   { return m_tree->rend(); }
};

#endif // __BT_ITERATORS_H__
