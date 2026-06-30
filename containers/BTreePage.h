#ifndef __BTREE_PAGE_H__
#define __BTREE_PAGE_H__

#include <iostream>
#include <vector>
#include <functional>
#include <cassert>
#include <utility>
#include "../types.h"
using namespace std;

enum bt_ErrorCode { bt_ok, bt_overflow, bt_underflow, bt_duplicate, bt_nofound, bt_rootmerged };

// ObjectInfo (anidado conceptualmente al BTreePage, parametrizado solo por Key)
template <typename Key>
struct BTreeObjectInfo {
    Key  key;
    Ref  ObjID;
    Ref  UseCounter;

    BTreeObjectInfo() : key(Key()), ObjID(Ref()), UseCounter(0) {}
    BTreeObjectInfo(const Key& k, Ref id) : key(k), ObjID(id), UseCounter(0) {}

    operator Key() const { return key; }
    Ref GetUseCounter() const { return UseCounter; }
};

// Traits con Comp flexible
template <typename Key, typename _Comp = less<Key>>
struct AscendingBTreeTrait {
    using key_type   = Key;
    using value_type = BTreeObjectInfo<Key>;
    using Comp       = _Comp;
};

template <typename Key, typename _Comp = greater<Key>>
struct DescendingBTreeTrait {
    using key_type   = Key;
    using value_type = BTreeObjectInfo<Key>;
    using Comp       = _Comp;
};

// Helpers libres (binary_search dice donde DEBERIA estar si no lo encuentra)
template <typename Container, typename ObjType, typename Comp>
size_t bt_binary_search(Container& container, size_t first, size_t last, const ObjType& object, Comp& comp) {
    if (first >= last) return first;
    while (first < last) {
        size_t mid = (first + last) / 2;
        ObjType midval = static_cast<ObjType>(container[mid]);
        if (!comp(object, midval) && !comp(midval, object)) return mid;
        if (comp(midval, object)) first = mid + 1;
        else                       last  = mid;
    }
    if (!comp(static_cast<ObjType>(container[first]), object)) return first;
    return last;
}

template <typename Container, typename ObjType>
void bt_insert_at(Container& container, const ObjType& object, size_t pos) {
    size_t size = container.size();
    for (size_t i = size - 1; i > pos; --i)
        container[i] = container[i - 1];
    container[pos] = object;
}

template <typename Container>
void bt_remove_at(Container& container, size_t pos) {
    size_t size = container.size();
    for (size_t i = pos + 1; i < size; ++i)
        container[i - 1] = container[i];
}


// BTreePage<Trait>
template <typename Trait>
class BTreePage {
public:
    using key_type   = typename Trait::key_type;
    using value_type = typename Trait::value_type;
    using Comp       = typename Trait::Comp;
    using MySelf     = BTreePage<Trait>;
    using PagePtr    = MySelf*;

    template <typename T> friend class BTree;

protected:
    size_t              m_MinKeys;
    size_t              m_MaxKeys;
    size_t              m_MaxKeysForChilds;
    bool                m_Unique;
    vector<value_type>  m_Keys;
    vector<PagePtr>     m_SubPages;
    size_t              m_KeyCount;
    Comp                m_comp;

public:
    BTreePage(size_t maxKeys, bool unique = true);
    virtual ~BTreePage();

    bt_ErrorCode  Insert(const key_type& key, Ref ObjID);
    bt_ErrorCode  Remove(const key_type& key, Ref ObjID);
    bool          Search(const key_type& key, Ref& ObjID);

    // ForEach y FirstThat como Variadic Templates
    // Unificados: ambos delegan en Traverse (UN SOLO BUCLE recursivo).
    template <typename Func, typename... Args>
    void ForEach(Func func, size_t level, Args&&... args);

    template <typename Func, typename... Args>
    value_type* FirstThat(Func func, size_t level, Args&&... args);

protected:
    template <typename Func, typename... Args>
    value_type* Traverse(Func func, size_t level, bool stop_on_true, int direction, Args&&... args);

