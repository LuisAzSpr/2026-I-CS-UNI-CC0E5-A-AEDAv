#ifndef __BTREE_H__
#define __BTREE_H__

#include <iostream>
#include <sstream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include "BTreePage.h"
#include "../types.h"
using namespace std;

constexpr size_t DEFAULT_BTREE_ORDER = 3;


// Iterador (forward y backward)
template <typename Container, int Direction>
class btree_iterator {
public:
    using value_type = typename Container::value_type;
    using EntryVec   = vector<value_type*>;

private:
    shared_ptr<EntryVec> m_entries;
    size_t               m_pos;
    size_t               m_size;

public:
    btree_iterator() : m_pos(0), m_size(0) {}
    btree_iterator(shared_ptr<EntryVec> entries, size_t pos, size_t size)
        : m_entries(entries), m_pos(pos), m_size(size) {}

    value_type& operator*() { return *(*m_entries)[m_pos]; }

    btree_iterator& operator++() { ++m_pos; return *this; }

    bool operator==(const btree_iterator& other) const {
        bool aEnd = (m_pos >= m_size);
        bool bEnd = (other.m_pos >= other.m_size);
        if (aEnd && bEnd) return true;
        if (aEnd != bEnd) return false;
        return m_entries == other.m_entries && m_pos == other.m_pos;
    }
};


// BTree<Trait>
template <typename Trait>
class BTree {
public:
    using key_type   = typename Trait::key_type;
    using value_type = typename Trait::value_type;
    using PageType   = BTreePage<Trait>;
    using MySelf     = BTree<Trait>;

    using forward_iterator  = btree_iterator<MySelf, +1>;
    using backward_iterator = btree_iterator<MySelf, -1>;

protected:
    PageType              m_Root;
    size_t                m_Height;
    size_t                m_Order;
    size_t                m_NumKeys;
    bool                  m_Unique;
    mutable shared_mutex  m_mtx;

    // Construye el snapshot recorriendo el arbol via Traverse (el unico bucle)
    template <int Direction>
    shared_ptr<vector<value_type*>> snapshot() {
        auto vec = make_shared<vector<value_type*>>();
        auto collector = [vec](value_type& info, size_t /*lv*/) -> bool {
            vec->push_back(&info);
            return false;
        };
        m_Root.Traverse(collector, 0, false, Direction);
        return vec;
    }

public:
    BTree(size_t order = DEFAULT_BTREE_ORDER, bool unique = true);
    virtual ~BTree() {}

    bool   Insert(const key_type& key, Ref ObjID);
    bool   Remove(const key_type& key, Ref ObjID);
    Ref    Search(const key_type& key);

    size_t size()     const { shared_lock<shared_mutex> lock(m_mtx); return m_NumKeys; }
    size_t height()   const { shared_lock<shared_mutex> lock(m_mtx); return m_Height;  }
    size_t GetOrder() const { shared_lock<shared_mutex> lock(m_mtx); return m_Order;   }

    forward_iterator  begin()  {
        shared_lock<shared_mutex> lock(m_mtx);
        auto vec = snapshot<+1>();
        return forward_iterator(vec, 0, vec->size());
    }
    forward_iterator  end()    { return forward_iterator(); }

    backward_iterator rbegin() {
        shared_lock<shared_mutex> lock(m_mtx);
        auto vec = snapshot<-1>();
        return backward_iterator(vec, 0, vec->size());
    }
    backward_iterator rend()   { return backward_iterator(); }

    // ForEach y FirstThat
    template <typename Func, typename... Args>
    void ForEach(Func func, Args&&... args) {
        shared_lock<shared_mutex> lock(m_mtx);
        m_Root.ForEach(func, 0, forward<Args>(args)...);
    }

    template <typename Func, typename... Args>
    value_type* FirstThat(Func func, Args&&... args) {
        shared_lock<shared_mutex> lock(m_mtx);
        return m_Root.FirstThat(func, 0, forward<Args>(args)...);
    }

    // operator<<
    friend ostream& operator<<(ostream& os, BTree& bt) {
        shared_lock<shared_mutex> lock(bt.m_mtx);
        bt.m_Root.ForEach([](value_type& info, size_t level, ostream& out) {
            for (size_t i = 0; i < level; ++i) out << "\t";
            out << info.key << "->" << info.ObjID << "\n";
        }, 0, os);
        return os;
    }

    // operator>>
    friend istream& operator>>(istream& is, BTree& bt) {
        Char ch;
        key_type key;
        Ref      objID;
        Char     comma, parenClose;
        while (is >> ch) {
            if (ch == '(') {
                if (is >> key >> comma >> objID >> parenClose) {
                    if (comma == ',' && parenClose == ')')
                        bt.Insert(key, objID);
                }
            }
        }
        return is;
    }
};


// El root tiene capacidad 2*order+1 internamente
template <typename Trait>
BTree<Trait>::BTree(size_t order, bool unique)
    : m_Root(2 * order + 1, unique),
      m_Height(1),
      m_Order(order),
      m_NumKeys(0),
      m_Unique(unique)
{
    m_Root.SetMaxKeysForChilds(order);
}

template <typename Trait>
bool BTree<Trait>::Insert(const key_type& key, Ref ObjID) {
    unique_lock<shared_mutex> lock(m_mtx);
    bt_ErrorCode error = m_Root.Insert(key, ObjID);
    if (error == bt_duplicate) return false;
    m_NumKeys++;
    // La altura solo aumenta cuando el root se divide
    if (error == bt_overflow) {
        m_Root.SplitRoot();
        m_Height++;
    }
    return true;
}

template <typename Trait>
bool BTree<Trait>::Remove(const key_type& key, Ref ObjID) {
    unique_lock<shared_mutex> lock(m_mtx);
    bt_ErrorCode error = m_Root.Remove(key, ObjID);
    if (error == bt_nofound) return false;
    m_NumKeys--;
    if (error == bt_rootmerged) m_Height--;
    return true;
}

template <typename Trait>
Ref BTree<Trait>::Search(const key_type& key) {
    shared_lock<shared_mutex> lock(m_mtx);
    Ref ObjID = -1;
    m_Root.Search(key, ObjID);
    return ObjID;
}

#endif
