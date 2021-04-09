//
// Created by kskun on 2021/3/24.
//
#include <iostream>

#include "solve.h"

int main(int argc, char *argv[]) {
#ifdef TEST
    freopen("../../data/training-1.txt", "r", stdin);
    freopen("../../data/training-1.out.txt", "w", stdout);
#endif

    auto *solver = new Solver;
    solver->solve();

    return 0;
}