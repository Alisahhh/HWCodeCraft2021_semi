//
// Created by kskun on 2021/3/25.
//
#pragma once

#include <algorithm>
#include <cstring>
#include <cmath>
#include <ctime>

#include "data.h"
#include "io.h"
#include "migrate.h"
#include "debug.h"

class Solver {
public:
    Solver() {
        io = new StdIO();
        migrator = new Migrator(aliveMachineList);
    }

    void solve() {
        // 初始化数据
        auto serverTypeList = io->readServerType();
        auto vmTypeList = io->readVMType();
        commonData = new CommonData(serverTypeList, vmTypeList);
        machineListForSort = serverTypeList;
#ifdef TEST
        std::clog << "COMMON DATA READ" << std::endl;
#endif

        auto dayCount = io->readDayCount();
        T = dayCount.first;
        // 预先读进来K天
        int K = dayCount.second;
#ifdef TEST
        std::clog << "T " << T << " K " << K << std::endl;
#endif
        /*queryListK.resize(K);
        for (int i = 1; i <= K; i++) {
#ifdef TEST
            std::clog << "READ DAY " << i << ", STORED IN " << i % K << std::endl;
#endif
            queryListK[i % K] = io->readDayQueries();
        }*/
        time_t startTime, endTime, timeSum = 0; // DEBUG USE
        time_t migTimeSum = 0; // DEBUG USE
        startTime = clock(); // DEBUG USE
        for (int day = 1; day <= T; day++) {
#ifdef TEST
            //if (day % 100 == 0) std::clog << "DAY " << day << std::endl;
            std::clog << "Day " << day << " ServCnt: " << Server::getServerCount() << " VMCnt: " << VM::getVMCount() << std::endl;
#endif

            // IO
            std::vector<Server *> purchaseList;
            std::vector<std::tuple<int, int, Server::DeployNode>> migrationList; // 0: vmID, 1: serverID, 2: deployNode
            std::vector<std::pair<Server *, Server::DeployNode>> deployList;

            // migration
            if (VM::getVMCount() > 34 * 6) {
                auto limit = VM::getVMCount() * 3 / 100; // 百分之3
                migrator->migrate(day, limit, migrationList);
            }

            //auto queryList = queryListK[day % K];
            auto queryList = io->readDayQueries();
            /*if (day != 1 && day + K - 1 <= T) {
                std::clog << "Day " << day << " read day " << day + K - 1 << std::endl;
                queryListK[(day + K - 1) % K] = io->readDayQueries();
            }*/
            std::vector<std::pair<VMType *, Query *>> addQueryLists[2];
            bool flagAddQueriesEmpty = true;
            for (const auto &query : queryList) {
                VMType *vmType = nullptr;
                if (query->type == Query::Type::ADD) vmType = commonData->getVMType(query->vmModel);

                // 先存一段 add 请求集中处理
                if (query->type == Query::Type::ADD) {
                    flagAddQueriesEmpty = false;
                    addQueryLists[vmType->deployType].emplace_back(vmType, query);
                }

                // 遇到一个 del 查询或当天操作处理完，统一处理目前存的 add 请求
                if (!flagAddQueriesEmpty &&
                    (query->type == Query::Type::DEL || query->id == (*queryList.rbegin())->id)) {
                    for (auto &addQueryList : addQueryLists) {
                        std::sort(addQueryList.begin(), addQueryList.end(), compareAddQuery);
                        handleAddQueries(addQueryList, purchaseList);
                    }

                    // IO
                    std::vector<std::pair<int, std::pair<Server *, Server::DeployNode>>> tmpDeployList;
                    for (const auto &addQueryList : addQueryLists) {
                        for (auto[vmType, query] : addQueryList) {
                            auto deployInfo = Server::getDeployInfo(query->vmID);
                            tmpDeployList.emplace_back(query->id, deployInfo);
                        }
                    }
                    std::sort(tmpDeployList.begin(), tmpDeployList.end());
                    for (const auto &deployInfo : tmpDeployList) {
                        deployList.push_back(deployInfo.second);
                    }

                    addQueryLists[0].clear();
                    addQueryLists[1].clear();
                    flagAddQueriesEmpty = true;
                }

                // 处理删除请求
                if (query->type == Query::Type::DEL) {
                    auto vm = VM::getVM(query->vmID);
                    auto server = Server::getDeployServer(query->vmID);
                    server->remove(vm);
                    VM::removeVM(vm->id);
                }
            }

            // 回收内存
            for (const auto &query : queryList) {
                delete query;
            }

            // IO
            io->writePurchaseList(purchaseList);
            io->writeMigrationList(migrationList);
            io->writeDeployList(deployList);
        }
#ifdef TEST
        endTime = clock();
        std::clog << "ALL TIME: ";
        LOG_TIME_SEC(endTime - startTime)
        std::clog << "MIG TIME: ";
        LOG_TIME_SEC(migTimeSum);
#endif
    }

private:
    int T;
    std::vector<std::vector<Query *>> queryListK;

