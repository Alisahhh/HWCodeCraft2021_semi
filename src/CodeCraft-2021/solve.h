//
// Created by kskun on 2021/3/25.
//
#pragma once

#include <algorithm>
#include <cstring>
#include <cmath>
#include <ctime>
#include <stdlib.h>

#include "data.h"
#include "io.h"
#include "migrate.h"
#include "debug.h"
#include "util.h"

class Solver {
public:
    Solver() {
        io = new StdIO();
        migrator = new Migrator(aliveMachineList);
    }

    void solve() {
        // 初始化数据
        srand(0x111);
        auto serverTypeList = io->readServerType();
        auto vmTypeList = io->readVMType();
        commonData = new CommonData(serverTypeList, vmTypeList);

        for (auto it:serverTypeList){
            if(it->memory <= 400 || it->cpu <= 400)
                findHighExpPMTypeList[SAME_LARGE].push_back(it);
            findHighExpPMTypeList[MORE_CPU].push_back(it);
            findHighExpPMTypeList[MORE_MEMORY].push_back(it);
        }

        int param = SAME_LARGE;
        std::sort(findHighExpPMTypeList[SAME_LARGE].begin(), findHighExpPMTypeList[SAME_LARGE].end(), [param, this](ServerType *a, ServerType *b) {
            if (a->category != b->category) {
                if (a->category == param) {
                    return true;
                }
                if (b->category == param) {
                    return false;
                }
                return a->category < b->category;
            } else {
                int aK = ((a->hardwareCost) / (a->energyCost)) >> 6;
                int bK = ((b->hardwareCost) / (b->energyCost)) >> 6;
                if(aK == bK) {
                    return fcmp(std::abs(1.0 - (double)a->cpu / (double)a->memory) - std::abs(1.0 - (double)b->cpu / (double)b->memory)) < 0;
                } else {
                    return aK < bK;
                }
            }
        });
#ifdef TEST
        fprintf(stderr,"%s %s %s\n", findHighExpPMTypeList[SAME_LARGE][0]->model.c_str(), findHighExpPMTypeList[SAME_LARGE][1]->model.c_str(), findHighExpPMTypeList[SAME_LARGE][2]->model.c_str());
#endif

        param = MORE_CPU;
        std::sort(findHighExpPMTypeList[MORE_CPU].begin(), findHighExpPMTypeList[MORE_CPU].end(), [param, this](ServerType *a, ServerType *b) {
            if (a->category != b->category) {
                if (a->category == param) {
                    return true;
                }
                if (b->category == param) {
                    return false;
                }
                return a->category < b->category;
            } else {
                int aK = ((a->hardwareCost) / (a->energyCost)) >> 6;
                int bK = ((b->hardwareCost) / (b->energyCost)) >> 6;
                if(aK == bK) {
                    return fcmp(std::abs(3.0 - (double)a->cpu / (double)a->memory) - std::abs(3.0 - (double)b->cpu / (double)b->memory)) < 0;
                } else {
                    return aK < bK;
                }
            }
        });
#ifdef TEST
        fprintf(stderr,"%s %s %s\n", findHighExpPMTypeList[MORE_CPU][0]->model.c_str(), findHighExpPMTypeList[MORE_CPU][1]->model.c_str(), findHighExpPMTypeList[MORE_CPU][2]->model.c_str());
#endif

        param = MORE_MEMORY;
        std::sort(findHighExpPMTypeList[MORE_MEMORY].begin(), findHighExpPMTypeList[MORE_MEMORY].end(), [param, this](ServerType *a, ServerType *b) {
            int aK = ((a->hardwareCost) / (a->energyCost)) >> 6;
            int bK = ((b->hardwareCost) / (b->energyCost)) >> 6;
            if(aK == bK) {
                return fcmp(std::abs(0.5 - (double)a->cpu / (double)a->memory) - std::abs(0.5 - (double)b->cpu / (double)b->memory)) < 0;
            } else {
                return aK < bK;
            }
        });
#ifdef TEST
        fprintf(stderr,"%s %s %s\n", findHighExpPMTypeList[MORE_MEMORY][0]->model.c_str(), findHighExpPMTypeList[MORE_MEMORY][1]->model.c_str(), findHighExpPMTypeList[MORE_MEMORY][2]->model.c_str());
#endif

#define TEST_KMEANS
#ifdef TEST_KMEANS
        auto *kMeans = new KMeans<ServerType *>;
        std::vector<std::pair<std::vector<double>, ServerType *>> testData;
        for (auto &i : serverTypeList) {
            testData.push_back({{(double) i->hardwareCost / i->energyCost}, i});
        }
        auto kMeansResultTmp = kMeans->kMeans(testData);
        delete kMeans;
        std::vector<std::set<ServerType *>> kMeansResult;
        for (auto &i : kMeansResultTmp) {
            std::set<ServerType *> tmp;
            for (auto &obj : i) {
                tmp.insert(obj.second);
            }
            kMeansResult.push_back(tmp);
        }
#endif

        for(int i = 1;i <= 4;i ++) {
            if(i == 3) continue;
            machineListForSort[i] = serverTypeList;
        }
        
#ifdef TEST
        std::clog << "COMMON DATA READ" << std::endl;
#endif

        auto dayCount = io->readDayCount();
        T = dayCount.first;
        // 预先读进来K天
        K = dayCount.second;
#ifdef TEST
        std::clog << "T " << T << " K " << K << std::endl;
#endif

#ifdef TEST_KMEANS
        std::vector<std::pair<double, double>> kMeansSepareteCost;
        for (auto &kmr:kMeansResult){
            volatile double sumHardwareCost = 0;
            volatile double sumEnergeyCost = 0;

            for (auto &sT:kmr){
                sumHardwareCost += (double)sT->hardwareCost / (double)(sT->cpu + sT->memory*0.45);
                sumEnergeyCost += (double)sT->energyCost / (double)(sT->cpu + sT->memory*0.45);
            }

            kMeansSepareteCost.push_back(std::make_pair(sumHardwareCost/kmr.size(), sumEnergeyCost/kmr.size()));
        }

        std::vector<double> catPMCostInDaySim(kMeansSepareteCost.size(), -1);
        std::vector<int> bestPMInDay(T+1, -1);

        for (int i=0;i<catPMCostInDaySim.size();i++){
            if (catPMCostInDaySim.size() != kMeansSepareteCost.size()) throw error_t(-1);
            catPMCostInDaySim[i] = kMeansSepareteCost[i].first;
        }

        // for the T Day
        int minimumCostCat = 0;
        double minimumCost = catPMCostInDaySim[0];
        for(int i=0;i<catPMCostInDaySim.size();i++){
            if (fcmp(catPMCostInDaySim[i] - minimumCost) < 0){
                minimumCost = catPMCostInDaySim[i];
                minimumCostCat = i;
            }
        }
        bestPMInDay[T] = minimumCostCat;
        int bestCatInEnd = minimumCostCat;
        int highExpDay = -1;
        // fprintf(stderr, "%d\n", minimumCostCat);

        // for init day to T-1 day
        for (int simToday = T-1; simToday > 0; simToday--){
            // update total cost, if pm added in simToday day
            for (int i=0;i<catPMCostInDaySim.size();i++){
                catPMCostInDaySim[i] += kMeansSepareteCost[i].second;
                // fprintf(stderr, "%lf\t",catPMCostInDaySim[i]);
            }
            // fprintf(stderr, "\n");
            int minimumCostCat = -1;
            int minimumCost = INT32_MAX;
            for (int i=0;i<catPMCostInDaySim.size();i++){
                if (fcmp(catPMCostInDaySim[i] - minimumCost) < 0){
                    minimumCost = catPMCostInDaySim[i];
                    minimumCostCat = i;
                }
            }
            bestPMInDay[simToday] = minimumCostCat;
            // fprintf(stderr, "%d %d\n", minimumCostCat, bestCatInEnd);
            if (minimumCostCat != bestCatInEnd && highExpDay == -1) {
                highExpDay = simToday;
            }
        }
#endif

#ifdef TEST
        fprintf(stderr, "highExpDay %d\n", highExpDay);
#endif

        queryListK.resize(K);
        for (int i = 1; i <= K; i++) {
#ifdef TEST
            std::clog << "READ DAY " << i << ", STORED IN " << i % K << std::endl;
#endif
            queryListK[i % K] = io->readDayQueries();
        }
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
            std::cout << "day " << day << std::endl;
//            if (day > 10) exit(0);
#endif

            // IO
            std::vector<Server *> purchaseList;
            std::vector<std::tuple<int, int, Server::DeployNode>> migrationList; // 0: vmID, 1: serverID, 2: deployNode
            std::vector<std::pair<Server *, Server::DeployNode>> deployList;

            static int lastDayLeftMigCnt = 0;
            //if(isPeak) lastDayLeftMigCnt = 0;
            // migration
            if (VM::getVMCount() > 100) {
                auto limit = VM::getVMCount() * 3 / 100; // 百分之3
                // limit = INT32_MAX;
                //limit -= migrator->clearHighExpensesPMs(day, lastDayLeftMigCnt*1.2, migrationList);
                //limit -= migrator->combineLowLoadRatePM(day, limit, migrationList, 0.7);
                limit -= migrator->migrateScatteredVM(day, limit, migrationList, 0.2);
                limit -= migrator->combineLowLoadRatePM(day, limit, migrationList);
                limit -= migrator->migrateScatteredVM(day, limit, migrationList, 0.05);
                lastDayLeftMigCnt = limit;
            }

#ifdef TEST
            checkUsedRate();
#endif
            auto queryList = queryListK[day % K];
            //auto queryList = io->readDayQueries();
            calcDailyResource(queryList);
            if (day != 1 && day + K - 1 <= T) {
#ifdef TEST
                std::clog << "Day " << day << " read day " << day + K - 1 << std::endl;
#endif
                queryListK[(day + K - 1) % K] = io->readDayQueries();
            }

            vmDieInK.clear();
            vmAddRecordInK.clear();

            const int kBest = 15;

            for(int i=0; i<std::min(K, kBest); i++){
                calcVmAliveDays(queryListK[(day + i) % K], K, day+i);
            }

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
                        handleAddQueries(addQueryList, purchaseList, 350);
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
                    //fprintf(stderr, "del %d %d %d %d %d\n",vm->id, vmAddRecord[vm->id].second, day, day -vmAddRecord[vm->id].second, vmAddRecord[vm->id].first);
                    if(vmAddRecord[vm->id].first == true && day -vmAddRecord[vm->id].second < 100){
                        vmAliveTimeCnt[(day -vmAddRecord[vm->id].second) / 10] ++;
                    }
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
#endif
    }

private:
    int T;
    int day; // 为了方便我使用时间
    int K;
    bool isPeak = false;
    long long hardwareCost = 0; // DEBUG USE
    long long totalCost = 0; // DEBUG USE
    std::vector<std::vector<Query *>> queryListK;
    std::unordered_map<int, std::pair<bool,int> > vmAddRecord;
    std::unordered_map<int, int> vmAddRecordInK;
    std::set<int> vmDieInK;
    int vmAliveTimeCnt[10] = {0};
    std::unordered_map<int, ServerType *> tmpPMCandy;
    std::vector<ServerType *> findHighExpPMTypeList[5];

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

