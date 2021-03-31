//
// Created by kskun on 2021/3/26.
//
#pragma once

#include <algorithm>
#include <unordered_map>
#include <set>

#include "data.h"
#include "util.h"
#include "debug.h"

class Migrator {
public:
    Migrator(std::unordered_map<int, Server *> *_aliveMachineList) {
        aliveMachineList = _aliveMachineList;
    }

    void migrate(int day, int limit, std::vector<std::tuple<int, int, Server::DeployNode>> &migrationList) {
        std::vector<int> pmIdList[2];
        int migCnt = 0;
        bool finFlag = false;
        for (int i = 0; i < 2; i++) {
            for (auto &pm : aliveMachineList[i]) {
                if (pm.second->empty()) continue;
                pmIdList[i].push_back(pm.first);
            }

            sortPM(static_cast<VM::DeployType>(i), pmIdList[i]);
        }

        int onePnt = 0;
        int twoPnt = 0;

        int skipCnt = 0;
        int tryCnt = 0;
        for (;;) {
            int i = -1;
            int outPMId = -1;
            if (onePnt >= pmIdList[0].size() && twoPnt >= pmIdList[1].size()) break;

            if (onePnt >= pmIdList[0].size()) {
                outPMId = pmIdList[1][twoPnt++];
                i = 1;
            } else if (twoPnt >= pmIdList[1].size()) {
                outPMId = pmIdList[0][onePnt++];
                i = 0;
            } else {
                auto pmOneNode = aliveMachineList[0][pmIdList[0][onePnt]];
                auto pmTwoNode = aliveMachineList[1][pmIdList[1][twoPnt]];

                if ((pmOneNode->getCPUUsage() + pmOneNode->getMemoryUsage() * MEMORY_PARA) <
                    (pmTwoNode->getCPUUsage() + pmTwoNode->getMemoryUsage() * MEMORY_PARA)) {
                    outPMId = pmIdList[0][onePnt++];
                    i = 0;
                } else {
                    outPMId = pmIdList[1][twoPnt++];
                    i = 1;
                }
            }

            std::set<int> cacheVMIdList;
            auto vmList = Server::getServer(outPMId)->getAllDeployedVMs();
            for (auto[vm, deployNode] : vmList) {
                cacheVMIdList.insert(vm->id);
            }

            for (auto localVMID : cacheVMIdList) {
                /*static int timeCnt = 0;
                timeCnt++;
                static clock_t startTime;
                if (timeCnt % 100 == 1) {
                    startTime = clock();
                }*/

                auto localVM = VM::getVM(localVMID);

                tryCnt++;
                int type = 0;
                int pos = -1;
                int inPMID = -1;
                if (i == 0) {
                    inPMID = findPM(pmIdList[i], onePnt - 1, localVM, static_cast<VMType::DeployType>(i), type, pos);
                } else {
                    inPMID = findPM(pmIdList[i], twoPnt - 1, localVM, static_cast<VMType::DeployType>(i), type, pos);
                }

                if (inPMID <= 0) {
                    skipCnt++;
                    /*if (skipCnt >= limit) {
                        //finFlag = true;
                        //break;
                    }*/
                    continue;
                } else {
                    skipCnt = 0;
                }

                placeVM(outPMId, inPMID, localVMID, static_cast<ServerType::DeployNode>(type));
                if (type == Server::NODE_0 || type == Server::NODE_1) {
                    auto pm = aliveMachineList[i][inPMID];
                    treeArray[i].update(pmIdList[i].size() - pos, std::make_pair(
                            std::max(pm->getLeftCPU(Server::NODE_0), pm->getLeftCPU(Server::NODE_1)),
                            std::max(pm->getLeftMemory(Server::NODE_0), pm->getLeftMemory(Server::NODE_1))));
                    pm = aliveMachineList[i][outPMId];
                    treeArray[i].update(pmIdList[i].size() - onePnt + 1,
                                        std::make_pair(std::max(pm->getLeftCPU(Server::NODE_0),
                                                                pm->getLeftCPU(Server::NODE_1)),
                                                       std::max(pm->getLeftMemory(Server::NODE_0),
                                                                pm->getLeftMemory(Server::NODE_1))));
                } else {
                    auto pm = aliveMachineList[i][inPMID];
                    treeArray[i].update(pmIdList[i].size() - pos, std::make_pair(pm->getLeftCPU(Server::NODE_0),
                                                                                 pm->getLeftMemory(Server::NODE_1)));
                    pm = aliveMachineList[i][outPMId];
                    treeArray[i].update(pmIdList[i].size() - twoPnt + 1, std::make_pair(
                            std::max(pm->getLeftCPU(Server::NODE_0), pm->getLeftCPU(Server::NODE_1)),
                            std::max(pm->getLeftMemory(Server::NODE_0), pm->getLeftMemory(Server::NODE_1))));
                }
                migrationList.emplace_back(localVMID, inPMID, static_cast<ServerType::DeployNode>(type));
                migCnt++;

                /*if (timeCnt >= 100) {
                    auto endTime = clock();
                    //LOG_TIME(startTime, endTime)
                    timeCnt = 0;
                }*/

                if (migCnt >= limit || finFlag) break;
            }
            if (migCnt >= limit || finFlag) break;
        }
#ifdef TEST
        // std::clog << "limit: " << limit << " migCnt: " << migCnt << " tryCnt: " << tryCnt << std::endl;
#endif
    }

private:
    // **参数说明**
    // 对服务器资源排序时内存数值的系数
    const double MEMORY_PARA = 0.5;

