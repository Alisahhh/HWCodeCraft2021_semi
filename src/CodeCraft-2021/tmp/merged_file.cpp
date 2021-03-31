//
// Created by kskun on 2021/3/24.
//
#include <iostream>

// begin of solve.h
//
// Created by kskun on 2021/3/25.
//

#include <algorithm>
#include <cstring>
#include <cmath>
#include <ctime>

// begin of data.h
//
// Created by kskun on 2021/3/24.
//

#include <cstdlib>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>

#define TEST

// **参数说明**
// 判断此服务器型号的 cpu / memory 比值：
//   MORE_CPU_RATIO: 高于此比值分类为 MORE_CPU
//   MORE_MEMORY_RATIO: 低于此比值分类为 MORE_MEMORY
//   SAME_LARGE_THR: CPU 与内存均高于此值分类为 SAME_LARGE
//   其余服务器分类为 SAME_SMALL

const double MORE_CPU_RATIO = 2.2,
        MORE_MEMORY_RATIO = 1 / 2.2;
const int SAME_LARGE_THR = 256;

enum Category : int {
    SAME_SMALL = 0, // CPU 内存差不多，比较小
    SAME_LARGE = 1, // CPU 内存差不多，比较大
    MORE_CPU = 2,   // 高 CPU
    MORE_MEMORY = 4 // 高内存
};

// 虚拟机参数
class VMType {
public:
    enum DeployType : int {
        SINGLE = 0, // 单节点部署
        DUAL = 1  // 双节点部署
    };

    const std::string model;
    const int cpu, memory;
    const DeployType deployType;
    const Category category;

    VMType(std::string _model, int _cpu, int _memory, DeployType _deployType) :
            model(std::move(_model)), cpu(_cpu), memory(_memory), deployType(_deployType), category(getCategory()) {}

    virtual std::string toString() const {
        std::ostringstream ss;
        ss << "VM TYPE DATA " << model << std::endl;
        ss << "  CPU: " << cpu << std::endl;
        ss << "  MEMORY " << memory << std::endl;
        ss << "  DEPLOY TYPE: " << deployType << std::endl;
        ss << std::endl;
        return ss.str();
    }

    friend std::ostream &operator<<(std::ostream &os, const VMType &vmType) {
        return os << vmType.toString();
    }

private:
    Category getCategory() const {
        Category _category;
        double cpuMemoryRatio = (double) cpu / memory;
        _category = Category::SAME_SMALL; // 默认设置为小类型
        if (cpuMemoryRatio > MORE_CPU_RATIO) {
            _category = Category::MORE_CPU;
        } else if (cpuMemoryRatio < MORE_MEMORY_RATIO) {
            _category = Category::MORE_MEMORY;
        } else if (cpu >= SAME_LARGE_THR && memory >= SAME_LARGE_THR) {
            _category = Category::SAME_LARGE;
        }
        return _category;
    }
};

class VM : public VMType {
public:
    const int id;

    std::string toString() const override {
        std::ostringstream ss;
        ss << "VM DATA " << id << std::endl;
        ss << "  MODEL: " << model << std::endl;
        ss << "  CPU: " << cpu << std::endl;
        ss << "  MEMORY " << memory << std::endl;
        ss << "  DEPLOY TYPE: " << deployType << std::endl;
        ss << std::endl;
        return ss.str();
    }

private:
    VM(int _id, const VMType &_type) : id(_id), VMType(_type) {}

    inline static std::unordered_map<int, VM *> vmMap;

public:
    static VM *newVM(int id, const VMType &type) {
        VM *vm = new VM(id, type);
        vmMap[vm->id] = vm;
        return vm;
    }

    static VM *getVM(int id) {
        auto it = vmMap.find(id);
        if (it == vmMap.end()) {
            throw std::logic_error("VM::getVM: given vm id does not match any of the vms");
        }
        return it->second;
    }

