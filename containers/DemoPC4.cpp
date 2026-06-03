#include <iostream>
#include "../types.h"
#include "BinaryTree.h"
#include "heap.h"
#include "hashtable.h"

using namespace std;

// range-for con views
void TestViews(){
    cout << "\nTEST: views (range-for cambia el recorrido)" << endl;
    BinaryTree<AscendingBTTrait<T1>> tree;
    T1 xs[] = {50, 30, 70, 20, 40, 60, 80};
    for(T1 x : xs) tree.insert(x, x);

    cout << "inorder forward: ";
    for(auto& x : tree) cout << x << " "; cout << endl;

    cout << "inorder backward: ";
    for(auto& x : tree.reverse()) cout << x << " "; cout << endl;

    cout << "preorder forward: ";
    for(auto& x : tree.preorder()) cout << x << " "; cout << endl;

    cout << "preorder backward: ";
    for(auto& x : tree.preorder_reverse()) cout << x << " "; cout << endl;

    cout << "postorder forward: ";
    for(auto& x : tree.postorder()) cout << x << " "; cout << endl;

    cout << "postorder backward: ";
    for(auto& x : tree.postorder_reverse()) cout << x << " "; cout << endl;
}

void DemoMinHeap(){
    cout << "\nTEST: MinHeap" << endl;
    Heap<MinHeapTrait<T1>> h;
    T1 xs[] = {5, 3, 7, 1, 4, 8, 2, 6};
    for(T1 x : xs) h.insert(x, x * 10);

    cout << "heap    : " << h << endl;
    cout << "extract : ";
    while(!h.empty()){
        auto [data, ref] = h.extract();
        cout << data << " ";
    }
    cout << endl;
}

void DemoMaxHeap(){
    cout << "\nTEST: MaxHeap" << endl;
    Heap<MaxHeapTrait<T1>> h;
    T1 xs[] = {5, 3, 7, 1, 4, 8, 2, 6};
    for(T1 x : xs) h.insert(x, x * 10);

    cout << "heap    : " << h << endl;
    cout << "extract : ";
    while(!h.empty()){
        auto [data, ref] = h.extract();
        cout << data << " ";
    }
    cout << endl;
}

void DemoHashTable(){
    cout << "\nTEST: HashTable" << endl;
    HashTable<HashTableTrait<T1, T1>> m(8);
    m[1] = 100;
    m[2] = 200;
    m[3] = 300;

    for(const auto& [key, value] : m)
        cout << key << " -> " << value << endl;
}

void DemoPC4(){
    TestViews();
    DemoMinHeap();
    DemoMaxHeap();
    DemoHashTable();
}
