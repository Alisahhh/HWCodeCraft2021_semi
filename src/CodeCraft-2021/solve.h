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
        for (day = 1; day <= T; day++) {
#ifdef TEST
            //if (day % 100 == 0) std::clog << "DAY " << day << std::endl;
            std::clog << "Day " << day << " ServCnt: " << Server::getServerCount() << " VMCnt: " << VM::getVMCount()
                      << std::endl;
#endif
#ifdef DEBUG_O3
            //if (day > 30) exit(0);
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
            calcDailyResource(queryList);
            calcMachineResource();
            prePurchase();
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
                        handleAddQueries(addQueryList, purchaseList);
                    }

                    // IO
                    std::vector<std::pair<int, std::pair<Server *, Server::DeployNode>>> tmpDeployList;
                    for (const auto &addQueryList : addQueryLists) {
                        for (auto[vmType, query] : addQueryList) {
                            auto deployInfo = Server::getDeployInfo(query->vmID);
                            tmpDeployList.emplace_back(query->id, deployInfo);
#ifdef DEBUG_O3
                            //std::clog << "vm " << query->vmID << " deployed at server " << deployInfo.first->id << std::endl;
#endif
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

#ifdef TEST
            for (int i = 0; i < 2; i++) {
                for (auto &aliveM : aliveMachineList[i]) {
                    if (!aliveM.second->empty()) {
                        totalCost += aliveM.second->energyCost;
                    }
                }
            }
#endif

            // IO
            io->writePurchaseList(purchaseList);
            io->writeMigrationList(migrationList);
            io->writeDeployList(deployList);
#ifdef TEST
            fprintf(stderr, "ispeak %d\n", isPeak);
            checkUsedRate();
#endif
        }
#ifdef TEST
        endTime = clock();
        std::clog << "ALL TIME: ";
        LOG_TIME_SEC(endTime - startTime)
        std::clog << "MIG TIME: ";
        LOG_TIME_SEC(migTimeSum);

        std::clog << "Total Cost: " << totalCost << std::endl;
        std::clog << "HardWare Cost: " << hardwareCost << std::endl;
        std::clog << "Test Cnt: " << testCnt << std::endl;
        
#endif
    }