    static void removeVM(int id) {
        auto it = vmMap.find(id);
        if (it == vmMap.end()) {
            throw std::logic_error("VM::removeVM: given vm id does not match any of the vms");
        }
        delete it->second; // 回收内存
        vmMap.erase(it);
    }

    static int getVMCount() {
        return vmMap.size();
    }
};

// 服务器参数
class ServerType {
public:
    const std::string model;
    const int cpu, memory, hardwareCost, energyCost;
    const Category category;

    ServerType(std::string _model, int _cpu, int _memory,
               int _hardwareCost, int _energyCost) :
            model(std::move(_model)), cpu(_cpu), memory(_memory),
            hardwareCost(_hardwareCost), energyCost(_energyCost), category(getCategory()) {}

    virtual std::string toString() const {
        std::ostringstream ss;
        ss << "SERVER TYPE DATA " << model << std::endl;
        ss << "  CPU: " << cpu << std::endl;
        ss << "  MEMORY " << memory << std::endl;
        ss << "  HARDWARE COST: " << hardwareCost << std::endl;
        ss << "  ENERGY COST: " << energyCost << std::endl;
        ss << "  CATEGORY: " << category << std::endl;
        ss << std::endl;
        return ss.str();
    }

    friend std::ostream &operator<<(std::ostream &os, const ServerType &serverType) {
        return os << serverType.toString();
    }

    enum DeployNode : int {
        NODE_0 = 0,
        NODE_1 = 1,
        DUAL_NODE = -1
    };

    bool canDeployVM(VMType *vmType) const {
        if (vmType->deployType == VMType::DUAL) {
            return cpu >= vmType->cpu && memory >= vmType->memory;
        } else {
            return cpu / 2 >= vmType->cpu && memory / 2 >= vmType->memory;
        }
    }

private:
    Category getCategory() const {
        Category _category;
        double cpuMemoryRatio = (double) cpu / memory;
        _category = Category::SAME_SMALL; // 默认设置为小类型
        if (cpuMemoryRatio > MORE_CPU_RATIO) {
            _category = Category::MORE_CPU;
        } else if (cpuMemoryRatio < MORE_MEMORY_RATIO) {
            _category = Category::MORE_MEMORY;
        } else if (cpu >= SAME_LARGE_THR && memory >= SAME_LARGE_THR) {
            _category = Category::SAME_LARGE;
        }
        return _category;
    }
};

// 服务器对象
class Server : public ServerType {
public:
    const int id;

    std::vector<std::pair<VM *, DeployNode>> getAllDeployedVMs() const {
        std::vector<std::pair<VM *, DeployNode>> vms;
        vms.reserve(deployedVMs.size());
        for (auto[vmID, deployNode] : deployedVMs) {
            auto vm = VM::getVM(vmID);
            vms.emplace_back(vm, deployNode);
        }
        return vms;
    }

    bool canDeployVM(VMType *vmType, DeployNode node = DUAL_NODE) const {
        if (vmType->deployType == VMType::DeployType::SINGLE && node == DUAL_NODE) {
            throw std::logic_error(
                    "Server::canDeployVM: wrong deploy node type");
        }
        if (vmType->deployType == VMType::DeployType::DUAL && node != DUAL_NODE) {
            throw std::logic_error(
                    "Server::canDeployVM: wrong deploy node type");
        }
        if (node == DUAL_NODE) {
            return nodes[0].canDeploy(vmType->cpu / 2, vmType->memory / 2) &&
                   nodes[1].canDeploy(vmType->cpu / 2, vmType->memory / 2);
        }
        return nodes[node].canDeploy(vmType->cpu, vmType->memory);
    }

