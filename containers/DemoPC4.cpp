#include <iostream>
#include "../types.h"
#include "BinaryTree.h"
#include "heap.h"
#include "hashtable.h"
#include "hashavl.h"

using namespace std;

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

void DemoHashAVL(){
    HashAVL<T1, string> m;
    m[1] = "A";
    m[2] = "For";
    m[3] = "B";
    cout << m << endl;
    m[3] = "C";
    cout << m << endl;

    for (const auto& [key, value] : m)
        cout << key << " => " << value << endl;
}

void DemoPC4(){
    DemoHashAVL();
}
