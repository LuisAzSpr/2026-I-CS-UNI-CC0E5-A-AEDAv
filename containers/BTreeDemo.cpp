#include <iostream>
#include "BTree.h"
#include "../types.h"
using namespace std;

const Char* keys1 = "D1XJ2xTg8zKL9AhijOPQcEowRSp0NbW567BUfCqrs4FdtYZakHIuvGV3eMylmn";
const Char* keys2 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
const Char* keys3 = "DYZakHIUwxVJ203ejOP9Qc8AdtuEop1XvTRghSNbW567BfiCqrs4FGMyzKLlmn";

const size_t BTreeSize = 3;

int main() {
    BTree<AscendingBTreeTrait<Char>> bt(BTreeSize);

    // Insert
    for (size_t i = 0; keys1[i]; ++i)
        bt.Insert(keys1[i], static_cast<Ref>(i * i));

    cout << "--- Arbol tras inserciones ---" << endl;
    cout << bt;

    // Range-for con el forward iterator
    cout << "--- Recorrido forward (range-for) ---" << endl;
    for (auto& info : bt)
        cout << info.key << " ";
    cout << endl;

    // ForEach variadic
    cout << "--- ForEach con lambda variadic ---" << endl;
    bt.ForEach([](auto& info, ostream& out) {
        out << "(" << info.key << "," << info.ObjID << ") ";
    }, cout);
    cout << endl;

    // ReverseForEach (usa backward iterator)
    cout << "--- ReverseForEach ---" << endl;
    bt.ReverseForEach([](auto& info, ostream& out) {
        out << info.key << " ";
    }, cout);
    cout << endl;

    // Search
    cout << "--- Search ---" << endl;
    for (size_t i = 0; keys2[i]; ++i) {
        Ref ObjID = bt.Search(keys2[i]);
        if (ObjID != -1)
            cout << "Encontrado " << keys2[i] << " ID = " << ObjID << endl;
        else
            cout << "No encontrado " << keys2[i] << endl;
    }

    // Remove
    cout << "--- Remove ---" << endl;
    for (size_t i = 0; keys3[i]; ++i) {
        if (bt.Remove(keys3[i], -1))
            cout << "Removido " << keys3[i] << endl;
        else
            cout << "No encontrado " << keys3[i] << endl;
    }

    cout << "--- Arbol tras remove ---" << endl;
    cout << bt;
    return 0;
}
