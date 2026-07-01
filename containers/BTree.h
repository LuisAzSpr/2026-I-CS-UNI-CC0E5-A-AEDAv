#ifndef __BTREE_H__
#define __BTREE_H__

#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include "BTreePage.h"
#include "general_iterator.h"
#include "../types.h"
using namespace std;

#define DEFAULT_BTREE_ORDER 3


// Iterador (Forward=true / Backward=false).
template <typename Container, bool Forward>
class btree_iterator : public general_iterator<Container, btree_iterator<Container, Forward>> {
public:
    using MySelf     = btree_iterator<Container, Forward>;
    using Parent     = general_iterator<Container, MySelf>;
    using value_type = typename Container::value_type;
    using EntryVec   = vector<value_type*>;

private:
    EntryVec m_entries;
    size_t   m_pos;

public:
    btree_iterator() : Parent(nullptr, nullptr), m_pos(0) {}

    btree_iterator(Container* cont, EntryVec entries, size_t pos)
        : Parent(cont, nullptr), m_entries(entries), m_pos(pos)
    {
        if (m_pos < m_entries.size())
            this->m_pNode = m_entries[m_pos];
    }

    MySelf operator++() {
        ++m_pos;
        this->m_pNode = (m_pos < m_entries.size()) ? m_entries[m_pos] : nullptr;
        return *this;
    }

    // 
    value_type& operator*() { return *(this->m_pNode); }
};


// BTree<Trait>
template <typename Trait>
class BTree {
public:
    using key_type   = typename Trait::key_type;
    using value_type = typename Trait::value_type;
    using Node       = value_type;              // requerido por general_iterator
    using PageType   = BTreePage<Trait>;
    using MySelf     = BTree<Trait>;

    using forward_iterator  = btree_iterator<MySelf, true>;
    using backward_iterator = btree_iterator<MySelf, false>;
    friend forward_iterator;
    friend backward_iterator;

protected:
    PageType              m_Root;
    size_t                m_Height;
    size_t                m_Order;
    size_t                m_NumKeys;
    bool                  m_Unique;
    mutable shared_mutex  m_mtx;

    // Construye el snapshot recorriendo el arbol via Collect
    template <bool Forward>
    vector<value_type*> snapshot() {
        vector<value_type*> vec;
        m_Root.Collect(vec, Forward);
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

    // begin, end, rbegin y rend NO toman lock: quien itera es responsable
    forward_iterator  begin()  {
        auto vec = snapshot<true>();
        return forward_iterator(this, vec, 0);
    }
    forward_iterator  end()    { return forward_iterator(); }

    backward_iterator rbegin() {
        auto vec = snapshot<false>();
        return backward_iterator(this, vec, 0);
    }
    backward_iterator rend()   { return backward_iterator(); }

    // ForEach y FirstThat: usan el forward iterator (mismo bucle range-for).
    template <typename Func, typename... Args>
    void ForEach(Func func, Args&&... args) {
        shared_lock<shared_mutex> lock(m_mtx);
        if (m_NumKeys == 0) return;
        for (auto& item : *this) {
            func(item, forward<Args>(args)...);
        }
    }

    template <typename Func, typename... Args>
    value_type* FirstThat(Func func, Args&&... args) {
        shared_lock<shared_mutex> lock(m_mtx);
        for (auto& item : *this) {
            if (func(item, forward<Args>(args)...))
                return &item;
        }
        return nullptr;
    }

    // ReverseForEach y ReverseFirstThat: usan el backward iterator.
    template <typename Func, typename... Args>
    void ReverseForEach(Func func, Args&&... args) {
        shared_lock<shared_mutex> lock(m_mtx);
        if (m_NumKeys == 0) return;
        for (auto it = rbegin(); !(it == rend()); ++it) {
            func(*it, forward<Args>(args)...);
        }
    }

    template <typename Func, typename... Args>
    value_type* ReverseFirstThat(Func func, Args&&... args) {
        shared_lock<shared_mutex> lock(m_mtx);
        for (auto it = rbegin(); !(it == rend()); ++it) {
            if (func(*it, forward<Args>(args)...))
                return &(*it);
        }
        return nullptr;
    }

    // operator<<
    friend ostream& operator<<(ostream& os, BTree& bt) {
        bt.ForEach([&os](value_type& info) {
            os << info.key << "->" << info.ObjID << "\n";
        });
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