    void  Create();
    void  Reset();
    void  Destroy() { Reset(); delete this; }
    void  clear()   { m_KeyCount = 0; }

    bool  Redistribute1(size_t& pos);
    bool  Redistribute2(size_t pos);
    void  RedistributeR2L(size_t pos);
    void  RedistributeL2R(size_t pos);

    bool  TreatUnderflow(size_t& pos) { return Redistribute1(pos) || Redistribute2(pos); }

    bt_ErrorCode  Merge(size_t pos);
    bt_ErrorCode  MergeRoot();
    void          SplitChild(size_t pos);

    value_type&   GetFirstObjectInfo();

    bool   Overflow()           { return m_KeyCount > m_MaxKeys; }
    bool   Underflow()          { return m_KeyCount < MinNumberOfKeys(); }
    bool   IsFull()             { return m_KeyCount >= m_MaxKeys; }
    size_t MinNumberOfKeys()    { return 2 * m_MaxKeys / 3; }
    size_t GetFreeCells()       { return m_MaxKeys - m_KeyCount; }
    size_t& NumberOfKeys()      { return m_KeyCount; }
    size_t GetNumberOfKeys()    { return m_KeyCount; }
    bool   IsRoot()             { return m_MaxKeysForChilds != m_MaxKeys; }
    void   SetMaxKeysForChilds(size_t orderforchilds) { m_MaxKeysForChilds = orderforchilds; }

    size_t GetFreeCellsOnLeft(size_t pos);
    size_t GetFreeCellsOnRight(size_t pos);

private:
    bool SplitRoot();
    void SplitPageInto3(vector<value_type>&  tmpKeys,
                        vector<PagePtr>&     tmpSubPages,
                        PagePtr&             pChild1,
                        PagePtr&             pChild2,
                        PagePtr&             pChild3,
                        value_type&          oi1,
                        value_type&          oi2);
    void MovePage(PagePtr pChildPage,
                  vector<value_type>& tmpKeys,
                  vector<PagePtr>&    tmpSubPages);
};


template <typename Trait>
BTreePage<Trait>::BTreePage(size_t maxKeys, bool unique)
    : m_MaxKeys(maxKeys), m_Unique(unique), m_KeyCount(0)
{
    Create();
    SetMaxKeysForChilds(m_MaxKeys);
}

template <typename Trait>
BTreePage<Trait>::~BTreePage() { Reset(); }

template <typename Trait>
void BTreePage<Trait>::Create() {
    Reset();
    m_Keys.resize(m_MaxKeys + 1);
    m_SubPages.resize(m_MaxKeys + 2, nullptr);
    m_KeyCount = 0;
    m_MinKeys  = 2 * m_MaxKeys / 3;
}

template <typename Trait>
void BTreePage<Trait>::Reset() {
    for (size_t i = 0; i < m_KeyCount; ++i)
        delete m_SubPages[i];
    clear();
}


// Insert
template <typename Trait>
bt_ErrorCode BTreePage<Trait>::Insert(const key_type& key, Ref ObjID) {
    size_t pos = bt_binary_search(m_Keys, size_t(0), m_KeyCount, key, m_comp);
    bt_ErrorCode error = bt_ok;

    if (pos < m_KeyCount && !m_comp(static_cast<key_type>(m_Keys[pos]), key)
                         && !m_comp(key, static_cast<key_type>(m_Keys[pos]))
                         && m_Unique)
        return bt_duplicate;

    if (!m_SubPages[pos]) { // es hoja
        bt_insert_at(m_Keys, value_type(key, ObjID), pos);
        NumberOfKeys()++;
        if (Overflow()) return bt_overflow;
        return bt_ok;
    }

    // El padre resuelve el problema de los hijos
    error = m_SubPages[pos]->Insert(key, ObjID);
    if (error == bt_overflow) {
        if (!Redistribute1(pos))
            SplitChild(pos);
        if (Overflow()) return bt_overflow;
        return bt_ok;
    }
    return bt_ok;
}