    std::vector<ServerType *> machineListForSort[5];
    std::unordered_map<int, Server *> aliveMachineList[2];

    void handleAddQueries(const std::vector<std::pair<VMType *, Query *>> &addQueryList,
                          std::vector<Server *> &purchaseList, const int highExpDay) {
        if(addQueryList.size() == 0) return;
        bool canLocateFlag = false;
        //const int highExpDay = 350;

        calcQueryListResource(addQueryList);
        volatile double param = 1.6;
        if (isPeak) param = 1.0;
        auto deployType = addQueryList[0].first->deployType;
        if(dailyMaxMemInPerType[deployType][1] != 0)
        std::sort(machineListForSort[1].begin(), machineListForSort[1].end(), 
        [highExpDay, param, deployType, this](ServerType *a, ServerType *b) {
            volatile double k = (dailyMaxCPUInPerType[deployType][1] + 0.0)/
                                dailyMaxMemInPerType[deployType][1];
            volatile double k1 = (a->cpu + 0.0) / a->memory;
            volatile double k2 = (b->cpu + 0.0)/ b->memory;

            volatile double absKa = fabs(k1 - k);
            volatile double absKb = fabs(k2 - k);
            if (a->category != b->category) {
                if (a->category == 1) {
                    return true;
                }
                if (b->category == 1) {
                    return false;
                }
                return a->category < b->category;
            } else {
                if (fcmp(absKa - 10) < 0 && fcmp(absKb - 10) < 0) {
                    if((fcmp(k1 -1) > 0 && fcmp(k2 -1) < 0) || (fcmp(k1 -1) < 0 && fcmp(k2 -1) > 0)) {
                        if(fcmp(k -1) > 0) {
                            if(fcmp(k1 -1) > 0) return true;
                            else return false;
                        }
                        if(fcmp(k -1) < 0) {
                            if(fcmp(k1 - 1) > 0) return false;
                            else return true;
                        }
                    }
                    if(day > highExpDay) {
                        int aK = ((a->hardwareCost) / (a->energyCost)) >> 5;
                        int bK = ((b->hardwareCost) / (b->energyCost)) >> 5;
                        if(aK == bK) {
                            return a->hardwareCost < b->hardwareCost;
                        } else {
                            return aK < bK;
                        }
                    }
                    return fcmp((a->hardwareCost + a->energyCost * (T - day) * param) -
                                (b->hardwareCost + b->energyCost * (T - day) * param)) < 0;
                } else {
                    return fcmp(absKa - absKb) < 0;
                }
            }
        });

        if(dailyMaxMemInPerType[deployType][2] != 0) 
        std::sort(machineListForSort[2].begin(), machineListForSort[2].end(), 
        [highExpDay, param, deployType, this](ServerType *a, ServerType *b) {
            volatile double k = (dailyMaxCPUInPerType[deployType][2] + 0.0)/
                                dailyMaxMemInPerType[deployType][2];
            volatile double k1 = (a->cpu + 0.0)/ a->memory;
            volatile double k2 = (b->cpu + 0.0)/ b->memory;

            volatile double absKa = fabs(k1 - k);
            volatile double absKb = fabs(k2 - k);
            if (a->category != b->category) {
                if (a->category == 2) {
                    return true;
                }
                if (b->category == 2) {
                    return false;
                }
                return a->category < b->category;
            } else {
                if (fcmp(absKa - 20) < 0 && fcmp(absKb - 20) < 0) {
                    if(day > highExpDay) {
                        int aK = ((a->hardwareCost) / (a->energyCost)) >> 5;
                        int bK = ((b->hardwareCost) / (b->energyCost)) >> 5;
                        if(aK == bK) {
                            return a->hardwareCost < b->hardwareCost;
                        } else {
                            return aK < bK;
                        }
                    }
                    return fcmp((a->hardwareCost + a->energyCost * (T - day) * param) -
                                (b->hardwareCost + b->energyCost * (T - day) * param)) < 0;
                } else {
                    return fcmp(absKa - absKb) < 0;
                }
            }
        });

        if(dailyMaxMemInPerType[deployType][4] != 0) 
        std::sort(machineListForSort[4].begin(), machineListForSort[4].end(), 
        [highExpDay, param, deployType, this](ServerType *a, ServerType *b) {
            volatile double k = (dailyMaxCPUInPerType[deployType][4] + 0.0)/
                                dailyMaxMemInPerType[deployType][4];
            volatile double k1 = (a->cpu + 0.0)/ a->memory;
            volatile double k2 = (b->cpu + 0.0)/ b->memory;

            volatile double absKa = fabs(k1 - k);
            volatile double absKb = fabs(k2 - k);
            if (a->category != b->category) {
                if (a->category == 4) {
                    return true;
                }
                if (b->category == 4) {
                    return false;
                }
                return a->category < b->category;
            } else {
                if (fcmp(absKa - 1) < 0 && fcmp(absKb - 1) < 0) {
                    if(day > highExpDay) {
                        int aK = ((a->hardwareCost) / (a->energyCost)) >> 5;
                        int bK = ((b->hardwareCost) / (b->energyCost)) >> 5;
                        if(aK == bK) {
                            return a->hardwareCost < b->hardwareCost;
                        } else {
                            return aK < bK;
                        }
                    }
                    return fcmp((a->hardwareCost + a->energyCost * (T - day + 1) * param) -
                                (b->hardwareCost + b->energyCost * (T - day + 1) * param)) < 0;
                } else {
                    return fcmp(absKa - absKb) < 0;
                }
            }
        });

        for (auto it = addQueryList.begin(); it != addQueryList.end(); it++) {
            auto vmType = it->first;
            auto query = it->second;
            auto vm = VM::newVM(query->vmID, *vmType);
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

                if (canLocateFlag) {
#ifdef DEBUG_O3
//                    std::clog << "type 1 vm " << vm->id << " deployed at server " << minAlivm->id << std::endl;
//                    std::clog << minAlivm->toString() << std::endl;
#endif
                    minAlivm->deploy(vm, Server::DUAL_NODE);
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
#ifdef DEBUG_O3
//                    std::clog << "type 2 vm " << vm->id << " deployed at server " << minAlivm->id << std::endl;
//                    std::clog << minAlivm->toString() << std::endl;
#endif
                    minAlivm->deploy(vm, lastType);
                }
            }


            if (!canLocateFlag) {
                Server *aliveM = nullptr;
                bool canSteal = false;
                std::vector<Server*>stealMachine;

                for(auto top : aliveMachineList[vm->deployType ^ 1]){
                    aliveM = top.second;
                    if(!aliveM->empty()) continue;
                    if(aliveM->category != vm->category) continue;
                    stealMachine.push_back(aliveM);
                    // if(vm->deployType == VMType::SINGLE) {
                    //     if(!aliveM->canDeployVM(vm, Server::NODE_0)) continue;
                    // } else {
                    //     if(!aliveM->canDeployVM(vm)) continue;
                    // }
                    // canSteal = true;
                    // aliveMachineList[vm->deployType ^ 1].erase(aliveMachineList[vm->deployType ^ 1].find(top.first));
                    // aliveMachineList[vm->deployType][aliveM->id] = aliveM;
                    // break;
                }
                // sort(stealMachine.begin(), stealMachine.end(), [vm, this](ServerType *a, ServerType *b){
                //     // if (a->category != b->category) {
                //     //     if(a->category == vm->category) {
                //     //         return true;
                //     //     }
                //     //     if(b->category == vm->category) {
                //     //         return false;
                //     //     }
                //     // }
                //     return a->energyCost < b->energyCost;
                // });

                for(auto &server : stealMachine) {
                    if(vm->deployType == VMType::SINGLE) {
                        if(!server->canDeployVM(vm, Server::NODE_0)) continue;
                    } else {
                        if(!server->canDeployVM(vm)) continue;
                    }
                    canSteal = true;
                    aliveM = server;
                    aliveMachineList[vm->deployType ^ 1].erase(aliveMachineList[vm->deployType ^ 1].find(aliveM->id));
                    aliveMachineList[vm->deployType][aliveM->id] = aliveM;
                    break;
                }

                if(!canSteal) {
                    ServerType *m;
                    const int kMinimumLimit = 10;
                    const double highRate = 0.2;

                    if (isPeak && ((vmDieInK.find(vm->id) != vmDieInK.end() && K > kMinimumLimit)||(K <= kMinimumLimit && rand() < RAND_MAX*highRate))){
                        for (auto &am : findHighExpPMTypeList[vm->category]) {
                            if (am->canDeployVM(vm)) {
                                m = am;
                                break;
                            }
                        }
                        aliveM = Server::newServer(*m);
                        aliveM->isHigh = true;
                    }else{
                        for (auto &am : machineListForSort[vm->category]) {
                            //if (am->cpu > 400 && am->memory > 400) continue;
                            if (am->canDeployVM(vm)) {
                                m = am;
                                break;
                            }
                        }
                        aliveM = Server::newServer(*m);
                    }

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

    void calcVmAliveDays(const std::vector<Query *> &queryList, int K, int day){
        for (auto &query : queryList) {
            if (query->type == Query::DEL) {
                if (vmAddRecordInK.find(query->vmID) != vmAddRecordInK.end()) {
                    vmDieInK.insert(query->vmID);
                }
            } else {
                vmAddRecordInK[query->vmID] = day;
            }
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
        int resourceTotalCPU[2] = {0};
        int resourceTotalMem[2] = {0};
        int resourceEmptyCPU[2] = {0};
        int resourceEmptyMem[2] = {0};
        int emptyMachine[2] = {0};

        int hresourceTotalCPU[2] = {0};
        int hresourceTotalMem[2] = {0};
        int hresourceEmptyCPU[2] = {0};
        int hresourceEmptyMem[2] = {0};
        int hemptyMachine[2] = {0};
        int htotalMachine[2] = {0};

        for (int i = 0; i < 2; i++) {
            for (auto &aliveM : aliveMachineList[i]) {
                if(aliveM.second->isHigh) {
                    htotalMachine[i]++;
                }
                if (aliveM.second->empty()) {
                    emptyMachine[i]++;
                    if(aliveM.second->isHigh){
                        hemptyMachine[i]++;
                    }
                    continue;
                }

                if (aliveM.second->isHigh){
                    hresourceEmptyCPU[i] += aliveM.second->getLeftCPU(Server::NODE_0);
                    hresourceEmptyCPU[i] += aliveM.second->getLeftCPU(Server::NODE_1);
                    hresourceEmptyMem[i] += aliveM.second->getLeftMemory(Server::NODE_0);
                    hresourceEmptyMem[i] += aliveM.second->getLeftMemory(Server::NODE_1);
                    hresourceTotalCPU[i] += aliveM.second->cpu;
                    hresourceTotalMem[i] += aliveM.second->memory;
                }else{
                    resourceEmptyCPU[i] += aliveM.second->getLeftCPU(Server::NODE_0);
                    resourceEmptyCPU[i] += aliveM.second->getLeftCPU(Server::NODE_1);
                    resourceEmptyMem[i] += aliveM.second->getLeftMemory(Server::NODE_0);
                    resourceEmptyMem[i] += aliveM.second->getLeftMemory(Server::NODE_1);
                    resourceTotalCPU[i] += aliveM.second->cpu;
                    resourceTotalMem[i] += aliveM.second->memory;
                }

            }
        }
        for (int i = 0; i < 2; i++) {
            volatile double cpuEmptyRate = 0;
            volatile double memEmptyRate = 0;
            volatile double hcpuEmptyRate = 0;
            volatile double hmemEmptyRate = 0;

            if (resourceTotalCPU[i])
                cpuEmptyRate = (resourceEmptyCPU[i] + 0.0) / resourceTotalCPU[i];
            if (resourceTotalMem[i])
                memEmptyRate = (resourceEmptyMem[i] + 0.0) / resourceTotalMem[i];

            if (hresourceTotalCPU[i])
                hcpuEmptyRate = (hresourceEmptyCPU[i] + 0.0) / hresourceTotalCPU[i];
            if (hresourceTotalMem[i])
                hmemEmptyRate = (hresourceEmptyMem[i] + 0.0) / hresourceTotalMem[i];


            int cpuLowCnt = 0;
            int cpuHighCnt = 0;
            int memLowCnt = 0;
            int memHighCnt = 0;
            int dualLowCnt = 0;
            int dualHighCnt = 0;

            for (auto &pm:aliveMachineList[i]) {
                bool cpuHighFlag = false;
                bool cpuLowFlag = false;
                bool memHighFlag = false;
                bool memLowFlag = false;

                if (pm.second->empty()) continue;

                if (fcmp((pm.second->getLeftCPU(Server::NODE_0) + pm.second->getLeftCPU(Server::NODE_1)) -
                         (0.1 * pm.second->cpu)) < 0)
                    cpuHighFlag = true;
                if (fcmp((pm.second->getLeftCPU(Server::NODE_0) + pm.second->getLeftCPU(Server::NODE_1)) -
                         (0.7 * pm.second->cpu)) > 0)
                    cpuLowFlag = true;
                if (fcmp((pm.second->getLeftMemory(Server::NODE_0) + pm.second->getLeftMemory(Server::NODE_1)) -
                         (0.1 * pm.second->memory)) < 0)
                    memHighFlag = true;
                if (fcmp((pm.second->getLeftMemory(Server::NODE_0) + pm.second->getLeftMemory(Server::NODE_1)) -
                         (0.7 * pm.second->memory)) > 0)
                    memLowFlag = true;

                if (cpuHighFlag && memHighFlag)
                    dualHighCnt++;
                else if (cpuLowFlag && memLowFlag)
                    dualLowCnt++;
                else {
                    if (cpuLowFlag) cpuLowCnt++;
                    else if (cpuHighFlag) cpuHighCnt++;

                    if (memLowFlag) memLowCnt++;
                    else if (memHighFlag) memHighCnt++;
                }
            }

            std::clog << cpuEmptyRate << ", " << memEmptyRate << ", "
                      << aliveMachineList[i].size() << ", " << emptyMachine[i] << ", "
                      << cpuHighCnt << ", " << cpuLowCnt << ", " << memHighCnt << ", "
                      << memLowCnt << ", " << dualHighCnt << ", " << dualLowCnt << std::endl;
            std::clog << hcpuEmptyRate << ", " << hmemEmptyRate << ", " << htotalMachine[i] << ", " << hemptyMachine[i] << std::endl;
        }

        fprintf(stderr, "%d %d %d %d %d %d %d %d %d %d\n",vmAliveTimeCnt[0],vmAliveTimeCnt[1],vmAliveTimeCnt[2],vmAliveTimeCnt[3],vmAliveTimeCnt[4],vmAliveTimeCnt[5],vmAliveTimeCnt[6],vmAliveTimeCnt[7],vmAliveTimeCnt[8],vmAliveTimeCnt[9]);

        std::clog << std::endl;
    }
};