    void deploy(VM *vm, DeployNode node = DeployNode::DUAL_NODE) {
        if (vmDeployMap.find(vm->id) != vmDeployMap.end()) {
            throw std::logic_error("Server::deploy: this vm has been deployed");
        }
        if (vm->deployType == VMType::DeployType::SINGLE) {
            if (!canDeployVM(vm, node)) {
                throw std::logic_error("Server::deploy: cannot deploy vm on given server");
            }
            deploySingle(vm, node);
        } else {
            if (!canDeployVM(vm, Server::DUAL_NODE)) {
                throw std::logic_error("Server::deploy: cannot deploy vm on given server");
            }
            deployDual(vm);
        }
    }

    void remove(VM *vm) {
        auto it = vmDeployMap.find(vm->id);
        if (it == vmDeployMap.end() || it->second.first != id) {
            throw std::logic_error("Server::remove: vm not found in this server");
        }
        if (vm->deployType == VMType::DeployType::SINGLE) {
            removeSingle(vm, it->second.second);
        } else {
            removeDual(vm);
        }
    }

    std::string toString() const override {
        std::ostringstream ss;
        ss << "SERVER DATA " << id << std::endl;
        ss << "  MODEL: " << model << std::endl;
        ss << "  CPU: " << cpu << std::endl;
        ss << "  MEMORY: " << memory << std::endl;
        ss << "  HARDWARE COST: " << hardwareCost << std::endl;
        ss << "  ENERGY COST: " << energyCost << std::endl;
        ss << "  CATEGORY: " << category << std::endl;
        ss << "  NODE INFO:" << std::endl;
        int idx = 0;
        for (const auto &node : nodes) {
            ss << "    NODE " << idx << ": CPU " << nodes[idx].leftCPU << ", MEMORY " << nodes[idx].leftMemory
               << std::endl;
            idx++;
        }
        ss << "  DEPLOY INFO:" << std::endl;
        auto vms = getAllDeployedVMs();
        for (auto[vm, deployNode] : vms) {
            ss << "    VM " << vm->id << ": NODE ";
            if (deployNode == DeployNode::DUAL_NODE) ss << "DUAL";
            else ss << deployNode;
            ss << std::endl;
        }
        ss << std::endl;
        return ss.str();
    }

    Category getCategory(Server::DeployNode deployNode) const {
        int nowcpu = 0;
        int nowmem = 0;
        if (deployNode == Server::DeployNode::DUAL_NODE) {
            nowcpu = nodes[0].leftCPU;
            nowmem = nodes[0].leftMemory;
        } else {
            if (deployNode == Server::DeployNode::NODE_0) {
                nowcpu = nodes[0].leftCPU;
                nowmem = nodes[0].leftMemory;
            } else {
                nowcpu = nodes[1].leftCPU;
                nowmem = nodes[1].leftMemory;
            }
        }
        if (nowcpu == 0 && nowmem == 0) return Category::SAME_LARGE;
        if (nowmem == 0) return Category::MORE_CPU;
        double k = (nowcpu + 0.0) / nowmem;
        if (k >= MORE_CPU_RATIO) return Category::MORE_CPU;
        else if (k <= MORE_MEMORY_RATIO) return Category::MORE_MEMORY;
        return Category::SAME_LARGE;
    }

    bool empty() const {
        return deployedVMs.empty();
    }

    int getLeftCPU(DeployNode node = DUAL_NODE) const {
        if (node == DUAL_NODE) {
            return nodes[0].leftCPU + nodes[1].leftCPU;
        }
        return nodes[node].leftCPU;
    }

    int getLeftMemory(DeployNode node = DUAL_NODE) const {
        if (node == DUAL_NODE) {
            return nodes[0].leftMemory + nodes[1].leftMemory;
        }
        return nodes[node].leftMemory;
    }

    double getCPUUsage(DeployNode node = DUAL_NODE) const {
        return (double) getLeftCPU(node) / cpu;
    }

    double getMemoryUsage(DeployNode node = DUAL_NODE) const {
        return (double) getLeftMemory(node) / memory;
    }

private:
    Server(int _id, const ServerType &_type) : id(_id), ServerType(_type) {
        nodes[0] = nodes[1] = {cpu / 2, memory / 2};
    }