template <typename Trait>
bool BTreePage<Trait>::Redistribute1(size_t& pos) {
    if (m_SubPages[pos]->Underflow()) {
        // nkol = Number of keys on left brother, nkor = Number of keys on right brother
        size_t nkol = 0,
               nkor = 0;
        // is this the first element or there are more elements on right brother
        if (pos > 0)
            nkol = m_SubPages[pos - 1]->NumberOfKeys();
        if (pos < NumberOfKeys())
            nkor = m_SubPages[pos + 1]->NumberOfKeys();

        if (nkol > nkor)
            if (m_SubPages[pos - 1]->NumberOfKeys() > m_SubPages[pos - 1]->MinNumberOfKeys())
                RedistributeL2R(pos - 1); // bring elements from left brother
            else
                if (pos == NumberOfKeys())
                    return (--pos, false);
                else
                    return false;
        else //nkol < nkor )
            if (m_SubPages[pos + 1]->NumberOfKeys() > m_SubPages[pos + 1]->MinNumberOfKeys())
                RedistributeR2L(pos + 1); // bring elements from right brother
            else
                if (pos == 0)
                    return (++pos, false);
                else
                    return false;
    }
    else // it is due to overflow
    {
        size_t fcol = GetFreeCellsOnLeft(pos),   // Free Cells On Left
               fcor = GetFreeCellsOnRight(pos);  // Free Cells On Right

        if (!fcol && !fcor && m_SubPages[pos]->IsFull())
            return false;
        if (fcol > fcor) // There is more space on left
            RedistributeR2L(pos);
        else
            RedistributeL2R(pos);
    }
    return true;
}

// Redistribute2 function
// it considers two brothers m_SubPages[pos-1] && m_SubPages[pos+1]
// if it fails the only way is merge !
template <typename Trait>
bool BTreePage<Trait>::Redistribute2(size_t pos) {
    assert(pos > 0 && pos < NumberOfKeys());
    assert(m_SubPages[pos - 1] != 0 && m_SubPages[pos] != 0 && m_SubPages[pos + 1] != 0);
    assert(m_SubPages[pos - 1]->Underflow() ||
           m_SubPages[ pos ]->Underflow() ||
           m_SubPages[pos + 1]->Underflow());

    if (m_SubPages[pos - 1]->Underflow())
    {       // Rotate R2L
        RedistributeR2L(pos + 1);
        RedistributeR2L(pos);
        if (m_SubPages[pos - 1]->Underflow())
            return false;
    }
    else if (m_SubPages[pos + 1]->Underflow())
    {       // Rotate L2R
        RedistributeL2R(pos - 1);
        RedistributeL2R(pos);
        if (m_SubPages[pos + 1]->Underflow())
            return false;
    }
    else // The problem is exactly at pos !
    {
        // Rotate L2R
        RedistributeL2R(pos - 1);
        RedistributeR2L(pos + 1);
        if (m_SubPages[pos]->Underflow())
            return false;
    }
    return true;
}


template <typename Trait>
void BTreePage<Trait>::RedistributeR2L(size_t pos) {
    PagePtr pSource = m_SubPages[ pos ],
            pTarget = m_SubPages[pos - 1];

    while (pSource->GetNumberOfKeys() > pSource->MinNumberOfKeys() &&
           pTarget->GetNumberOfKeys() < pSource->GetNumberOfKeys())
    {
        // Move from this page to the down-left page \/
        bt_insert_at(pTarget->m_Keys, m_Keys[pos - 1], pTarget->NumberOfKeys()++);
        // Move the pointer leftest pointer to the rightest position
        bt_insert_at(pTarget->m_SubPages, pSource->m_SubPages[0], pTarget->NumberOfKeys());

        // Move the leftest element to the root
        m_Keys[pos - 1] = pSource->m_Keys[0];

        // Remove the leftest element from rigth page
        bt_remove_at(pSource->m_Keys    , 0);
        bt_remove_at(pSource->m_SubPages, 0);
        pSource->NumberOfKeys()--;
    }
}