    StdIO *io;
    CommonData *commonData;
    Migrator *migrator;

    // **参数说明**
    // 用于 compareAddQuery 对新增虚拟机请求排序的参数
    static constexpr double COMPARE_ADD_QUERY_RATIO = 1 / 2.2;

    static bool compareAddQuery(const std::pair<VMType *, Query *> &a, const std::pair<VMType *, Query *> &b) {
        auto vmA = a.first, vmB = b.first;
        if (vmA->category != vmB->category) {
            return vmA->category < vmB->category;
        }
        if (vmA->category == Category::SAME_LARGE) {
            return (vmA->memory + vmA->cpu) > (vmB->memory + vmB->cpu);
        } else if (vmA->category == Category::MORE_CPU) {
            return (vmA->memory + vmA->cpu * COMPARE_ADD_QUERY_RATIO) >
                   (vmB->memory + vmB->cpu * COMPARE_ADD_QUERY_RATIO);
        } else {
            return (vmA->memory * COMPARE_ADD_QUERY_RATIO + vmA->cpu) >
                   (vmB->memory * COMPARE_ADD_QUERY_RATIO + vmB->cpu);
        }
    }

    std::vector<ServerType *> machineListForSort;
    std::unordered_map<int, Server *> aliveMachineList[2];

    void handleAddQueries(const std::vector<std::pair<VMType *, Query *>> &addQueryList,
                          std::vector<Server *> &purchaseList) {
        bool canLocateFlag = false;

        calcDailyResource(addQueryList);

        for (auto it = addQueryList.begin(); it != addQueryList.end(); it++) {
            auto vmType = it->first;
            auto query = it->second;
            auto vm = VM::newVM(query->vmID, *vmType);
            if (it == addQueryList.begin() || vm->category != (it - 1)->first->category) {
                std::sort(machineListForSort.begin(), machineListForSort.end(),
                          [vm, &it, this](ServerType *a, ServerType *b) {
                              auto deployType = vm->deployType;
                              double k = dailyMaxCPUInPerType[deployType][vm->category] /
                                         dailyMaxMemInPerType[deployType][vm->category];
                              double k1 = a->cpu / a->memory;
                              double k2 = b->cpu / b->memory;

                              double absKa = fabs(k1 - k);
                              double absKb = fabs(k2 - k);
                              if (vm->category == Category::SAME_LARGE) {
                                  if (a->category != b->category) {
                                      if (a->category == vm->category) {
                                          return true;
                                      }
                                      if (b->category == vm->category) {
                                          return false;
                                      }
                                      return a->category < b->category;
                                  } else {
                                      if (absKa < 0.5 && absKb < 0.5) {
                                          return a->hardwareCost < b->hardwareCost;
                                      } else {
                                          return absKa < absKb;
                                      }
                                  }
                              } else if (vm->category == Category::MORE_CPU) {
                                  if (a->category != b->category) {
                                      if (a->category == vm->category) {
                                          return true;
                                      }
                                      if (b->category == vm->category) {
                                          return false;
                                      }
                                      return a->category < b->category;
                                  } else {
                                      if (absKa < 2 && absKb < 2) {
                                          return a->hardwareCost < b->hardwareCost;
                                      } else {
                                          return absKa < absKb;
                                      }
                                  }
                              } else if (vm->category == Category::MORE_MEMORY) {
                                  if (a->category != b->category) {
                                      if (a->category == vm->category) {
                                          return true;
                                      }
                                      if (b->category == vm->category) {
                                          return false;
                                      }
                                      return a->category < b->category;
                                  } else {
                                      if (absKa < 0.2 && absKb < 0.2) {
                                          return a->hardwareCost < b->hardwareCost;
                                      } else {
                                          return absKa < absKb;
                                      }
                                  }

                              }
                          });
            }
            canLocateFlag = false;
            if (vm->deployType == VMType::DeployType::DUAL) {
                Server *minAlivm = nullptr;
                for (auto top : aliveMachineList[vm->deployType]) {
                    auto aliveM = top.second;
                    if (aliveM->canDeployVM(vm)) {
                        if (!minAlivm || compareAliveM(aliveM, minAlivm, vm, Server::DUAL_NODE) < 0) {
                            minAlivm = aliveM;
                        }
                        canLocateFlag = true;
                    }

                }

                if (canLocateFlag) minAlivm->deploy(vm, Server::DUAL_NODE);

            } else {
                Server *minAlivm = nullptr;
                Server::DeployNode lastType = Server::DUAL_NODE;
                for (auto &top : aliveMachineList[vm->deployType]) {
                    auto aliveM = top.second;
                    int flagA = aliveM->canDeployVM(vm, Server::NODE_0);
                    int flagB = aliveM->canDeployVM(vm, Server::NODE_1);
                    if (flagA) {
                        if (!minAlivm || compareAliveM(aliveM, minAlivm, vm, Server::NODE_0, lastType) < 0) {
                            minAlivm = aliveM;
                            lastType = Server::NODE_0;
                        }
                        canLocateFlag = true;
                    }
                    if (flagB) {
                        if (!minAlivm || compareAliveM(aliveM, minAlivm, vm, Server::NODE_1, lastType) < 0) {
                            minAlivm = aliveM;
                            lastType = Server::NODE_1;
                        }
                        canLocateFlag = true;
                    }
                }
                if (canLocateFlag) minAlivm->deploy(vm, lastType);
            }


            if (!canLocateFlag) {
                ServerType *m;
                for (auto &am : machineListForSort) {
                    if (am->canDeployVM(vm)) {
                        m = am;
                        break;
                    }
                }
                Server *aliveM = Server::newServer(*m);
                aliveMachineList[vm->deployType][aliveM->id] = aliveM;
                purchaseList.push_back(aliveM);
                if (vm->deployType == VMType::DUAL) {
                    if (aliveM->canDeployVM(vm)) {
                        aliveM->deploy(vm);
                        canLocateFlag = true;
                    }
                } else {
                    int flagA = aliveM->canDeployVM(vm, Server::NODE_0);
                    int flagB = aliveM->canDeployVM(vm, Server::NODE_1);
                    if (flagA) {
                        aliveM->deploy(vm, Server::NODE_0);
                        canLocateFlag = true;
                    } else if (flagB) {
                        aliveM->deploy(vm, Server::NODE_1);
                        canLocateFlag = true;
                    }
                }
            }
        }
    }