private:
    int T;
    int day; // 为了方便我使用时间
    bool isPeak = false;
    long long hardwareCost = 0; // DEBUG USE
    long long totalCost = 0; // DEBUG USE
    int testCnt; // 
    std::vector<std::vector<Query *>> queryListK;

    StdIO *io;
    CommonData *commonData;
    Migrator *migrator;

    // **参数说明**
    // 用于 compareAddQuery 对新增虚拟机请求排序的参数
    static constexpr double COMPARE_ADD_QUERY_RATIO = 1 / 2.2;

    // 用于判断是不是峰值
    static constexpr int PEAK_CPU = 30000;
    static constexpr int PEAK_MEM = 30000;


    static bool compareAddQuery(const std::pair<VMType *, Query *> &a, const std::pair<VMType *, Query *> &b) {
        auto vmA = a.first, vmB = b.first;
        if (vmA->category != vmB->category) {
            return vmA->category < vmB->category;
        }
        if (vmA->category == Category::SAME_LARGE) {
            return (vmA->memory + vmA->cpu) > (vmB->memory + vmB->cpu);
        } else if (vmA->category == Category::MORE_CPU) {
            return fcmp((vmA->memory + vmA->cpu * COMPARE_ADD_QUERY_RATIO) -
                        (vmB->memory + vmB->cpu * COMPARE_ADD_QUERY_RATIO)) > 0;
        } else {
            return fcmp((vmA->memory * COMPARE_ADD_QUERY_RATIO + vmA->cpu) -
                   (vmB->memory * COMPARE_ADD_QUERY_RATIO + vmB->cpu)) > 0;
        }
    }

    static bool compareAddQueryByK(const std::pair<VMType *, Query *> &a, const std::pair<VMType *, Query *> &b) {
        auto vmA = a.first, vmB = b.first;
        if(vmA->deployType != vmB->deployType) {
           return vmA->deployType < vmB->deployType;
        }
        return fcmp(vmA->K - vmB->K) > 0;
    }

    std::vector<ServerType *> machineListForSort;
    std::unordered_map<int, Server *> aliveMachineList[2];
    void prePurchase() {
        for(int i = 0;i < 2;i ++) {
            int needCPU = dailyMaxCPU[i] - dailyEmptyCPU[i];
            int needMem = dailyMaxMem[i] - dailyEmptyMem[i];
            if(needCPU <= 0 && needMem <= 0) continue;
    #ifdef TEST
            std::clog << "needCPU: " << needCPU << std::endl;
            std::clog << "needMem: " << needMem << std::endl;
    #endif
            Server *miniServer = nullptr;
            int miniCost = 1e9 + 7; 
            for(auto server : machineListForSort) {
                if(server->cpu < SAME_TOO_LARGE_THR || server->memory < SAME_TOO_LARGE_THR) continue;
                // if(server->category != Category::SAME_TOO_LARGE) continue;
                // int cnt = std::max(needCPU / server->cpu, needMem / server->memory);
                // if(cnt * )
            }
        }
    }

    void handleAddQueries(std::vector<std::pair<VMType *, Query *>> &addQueryList,
                          std::vector<Server *> &purchaseList) {
        
        bool canLocateFlag = false;
        // step 1 只放到自己应该在的类型
        std::sort(addQueryList.begin(), addQueryList.end(), compareAddQueryByK);
        auto lptr = addQueryList.begin();
        auto rptr = addQueryList.end() - 1;
        while(lptr < rptr && fcmp(lptr->first->K - 5) > 0) lptr++;
        while(lptr < rptr && fcmp(rptr->first->K - 0.2) < 0) rptr--;
        while(lptr < rptr) {
            auto vmTypeA = lptr->first, vmTypeB = rptr->first;
            auto queryA = lptr->second, queryB = rptr->second;
            auto vmA = VM::newVM(queryA->vmID, *vmTypeA);
            auto vmB = VM::newVM(queryB->vmID, *vmTypeB);
            if(fcmp(fabs(vmA->K * vmB->K - 1.0) - 0.05) < 0) {
                canLocateFlag = false;
                VM *vm = VM::newTmpVM(vmA->cpu + vmB->cpu, vmA->memory + vmB->memory, vmA->deployType);
                if (vm->deployType == VMType::DeployType::DUAL) {
                    Server *minAlivm = nullptr;
                    for (auto top : aliveMachineList[vmA->deployType]) {
                        auto aliveM = top.second;
                        if (aliveM->canDeployVM(vm)) {
                            if (!minAlivm || compareAliveM(aliveM, minAlivm, vm, Server::DUAL_NODE) < 0) {
                                minAlivm = aliveM;
                            }
                            canLocateFlag = true;
                        }

                    }

                    if (canLocateFlag) {
                        minAlivm->deploy(vmA, Server::DUAL_NODE);
                        minAlivm->deploy(vmB, Server::DUAL_NODE);
                        queryA->done = true;
                        queryB->done = true;
                    }

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
                    if (canLocateFlag) {
                        minAlivm->deploy(vmA, lastType);
                        minAlivm->deploy(vmB, lastType);
                        queryA->done = true;
                        queryB->done = true;
                        // minAlivm->deploy(vm, lastType);
                    }
                }


                // if (!canLocateFlag) {
                //     Server *aliveM = nullptr;
                //     bool canSteal = false;
                //     for(auto top : aliveMachineList[vm->deployType ^ 1]){
                //         aliveM = top.second;
                //         if(!aliveM->empty()) continue;
                //         if(aliveM->category != vm->category) continue;
                //         if(vm->deployType == VMType::SINGLE) {
                //             if(!aliveM->canDeployVM(vm, Server::NODE_0)) continue;
                //         } else {
                //             if(!aliveM->canDeployVM(vm)) continue;
                //         }
                //         canSteal = true;
                //         aliveMachineList[vm->deployType ^ 1].erase(aliveMachineList[vm->deployType ^ 1].find(top.first));
                //         aliveMachineList[vm->deployType][aliveM->id] = aliveM;
                //         break;
                //     }
                //     if(!canSteal) {
                //         ServerType *m;
                //         for (auto &am : machineListForSort) {
                //             if (am->canDeployVM(vm)) {
                //                 m = am;
                //                 break;
                //             }
                //         }
                //         aliveM = Server::newServer(*m);
                //         aliveMachineList[vm->deployType][aliveM->id] = aliveM;
                //         purchaseList.push_back(aliveM);
                //         #ifdef TEST
                //             hardwareCost += aliveM->hardwareCost;
                //             totalCost += aliveM->hardwareCost;
                //         #endif
                //     }
                //     if (vm->deployType == VMType::DUAL) {
                //         if (aliveM->canDeployVM(vm)) {
                //             aliveM->deploy(vm);
                //             canLocateFlag = true;
                //         }
                //     } else {
                //         int flagA = aliveM->canDeployVM(vm, Server::NODE_0);
                //         int flagB = aliveM->canDeployVM(vm, Server::NODE_1);
                //         if (flagA) {
                //             aliveM->deploy(vm, Server::NODE_0);
                //             canLocateFlag = true;
                //         } else if (flagB) {
                //             aliveM->deploy(vm, Server::NODE_1);
                //             canLocateFlag = true;
                //         }
                //     }
                // }
                lptr++;
                rptr--;
            } else {
                if(fcmp(vmA->K * vmB->K - 1) < 0) {
                    rptr--;
                } else {
                    lptr++;
                }
            }
        }


        std::sort(addQueryList.begin(), addQueryList.end(), compareAddQuery);

        calcQueryListResource(addQueryList);

        bool canLocateFlag = false;
        for (auto it = addQueryList.begin(); it != addQueryList.end(); it++) {
            auto vmType = it->first;
            auto query = it->second;
            if(query->done) {
                testCnt++;
                continue;
            }
            volatile double param = 1.2;
            if (isPeak) param = 1.0;
            auto vm = VM::newVM(query->vmID, *vmType);

            if (it == addQueryList.begin() || vm->category != (it - 1)->first->category) {
                std::sort(machineListForSort.begin(), machineListForSort.end(),
                          [vm, param, &it, this](ServerType *a, ServerType *b) {
                              auto deployType = vm->deployType;
                              volatile double k = dailyMaxCPUInPerType[deployType][vm->category] /
                                                  dailyMaxMemInPerType[deployType][vm->category];
                              volatile double k1 = a->cpu / a->memory;
                              volatile double k2 = b->cpu / b->memory;

                              volatile double absKa = fabs(k1 - k);
                              volatile double absKb = fabs(k2 - k);
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
                                          return fcmp((a->hardwareCost + a->energyCost * (T - day + 1) * param) -
                                                      (b->hardwareCost + b->energyCost * (T - day + 1) * param)) < 0;
                                      } else {
                                          return fcmp(absKa - absKb) < 0;
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
                                          return fcmp((a->hardwareCost + a->energyCost * (T - day + 1) * param) -
                                                      (b->hardwareCost + b->energyCost * (T - day + 1) * param)) < 0;
                                      } else {
                                          return fcmp(absKa - absKb) < 0;
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
                                      if (fcmp(absKa - 0.2) < 0 && fcmp(absKb - 0.2) < 0) {
                                          return fcmp((a->hardwareCost + a->energyCost * (T - day + 1) * param) -
                                                      (b->hardwareCost + b->energyCost * (T - day + 1) * param)) < 0;
                                      } else {
                                          return fcmp(absKa - absKb) < 0;
                                      }
                                  }

                              }
                              throw std::logic_error("unexpected vm category");
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
                Server *aliveM = nullptr;
                bool canSteal = false;
                for(auto top : aliveMachineList[vm->deployType ^ 1]){
                    aliveM = top.second;
                    if(!aliveM->empty()) continue;
                    if(aliveM->category != vm->category) continue;
                    if(vm->deployType == VMType::SINGLE) {
                        if(!aliveM->canDeployVM(vm, Server::NODE_0)) continue;
                    } else {
                        if(!aliveM->canDeployVM(vm)) continue;
                    }
                    canSteal = true;
                    aliveMachineList[vm->deployType ^ 1].erase(aliveMachineList[vm->deployType ^ 1].find(top.first));
                    aliveMachineList[vm->deployType][aliveM->id] = aliveM;
                    break;
                }
                if(!canSteal) {
                    ServerType *m;
                    for (auto &am : machineListForSort) {
                        if (am->canDeployVM(vm)) {
                            m = am;
                            break;
                        }
                    }
                    aliveM = Server::newServer(*m);
                    aliveMachineList[vm->deployType][aliveM->id] = aliveM;
                    purchaseList.push_back(aliveM);
                    #ifdef TEST
                        hardwareCost += aliveM->hardwareCost;
                        totalCost += aliveM->hardwareCost;
                    #endif
                }
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

    void calcQueryListResource(const std::vector<std::pair<VMType *, Query *>> &addQueryList) {
        memset(dailyMaxCPU, 0, sizeof(dailyMaxCPU));
        memset(dailyMaxMem, 0, sizeof(dailyMaxMem));
        memset(dailyMaxCPUInPerType, 0, sizeof(dailyMaxCPUInPerType));
        memset(dailyMaxMemInPerType, 0, sizeof(dailyMaxMemInPerType));
        int nowCPU[2][5], nowMem[2][5];
        memset(nowCPU, 0, sizeof(nowCPU));
        memset(nowMem, 0, sizeof(nowMem));

        for (auto[vm, query] : addQueryList) {
            if(query->done) continue;
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

    void calcDailyResource(const std::vector<Query *> &queryList) {
        memset(dailyMaxCPU, 0, sizeof(dailyMaxCPU));
        memset(dailyMaxMem, 0, sizeof(dailyMaxMem));
        memset(dailyMaxCPUInPerType, 0, sizeof(dailyMaxCPUInPerType));
        memset(dailyMaxMemInPerType, 0, sizeof(dailyMaxMemInPerType));
        int nowCPU[2][5], nowMem[2][5];
        memset(nowCPU, 0, sizeof(nowCPU));
        memset(nowMem, 0, sizeof(nowMem));

        for (auto &query : queryList) {
            if (query->type == Query::DEL) {
                auto vm = VM::getVM(query->vmID);
                nowCPU[vm->deployType][vm->category] -= vm->cpu;
                nowMem[vm->deployType][vm->category] -= vm->memory;

            } else {
                auto vm = commonData->getVMType(query->vmModel);
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
        if (dailyMaxCPU[0] + dailyMaxCPU[1] >= PEAK_CPU && dailyMaxMem[0] + dailyMaxMem[1] >= PEAK_MEM) {
            // std::clog << "PEAK DAY: " << day << std::endl;
            isPeak = true;
        } else {
            isPeak = false;
        }
    }

    // 机器的剩余CPU
    // 0表示单节点，1表示双节点
    int dailyEmptyCPUInPerType[2][5] = {};

    // 机器的剩余MEM
    // 0表示单节点，1表示双节点
    int dailyEmptyMemInPerType[2][5] = {};

    int dailyEmptyCPU[2];
    int dailyEmptyMem[2];

    void calcMachineResource() {
        memset(dailyEmptyCPU, 0 ,sizeof(dailyEmptyCPU));
        memset(dailyEmptyMem, 0 ,sizeof(dailyEmptyMem));
        for(int i = 0;i < 2;i ++) {
            for(auto [id, server] : aliveMachineList[i]) {
                dailyEmptyCPU[i] += server->getLeftCPU();
                dailyEmptyMem[i] += server->getLeftMemory();
            }
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
        }
    }

    void checkUsedRate() {
        int resourceTotalCPU[2] = {};
        int resourceTotalMem[2] = {};
        int resourceEmptyCPU[2] = {};
        int resourceEmptyMem[2] = {};
        int emptyMachine[2] = {};
        for (int i = 0; i < 2; i++) {
            for (auto &aliveM : aliveMachineList[i]) {
                if (aliveM.second->empty()) {
                    emptyMachine[i]++;
                    continue;
                }
                // if(aliveM.second.empty()) continue;
                resourceEmptyCPU[i] += aliveM.second->getLeftCPU(Server::NODE_0);
                resourceEmptyCPU[i] += aliveM.second->getLeftCPU(Server::NODE_1);
                resourceEmptyMem[i] += aliveM.second->getLeftMemory(Server::NODE_0);
                resourceEmptyMem[i] += aliveM.second->getLeftMemory(Server::NODE_1);
                resourceTotalCPU[i] += aliveM.second->cpu;
                resourceTotalMem[i] += aliveM.second->memory;

            }
        }
        for (int i = 0; i < 2; i++) {
            double cpuEmptyRate = 0;
            double memEmptyRate = 0;

            if (resourceTotalCPU[i])
                cpuEmptyRate = (resourceEmptyCPU[i] + 0.0) / resourceTotalCPU[i];
            if (resourceTotalMem[i])
                memEmptyRate = (resourceEmptyMem[i] + 0.0) / resourceTotalMem[i];

            int cpuLowCnt = 0;
            int cpuHighCnt = 0;
            int memLowCnt = 0;
            int memHighCnt = 0;
            int dualLowCnt = 0;
            int dualHighCnt = 0;

            for(auto &pm:aliveMachineList[i]){
                bool cpuHighFlag = false;
                bool cpuLowFlag = false;
                bool memHighFlag = false;
                bool memLowFlag = false;

                if (pm.second->empty()) continue;

                if (pm.second->getLeftCPU(Server::NODE_0)+pm.second->getLeftCPU(Server::NODE_1) < 0.1*pm.second->cpu) cpuHighFlag = true;
                if (pm.second->getLeftCPU(Server::NODE_0)+pm.second->getLeftCPU(Server::NODE_1) > 0.7*pm.second->cpu) cpuLowFlag = true;
                if (pm.second->getLeftMemory(Server::NODE_0)+pm.second->getLeftMemory(Server::NODE_1) < 0.1*pm.second->memory) memHighFlag = true;
                if (pm.second->getLeftMemory(Server::NODE_0)+pm.second->getLeftMemory(Server::NODE_1) > 0.7*pm.second->memory) memLowFlag = true;

                if(cpuHighFlag && memHighFlag)
                    dualHighCnt++;
                else if(cpuLowFlag && memLowFlag)
                    dualLowCnt++;
                else{
                    if(cpuLowFlag) cpuLowCnt++;
                    else if(cpuHighFlag) cpuHighCnt++;
                    
                    if(memLowFlag) memLowCnt++;
                    else if(memHighFlag) memHighCnt++;
                }
            }

            std::clog << cpuEmptyRate << ", " << memEmptyRate << ", "
                << aliveMachineList[i].size() << ", " << emptyMachine[i] << ", "
                << cpuHighCnt << ", " << cpuLowCnt << ", " << memHighCnt << ", "
                << memLowCnt << ", " << dualHighCnt << ", " << dualLowCnt << std::endl;
        }

        std::clog << std::endl;
    }
};