template <typename Trait>
void BTreePage<Trait>::RedistributeL2R(size_t pos) {
    PagePtr pSource = m_SubPages[pos],
            pTarget = m_SubPages[pos + 1];
    while (pSource->GetNumberOfKeys() > pSource->MinNumberOfKeys() &&
           pTarget->GetNumberOfKeys() < pSource->GetNumberOfKeys())
    {
        // Move from this page to the down-RIGHT page \/
        bt_insert_at(pTarget->m_Keys, m_Keys[pos], 0);
        // Move the pointer rightest pointer to the leftest position
        bt_insert_at(pTarget->m_SubPages, pSource->m_SubPages[pSource->NumberOfKeys()], 0);
        pTarget->NumberOfKeys()++;

        // Move the rightest element to the root
        m_Keys[pos] = pSource->m_Keys[pSource->NumberOfKeys() - 1];

        // Remove the leftest element from rigth page
        // it is not necessary erase because m_KeyCount controls
        pSource->NumberOfKeys()--;
    }
}

template <typename Trait>
void BTreePage<Trait>::SplitChild(size_t pos) {
    // FIRST: deciding the second page to split
    PagePtr pChild1 = nullptr, pChild2 = nullptr;
    if (pos > 0)                                  // is left page full ?
        if (m_SubPages[pos - 1]->IsFull())
        {
            pChild1 = m_SubPages[pos - 1];
            pChild2 = m_SubPages[pos--];
        }
    if (pos < GetNumberOfKeys())                  // is right page full ?
        if (m_SubPages[pos + 1]->IsFull())
        {
            pChild1 = m_SubPages[pos];
            pChild2 = m_SubPages[pos + 1];
        }

    // SECOND: copy both pages to a temporal one
    // Create two tmp vector
    vector<value_type> tmpKeys;
    vector<PagePtr>    tmpSubPages;

    // Prepara el vectpor unificado de las 2 paginas a ser divididas en 3
    // copy from left child
    MovePage(pChild1, tmpKeys, tmpSubPages);
    // copy a key from parent
    tmpKeys.push_back(m_Keys[pos]);

    // copy from right child
    MovePage(pChild2, tmpKeys, tmpSubPages);

    PagePtr pChild3 = nullptr;
    value_type oi1, oi2;
    SplitPageInto3(tmpKeys, tmpSubPages, pChild1, pChild2, pChild3, oi1, oi2);

    // copy the first element to the root
    m_Keys[pos]     = oi1;
    m_SubPages[pos] = pChild1;

    // copy the second element to the root
    bt_insert_at(m_Keys, oi2, pos + 1);
    bt_insert_at(m_SubPages, pChild2, pos + 1);
    NumberOfKeys()++;

    m_SubPages[pos + 2] = pChild3;
}

template <typename Trait>
void BTreePage<Trait>::SplitPageInto3(vector<value_type>& tmpKeys,
                                      vector<PagePtr>&    tmpSubPages,
                                      PagePtr&            pChild1,
                                      PagePtr&            pChild2,
                                      PagePtr&            pChild3,
                                      value_type&         oi1,
                                      value_type&         oi2)
{
    assert(tmpKeys.size() >= 8);
    assert(tmpSubPages.size() >= 9);
    if (!pChild1)
        pChild1 = new BTreePage(m_MaxKeysForChilds, m_Unique);

    // Split tmpKeys page into 3 pages
    // copy 1/3 elements to the first child
    pChild1->clear();
    size_t nKeys = (tmpKeys.size() - 2) / 3;
    size_t i = 0;
    for (; i < nKeys; ++i)
    {
        pChild1->m_Keys    [i] = tmpKeys    [i];
        pChild1->m_SubPages[i] = tmpSubPages[i];
        pChild1->NumberOfKeys()++;
    }
    pChild1->m_SubPages[i] = tmpSubPages[i];

    // first element to go up !
    oi1 = tmpKeys[i++];

    if (!pChild2)
        pChild2 = new BTreePage(m_MaxKeysForChilds, m_Unique);
    pChild2->clear();
    // copy 1/3 to the second child
    nKeys += (tmpKeys.size() - 2) / 3 + 1;
    size_t j = 0;
    for (; i < nKeys; ++i, ++j)
    {
        pChild2->m_Keys    [j] = tmpKeys    [i];
        pChild2->m_SubPages[j] = tmpSubPages[i];
        pChild2->NumberOfKeys()++;
    }
    pChild2->m_SubPages[j] = tmpSubPages[i];

    // copy the second element to the root
    oi2 = tmpKeys[i++];

    // copy 1/3 to the third child
    if (!pChild3)
        pChild3 = new BTreePage(m_MaxKeysForChilds, m_Unique);
    pChild3->clear();
    nKeys = tmpKeys.size();
    for (j = 0; i < nKeys; ++i, ++j)
    {
        pChild3->m_Keys    [j] = tmpKeys    [i];
        pChild3->m_SubPages[j] = tmpSubPages[i];
        pChild3->NumberOfKeys()++;
    }
    pChild3->m_SubPages[j] = tmpSubPages[i];
}

