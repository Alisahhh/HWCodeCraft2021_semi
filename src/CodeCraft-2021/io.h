//
// Created by kskun on 2021/3/25.
//
#pragma once

#include <iostream>

#include "data.h"

class StdIO {
public:
    std::vector<ServerType *> readServerType() {
        int n;
        std::string line;
        std::vector<ServerType *> serverTypeList;

        std::cin >> n;
        serverTypeList.reserve(n);
        std::getline(std::cin, line);
        while (n--) {
            std::getline(std::cin, line);
            serverTypeList.push_back(parseServerType(line));
        }

        return serverTypeList;
    }

    std::vector<VMType *> readVMType() {
        int m;
        std::string line;
        std::vector<VMType *> vmTypeList;

        std::cin >> m;
        vmTypeList.reserve(m);
        std::getline(std::cin, line);
        while (m--) {
            std::getline(std::cin, line);
            vmTypeList.push_back(parseVMType(line));
        }
        return vmTypeList;
    }

    std::pair<int, int> readDayCount() {
        int K;
        std::cin >> T >> K;
        return {T, K};
    }

    std::vector<Query *> readDayQueries() {
        if (nowDay == T) {
            throw std::logic_error("StdIO::readDayQueries: EOF");
        }
        nowDay++;

        int r;
        std::string line;
        std::vector<Query *> queryList;

        std::cin >> r;
        queryList.reserve(r);
        std::getline(std::cin, line);
        while (r--) {
            std::getline(std::cin, line);
            queryList.push_back(parseQuery(line, nowDay));
        }
        return queryList;
    }

    void writePurchaseList(std::vector<Server *> serverList) {
        std::sort(serverList.begin(), serverList.end(), [](Server *a, Server *b) {
            return a->model < b->model;
        });
        std::vector<std::pair<std::string, int>> serverModelCountList; // first: model, second: count
        for (auto server : serverList) {
            outputServerIDMap[server->id] = outputServerIDCount++;
            if (serverModelCountList.empty() || serverModelCountList.rbegin()->first != server->model) {
                serverModelCountList.emplace_back(server->model, 0);
            }
            serverModelCountList.rbegin()->second++;
        }

        std::cout << "(purchase, " << serverModelCountList.size() << ")" << std::endl;
        for (const auto &modelCount : serverModelCountList) {
            std::cout << "(" << modelCount.first << ", " << modelCount.second << ")" << std::endl;
        }
        std::cout.flush();
    }

    void writeMigrationList(const std::vector<std::tuple<int, int, Server::DeployNode>> &migrationList) {
        std::cout << "(migration, " << migrationList.size() << ")" << std::endl;
        for (auto[vmID, serverID, deployNode] : migrationList) {
            std::cout << "(" << vmID << ", " << getOutputServerID(serverID);
            if (deployNode != Server::DUAL_NODE) {
                std::cout << ", " << getDeployNodeStr(deployNode);
            }
            std::cout << ")" << std::endl;
        }
        std::cout.flush();
    }

    void writeDeployList(const std::vector<std::pair<Server *, Server::DeployNode>> &deployList) {
#ifdef DEBUG_O3
        int id = 0;
#endif
        for (auto[server, deployNode] : deployList) {
#ifdef DEBUG_O3
            id++;
            std::cout << "id " << id << " ";
#endif
            std::cout << "(" << getOutputServerID(server->id);
            if (deployNode != Server::DeployNode::DUAL_NODE) {
                std::cout << ", " << getDeployNodeStr(deployNode);
            }
            std::cout << ")" << std::endl;
        }
        std::cout.flush();
    }

private:
    int T, nowDay = 0;

    std::unordered_map<int, int> outputServerIDMap;
    int outputServerIDCount = 0;

    int getOutputServerID(int serverID) {
#ifdef DEBUG_O3
        return serverID;
#endif
        return outputServerIDMap[serverID];
    }

    static std::string getDeployNodeStr(Server::DeployNode node) {
        switch (node) {
            case Server::DeployNode::NODE_0:
                return "A";
            case Server::DeployNode::NODE_1:
                return "B";
            case Server::DeployNode::DUAL_NODE:
                throw std::logic_error("StdIO::getDeployNodeStr: dual node deployment does not need output");
        }
        throw std::logic_error("StdIO::getDeployNodeStr: invalid deploy node");
    }

    static ServerType *parseServerType(std::string line) {
        line = line.substr(1, line.length() - 2) + ",";

        int lastIdx = 0, idx = line.find(',', lastIdx + 1);
        std::string model = line.substr(lastIdx, idx - lastIdx);

        lastIdx = idx + 2;
        idx = line.find(',', lastIdx + 1);
        int cpu = atoi(line.substr(lastIdx, idx - lastIdx).c_str());

        lastIdx = idx + 2;
        idx = line.find(',', lastIdx + 1);
        int memory = atoi(line.substr(lastIdx, idx - lastIdx).c_str());

        lastIdx = idx + 2;
        idx = line.find(',', lastIdx + 1);
        int hardwareCost = atoi(line.substr(lastIdx, idx - lastIdx).c_str());

        lastIdx = idx + 2;
        idx = line.find(',', lastIdx + 1);
        int energyCost = atoi(line.substr(lastIdx, idx - lastIdx).c_str());

        return new ServerType(model, cpu, memory, hardwareCost, energyCost);
    }

    static VMType *parseVMType(std::string line) {
        line = line.substr(1, line.length() - 2) + ",";

        int lastIdx = 0, idx = line.find(',', lastIdx + 1);
        std::string model = line.substr(lastIdx, idx - lastIdx);

        lastIdx = idx + 2;
        idx = line.find(',', lastIdx + 1);
        int cpu = atoi(line.substr(lastIdx, idx - lastIdx).c_str());

        lastIdx = idx + 2;
        idx = line.find(',', lastIdx + 1);
        int memory = atoi(line.substr(lastIdx, idx - lastIdx).c_str());

        lastIdx = idx + 2;
        idx = line.find(',', lastIdx + 1);
        int isDual = atoi(line.substr(lastIdx, idx - lastIdx).c_str());
        auto deployType = isDual == 0 ? VMType::DeployType::SINGLE : VMType::DeployType::DUAL;

        return new VMType(model, cpu, memory, deployType);
    }

    static Query *parseQuery(std::string line, int day) {
        static int nowDay = 0, addQueryIDCount = 0;
        if (nowDay != day) {
            nowDay = day;
            addQueryIDCount = 0;
        }

        line = line.substr(1, line.length() - 2) + ",";

        int lastIdx = 0, idx = line.find(',', lastIdx + 1);
        Query::Type type = parseType(line.substr(lastIdx, idx - lastIdx));

        std::string model;
        if (type == Query::Type::ADD) {
            lastIdx = idx + 2;
            idx = line.find(',', lastIdx + 1);
            model = line.substr(lastIdx, idx - lastIdx);
        }

        lastIdx = idx + 2;
        idx = line.find(',', lastIdx + 1);
        int id = atoi(line.substr(lastIdx, idx - lastIdx).c_str());

        int queryID = 0;
        if (type == Query::Type::ADD) {
            queryID = ++addQueryIDCount;
        }

        return new Query(queryID, day, type, id, model);
    }

    static Query::Type parseType(const std::string &str) {
        if (str == "add") {
            return Query::Type::ADD;
        } else if (str == "del") {
            return Query::Type::DEL;
        }
        throw std::logic_error("StdIO::parseType: invalid query type");
    }
};