    // 每天需要的CPU峰值
    // 0表示单节点，1表示双节点
    int dailyMaxCPUInPerType[2][5] = {};

    // 每天需要的Mem峰值
    // 0表示单节点，1表示双节点
    int dailyMaxMemInPerType[2][5] = {};

    int dailyMaxCPU[2];
    int dailyMaxMem[2];

    void calcDailyResource(const std::vector<std::pair<VMType *, Query *>> &addQueryList) {
        memset(dailyMaxCPU, 0, sizeof(dailyMaxCPU));
        memset(dailyMaxMem, 0, sizeof(dailyMaxMem));
        memset(dailyMaxCPUInPerType, 0, sizeof(dailyMaxCPUInPerType));
        memset(dailyMaxMemInPerType, 0, sizeof(dailyMaxMemInPerType));
        int nowCPU[2][5], nowMem[2][5];
        memset(nowCPU, 0, sizeof(nowCPU));
        memset(nowMem, 0, sizeof(nowMem));

        for (auto[vm, query] : addQueryList) {
            nowCPU[vm->deployType][vm->category] += vm->cpu;
            nowMem[vm->deployType][vm->category] += vm->memory;
            int nowTotalCPU = 0, nowTotalMem = 0;
            for (int j = 1; j <= 4; j++) {
                if (j == 3) continue;
                nowTotalCPU += nowCPU[vm->deployType][j];
                nowTotalMem += nowMem[vm->deployType][j];
            }
            dailyMaxCPU[vm->deployType] = std::max(dailyMaxCPU[vm->deployType], nowTotalCPU);
            dailyMaxMem[vm->deployType] = std::max(dailyMaxMem[vm->deployType], nowTotalMem);
            dailyMaxCPUInPerType[vm->deployType][vm->category] = std::max(
                    dailyMaxCPUInPerType[vm->deployType][vm->category],
                    nowCPU[vm->deployType][vm->category]);
            dailyMaxMemInPerType[vm->deployType][vm->category] = std::max(
                    dailyMaxMemInPerType[vm->deployType][vm->category],
                    nowMem[vm->deployType][vm->category]);
        }
    }