template <typename Trait>
bool BTreePage<Trait>::SplitRoot() {
    PagePtr pChild1 = nullptr, pChild2 = nullptr, pChild3 = nullptr;
    value_type oi1, oi2;
    SplitPageInto3(m_Keys, m_SubPages, pChild1, pChild2, pChild3, oi1, oi2);
    clear();

    // copy the first element to the root
    m_Keys    [0] = oi1;
    m_SubPages[0] = pChild1;
    NumberOfKeys()++;

    // copy the second element to the root
    m_Keys    [1] = oi2;
    m_SubPages[1] = pChild2;
    NumberOfKeys()++;

    m_SubPages[2] = pChild3;
    return true;
}


// Search
template <typename Trait>
bool BTreePage<Trait>::Search(const key_type& key, Ref& ObjID) {
    size_t pos = bt_binary_search(m_Keys, size_t(0), m_KeyCount, key, m_comp);
    if (pos >= m_KeyCount) {
        if (m_SubPages[pos]) return m_SubPages[pos]->Search(key, ObjID);
        return false;
    }
    if (!m_comp(key, m_Keys[pos].key) && !m_comp(m_Keys[pos].key, key)) {
        ObjID = m_Keys[pos].ObjID;
        m_Keys[pos].UseCounter++;
        return true;
    }
    if (m_comp(key, m_Keys[pos].key)) {
        if (m_SubPages[pos]) return m_SubPages[pos]->Search(key, ObjID);
    }
    return false;
}


// Traverse (UN SOLO BUCLE recursivo, unifica ForEach y FirstThat)
template <typename Trait>
template <typename Func, typename... Args>
typename BTreePage<Trait>::value_type*
BTreePage<Trait>::Traverse(Func func, size_t level, bool stop_on_true, int direction, Args&&... args)
{
    auto visit_subpage = [&](size_t idx) -> value_type* {
        if (m_SubPages[idx])
            return m_SubPages[idx]->Traverse(func, level + 1, stop_on_true, direction, forward<Args>(args)...);
        return nullptr;
    };

    if (direction > 0) {
        for (size_t i = 0; i < m_KeyCount; ++i) {
            if (auto* r = visit_subpage(i)) return r;
            if (func(m_Keys[i], level, forward<Args>(args)...)) {
                if (stop_on_true) return &m_Keys[i];
            }
        }
        return visit_subpage(m_KeyCount);
    } else {
        if (auto* r = visit_subpage(m_KeyCount)) return r;
        for (size_t i = m_KeyCount; i-- > 0;) {
            if (func(m_Keys[i], level, forward<Args>(args)...)) {
                if (stop_on_true) return &m_Keys[i];
            }
            if (auto* r = visit_subpage(i)) return r;
        }
        return nullptr;
    }
}

