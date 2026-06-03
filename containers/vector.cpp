#include <cstddef>
#include <iostream>
#include <string>
#include <fstream>
#include <thread>
#include "vector.h"
using namespace std;

void AddOne(T1& n){
    static mutex mtx;
    scoped_lock lock(mtx);
    ++n;
}

template <typename T>
void Add(T& n, T value){
    n += value;
}

void DemoVector(){
    Vector<AscendingVectorTrait<T1>> v1(3);
    v1.push_back(1, 11);
    v1.push_back(2, 22);
    v1.push_back(-1, -15);
    v1.push_back(4, 45);
    cout << v1.toString() << endl;
    cout << v1 << endl;

    Vector<AscendingVectorTrait<string>> v2(10);
    v2.push_back("Hola", 5);
    v2.push_back("Mundo", 6);
    v2.push_back("!", 1);
    cout << v2 << endl;
    cout << v2.toString() << endl;

    ofstream of("temp.txt");
    of << v1 << endl;
    of << v2 << endl;

    ForEach(v1, AddOne);
    Print(v1, cout);
    ForEach(v1, Add<T1>, 10);
    cout << "Imprimiendo desde adentro de Vector:" << endl;
    v1.ForEach(Add<T1>, 5);
    Print(v1, cout);
    v1.ForEach(PrintX<T1>, cout, ", ");
    cout << endl;
    v1.ReverseForEach(PrintX<T1>, cout, ", ");
    cout << endl;

    Print(v2, cout);
    ForEach(v2, Add<string>, string("XYZ"));
    cout << v2 << endl;

    Print(v1, of);
    Print(v2, of);
}