    static int compareAliveM(Server *nowNode, Server *lastNode, VM *vm, Server::DeployNode deployNode,
                             Server::DeployNode lastDeployNode = Server::NODE_0) {
        if (nowNode->getCategory(deployNode) != lastNode->getCategory(lastDeployNode)) {
            if (nowNode->getCategory(deployNode) == vm->category) {
                return -1;
            }
            if (lastNode->getCategory(lastDeployNode) == vm->category) {
                return 1;
            }
            if (vm->category == 2 && lastNode->getCategory(lastDeployNode) == 4) {
                return -1;
            }
            if (vm->category == 2 && nowNode->getCategory(deployNode) == 4) {
                return 1;
            }
            if (vm->category == 4 && lastNode->getCategory(lastDeployNode) == 2) {
                return -1;
            }
            if (vm->category == 4 && nowNode->getCategory(deployNode) == 2) {
                return 1;
            }
        }
        if (lastNode->empty() != nowNode->empty()) {
            if (nowNode->empty()) return 1;
            if (lastNode->empty()) return -1;
        }

        if (deployNode == Server::DeployNode::DUAL_NODE) {
            if (nowNode->getLeftCPU(Server::NODE_0) <= lastNode->getLeftCPU(Server::NODE_0) &&
                nowNode->getLeftMemory(Server::NODE_0) <= lastNode->getLeftMemory(Server::NODE_0)) {
                return -1;
            }
            if (nowNode->getLeftCPU(Server::NODE_0) > lastNode->getLeftCPU(Server::NODE_0) &&
                nowNode->getLeftMemory(Server::NODE_0) > lastNode->getLeftMemory(Server::NODE_0)) {
                return 1;
            }
            double k = (double) vm->cpu / vm->memory;
            if (nowNode->getLeftCPU(Server::NODE_0) + nowNode->getLeftMemory(Server::NODE_0) * k <=
                lastNode->getLeftCPU(Server::NODE_0) + lastNode->getLeftMemory(Server::NODE_0) * k) {
                return -1;
            }
            return 1;
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
            double k = (double) vm->cpu / vm->memory;
            if (nowCPU + nowMem * k <= lastCPU + lastMem * k) {
                return -1;
            }
            return 1;
        }
    }
};