    struct Node {
        int leftCPU, leftMemory;

        bool canDeploy(int cpu, int memory) const {
            return leftCPU >= cpu && leftMemory >= memory;
        }
    };

    Node nodes[2];

    inline static int serverIDCounter;
    inline static std::unordered_map<int, Server *> serverMap;

    inline static std::unordered_map<int, std::pair<int, DeployNode>> vmDeployMap;
    std::unordered_map<int, DeployNode> deployedVMs;

    int getNodeCPU() const { return cpu / 2; }

    int getNodeMemory() const { return memory / 2; };

    void deploySingle(VM *vm, DeployNode node) {
        nodes[node].leftCPU -= vm->cpu;
        nodes[node].leftMemory -= vm->memory;
        vmDeployMap[vm->id] = {id, node};
        deployedVMs[vm->id] = node;
    }

    void deployDual(VM *vm) {
        nodes[0].leftCPU -= vm->cpu / 2;
        nodes[0].leftMemory -= vm->memory / 2;
        nodes[1].leftCPU -= vm->cpu / 2;
        nodes[1].leftMemory -= vm->memory / 2;
        vmDeployMap[vm->id] = {id, DeployNode::DUAL_NODE};
        deployedVMs[vm->id] = DeployNode::DUAL_NODE;
    }

    void removeSingle(VM *vm, DeployNode node) {
        nodes[node].leftCPU += vm->cpu;
        nodes[node].leftMemory += vm->memory;
        vmDeployMap.erase(vm->id);
        deployedVMs.erase(vm->id);
    }

    void removeDual(VM *vm) {
        nodes[0].leftCPU += vm->cpu / 2;
        nodes[0].leftMemory += vm->memory / 2;
        nodes[1].leftCPU += vm->cpu / 2;
        nodes[1].leftMemory += vm->memory / 2;
        vmDeployMap.erase(vm->id);
        deployedVMs.erase(vm->id);
    }

public:
    static Server *newServer(const ServerType &type) {
        auto *server = new Server(++serverIDCounter, type);
        serverMap[server->id] = server;
        return server;
    }

    static Server *getServer(int id) {
        auto it = serverMap.find(id);
        if (it == serverMap.end()) {
            throw std::logic_error("VM::getVM: given vm id does not match any of the vms");
        }
        return it->second;
    }

    static Server *getDeployServer(int vmID) {
        auto it = vmDeployMap.find(vmID);
        if (it == vmDeployMap.end()) {
            throw std::logic_error("VM::getVM: given vm have not been deployed");
        }
        return getServer(it->second.first);
    }

    static std::pair<Server *, DeployNode> getDeployInfo(int vmID) {
        auto it = vmDeployMap.find(vmID);
        if (it == vmDeployMap.end()) {
            throw std::logic_error("VM::getVM: given vm have not been deployed");
        }
        return {getServer(it->second.first), it->second.second};
    }

    static int getServerCount() {
        return serverMap.size();
    }
};

// 公共信息（服务器与虚拟机参数）
class CommonData {
public:
    CommonData(const std::vector<ServerType *> &serverTypeList, const std::vector<VMType *> &vmTypeList) {
        for (const auto &serverType : serverTypeList) {
            serverTypeMap[serverType->model] = serverType;
        }
        for (const auto &vmType : vmTypeList) {
            vmTypeMap[vmType->model] = vmType;
        }
    }

    ServerType *getServerType(const std::string &model) const {
        auto it = serverTypeMap.find(model);
        if (it == serverTypeMap.end()) {
            throw std::logic_error("CommonData::getServerType: given model does not exist");
        }
        return it->second;
    }

    VMType *getVMType(const std::string &model) const {
        auto it = vmTypeMap.find(model);
        if (it == vmTypeMap.end()) {
            throw std::logic_error("CommonData::getVMType: given model does not exist");
        }
        return it->second;
    }

private:
    std::unordered_map<std::string, ServerType *> serverTypeMap;
    std::unordered_map<std::string, VMType *> vmTypeMap;
};

