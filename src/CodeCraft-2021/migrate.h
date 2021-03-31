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
        std::vector<int> pmIdListForOut[2];

        int migCnt = 0;
        bool finFlag = false;
        for (int i = 0; i < 2; i++) {
            for (auto &pm : aliveMachineList[i]) {
                if (pm.second->empty()) continue;
                pmIdList[i].push_back(pm.first);
                pmIdListForOut[i].push_back(pm.first);
            }

            sortPM(static_cast<VM::DeployType>(i), pmIdList[i]);
            sortPMByUsedAmount(static_cast<VM::DeployType>(i), pmIdListForOut[i]);
/*
            for(auto it:pmIdListForOut[i]){
                fprintf(stderr,"%s %d %d %lf %d %lf\n",aliveMachineList[i][it]->model.c_str(),it,aliveMachineList[i][it]->cpu - aliveMachineList[i][it]->getLeftCPU(), 1-aliveMachineList[i][it]->getCPUUsage(), aliveMachineList[i][it]->memory- aliveMachineList[i][it]->getLeftMemory(),1-aliveMachineList[i][it]->getMemoryUsage());
            }
            */
        }

        int onePnt = 0;
        int twoPnt = 0;

        int skipCnt = 0;
        int tryCnt = 0;
        for (;;) {
            int i = -1;
            int outPMId = -1;
            if (onePnt >= pmIdListForOut[0].size() && twoPnt >= pmIdList[1].size()) break;

            if (onePnt >= pmIdListForOut[0].size()) {
                outPMId = pmIdListForOut[1][twoPnt++];
                i = 1;
            } else if (twoPnt >= pmIdList[1].size()) {
                outPMId = pmIdListForOut[0][onePnt++];
                i = 0;
            } else {
                auto pmOneNode = aliveMachineList[0][pmIdListForOut[0][onePnt]];
                auto pmTwoNode = aliveMachineList[1][pmIdListForOut[1][twoPnt]];

                if ((pmOneNode->cpu-pmOneNode->getLeftCPU() + (pmOneNode->memory-pmOneNode->getLeftMemory()) * MEMORY_PARA) <
                    (pmTwoNode->cpu-pmTwoNode->getLeftCPU() + (pmTwoNode->memory-pmTwoNode->getLeftMemory()) * MEMORY_PARA)) {
                    outPMId = pmIdListForOut[0][onePnt++];
                    i = 0;
                } else {
                    outPMId = pmIdListForOut[1][twoPnt++];
                    i = 1;
                }
            }

            auto outPM = Server::getServer(outPMId);
            //fprintf(stderr, "outpm %s %d\n",outPM->model.c_str(), outPMId);
            if (outPM->getMemoryUsage() < 0.1 && outPM->getCPUUsage() < 0.1) continue;

            std::set<int> cacheVMIdList;
            auto vmList = Server::getServer(outPMId)->getAllDeployedVMs();
            for (auto[vm, deployNode] : vmList) {
                cacheVMIdList.insert(vm->id);
            }

            for (auto localVMID : cacheVMIdList) {
                auto localVM = VM::getVM(localVMID);

                tryCnt++;
                int type = 0;
                int pos = -1;
                int inPMID = -1;

                inPMID = findPM(pmIdList[i], outPMId, localVM, static_cast<VMType::DeployType>(i), type, pos);

                if (inPMID <= 0) {
                    skipCnt++;
                    //fprintf(stderr, "not found\n");
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

                if (migCnt >= limit || finFlag) break;
            }
            if (migCnt >= limit || finFlag) break;
        }
#ifdef TEST
        std::clog << "limit: " << limit << " migCnt: " << migCnt << " tryCnt: " << tryCnt << std::endl;
#endif
    }

private:
    // **参数说明**
    // 对服务器资源排序时内存数值的系数
    const double MEMORY_PARA = 0.3;
    const double FIND_PM_REMAIN_MEMORY_WRIGHT[5] = {0.3, 0.3, 0.3, 0.3, 0.3};
    const double FIND_PM_REMAIN_CPU_WRIGHT[5] = {1, 1, 1, 1, 1};

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

    int sortPMByUsedAmount(VMType::DeployType deployType, std::vector<int> &pmIdList) {
      std::sort(pmIdList.begin(), pmIdList.end(),
                [deployType, this](int &pmIdi, int &pmIdj) {
                  auto pmi = aliveMachineList[deployType][pmIdi];
                  auto pmj = aliveMachineList[deployType][pmIdj];

                  return (pmi->cpu - pmi->getLeftCPU()+
                          (pmi->memory - pmi->getLeftMemory()) * MEMORY_PARA <
                         pmj->cpu - pmj->getLeftCPU()+
                          (pmj->memory - pmj->getLeftMemory()) * MEMORY_PARA);
                });

      return pmIdList.size();
    }

    int getRemainResourceWeightedSum(Server *pm, VM *vm, Server::DeployNode dn = Server::DUAL_NODE) {
      return (pm->getLeftCPU(dn) - vm->cpu) *
                 FIND_PM_REMAIN_CPU_WRIGHT[vm->category] +
             (pm->getLeftMemory(dn) - vm->memory) *
                 FIND_PM_REMAIN_MEMORY_WRIGHT[vm->category];
    }

    int findPM(std::vector<int> &pmIdList, int outPmID, VM *vm, VMType::DeployType deployType, int &type, int &pos) {
        //fprintf(stderr,"curvm %d %d\n",vm->cpu,vm->memory);
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
        
        auto outPM = aliveMachineList[deployType][outPmID];
        int curMinimalRemainder = outPM->getLeftCPU()*FIND_PM_REMAIN_CPU_WRIGHT[Server::DUAL_NODE] + outPM->getLeftMemory()*FIND_PM_REMAIN_MEMORY_WRIGHT[Server::DUAL_NODE];
        int minusCnt = 0;
        int findCnt = 0;
        
        if (deployType == VMType::DUAL){
            for (int i = pmIdList.size() - 1 - ans; i >= 0 && targetPMId < 0; i--) {
                auto curPMId = static_cast<std::vector<int>::iterator>(&pmIdList[i]);
                // avoid migrating vm to even lower load machines or to the same machine
                if (*curPMId == outPmID) continue;

                auto curPM = aliveMachineList[deployType][*curPMId];
                //fprintf(stderr,"curpmP %d %d %d\n",*curPMId,curPM->getLeftCPU(),curPM->getLeftMemory());
                // avoid startup an empty machine
                if (curPM->empty()) continue;

                int remainResourceWeightedSum = INT32_MAX;
                if (curPM->canDeployVM(vm) && curPM->getCategory(Server::DUAL_NODE) == vm->category) {
                    remainResourceWeightedSum = getRemainResourceWeightedSum(curPM, vm);
                    findCnt++;
                    if (remainResourceWeightedSum < curMinimalRemainder) {
                        targetPMId = curPM->id;
                        targetType = Server::DUAL_NODE;
                        position = i;
                        curMinimalRemainder = remainResourceWeightedSum;
                        minusCnt++;
                    }
                } 
            }
        }
        else if(deployType == VMType::SINGLE) {
            for (int i = pmIdList.size() - 1 - ans; i >= 0 && targetPMId < 0; i--) {
                auto curPMId = static_cast<std::vector<int>::iterator>(&pmIdList[i]);
                // avoid migrating vm to even lower load machines or to the same machine
                if (*curPMId == outPmID) continue;

                auto curPM = aliveMachineList[deployType][*curPMId];
                // avoid startup an empty machine
                if (curPM->empty()) continue;

                int remainResourceWeightedSum = INT32_MAX;
                    bool FlagA = curPM->canDeployVM(vm, Server::NODE_0);
                    if (curPM->getCategory(Server::NODE_0) != vm->category) FlagA = false;
                    bool FlagB = curPM->canDeployVM(vm, Server::NODE_1);
                    if (curPM->getCategory(Server::NODE_1) != vm->category) FlagB = false;

                    if (FlagA) {
                        findCnt++;
                        remainResourceWeightedSum =
                        getRemainResourceWeightedSum(curPM, vm, Server::NODE_0);
                        if (remainResourceWeightedSum < curMinimalRemainder) {
                            targetPMId = curPM->id;
                            targetType = Server::NODE_0;
                            position = i;
                            curMinimalRemainder = remainResourceWeightedSum;
                            minusCnt++;
                        }
                    } else if (FlagB) {
                        findCnt++;
                        remainResourceWeightedSum =
                        getRemainResourceWeightedSum(curPM, vm, Server::NODE_1);
                        if (remainResourceWeightedSum < curMinimalRemainder) {
                            targetPMId = curPM->id;
                            targetType = Server::NODE_1;
                            position = i;
                            curMinimalRemainder = remainResourceWeightedSum;
                            minusCnt++;
                        }
                    }
                //if(minusCnt > 3 ) break;
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

        #ifdef TEST2
        auto outPm = aliveMachineList[deployType][outPmID];
        auto inPm = aliveMachineList[deployType][inPmID];
        fprintf(stderr, "%d %d %d %d %lf %lf %lf %lf\n", deployType, outPmID,
                inPmID, vmID, 1-outPm->getCPUUsage(), 1-outPm->getMemoryUsage(),
                1-inPm->getCPUUsage(), 1-inPm->getMemoryUsage());
        #endif
    }
};
