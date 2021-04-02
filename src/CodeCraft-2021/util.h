//
// Created by kskun on 2021/3/26.
//
#pragma once

#include <vector>
#include <utility>
#include <random>
#include <unordered_set>

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

class Random {
private:
    std::random_device device;
public:
    // 随机在 [l, r] 内选出一个整数
    int randInt(int l, int r) {
        return l + device() % (r - l + 1);
    }

    // 随机产生一个在 [0, 1] 之间的浮点数
    double randNormal() {
        return (double) device() / std::random_device::max();
    }

    // 随机在 [l, r] 选出 k 个整数
    std::vector<int> randInts(int l, int r, int k) {
        std::unordered_set<int> s;
        while (k--) {
            int t;
            do {
                t = randInt(l, r);
            } while (s.count(t) > 0);
            s.insert(t);
        }
        std::vector<int> res;
        res.reserve(k);
        for (auto i : s) {
            res.push_back(i);
        }
        return res;
    }
};

class KMeans {
private:
    Random rand;

    static const int MAX_ITER_COUNT = 100;

public:
    typedef std::vector<double> Vector;
    typedef std::pair<Vector, int> Object;
    typedef std::vector<Object> ObjectList;

    static double distance(const Vector &a, const Vector &b) {
        if (a.empty() || b.empty()) throw std::logic_error("KMeans::distance: empty vector");
        if (a.size() != b.size()) throw std::logic_error("KMeans::distance: vector size not equal");
        double dist = 0;
        for (int i = 0; i < a.size(); i++) {
            dist += (a[i] - b[i]) * (a[i] - b[i]);
        }
        return sqrt(dist);
    }

    std::vector<ObjectList> kMeans(ObjectList src, int k) {
        if (src.empty()) throw std::logic_error("KMeans::kMeans: empty list");
        int n = src.size(), dim = src[0].first.size();
        std::vector<int> c;
        c.resize(src.size());

        std::vector<Vector> centroids;
        centroids.reserve(k);
        auto initCentList = rand.randInts(0, src.size() - 1, k);
        for (auto initCentIdx : initCentList) {
            centroids.push_back(src[initCentIdx].first);
        }
        // TODO: finish this
    }
};