// 请求信息
class Query {
public:
    enum Type {
        ADD,
        DEL
    };

    const int id, day;
    const Type type;
    const std::string vmModel;
    const int vmID;

    Query(int _id, int _day, Type _type, int _vmID, std::string _vmModel = "") :
            id(_id), day(_day), type(_type), vmModel(std::move(_vmModel)), vmID(_vmID) {}
};

// end of data.h
// begin of io.h
//
// Created by kskun on 2021/3/25.
//

#include <iostream>

// begin of data.h
// end of data.h

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
        for (auto[server, deployNode] : deployList) {
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

        return new Query(++addQueryIDCount, day, type, id, model);
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
// end of io.h
// begin of migrate.h
//
// Created by kskun on 2021/3/26.
//

#include <algorithm>
#include <unordered_map>
#include <set>

// begin of data.h
// end of data.h
// begin of util.h
//
// Created by kskun on 2021/3/26.
//

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
// end of util.h
// begin of debug.h
//
// Created by kskun on 2021/3/30.
//

#define LOG_TIME(time) std::clog << (double) (time) / CLOCKS_PER_SEC * 1000 << "ms" << std::endl;
#define LOG_TIME_SEC(time) std::clog << (double) (time) / CLOCKS_PER_SEC << "s" << std::endl;

// end of debug.h

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

// end of migrate.h
// begin of debug.h
// end of debug.h

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

            #ifdef TEST
            for(int i = 0; i < 2;i ++) {
                for(auto &aliveM : aliveMachineList[i] ) {
                    if(!aliveM.second->empty()) {
                        totalCost += aliveM.second->energyCost;
                    }
                }
            }
            #endif

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

        std::clog << "Total Cost: " << totalCost << std::endl;
        std::clog << "HardWare Cost: " << hardwareCost << std::endl; 
#endif
    }

private:
    int T;
    int day; // 为了方便我使用时间
    bool isPeak = false;
    long long hardwareCost = 0; // DEBUG USE
    long long totalCost = 0; // DEBUG USE
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
            double param = 0.8;
            if(isPeak) param = 0.3;
            auto vm = VM::newVM(query->vmID, *vmType);
            if (it == addQueryList.begin() || vm->category != (it - 1)->first->category) {
                std::sort(machineListForSort.begin(), machineListForSort.end(),
                          [vm, param, &it, this](ServerType *a, ServerType *b) {
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
                                          return a->hardwareCost + a->energyCost * (T - day) * param < b->hardwareCost + b->energyCost * (T - day) * param;
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
                                           return a->hardwareCost + a->energyCost * (T - day) * param < b->hardwareCost + b->energyCost * (T - day) * param;
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
                                           return a->hardwareCost + a->energyCost * (T - day) * param < b->hardwareCost + b->energyCost * (T - day) * param;
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
        #ifdef TEST
                hardwareCost += aliveM->hardwareCost;
                totalCost += aliveM->hardwareCost;
        #endif
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
        if(day == 134) {
            // std::clog << dailyMaxCPU[0] + dailyMaxCPU[1] << std::endl;
            // std::clog << dailyMaxMem[0] + dailyMaxMem[1] << std::endl;
        }
        if(dailyMaxCPU[0] + dailyMaxCPU[1] >= PEAK_CPU && dailyMaxMem[0] + dailyMaxMem[1] >= PEAK_MEM) {
            // std::clog << "PEAK DAY: " << day << std::endl;
            isPeak = true;
        } else {
            isPeak = false;
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

// end of solve.h

int main(int argc, char *argv[]) {
#ifdef TEST
    freopen("../../data/training-2.txt", "r", stdin);
    freopen("../../data/training-2.out.txt", "w", stdout);
#else
    // IO 优化
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cout.tie(nullptr);
#endif

    auto *solver = new Solver;
    solver->solve();

    return 0;
}