    std::unordered_map<int, Server *> *aliveMachineList;
    BinIndexTree treeArray[2];

    int sortPM(VMType::DeployType deployType, std::vector<int> &pmIdList) {
        std::sort(pmIdList.begin(), pmIdList.end(),
                  [deployType, this](int &pmIdi, int &pmIdj) {

                      auto pmi = aliveMachineList[deployType][pmIdi];
                      auto pmj = aliveMachineList[deployType][pmIdj];

                      return (pmi->getLeftCPU(Server::NODE_0) + pmi->getLeftCPU(Server::NODE_1) +
                              (pmi->getLeftMemory(Server::NODE_0) + pmi->getLeftMemory(Server::NODE_1)) * MEMORY_PARA) >
                             (pmj->getLeftCPU(Server::NODE_0) + pmj->getLeftCPU(Server::NODE_1) +
                              (pmj->getLeftMemory(Server::NODE_0) + pmj->getLeftMemory(Server::NODE_1)) * MEMORY_PARA);
                  });
        treeArray[deployType].setSize(pmIdList.size());
        for (int i = 1; i <= pmIdList.size(); i++) {
            Server *pm = aliveMachineList[deployType][pmIdList[pmIdList.size() - i]];
            if (deployType == VMType::SINGLE) {
                treeArray[deployType].update(i, std::make_pair(
                        std::max(pm->getLeftCPU(Server::NODE_0), pm->getLeftCPU(Server::NODE_1)),
                        std::max(pm->getLeftMemory(Server::NODE_0), pm->getLeftMemory(Server::NODE_1))));
            } else {
                treeArray[deployType].update(i, std::make_pair(pm->getLeftCPU(Server::NODE_0),
                                                               pm->getLeftMemory(Server::NODE_0)));
            }
        }
        return pmIdList.size();
    }

    int findPM(std::vector<int> &pmIdList, int outPos, VM *vm, VMType::DeployType deployType, int &type, int &pos) {
        int targetPMId = -1;
        int targetType = -1;
        int position = -1;
        int L = 0, R = pmIdList.size() - 1;
        int ans = pmIdList.size();
        while (L <= R) {
            int mid = (L + R) >> 1;
            auto retRes = treeArray[deployType].query(mid);
            if ((deployType == VMType::SINGLE && retRes.first >= vm->cpu && retRes.second >= vm->memory) ||
                (deployType == VMType::DUAL && retRes.first >= vm->cpu / 2 && retRes.second >= vm->memory / 2)) {
                ans = mid;
                R = mid - 1;
            } else {
                L = mid + 1;
            }
        }
        for (int i = pmIdList.size() - 1 - ans; i >= 0 && targetPMId <= 0; i--) {
            if (i <= outPos) break;
            auto curPMId = static_cast<std::vector<int>::iterator>(&pmIdList[i]);
            auto curPM = aliveMachineList[deployType][*curPMId];
            if (deployType == VMType::DUAL && curPM->canDeployVM(vm)) {
                if (curPM->getCategory(Server::DUAL_NODE) != vm->category) continue;
                targetPMId = curPM->id;
                targetType = Server::DUAL_NODE;
                position = i;
            } else if (deployType == VMType::SINGLE) {
                bool FlagA = curPM->canDeployVM(vm, Server::NODE_0);
                if (curPM->getCategory(Server::NODE_0) != vm->category) FlagA = false;
                bool FlagB = curPM->canDeployVM(vm, Server::NODE_1);
                if (curPM->getCategory(Server::NODE_1) != vm->category) FlagB = false;
                if (FlagA) {
                    targetPMId = curPM->id;
                    targetType = Server::NODE_0;
                    position = i;
                } else if (FlagB) {
                    targetPMId = curPM->id;
                    targetType = Server::NODE_1;
                    position = i;
                }
            }
        }

        type = targetType;
        pos = position;
        return targetPMId;
    }

    void placeVM(int outPmID, int inPmID, int vmID, Server::DeployNode node = Server::DUAL_NODE) {
        // delete in old one
        auto vm = VM::getVM(vmID);
        auto deployType = vm->deployType;

        auto pm = aliveMachineList[deployType][outPmID];
        pm->remove(vm);

        // add in new one
        pm = aliveMachineList[deployType][inPmID];
        pm->deploy(vm, node);
    }
};
