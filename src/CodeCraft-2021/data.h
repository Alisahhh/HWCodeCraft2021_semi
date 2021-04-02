//
// Created by kskun on 2021/3/24.
//
#pragma once

#include <cstdlib>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>

const double EPS = 1e-6;

inline int fcmp(double a) {
    if (fabs(a) <= EPS) return 0;
    else return a < 0 ? -1 : 1;
}

// **参数说明**
// 判断此服务器型号的 cpu / memory 比值：
//   MORE_CPU_RATIO: 高于此比值分类为 MORE_CPU
//   MORE_MEMORY_RATIO: 低于此比值分类为 MORE_MEMORY
//   SAME_LARGE_THR: CPU 与内存均高于此值分类为 SAME_LARGE
//   其余服务器分类为 SAME_SMALL

const double MORE_CPU_RATIO = 2.1,
        MORE_MEMORY_RATIO = 1 / 2.1;
const int SAME_LARGE_THR = 300;
const int SAME_TOO_LARGE_THR = 1300;

enum Category : int {
    SAME_SMALL = 0, // CPU 内存差不多，比较小
    SAME_LARGE = 1, // CPU 内存差不多，比较大
    MORE_CPU = 2,   // 高 CPU
    SAME_TOO_LARGE = 3, // CPU 内存差不多，非常大
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
        volatile double cpuMemoryRatio = (double) cpu / memory;
        _category = Category::SAME_LARGE; // 默认设置为 1
        if (fcmp(cpuMemoryRatio - MORE_CPU_RATIO) > 0) {
            _category = Category::MORE_CPU;
        } else if (fcmp(cpuMemoryRatio - MORE_MEMORY_RATIO) < 0) {
            _category = Category::MORE_MEMORY;
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
        volatile double cpuMemoryRatio = (double) cpu / memory;
        _category = Category::SAME_SMALL; // 默认设置为小类型
        if (fcmp(cpuMemoryRatio - MORE_CPU_RATIO) > 0) {
            _category = Category::MORE_CPU;
        } else if (fcmp(cpuMemoryRatio - MORE_MEMORY_RATIO) < 0) {
            _category = Category::MORE_MEMORY;
        } else if (cpu >= SAME_LARGE_THR && memory >= SAME_LARGE_THR) {
            _category = Category::SAME_LARGE;
            if(cpu >= SAME_TOO_LARGE_THR && memory >= SAME_TOO_LARGE_THR) {
                _category = Category::SAME_TOO_LARGE;
            }
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
        volatile double k = (double) nowcpu / nowmem;
        if (fcmp(k - MORE_CPU_RATIO) >= 0) return Category::MORE_CPU;
        else if (fcmp(k - MORE_MEMORY_RATIO) <= 0) return Category::MORE_MEMORY;
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

    volatile double getCPUUsage(DeployNode node = DUAL_NODE) const {
        return (double) getLeftCPU(node) / cpu;
    }

    volatile double getMemoryUsage(DeployNode node = DUAL_NODE) const {
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