template <typename Trait>
template <typename Func, typename... Args>
void BTreePage<Trait>::ForEach(Func func, size_t level, Args&&... args) {
    // Adaptamos para que Traverse siempre vea un Func "bool"
    auto wrapper = [&](value_type& info, size_t lv, Args&&... a) -> bool {
        func(info, lv, forward<Args>(a)...);
        return false;
    };
    Traverse(wrapper, level, /*stop_on_true=*/false, /*direction=*/+1, forward<Args>(args)...);
}

template <typename Trait>
template <typename Func, typename... Args>
typename BTreePage<Trait>::value_type*
BTreePage<Trait>::FirstThat(Func func, size_t level, Args&&... args) {
    return Traverse(func, level, /*stop_on_true=*/true, /*direction=*/+1, forward<Args>(args)...);
}


template <typename Trait>
bt_ErrorCode BTreePage<Trait>::Remove(const key_type& key, Ref ObjID) {
    bt_ErrorCode error = bt_ok;
    size_t pos = bt_binary_search(m_Keys, size_t(0), m_KeyCount, key, m_comp);
    if (pos < NumberOfKeys()
        && !m_comp(key, m_Keys[pos].key) && !m_comp(m_Keys[pos].key, key) /*&& m_Keys[pos].m_ObjID == ObjID*/) // We found it !
    {
        // This is a leave: First
        if (!m_SubPages[pos + 1])  // This is a leave ? FIRST CASE !
        {
            bt_remove_at(m_Keys, pos);
            NumberOfKeys()--;
            if (Underflow())
                return bt_underflow;
            return bt_ok;
        }

        // We FOUND IT BUT it is NOT a leave ? SECOND CASE !
        {
            // Get the first element from right branch
            value_type& rFirstFromRight = m_SubPages[pos + 1]->GetFirstObjectInfo();
            // change with a leave
            swap(m_Keys[pos], rFirstFromRight);
            // Remove it from this leave

            //Print(cout);
            error = m_SubPages[++pos]->Remove(key, ObjID);
        }
    }
    else if (pos == NumberOfKeys()) // it is not here, go by the last branch
        error = m_SubPages[pos]->Remove(key, ObjID);
    else if (!m_comp(m_Keys[pos].key, key)) { // = is because identical keys are inserted on left (see Insert)
        if (m_SubPages[pos])
            error = m_SubPages[pos]->Remove(key, ObjID);
        else
            return bt_nofound;
    }
    if (error == bt_underflow) {
        // THIRD CASE: After removing the element we have an underflow
        //Print(cout);
        if (TreatUnderflow(pos))
            return bt_ok;
        // FOURTH CASE: it was not possible to redistribute -> Merge
        if (IsRoot() && NumberOfKeys() == 2)
            return MergeRoot();
        return Merge(pos);
    }
    if (error == bt_nofound)
        return bt_nofound;
    return bt_ok;
}


template <typename Trait>
bt_ErrorCode BTreePage<Trait>::Merge(size_t pos) {
    assert(m_SubPages[pos - 1]->NumberOfKeys() +
           m_SubPages[ pos ]->NumberOfKeys() +
           m_SubPages[pos + 1]->NumberOfKeys() ==
           3 * m_SubPages[ pos ]->MinNumberOfKeys() - 1);

    // FIRST: Put all the elements into a vector
    vector<value_type> tmpKeys;
    //tmpKeys.resize(nKeys);
    vector<PagePtr>    tmpSubPages;

    PagePtr pChild1 = m_SubPages[pos - 1],
            pChild2 = m_SubPages[ pos ],
            pChild3 = m_SubPages[pos + 1];
    MovePage(pChild1, tmpKeys, tmpSubPages);
    tmpKeys.push_back(m_Keys[pos - 1]);
    MovePage(pChild2, tmpKeys, tmpSubPages);
    tmpKeys.push_back(m_Keys[ pos ]);
    MovePage(pChild3, tmpKeys, tmpSubPages);
    pChild3->Destroy();

    // Move 1/2 elements to pChild1
    size_t nKeys = pChild1->GetFreeCells();
    size_t i = 0;
    for (; i < nKeys; ++i)
    {
        pChild1->m_Keys    [i] = tmpKeys    [i];
        pChild1->m_SubPages[i] = tmpSubPages[i];
        pChild1->NumberOfKeys()++;
    }
    pChild1->m_SubPages[i] = tmpSubPages[i];

    m_Keys    [pos - 1] = tmpKeys[i];
    m_SubPages[pos - 1] = pChild1;

    bt_remove_at(m_Keys    , pos);
    bt_remove_at(m_SubPages, pos);
    NumberOfKeys()--;

    nKeys = pChild2->GetFreeCells();
    size_t j = ++i;
    for (i = 0; i < nKeys; ++i, ++j)
    {
        pChild2->m_Keys    [i] = tmpKeys    [j];
        pChild2->m_SubPages[i] = tmpSubPages[j];
        pChild2->NumberOfKeys()++;
    }
    pChild2->m_SubPages[i] = tmpSubPages[j];
    m_SubPages[ pos ]      = pChild2;

    if (Underflow())
        return bt_underflow;
    return bt_ok;
}

