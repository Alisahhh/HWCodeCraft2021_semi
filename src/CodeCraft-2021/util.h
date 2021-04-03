//
// Created by kskun on 2021/3/26.
//
#pragma once

#include <vector>
#include <utility>
#include <random>
#include <unordered_set>
#include <algorithm>

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
        for (int i = 0; i < k; i++) {
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
public:
    typedef std::vector<double> Vector;
    typedef std::vector<Vector> VectorList;
    typedef std::pair<Vector, int> Object;
    typedef std::vector<Object> ObjectList;

private:
    Random rand;

    static const int MAX_ITER_COUNT = 100;
    constexpr volatile static const double EPS = 1e-2;

    static int fcmp(double a) {
        if (fabs(a) < EPS) return 0;
        else return a < 0 ? -1 : 1;
    }

    static int cmpVector(const Vector &a, const Vector &b) {
        for (int i = 0; i < a.size(); i++) {
            int cmp = fcmp(a[i] - b[i]);
            if (cmp < 0) return -1;
            if (cmp > 0) return 1;
        }
        return 0;
    }

    static bool cmpVectorForSort(const Vector &a, const Vector &b) {
        return cmpVector(a, b) < 0;
    }

    // 计算两向量间的欧氏距离
    static double distance(const Vector &a, const Vector &b) {
        if (a.empty() || b.empty()) throw std::logic_error("KMeans::distance: empty vector");
        if (a.size() != b.size()) throw std::logic_error("KMeans::distance: vector size not equal");
        double dist = 0;
        for (int i = 0; i < a.size(); i++) {
            dist += (a[i] - b[i]) * (a[i] - b[i]);
        }
        return sqrt(dist);
    }

    static bool isCentroidListEqual(VectorList a, VectorList b) {
        std::sort(a.begin(), a.end(), cmpVectorForSort);
        std::sort(b.begin(), b.end(), cmpVectorForSort);
        for (int i = 0; i < a.size(); i++) {
            if (cmpVector(a[i], b[i]) != 0) return false;
        }
        return true;
    }

    // 计算一个聚类的重心
    static Vector getCentroid(const VectorList &list) {
        if (list.empty()) throw std::logic_error("KMeans::getCentroid: empty list");
        Vector res;
        res.resize(list[0].size());
        for (const auto &vec : list) {
            for (int i = 0; i < vec.size(); i++) {
                res[i] += vec[i];
            }
        }
        for (double &i : res) {
            i /= list.size();
        }
        return res;
    }

    static Vector getCentroid(const ObjectList &list) {
        VectorList tmp;
        tmp.reserve(list.size());
        for (auto &obj : list) {
            tmp.push_back(obj.first);
        }
        return getCentroid(tmp);
    }

public:
    std::vector<ObjectList> kMeans(ObjectList src, int k) {
        if (src.empty()) throw std::logic_error("KMeans::kMeans: empty list");
        int n = src.size(), dim = src[0].first.size();
        std::vector<int> c;
        c.resize(n);

        VectorList centroids, newCentroids;
        centroids.reserve(k);
        auto initCentList = rand.randInts(0, n - 1, k);
        for (auto initCentIdx : initCentList) {
            centroids.push_back(src[initCentIdx].first);
        }
        int iterCnt = 0;
        do {
            iterCnt++;
            newCentroids.clear();
            newCentroids.reserve(k);
            // 聚类
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < k; j++) {
                    if (distance(src[i].first, centroids[c[i]]) > distance(src[i].first, centroids[j])) {
                        c[i] = j;
                    }
                }
            }
            // 计算重心
            for (int i = 0; i < k; i++) {
                ObjectList list;
                list.reserve(n);
                for (int j = 0; j < n; j++) {
                    if (c[j] == i) list.push_back(src[j]);
                }
                newCentroids.push_back(getCentroid(list));
            }
        } while (!isCentroidListEqual(centroids, newCentroids) && iterCnt < MAX_ITER_COUNT);

        std::vector<ObjectList> res;
        res.resize(k);
        for (int i = 0; i < k; i++) {
            for (int j = 0; j < n; j++) {
                if (c[j] == i) res[i].push_back(src[j]);
            }
        }
        return res;
    }
};