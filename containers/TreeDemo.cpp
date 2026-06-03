#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include "../types.h"
#include "BinaryTree.h"

using namespace std;

// Test: range-for con views
void TestViews(){
    cout << "\nTEST: views (range-for cambia el recorrido)" << endl;
    BinaryTree<AscendingBTTrait<T1>> tree;
    T1 xs[] = {50, 30, 70, 20, 40, 60, 80};
    for(T1 x : xs) tree.insert(x, x);

    cout << "inorder forward    : ";
    for(auto& x : tree) cout << x << " "; cout << endl;

    cout << "inorder backward   : ";
    for(auto& x : tree.reverse()) cout << x << " "; cout << endl;

    cout << "preorder forward   : ";
    for(auto& x : tree.preorder()) cout << x << " "; cout << endl;

    cout << "preorder backward  : ";
    for(auto& x : tree.preorder_reverse()) cout << x << " "; cout << endl;

    cout << "postorder forward  : ";
    for(auto& x : tree.postorder()) cout << x << " "; cout << endl;

    cout << "postorder backward : ";
    for(auto& x : tree.postorder_reverse()) cout << x << " "; cout << endl;
}

void TreeDemo(){
    TestViews();
}
