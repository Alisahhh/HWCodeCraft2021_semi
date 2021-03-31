//
// Created by kskun on 2021/3/26.
//
#pragma once

#include <vector>
#include <utility>

class BinIndexTree {
public:
    void update(int k, std::pair<int, int> newVal) {
        nodeCPU[k] = newVal.first;
        nodeMem[k] = newVal.second;

        while (k < maxCPU.size()) {
            maxCPU[k] = nodeCPU[k];
            maxMem[k] = nodeMem[k];
            int lk = lowbit(k);
            for (int i = 1; i < lk; i <<= 1) {
                maxCPU[k] = std::max(maxCPU[k], maxCPU[k - i]);
                maxMem[k] = std::max(maxMem[k], maxMem[k - i]);
            }
            k += lowbit(k);
        }
    }

    std::pair<int, int> query(int k) {
        int retCPU = 0, retMem = 0;
        for (int i = k; i; i -= lowbit(i)) {
            retCPU = std::max(retCPU, maxCPU[i]);
            retMem = std::max(retMem, maxMem[i]);
        }
        return std::make_pair(retCPU, retMem);
    }

    void setSize(int len) {
        maxCPU.resize(len + 1);
        maxMem.resize(len + 1);
        nodeCPU.resize(len + 1);
        nodeMem.resize(len + 1);
    }

private:
    std::vector<int> maxCPU;
    std::vector<int> maxMem;
    std::vector<int> nodeCPU;
    std::vector<int> nodeMem;

    static int lowbit(int x) {
        return x & (-x);
    }
};