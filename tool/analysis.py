import sys


class VM:
    'VM类'

    def __init__(self, id, model, cpu, memory, dual):
        self.id = id
        self.model = model
        self.cpu = cpu
        self.memory = memory
        self.dual = dual

class Server:
    'PM类'
    vm_dict = {}

    def __init__(self, id, model, cpu, memory, hardware_cost, energy_cost):
        self.id = id
        self.model = model
        self.cpu = [cpu / 2, cpu / 2]
        self.memory = [memory / 2, memory / 2]
        self.hardware_cost = hardware_cost
        self.energy_cost = energy_cost
        self.vm_count = 0

    def deploy(self, vm: VM, node=2):
        Server.vm_dict[vm.id] = (self.id, node)
        self.vm_count += 1
        if node != 2:
            self.cpu[node] -= vm.cpu
            self.memory[node] -= vm.memory
        else:
            self.cpu[0] -= vm.cpu / 2
            self.memory[0] -= vm.memory / 2
            self.cpu[1] -= vm.cpu / 2
            self.memory[1] -= vm.memory / 2

    def remove(self, vm: VM):
        self.vm_count -= 1
        node = Server.vm_dict[vm.id][1]
        if node != 2:
            self.cpu[node] += vm.cpu
            self.memory[node] += vm.memory
        else:
            self.cpu[0] += vm.cpu / 2
            self.memory[0] += vm.memory / 2
            self.cpu[1] += vm.cpu / 2
            self.memory[1] += vm.memory / 2

        del Server.vm_dict[vm.id]

vm_list = {}
server_list = {}
server_id_count = 0

vm_type_list = {}
server_type_list = {}

# 统计数据
hardware_cost_sum = 0
energy_cost_sum = 0

if __name__ == '__main__':
    in_f = open(sys.argv[1], 'r')

    # 读入PM信息
    N = int(in_f.readline().strip())
    for i in range(N):
        line = in_f.readline()
        line = line[1:-1]
        obj = line.split(',')

        model = obj[0].strip()
        cpu = int(obj[1].strip())
        memory = int(obj[2].strip())
        hardware_cost = int(obj[3].strip())
        energy_cost = int(obj[4].strip())
        server = Server(model, cpu, memory, hardware_cost, energy_cost)

        server_type_list[model] = server
    
    # 读入VM信息
    M = int(in_f.readline().strip())
    for i in range(M):
        line = in_f.readline()
        line = line[1:-1]
        obj = line.split(',')

        model = obj[0].strip()
        cpu = int(obj[1].strip())
        memory = int(obj[2].strip())
        dual = int(obj[3].strip())
        vm = VM(model, cpu, memory, bool(dual))

        vm_type_list[model] = vm
    
    T = int(in_f.readline().split()[0].strip())

    ans_f = open(sys.argv[2], 'r')

    # 处理天数
    for day in range(1, T + 1):
        hardware_cost = 0
        energy_cost = 0

        line = ans_f.readline()
        if len(line) == 0:
            break
        line = line[1:-1]
        obj = line.split(',')
        
        # 购买
        Q = int(obj[1].strip())
        for i in range(Q):
            line = ans_f.readline()
            line = line[1:-1]
            obj = line.split(',')

            model = obj[0].strip()
            count = int(obj[1].strip())
            for i in range(count):
                server: Server = server_type_list[model]
                server.id = server_id_count
                server_id_count += 1
                server_list[server.id] = server

        line = ans_f.readline()
        line = line[1:-1]
        obj = line.split(',')
        
        # 迁移
        W = int(obj[1].strip())
        for i in range(W):
            line = ans_f.readline()
            line = line[1:-1]
            obj = line.split(',')

            vm_id = int(obj[0].strip())
            server_id = int(obj[1].strip())
            node = 2
            vm: VM = vm_list[vm_id]
            if not vm.dual:
                node_char = obj[2].strip()
                if node_char == 'A':
                    node = 0
                else:
                    node = 1

            last_server_id = Server.vm_dict[vm_id][0]
            last_server: Server = server_list[last_server_id]
            last_server.remove(vm)
            server_list[last_server_id] = last_server
            server: Server = server_list[server_id]
            server.deploy(vm, node)
            server_list[server_id] = server
        
        # 执行请求
        R = int(in_f.readline().strip())
        for i in range(R):
            line = in_f.readline()
            line = line[1:-1]
            obj = line.split(',')

            if obj[0].strip() == 'add':
                
            else:





