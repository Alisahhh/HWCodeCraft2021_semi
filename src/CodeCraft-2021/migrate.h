//
// Created by kskun on 2021/3/26.
//
#pragma once

#include <algorithm>
#include <iomanip>
#include <set>
#include <unordered_map>

#include "data.h"
#include "debug.h"
#include "util.h"

class Migrator {
  public:
    Migrator(std::unordered_map<int, Server *> *_aliveMachineList) {
        aliveMachineList = _aliveMachineList;
    }

    int migrateScatteredVM(
        int day, int limit,
        std::vector<std::tuple<int, int, Server::DeployNode>> &migrationList,
        volatile double thr = 0.1) {
        if (limit == 0)
            return 0;
        std::vector<int> pmIdList[2];
        std::vector<int> pmIdListForVMSelect[2];

        int migCnt = 0;
        bool finFlag = false;
        for (int i = 0; i < 2; i++) {
            for (auto &pm : aliveMachineList[i]) {
                if (pm.second->empty()) {
                    emptyPMCnt++;
                    continue;
                }
                pmIdList[i].push_back(pm.first);
                pmIdListForVMSelect[i].push_back(pm.first);
            }

            sortPMByRemainAmount(static_cast<VM::DeployType>(i), pmIdList[i]);
            sortPMByUsedAmount(static_cast<VM::DeployType>(i),
                               pmIdListForVMSelect[i]);
        }

        int onePnt = 0;
        int twoPnt = 0;

        int skipCnt = 0;
        int tryCnt = 0;
        for (;;) {
            int i = -1;
            int outPMId = -1;
            if (onePnt >= pmIdListForVMSelect[0].size() &&
                twoPnt >= pmIdListForVMSelect[1].size())
                break;

            if (onePnt >= pmIdListForVMSelect[0].size()) {
                outPMId = pmIdListForVMSelect[1][twoPnt++];
                i = 1;
            } else if (twoPnt >= pmIdListForVMSelect[1].size()) {
                outPMId = pmIdListForVMSelect[0][onePnt++];
                i = 0;
            } else {
                auto pmOneNode =
                    aliveMachineList[0][pmIdListForVMSelect[0][onePnt]];
                auto pmTwoNode =
                    aliveMachineList[1][pmIdListForVMSelect[1][twoPnt]];

                if (fcmp((pmOneNode->cpu - pmOneNode->getLeftCPU() +
                          (pmOneNode->memory - pmOneNode->getLeftMemory()) *
                              MEMORY_PARA) -
                         (pmTwoNode->cpu - pmTwoNode->getLeftCPU() +
                          (pmTwoNode->memory - pmTwoNode->getLeftMemory()) *
                              MEMORY_PARA)) < 0) {
                    outPMId = pmIdListForVMSelect[0][onePnt++];
                    i = 0;
                } else {
                    outPMId = pmIdListForVMSelect[1][twoPnt++];
                    i = 1;
                }
            }

            auto outPM = Server::getServer(outPMId);
            // fprintf(stderr, "outpm %s %d\n",outPM->model.c_str(), outPMId);
#ifdef DEBUG_O3
            std::clog << "outPM: " << outPM->id << std::fixed
                      << std::setprecision(10)
                      << " mem use: " << outPM->getMemoryUsage()
                      << " cpu use: " << outPM->getCPUUsage() << std::endl;
#endif
            if (fcmp(outPM->getMemoryUsage() - thr) < 0 &&
                fcmp(outPM->getCPUUsage() - thr) < 0)
                continue;

            std::set<int> cacheVMIdList;
            auto vmList = Server::getServer(outPMId)->getAllDeployedVMs();
            for (auto [vm, deployNode] : vmList) {
                cacheVMIdList.insert(vm->id);
            }

            for (auto localVMID : cacheVMIdList) {
                auto localVM = VM::getVM(localVMID);

                tryCnt++;
                int type = 0;
                int pos = -1;
                int inPMID = -1;

                inPMID = findPM(pmIdList[i], outPMId, localVM,
                                static_cast<VMType::DeployType>(i), type, pos);

                if (inPMID <= 0) {
                    skipCnt++;
                    continue;
                } else {
                    skipCnt = 0;
                }

                placeVM(outPMId, inPMID, localVMID,
                        static_cast<ServerType::DeployNode>(type));
                if (type == Server::NODE_0 || type == Server::NODE_1) {
                    auto pm = aliveMachineList[i][inPMID];
                    treeArray[i].update(
                        pmIdList[i].size() - pos,
                        std::make_pair(
                            std::max(pm->getLeftCPU(Server::NODE_0),
                                     pm->getLeftCPU(Server::NODE_1)),
                            std::max(pm->getLeftMemory(Server::NODE_0),
                                     pm->getLeftMemory(Server::NODE_1))));
                    pm = aliveMachineList[i][outPMId];
                    treeArray[i].update(
                        pmIdList[i].size() - onePnt + 1,
                        std::make_pair(
                            std::max(pm->getLeftCPU(Server::NODE_0),
                                     pm->getLeftCPU(Server::NODE_1)),
                            std::max(pm->getLeftMemory(Server::NODE_0),
                                     pm->getLeftMemory(Server::NODE_1))));
                } else {
                    auto pm = aliveMachineList[i][inPMID];
                    treeArray[i].update(
                        pmIdList[i].size() - pos,
                        std::make_pair(pm->getLeftCPU(Server::NODE_0),
                                       pm->getLeftMemory(Server::NODE_1)));
                    pm = aliveMachineList[i][outPMId];
                    treeArray[i].update(
                        pmIdList[i].size() - twoPnt + 1,
                        std::make_pair(
                            std::max(pm->getLeftCPU(Server::NODE_0),
                                     pm->getLeftCPU(Server::NODE_1)),
                            std::max(pm->getLeftMemory(Server::NODE_0),
                                     pm->getLeftMemory(Server::NODE_1))));
                }
                migrationList.emplace_back(
                    localVMID, inPMID,
                    static_cast<ServerType::DeployNode>(type));
                migCnt++;

                if (migCnt >= limit || finFlag)
                    break;
            }
            if (migCnt >= limit || finFlag)
                break;
        }
#ifdef TEST
        std::clog << "limit: " << limit << " migCnt: " << migCnt
                  << " tryCnt: " << tryCnt << std::endl;
#endif
        return migCnt;
    }

    int combineLowLoadRatePM(
        int day, int limit,
        std::vector<std::tuple<int, int, Server::DeployNode>> &migrationList,
        double thr = 0.3) {
        std::vector<int> pmIdAllList[2];
        std::vector<int> pmIdSelectedList[2];
        std::set<int> vmIdSet[2];
        std::unordered_map<int, std::pair<int, int>> shakeLink[2];
        std::unordered_map<int, ServerType::DeployNode> shakeType[2];
        std::vector<int> shakeOrder[2];
        std::set<int> shakeFixVM[2];
        ServerShadowFactory migrateSimulationServer;

        bool finFlag = false;

        for (int i = 0; i < 2; i++) {
            for (auto &pm : aliveMachineList[i]) {
                if (pm.second->empty())
                    continue;
                if (pm.second->isHigh)
                    continue;
                pmIdAllList[i].push_back(pm.first);
            }

            sortPMByUsedAmount(static_cast<VM::DeployType>(i), pmIdAllList[i]);
        }

        int onePnt = 0;
        int twoPnt = 0;

        int selectVmCnt = 0;
        int migCnt = 0;
        for (;;) {
            int i = -1;
            int outPMId = -1;
            if ((onePnt >= pmIdAllList[0].size() &&
                 twoPnt >= pmIdAllList[1].size()) ||
                selectVmCnt >= limit)
                break;

            if (onePnt >= pmIdAllList[0].size()) {
                outPMId = pmIdAllList[1][twoPnt++];
                i = 1;
            } else if (twoPnt >= pmIdAllList[1].size()) {
                outPMId = pmIdAllList[0][onePnt++];
                i = 0;
            } else {
                auto pmOneNode = aliveMachineList[0][pmIdAllList[0][onePnt]];
                auto pmTwoNode = aliveMachineList[1][pmIdAllList[1][twoPnt]];

                if (fcmp((pmOneNode->cpu - pmOneNode->getLeftCPU() +
                          (pmOneNode->memory - pmOneNode->getLeftMemory()) *
                              MEMORY_PARA) -
                         (pmTwoNode->cpu - pmTwoNode->getLeftCPU() +
                          (pmTwoNode->memory - pmTwoNode->getLeftMemory()) *
                              MEMORY_PARA)) < 0) {
                    outPMId = pmIdAllList[0][onePnt++];
                    i = 0;
                } else {
                    outPMId = pmIdAllList[1][twoPnt++];
                    i = 1;
                }
            }

            auto pm = aliveMachineList[i][outPMId];
            const double migRate = 0.3;
            if (fcmp(pm->getCPUUsage() - thr) > 0 &&
                fcmp(pm->getMemoryUsage() - thr) > 0) {
                auto outPMDeployedVMNums = pm->getAllDeployedVMs().size();
                selectVmCnt += outPMDeployedVMNums;
                pmIdSelectedList[i].push_back(outPMId);
                migrateSimulationServer.getServerShadow(pm);
                if (selectVmCnt * migRate >= limit)
                    break;
            }
        }

        int fixVMSize = 0;
        do {
            // fprintf(stderr, "new shake round\n");

            fixVMSize = shakeFixVM[0].size() + shakeFixVM[1].size();

            migCnt = 0;
            migrateSimulationServer.resetAll();
            for (int i = 0; i < 2; i++) {
                shakeOrder[i].clear();
                shakeLink[i].clear();
                shakeType[i].clear();
            }

            sortPMByScaleRev(pmIdSelectedList[0]);
            sortPMByScaleRev(pmIdSelectedList[1]);

            for (int i = 0; i < 2; i++) {
                vmIdSet[i].clear();
                for (auto &curPM : pmIdSelectedList[i]) {
                    auto vmList =
                        migrateSimulationServer.getServerShadow(curPM)
                            ->getAllDeployedVMs();
                    for (auto [vm, deployNode] : vmList) {
                        vmIdSet[i].insert(vm->id);
                    }
                }

                for (auto fixVM : shakeFixVM[i]) {
                    if (vmIdSet[i].find(fixVM) != vmIdSet[i].end()) {
                        vmIdSet[i].erase(fixVM);
                    }
                }
            }

            int nodeType = 1;
            for (auto &curPMID : pmIdSelectedList[nodeType]) {
                if (migCnt >= limit || finFlag)
                    break;
                bool canDeployFlag = false;
                int minRemainResourceWeightedSum = INT32_MAX;
                int minVMId = -1;
                auto curPM = migrateSimulationServer.getServerShadow(curPMID);

                do {
                    if (migCnt >= limit || finFlag)
                        break;
                    canDeployFlag = false;
                    minRemainResourceWeightedSum = INT32_MAX;
                    minVMId = -1;
                    for (auto &curVM : vmIdSet[nodeType]) {
                        if (curPM->canDeployVM(VM::getVM(curVM)) &&
                            migrateSimulationServer.getDeployServer(curVM)
                                    ->id != curPMID) {
                            int curRemainResourceWeightedSum =
                                getRemainResourceWeightedSum(
                                    curPM, VM::getVM(curVM), Server::DUAL_NODE);
                            if (curRemainResourceWeightedSum <
                                minRemainResourceWeightedSum) {
                                minRemainResourceWeightedSum =
                                    curRemainResourceWeightedSum;
                                minVMId = curVM;
                            }
                            canDeployFlag = true;
                        }
                    }

                    if (canDeployFlag) {
                        auto outPM =
                            migrateSimulationServer.getDeployServer(minVMId);
                        shakeOrder[0].push_back(minVMId);
                        shakeLink[0][minVMId] =
                            std::make_pair(outPM->id, curPMID);
                        shakeType[0][minVMId] = Server::DUAL_NODE;
                        migCnt++;
                        placeVMShadow(migrateSimulationServer, outPM->id,
                                      curPMID, minVMId, Server::DUAL_NODE);
                        vmIdSet[nodeType].erase(minVMId);
                        // migrationList.emplace_back(minVMId, curPM,
                        // Server::DUAL_NODE);
                    }
                } while (canDeployFlag);
                if (migCnt >= limit || finFlag)
                    break;

                auto vmList = curPM->getAllDeployedVMs();
                for (auto [vm, deployNode] : vmList) {
                    if (vmIdSet[nodeType].find(vm->id) !=
                        vmIdSet[nodeType].end()) {
                        vmIdSet[nodeType].erase(vm->id);
                    }
                }
            }

            nodeType = 0;
            for (auto &curPMID : pmIdSelectedList[nodeType]) {
                if (migCnt >= limit || finFlag)
                    break;
                bool canDeployFlag = false;
                int minRemainResourceWeightedSum = INT32_MAX;
                int minVMId = -1;
                ServerType::DeployNode minType;
                auto curPM = migrateSimulationServer.getServerShadow(curPMID);

                do {
                    if (migCnt >= limit || finFlag)
                        break;
                    canDeployFlag = false;
                    minRemainResourceWeightedSum = INT32_MAX;
                    minVMId = -1;
                    for (auto &curVM : vmIdSet[nodeType]) {
                        if (curPM->canDeployVM(VM::getVM(curVM),
                                               Server::NODE_0) &&
                            (migrateSimulationServer.getDeployServer(curVM)
                                    ->id != curPMID /*|| migrateSimulationServer.getDeployInfo(curVM).second != Server::NODE_0*/)) {
                            int curRemainResourceWeightedSum =
                                getRemainResourceWeightedSum(
                                    curPM, VM::getVM(curVM), Server::NODE_0);
                            if (curRemainResourceWeightedSum <
                                minRemainResourceWeightedSum) {
                                minRemainResourceWeightedSum =
                                    curRemainResourceWeightedSum;
                                minVMId = curVM;
                                minType = Server::NODE_0;
                            }
                            canDeployFlag = true;
                        }
                        if (curPM->canDeployVM(VM::getVM(curVM),
                                               Server::NODE_1) &&
                            (migrateSimulationServer.getDeployServer(curVM)
                                    ->id != curPMID /*|| migrateSimulationServer.getDeployInfo(curVM).second != Server::NODE_1*/)) {
                            int curRemainResourceWeightedSum =
                                getRemainResourceWeightedSum(
                                    curPM, VM::getVM(curVM), Server::NODE_1);
                            if (curRemainResourceWeightedSum <
                                minRemainResourceWeightedSum) {
                                minRemainResourceWeightedSum =
                                    curRemainResourceWeightedSum;
                                minVMId = curVM;
                                minType = Server::NODE_1;
                            }
                            canDeployFlag = true;
                        }
                    }

                    if (canDeployFlag) {
                        auto outPM =
                            migrateSimulationServer.getDeployServer(minVMId);
                        shakeOrder[0].push_back(minVMId);
                        shakeLink[0][minVMId] =
                            std::make_pair(outPM->id, curPMID);
                        shakeType[0][minVMId] = minType;
                        migCnt++;
                        placeVMShadow(migrateSimulationServer, outPM->id,
                                      curPMID, minVMId, minType);
                        vmIdSet[nodeType].erase(minVMId);
                        // migrationList.emplace_back(minVMId, curPM, minType);
                    }
                } while (canDeployFlag);
                if (migCnt >= limit || finFlag)
                    break;

                auto vmList = curPM->getAllDeployedVMs();
                for (auto [vm, deployNode] : vmList) {
                    if (vmIdSet[nodeType].find(vm->id) !=
                        vmIdSet[nodeType].end()) {
                        vmIdSet[nodeType].erase(vm->id);
                    }
                }
            }

            //************************* Shake back *************************

            sortPMByScale(pmIdSelectedList[0]);
            sortPMByScale(pmIdSelectedList[1]);

            for (int i = 0; i < 2; i++) {
                vmIdSet[i].clear();
                for (auto &curPM : pmIdSelectedList[i]) {
                    auto vmList =
                        migrateSimulationServer.getServerShadow(curPM)
                            ->getAllDeployedVMs();
                    for (auto [vm, deployNode] : vmList) {
                        vmIdSet[i].insert(vm->id);
                    }
                }

                for (auto fixVM : shakeFixVM[i]) {
                    if (vmIdSet[i].find(fixVM) != vmIdSet[i].end()) {
                        vmIdSet[i].erase(fixVM);
                    }
                }
            }

            nodeType = 1;
            for (auto &curPMID : pmIdSelectedList[nodeType]) {
                if (migCnt >= limit || finFlag)
                    break;
                bool canDeployFlag = false;
                int minRemainResourceWeightedSum = INT32_MAX;
                int minVMId = -1;
                auto curPM = migrateSimulationServer.getServerShadow(curPMID);

                do {
                    if (migCnt >= limit || finFlag)
                        break;
                    canDeployFlag = false;
                    minRemainResourceWeightedSum = INT32_MAX;
                    minVMId = -1;
                    for (auto &curVM : vmIdSet[nodeType]) {
                        if (curPM->canDeployVM(VM::getVM(curVM)) &&
                            migrateSimulationServer.getDeployServer(curVM)
                                    ->id != curPMID) {
                            int curRemainResourceWeightedSum =
                                getRemainResourceWeightedSum(
                                    curPM, VM::getVM(curVM), Server::DUAL_NODE);
                            if (curRemainResourceWeightedSum <
                                minRemainResourceWeightedSum) {
                                minRemainResourceWeightedSum =
                                    curRemainResourceWeightedSum;
                                minVMId = curVM;
                            }
                            canDeployFlag = true;
                        }
                    }

                    if (canDeployFlag) {
                        auto outPM =
                            migrateSimulationServer.getDeployServer(minVMId);
                        if (shakeLink[0].find(minVMId) != shakeLink[0].end() &&
                            curPMID == shakeLink[0][minVMId].first &&
                            Server::DUAL_NODE == shakeType[0][minVMId]) {
                            shakeLink[0].erase(minVMId);
                            shakeFixVM[nodeType].insert(minVMId);
                            migCnt--;
                        } else {
                            shakeOrder[1].push_back(minVMId);
                            shakeLink[1][minVMId] =
                                std::make_pair(outPM->id, curPMID);
                            shakeType[1][minVMId] = Server::DUAL_NODE;
                            migCnt++;
                        }
                        placeVMShadow(migrateSimulationServer, outPM->id,
                                      curPMID, minVMId, Server::DUAL_NODE);
                        vmIdSet[nodeType].erase(minVMId);
                        // migrationList.emplace_back(minVMId, curPM,
                        // Server::DUAL_NODE);
                    }
                } while (canDeployFlag);
                if (migCnt >= limit || finFlag)
                    break;

                auto vmList = migrateSimulationServer.getServerShadow(curPM)
                                  ->getAllDeployedVMs();
                for (auto [vm, deployNode] : vmList) {
                    if (vmIdSet[nodeType].find(vm->id) !=
                        vmIdSet[nodeType].end()) {
                        vmIdSet[nodeType].erase(vm->id);
                    }
                }
            }

            nodeType = 0;
            for (auto &curPMID : pmIdSelectedList[nodeType]) {
                if (migCnt >= limit || finFlag)
                    break;
                bool canDeployFlag = false;
                int minRemainResourceWeightedSum = INT32_MAX;
                int minVMId = -1;
                ServerType::DeployNode minType;
                auto curPM = migrateSimulationServer.getServerShadow(curPMID);

                do {
                    if (migCnt >= limit || finFlag)
                        break;
                    canDeployFlag = false;
                    minRemainResourceWeightedSum = INT32_MAX;
                    minVMId = -1;
                    for (auto &curVM : vmIdSet[nodeType]) {
                        if (curPM->canDeployVM(VM::getVM(curVM),
                                               Server::NODE_0) &&
                            (migrateSimulationServer.getDeployServer(curVM)
                                    ->id != curPMID  /*|| migrateSimulationServer.getDeployInfo(curVM).second != Server::NODE_0*/)) {
                            int curRemainResourceWeightedSum =
                                getRemainResourceWeightedSum(
                                    curPM, VM::getVM(curVM), Server::NODE_0);
                            if (curRemainResourceWeightedSum <
                                minRemainResourceWeightedSum) {
                                minRemainResourceWeightedSum =
                                    curRemainResourceWeightedSum;
                                minVMId = curVM;
                                minType = Server::NODE_0;
                            }
                            canDeployFlag = true;
                        }
                        if (curPM->canDeployVM(VM::getVM(curVM),
                                               Server::NODE_1) && (
                            migrateSimulationServer.getDeployServer(curVM)
                                    ->id != curPMID /*|| migrateSimulationServer.getDeployInfo(curVM).second != Server::NODE_1*/)) {
                            int curRemainResourceWeightedSum =
                                getRemainResourceWeightedSum(
                                    curPM, VM::getVM(curVM), Server::NODE_1);
                            if (curRemainResourceWeightedSum <
                                minRemainResourceWeightedSum) {
                                minRemainResourceWeightedSum =
                                    curRemainResourceWeightedSum;
                                minVMId = curVM;
                                minType = Server::NODE_1;
                            }
                            canDeployFlag = true;
                        }
                    }

                    if (canDeployFlag) {
                        auto outPM =
                            migrateSimulationServer.getDeployServer(minVMId);
                        if (shakeLink[0].find(minVMId) != shakeLink[0].end() &&
                            curPMID == shakeLink[0][minVMId].first &&
                            minType == shakeType[0][minVMId]) {
                            shakeLink[0].erase(minVMId);
                            shakeFixVM[nodeType].insert(minVMId);
                            migCnt--;
                        } else {
                            shakeOrder[1].push_back(minVMId);
                            shakeLink[1][minVMId] =
                                std::make_pair(outPM->id, curPMID);
                            shakeType[1][minVMId] = minType;
                            migCnt++;
                        }
                        placeVMShadow(migrateSimulationServer, outPM->id,
                                      curPMID, minVMId, minType);
                        vmIdSet[nodeType].erase(minVMId);
                        // migrationList.emplace_back(minVMId, curPM, minType);
                    }
                } while (canDeployFlag);
                if (migCnt >= limit || finFlag)
                    break;

                auto vmList = migrateSimulationServer.getServerShadow(curPM)
                                  ->getAllDeployedVMs();
                for (auto [vm, deployNode] : vmList) {
                    if (vmIdSet[nodeType].find(vm->id) !=
                        vmIdSet[nodeType].end()) {
                        vmIdSet[nodeType].erase(vm->id);
                    }
                }
            }
        } while (shakeFixVM[0].size() + shakeFixVM[1].size() > fixVMSize);

        for (auto &migVM : shakeOrder[0]) {
            if (shakeLink[0].find(migVM) != shakeLink[0].end() &&
                shakeType[0].find(migVM) != shakeType[0].end()) {
                placeVM(shakeLink[0][migVM].first, shakeLink[0][migVM].second,
                        migVM, shakeType[0][migVM]);
                migrationList.emplace_back(migVM, shakeLink[0][migVM].second,
                                           shakeType[0][migVM]);
#ifdef TEST2
                fprintf(stderr, "migemp 0 %d, %d, %d\n", migVM,
                        shakeLink[0][migVM].second, shakeType[0][migVM]);
#endif
            }
        }

        for (auto &migVM : shakeOrder[1]) {
            if (shakeLink[1].find(migVM) != shakeLink[1].end() &&
                shakeType[1].find(migVM) != shakeType[1].end()) {
                placeVM(shakeLink[1][migVM].first, shakeLink[1][migVM].second,
                        migVM, shakeType[1][migVM]);
                migrationList.emplace_back(migVM, shakeLink[1][migVM].second,
                                           shakeType[1][migVM]);
#ifdef TEST2
                fprintf(stderr, "migemp 1 %d, %d, %d\n", migVM,
                        shakeLink[1][migVM].second, shakeType[1][migVM]);
#endif
            }
        }

#ifdef TEST
        std::clog << "limit: " << limit << " migCnt: " << migCnt << std::endl;
#endif

        return migCnt;
    }

    int clearHighExpensesPMs(
        int day, int limit,
        std::vector<std::tuple<int, int, Server::DeployNode>> &migrationList) {
        std::vector<int> emptyPMIdList[2];
        std::vector<int> nonEmptyPMIdList[2];
        std::vector<int> highExpPMIdList[2];

        if (limit <= 0)
            return 0;

        int emptyCnt = 0;

        for (int nodeType = 0; nodeType < 2; nodeType++) {
            for (auto &pm : aliveMachineList[nodeType]) {
                if (pm.second->empty()) {
                    emptyPMIdList[nodeType].push_back(pm.first);
                    emptyCnt++;
                } else {
                    nonEmptyPMIdList[nodeType].push_back(pm.first);
                }
            }
        }

        if (emptyCnt * 20 < Server::getServerCount())
            return 0;

        for (int nodeType = 0; nodeType < 2; nodeType++) {
            sortPMByDailyCost(emptyPMIdList[nodeType]);
            sortPMByDailyCost(nonEmptyPMIdList[nodeType]);
        }

        int ableToMigCnt[2] = {(limit * (int)emptyPMIdList[0].size()) / emptyCnt,
                               (limit * (int)emptyPMIdList[1].size()) / emptyCnt};
        int migCnt = 0;
        // not used
        const int pmPoolLimit = 5;
        const int ignoreMigTimesLimit = 10;

        for (int nodeType = 0; nodeType < 2 && migCnt < limit; nodeType++) {
            std::set<int> vmPool;
            std::set<int> pmPool;
            bool noMoreMig[10] = {false};

            for (int pmIndex = 0; pmIndex < emptyPMIdList[nodeType].size() &&
                                  ableToMigCnt[nodeType] > ignoreMigTimesLimit;
                 pmIndex++) {
                auto curPMId = emptyPMIdList[nodeType][pmIndex];
                auto curPM = Server::getServer(curPMId);
                if (noMoreMig[curPM->category])
                    continue;

                auto canDeployFlag = false;
                int minRemainResourceWeightedSum = INT32_MAX;
                auto minVMId = -1;
                auto minType = Server::DUAL_NODE;
                int pmPoolIndex = nonEmptyPMIdList[nodeType].size();

                pmPool.clear();
                vmPool.clear();

                do {
                    for (; pmPool.size() < pmPoolLimit &&
                           !noMoreMig[curPM->category];) {
                        int curHighExpPMId = 0;
                        Server *curHighExpPM = nullptr;
                        do {
                            pmPoolIndex--;
                            if (pmPoolIndex < 0) break;
                            curHighExpPMId =
                                nonEmptyPMIdList[nodeType][pmPoolIndex];
                            curHighExpPM = Server::getServer(curHighExpPMId);
                        } while (curHighExpPM->empty() ||
                                 curPM->category != curHighExpPM->category);

                        if(pmPoolIndex < 0){
                            noMoreMig[curPM->category] = true;
                            break;
                        }

                        if (((curHighExpPM->hardwareCost / curHighExpPM->energyCost) >> pmCatHERateSpace) >=
                            ((curPM->hardwareCost / curPM->energyCost) >> pmCatHERateSpace)) {
                            // fprintf(stderr, "no more ECR diff\n");
                            noMoreMig[curPM->category] = true;
                            break;
                        }
                        pmPool.insert(curHighExpPMId);
                        for (auto &vm : Server::getServer(curHighExpPMId)
                                            ->getAllDeployedVMs()) {
                            vmPool.insert(vm.first->id);
                        }
                        // fprintf(stderr, "pool state %d %d\n",pmPool.size(),
                        // vmPool.size());
                    }

                    if (nodeType == 1) {
                        canDeployFlag = false;
                        minRemainResourceWeightedSum = INT32_MAX;
                        minVMId = -1;

                        for (auto &curVM : vmPool) {
                            if (curPM->canDeployVM(VM::getVM(curVM)) &&
                                Server::getDeployServer(curVM)->id != curPMId) {
                                int curRemainResourceWeightedSum =
                                    getRemainResourceWeightedSum(
                                        curPM, VM::getVM(curVM),
                                        Server::DUAL_NODE);
                                if (curRemainResourceWeightedSum <
                                    minRemainResourceWeightedSum) {
                                    minRemainResourceWeightedSum =
                                        curRemainResourceWeightedSum;
                                    minVMId = curVM;
                                    canDeployFlag = true;
                                }
                            }
                        }

                        if (canDeployFlag) {
                            auto outPM = Server::getDeployServer(minVMId);
                            migCnt++;
                            ableToMigCnt[nodeType]--;
                            vmPool.erase(minVMId);
                            placeVM(outPM->id, curPMId, minVMId,
                                    Server::DUAL_NODE);
                            migrationList.emplace_back(minVMId, curPMId,
                                                       Server::DUAL_NODE);
                        }
                    } else if (nodeType == 0) {
                        canDeployFlag = false;
                        minRemainResourceWeightedSum = INT32_MAX;
                        minVMId = -1;

                        for (auto &curVM : vmPool) {
                            if (curPM->canDeployVM(VM::getVM(curVM),
                                                   Server::NODE_0) &&
                                (Server::getDeployServer(curVM)->id != curPMId || Server::getDeployInfo(curVM).second != Server::NODE_0)) {
                                int curRemainResourceWeightedSum =
                                    getRemainResourceWeightedSum(
                                        curPM, VM::getVM(curVM),
                                        Server::NODE_0);
                                if (curRemainResourceWeightedSum <
                                    minRemainResourceWeightedSum) {
                                    minRemainResourceWeightedSum =
                                        curRemainResourceWeightedSum;
                                    minVMId = curVM;
                                    minType = Server::NODE_0;
                                    canDeployFlag = true;
                                }
                            }
                            if (curPM->canDeployVM(VM::getVM(curVM),
                                                   Server::NODE_1) &&
                                (Server::getDeployServer(curVM)->id != curPMId || Server::getDeployInfo(curVM).second != Server::NODE_1)) {
                                int curRemainResourceWeightedSum =
                                    getRemainResourceWeightedSum(
                                        curPM, VM::getVM(curVM),
                                        Server::NODE_1);
                                if (curRemainResourceWeightedSum <
                                    minRemainResourceWeightedSum) {
                                    minRemainResourceWeightedSum =
                                        curRemainResourceWeightedSum;
                                    minVMId = curVM;
                                    minType = Server::NODE_1;
                                    canDeployFlag = true;
                                }
                            }
                        }

                        if (canDeployFlag) {
                            auto outPM = Server::getDeployServer(minVMId);
                            migCnt++;
                            ableToMigCnt[nodeType]--;
                            vmPool.erase(minVMId);
                            placeVM(outPM->id, curPMId, minVMId, minType);
                            migrationList.emplace_back(minVMId, curPMId,
                                                       minType);
                        }
                    }

                    std::vector<int> pmNeedDel;

                    for (auto &pm : pmPool) {
                        auto curHighExpPM = Server::getServer(pm);
                        if (curHighExpPM->empty()) {
                            pmNeedDel.push_back(pm);
                        }
                    }
                    for (auto &pm : pmNeedDel){
                        pmPool.erase(pm);
                    }

                } while (canDeployFlag && ableToMigCnt[nodeType] > 0);
            }
        }

#ifdef TEST
        std::clog << "limit: " << limit << " migCnt: " << migCnt << std::endl;
#endif

        return migCnt;
    }

  private:
    // **????????????**
    // ????????????????????????????????????????????????
    // TODO:
    volatile const double MEMORY_PARA = 0.47;
    // FIXME:
    volatile const double FIND_PM_REMAIN_MEMORY_WRIGHT[5] = {0.55, 0.55, 0.55, 0.55,
                                                             0.55};
    volatile const double FIND_PM_REMAIN_CPU_WRIGHT[5] = {1, 1, 1, 1, 1};

    int emptyPMCnt = 0;

    std::unordered_map<int, Server *> *aliveMachineList;
    BinIndexTree treeArray[2];

    int sortPMByRemainAmount(VMType::DeployType deployType,
                             std::vector<int> &pmIdList) {
        std::sort(pmIdList.begin(), pmIdList.end(),
                  [deployType, this](int &pmIdi, int &pmIdj) {
                      auto pmi = aliveMachineList[deployType][pmIdi];
                      auto pmj = aliveMachineList[deployType][pmIdj];

                      return fcmp((pmi->getLeftCPU(Server::NODE_0) +
                                   pmi->getLeftCPU(Server::NODE_1) +
                                   (pmi->getLeftMemory(Server::NODE_0) +
                                    pmi->getLeftMemory(Server::NODE_1)) *
                                       MEMORY_PARA) -
                                  (pmj->getLeftCPU(Server::NODE_0) +
                                   pmj->getLeftCPU(Server::NODE_1) +
                                   (pmj->getLeftMemory(Server::NODE_0) +
                                    pmj->getLeftMemory(Server::NODE_1)) *
                                       MEMORY_PARA)) > 0;
                  });
        treeArray[deployType].setSize(pmIdList.size());
        for (int i = 1; i <= pmIdList.size(); i++) {
            Server *pm =
                aliveMachineList[deployType][pmIdList[pmIdList.size() - i]];
            if (deployType == VMType::SINGLE) {
                treeArray[deployType].update(
                    i, std::make_pair(
                           std::max(pm->getLeftCPU(Server::NODE_0),
                                    pm->getLeftCPU(Server::NODE_1)),
                           std::max(pm->getLeftMemory(Server::NODE_0),
                                    pm->getLeftMemory(Server::NODE_1))));
            } else {
                treeArray[deployType].update(
                    i, std::make_pair(pm->getLeftCPU(Server::NODE_0),
                                      pm->getLeftMemory(Server::NODE_0)));
            }
        }
        return pmIdList.size();
    }

    int sortPMByDailyCost(std::vector<int> &pmIdList) {
        std::sort(pmIdList.begin(), pmIdList.end(),
                  [this](int &pmIdi, int &pmIdj) {
                      auto pmi = Server::getServer(pmIdi);
                      auto pmj = Server::getServer(pmIdj);

                      return (pmi->hardwareCost) * (pmj->energyCost) >
                             (pmi->energyCost) * (pmj->hardwareCost);
                  });

        return pmIdList.size();
    }

    int sortPMByDailyCostRev(std::vector<int> &pmIdList) {
        std::sort(pmIdList.begin(), pmIdList.end(),
                  [this](int &pmIdi, int &pmIdj) {
                      auto pmi = Server::getServer(pmIdi);
                      auto pmj = Server::getServer(pmIdj);

                      return (pmi->hardwareCost) * (pmj->energyCost) <
                             (pmi->energyCost) * (pmj->hardwareCost);
                  });

        return pmIdList.size();
    }

    int sortPMByScale(std::vector<int> &pmIdList) {
        std::sort(pmIdList.begin(), pmIdList.end(),
                  [this](int &pmIdi, int &pmIdj) {
                      auto pmi = Server::getServer(pmIdi);
                      auto pmj = Server::getServer(pmIdj);

                      return fcmp((pmi->cpu + (pmi->memory) * MEMORY_PARA) -
                                  (pmj->cpu + (pmj->memory) * MEMORY_PARA)) > 0;
                  });

        return pmIdList.size();
    }

    int sortPMByScaleRev(std::vector<int> &pmIdList) {
        std::sort(pmIdList.begin(), pmIdList.end(),
                  [this](int &pmIdi, int &pmIdj) {
                      auto pmi = Server::getServer(pmIdi);
                      auto pmj = Server::getServer(pmIdj);

                      return fcmp((pmi->cpu + (pmi->memory) * MEMORY_PARA) -
                                  (pmj->cpu + (pmj->memory) * MEMORY_PARA)) < 0;
                  });

        return pmIdList.size();
    }

    int sortPMByUsedAmount(VMType::DeployType deployType,
                           std::vector<int> &pmIdList) {
        std::sort(pmIdList.begin(), pmIdList.end(),
                  [deployType, this](int &pmIdi, int &pmIdj) {
                      auto pmi = aliveMachineList[deployType][pmIdi];
                      auto pmj = aliveMachineList[deployType][pmIdj];
                        if(pmi->isHigh != pmj->isHigh){
                            return pmi->isHigh;
                        }

                      return fcmp((pmi->cpu - pmi->getLeftCPU() +
                                   (pmi->memory - pmi->getLeftMemory()) *
                                       MEMORY_PARA) -
                                  (pmj->cpu - pmj->getLeftCPU() +
                                   (pmj->memory - pmj->getLeftMemory()) *
                                       MEMORY_PARA)) < 0;
                  });

        return pmIdList.size();
    }

    volatile int
    getRemainResourceWeightedSum(Server *pm, VM *vm,
                                 Server::DeployNode dn = Server::DUAL_NODE) {
        volatile double diff = std::abs(
            (double)(pm->getLeftCPU(dn) - vm->cpu) / (pm->cpu) -
            (double)(pm->getLeftMemory(dn) - vm->memory) / (pm->memory));
        volatile const double arg1 = 10;
        const int arg2 = 3;

        return ((pm->getLeftCPU(dn) - vm->cpu) *
                    FIND_PM_REMAIN_CPU_WRIGHT[vm->category] +
                (pm->getLeftMemory(dn) - vm->memory) *
                    FIND_PM_REMAIN_MEMORY_WRIGHT[vm->category]) *
               (1 + pow(diff * arg1, arg2));

        // return std::abs((pm->getLeftCPU(dn) - vm->cpu) -
        // (pm->getLeftMemory(dn) - vm->memory));
    }

    int compareAliveM(Server *nowNode, Server *lastNode, VM *vm, Server::DeployNode deployNode,
                             Server::DeployNode lastDeployNode = Server::NODE_0) {
        if (nowNode->getCategory(deployNode) != lastNode->getCategory(lastDeployNode)) {
            if (nowNode->getCategory(deployNode) == vm->category) {
                return -1;
            }
            if (lastNode->getCategory(lastDeployNode) == vm->category) {
                return 1;
            }
            if (vm->category == Category::MORE_CPU && lastNode->getCategory(lastDeployNode) == Category::MORE_MEMORY) {
                return -1;
            }
            if (vm->category == Category::MORE_CPU && nowNode->getCategory(deployNode) == Category::MORE_MEMORY) {
                return 1;
            }
            if (vm->category == Category::MORE_MEMORY && lastNode->getCategory(lastDeployNode) == Category::MORE_CPU) {
                return -1;
            }
            if (vm->category == Category::MORE_MEMORY && nowNode->getCategory(deployNode) == Category::MORE_CPU) {
                return 1;
            }
        }
        if (lastNode->empty() != nowNode->empty()) {
            if (nowNode->empty()) return 1;
            if (lastNode->empty()) return -1;
        }

        if (lastNode->empty() && nowNode->empty()) {
            if (lastNode->energyCost < nowNode->energyCost) {
                return 1;
            }
            if (lastNode->energyCost > nowNode->energyCost) {
                return -1;
            }
        }

        if (deployNode == Server::DeployNode::DUAL_NODE) {
            if (fcmp(nowNode->getLeftCPU(Server::NODE_0) - lastNode->getLeftCPU(Server::NODE_0)) <= 0 &&
                fcmp(nowNode->getLeftMemory(Server::NODE_0) - lastNode->getLeftMemory(Server::NODE_0)) <= 0) {
                return -1;
            }
            if (fcmp(nowNode->getLeftCPU(Server::NODE_0) - lastNode->getLeftCPU(Server::NODE_0)) > 0 &&
                fcmp(nowNode->getLeftMemory(Server::NODE_0) - lastNode->getLeftMemory(Server::NODE_0)) > 0) {
                return 1;
            }
            volatile double k = (double) vm->cpu / vm->memory;
            if (fcmp((nowNode->getLeftCPU(Server::NODE_0) + nowNode->getLeftMemory(Server::NODE_0) * k) -
                     (lastNode->getLeftCPU(Server::NODE_0) + lastNode->getLeftMemory(Server::NODE_0) * k)) <= 0) {
                return -1;
            }
            return 1;
            /*
            if (getRemainResourceWeightedSum(lastNode, vm, lastDeployNode) > getRemainResourceWeightedSum(nowNode, vm, deployNode)){
                return -1;
            }else{
                return 1;
            }
            */
        } else {
            int nowCPU, nowMem, lastCPU, lastMem;
            if (deployNode == Server::NODE_0) {
                nowCPU = nowNode->getLeftCPU(Server::NODE_0);
                nowMem = nowNode->getLeftMemory(Server::NODE_0);
            } else {
                nowCPU = nowNode->getLeftCPU(Server::NODE_1);
                nowMem = nowNode->getLeftMemory(Server::NODE_1);
            }
            if (lastDeployNode == Server::NODE_0) {
                lastCPU = lastNode->getLeftCPU(Server::NODE_0);
                lastMem = lastNode->getLeftMemory(Server::NODE_0);
            } else {
                lastCPU = lastNode->getLeftCPU(Server::NODE_1);
                lastMem = lastNode->getLeftMemory(Server::NODE_1);
            }
            if (nowCPU <= lastCPU && nowMem <= lastMem) {
                return -1;
            }
            if (nowCPU > lastCPU && nowMem > lastMem) {
                return 1;
            }
            volatile double k = (double) vm->cpu / vm->memory;
            if (fcmp((nowCPU + nowMem * k) - (lastCPU + lastMem * k)) <= 0) {
                return -1;
            }
            return 1;
            /*
            if (getRemainResourceWeightedSum(lastNode, vm, lastDeployNode) > getRemainResourceWeightedSum(nowNode, vm, deployNode)){
                return -1;
            }else{
                return 1;
            }
            */
        }
    }

    int findPM(std::vector<int> &pmIdList, int outPmID, VM *vm, VMType::DeployType deployType, int &type, int &pos) {
        int targetPMId = -1;
        ServerType::DeployNode targetType = ServerType::DUAL_NODE;
        int position = -1;
        int L = 0, R = pmIdList.size() - 1;
        int ans = pmIdList.size();
        while (L <= R) {
            int mid = (L + R) >> 1;
            auto retRes = treeArray[deployType].query(mid);
            if ((deployType == VMType::SINGLE && retRes.first >= vm->cpu &&
                 retRes.second >= vm->memory) ||
                (deployType == VMType::DUAL && retRes.first >= vm->cpu / 2 &&
                 retRes.second >= vm->memory / 2)) {
                ans = mid;
                R = mid - 1;
            } else {
                L = mid + 1;
            }
        }

        auto outPM = aliveMachineList[deployType][outPmID];
        // int curMinimalRemainder = getRemainResourceWeightedSum(outPM, vm,
        // Server::getDeployInfo(vm->id).second);
        int curMinimalRemainder = INT32_MAX;
        int minusCnt = 0;
        int findCnt = 0;

        if (deployType == VMType::DUAL) {
            for (int i = pmIdList.size() - 1 - ans; i >= 0; i--) {
                auto curPMId =
                    static_cast<std::vector<int>::iterator>(&pmIdList[i]);
                // avoid migrating vm to even lower load machines or to the same
                // machine
                if (*curPMId == outPmID)
                    continue;

                auto curPM = aliveMachineList[deployType][*curPMId];
                // fprintf(stderr,"curpmP %d %d
                // %d\n",*curPMId,curPM->getLeftCPU(),curPM->getLeftMemory());
                // avoid startup an empty machine
                if (curPM->empty())
                    continue;

                int remainResourceWeightedSum = INT32_MAX;
                if (curPM->canDeployVM(vm)) {
                    if (curPM->isHigh && !outPM->isHigh){
                        continue;
                    }
                    // FIXME:
                    /*
                    if (curPM->getCategory(Server::DUAL_NODE) == vm->category) {
                        remainResourceWeightedSum =
                            getRemainResourceWeightedSum(curPM, vm);
                        // findCnt++;
                        // step = 1;
                        if (remainResourceWeightedSum < curMinimalRemainder) {
                            targetPMId = curPM->id;
                            targetType = Server::DUAL_NODE;
                            position = i;
                            curMinimalRemainder = remainResourceWeightedSum;
                            minusCnt++;
                        }
                    }
                    */
                   if (targetPMId < 0 || compareAliveM(curPM, Server::getServer(targetPMId), vm, ServerType::DUAL_NODE, ServerType::DUAL_NODE) < 0){
                       targetPMId = curPM->id;
                       targetType = ServerType::DUAL_NODE;
                       position = i;
                       minusCnt++;
                   }
                }
            }
        } else if (deployType == VMType::SINGLE) {
            for (int i = pmIdList.size() - 1 - ans; i >= 0; i--) {
                auto curPMId =
                    static_cast<std::vector<int>::iterator>(&pmIdList[i]);
                // avoid migrating vm to even lower load machines or to the same
                // machine

                auto curPM = aliveMachineList[deployType][*curPMId];
                // avoid startup an empty machine
                if (curPM->empty())
                    continue;
                if (curPM->isHigh && !outPM->isHigh){
                    continue;
                }

                int remainResourceWeightedSum = INT32_MAX;

                bool FlagA = false, FlagB = false;
                if (curPM->canDeployVM(vm, Server::NODE_0)) {
                    if (curPM->getCategory(Server::NODE_0) == vm->category) {
                        if (*curPMId != outPmID ||
                            Server::getDeployInfo(vm->id).second !=
                                Server::NODE_0) {
                            FlagA = true;
                        }
                        // step = 1;
                    }
                }
                if (curPM->canDeployVM(vm, Server::NODE_1)) {
                    if (curPM->getCategory(Server::NODE_1) == vm->category) {
                        if (*curPMId != outPmID ||
                            Server::getDeployInfo(vm->id).second !=
                                Server::NODE_1) {
                            FlagB = true;
                        }
                        // step = 1;
                    }
                }

                if (FlagA) {
                    //findCnt++;
                    
                    remainResourceWeightedSum =
                        getRemainResourceWeightedSum(curPM, vm, Server::NODE_0);
                    // FIXME:
                    if (remainResourceWeightedSum < curMinimalRemainder) {
                        targetPMId = curPM->id;
                        targetType = Server::NODE_0;
                        position = i;
                        curMinimalRemainder = remainResourceWeightedSum;
                        minusCnt++;
                    }
                    /*
                    if (targetPMId < 0 || compareAliveM(curPM, Server::getServer(targetPMId), vm, ServerType::NODE_0, targetType) < 0){
                       targetPMId = curPM->id;
                       targetType = Server::NODE_0;
                       position = i;
                       minusCnt++;
                   }
                   */
                }
                if (FlagB) {
                    //findCnt++;
                    
                    remainResourceWeightedSum =
                        getRemainResourceWeightedSum(curPM, vm, Server::NODE_1);
                    // FIXME:
                    if (remainResourceWeightedSum < curMinimalRemainder) {
                        targetPMId = curPM->id;
                        targetType = Server::NODE_1;
                        position = i;
                        curMinimalRemainder = remainResourceWeightedSum;
                        minusCnt++;
                    }
                    /*
                    if (targetPMId < 0 || compareAliveM(curPM, Server::getServer(targetPMId), vm, ServerType::NODE_1, targetType) < 0){
                       targetPMId = curPM->id;
                       targetType = Server::NODE_1;
                       position = i;
                       minusCnt++;
                   }
                   */
                }
                // if(minusCnt > 3 ) break;
            }
        }

        type = targetType;
        pos = position;
        return targetPMId;
    }

    void placeVM(int outPmID, int inPmID, int vmID,
                 Server::DeployNode node = Server::DUAL_NODE) {
        // delete in old one
        auto vm = VM::getVM(vmID);

        Server::getServer(outPmID)->remove(vm);

        // add in new one
        Server::getServer(inPmID)->deploy(vm, node);
    }

    void placeVMShadow(ServerShadowFactory &serSim, int outPmID, int inPmID,
                       int vmID, Server::DeployNode node = Server::DUAL_NODE) {
        // delete in old one
        auto vm = VM::getVM(vmID);
        auto deployType = vm->deployType;

        auto pm = serSim.getServerShadow(outPmID);
        pm->remove(vm);

        // add in new one
        pm = serSim.getServerShadow(inPmID);
        pm->deploy(vm, node);

#ifdef TEST2
        auto outPm = serSim.getServerShadow(outPmID);
        auto inPm = serSim.getServerShadow(inPmID);
        fprintf(stderr, "sim: %d %d %d %d %lf %lf %lf %lf\n", node, outPmID,
                inPmID, vmID, 1 - outPm->getCPUUsage(),
                1 - outPm->getMemoryUsage(), 1 - inPm->getCPUUsage(),
                1 - inPm->getMemoryUsage());
#endif
    }
};