template <typename Trait>
bt_ErrorCode BTreePage<Trait>::MergeRoot() {
    size_t pos = 1;
    assert(m_SubPages[pos - 1]->NumberOfKeys() +
           m_SubPages[ pos ]->NumberOfKeys() +
           m_SubPages[pos + 1]->NumberOfKeys() ==
           3 * m_SubPages[ pos ]->MinNumberOfKeys() - 1);

    PagePtr pChild1 = m_SubPages[pos - 1], pChild2 = m_SubPages[ pos ], pChild3 = m_SubPages[pos + 1];
    size_t nKeys = pChild1->NumberOfKeys() + pChild2->NumberOfKeys() + pChild3->NumberOfKeys() + 2;

    // FIRST: Put all the elements into a vector
    vector<value_type> tmpKeys;
    //tmpKeys.resize(nKeys);
    vector<PagePtr>    tmpSubPages;

    MovePage(pChild1, tmpKeys, tmpSubPages);
    tmpKeys.push_back(m_Keys[pos - 1]);
    MovePage(pChild2, tmpKeys, tmpSubPages);
    tmpKeys.push_back(m_Keys[ pos ]);
    MovePage(pChild3, tmpKeys, tmpSubPages);

    clear();
    size_t i = 0;
    for (; i < nKeys; ++i) {
        m_Keys    [i] = tmpKeys    [i];
        m_SubPages[i] = tmpSubPages[i];
        NumberOfKeys()++;
    }
    m_SubPages[i] = tmpSubPages[i];

    //Print(cout);
    pChild1->Destroy();
    pChild2->Destroy();
    pChild3->Destroy();

    return bt_rootmerged;
}


template <typename Trait>
typename BTreePage<Trait>::value_type&
BTreePage<Trait>::GetFirstObjectInfo() {
    if (m_SubPages[0])
        return m_SubPages[0]->GetFirstObjectInfo();
    return m_Keys[0];
}

template <typename Trait>
void BTreePage<Trait>::MovePage(PagePtr pChildPage,
                                vector<value_type>& tmpKeys,
                                vector<PagePtr>&    tmpSubPages)
{
    size_t nKeys = pChildPage->GetNumberOfKeys();
    size_t i = 0;
    for (; i < nKeys; ++i)
    {
        tmpKeys    .push_back(pChildPage->m_Keys    [i]);
        tmpSubPages.push_back(pChildPage->m_SubPages[i]);
    }
    tmpSubPages.push_back(pChildPage->m_SubPages[i]);
    pChildPage->clear();
}

template <typename Trait>
size_t BTreePage<Trait>::GetFreeCellsOnLeft(size_t pos) {
    if (pos > 0)                                  // there is some page on left ?
        return m_SubPages[pos - 1]->GetFreeCells();
    return 0;
}

template <typename Trait>
size_t BTreePage<Trait>::GetFreeCellsOnRight(size_t pos) {
    if (pos < GetNumberOfKeys())                  // there is some page on right ?
        return m_SubPages[pos + 1]->GetFreeCells();
    return 0;
}

#endif
