//
// Created by kskun on 2021/3/24.
//
#include <iostream>

#include "solve.h"

int main(int argc, char *argv[]) {
#ifdef TEST
    freopen("../../data/training-2.txt", "r", stdin);
    freopen("../../data/training-2.out.txt", "w", stdout);
#else
    // IO 优化
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cout.tie(nullptr);
#endif

    auto *solver = new Solver;
    solver->solve();

    return 0;
}