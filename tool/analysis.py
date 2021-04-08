import sys
import copy
import matplotlib.pyplot as plt
import numpy as np

class VM:
    'VM类'

    def __init__(self, id, model, cpu, memory, dual):
        self.id = id
        self.model = model
        self.cpu = cpu
        self.memory = memory
        self.dual = dual

        self.cat = 1
        if float(cpu) / memory > 2.1:
            self.cat = 2
        if float(cpu) / memory < 1 / 2.1:
            self.cat = 4

class Server:
    'PM类'
    vm_dict = {}

    def __init__(self, id, model, cpu, memory, hardware_cost, energy_cost):
        self.id = id
        self.model = model
        self.cpu_src = cpu
        self.cpu = [cpu / 2, cpu / 2]
        self.memory_src = memory
        self.memory = [memory / 2, memory / 2]
        self.hardware_cost = hardware_cost
        self.energy_cost = energy_cost
        self.vm_count = 0

        self.cat = 0
        if float(cpu) / memory > 2.1:
            self.cat = 2
        if float(cpu) / memory < 1 / 2.1:
            self.cat = 4
        if cpu >= 300 and memory >= 300:
            self.cat = 1

    def copy(self, id):
        return Server(id, self.model, self.cpu_src, self.memory_src, self.hardware_cost, self.energy_cost)

    def get_cpu_usage(self):
        return 1 - float(self.cpu[0] + self.cpu[1]) / self.cpu_src

    def get_memory_usage(self):
        return 1 - float(self.memory[0] + self.memory[1]) / self.memory_src

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
hardware_cost_list = []
energy_cost_list = []
cost_list = []

server_model_cnt = {} # 不同model的计数
server_cnt = [] # list<5>: 四种cat的计数
server_empty_cnt = [] # 空服务器计数
server_inuse_cnt = [] # 在用服务器计数
server_usage_cnt = [] # list<6>: CPU高低、内存高低、都高低

cpu_usage = [] # CPU占用率
memory_usage = [] # 内存占用率

vm_cnt = [] # list<5>: 四种cat的计数

def get_server_model_cnt():
    for serv in server_list.values():
        if not serv.model in server_model_cnt.keys():
            server_model_cnt[serv.model] = 1
        else:
            server_model_cnt[serv.model] += 1

def get_day_server_cnt():
    day_server_cnt = [0, 0, 0, 0, 0]
    for serv in server_list.values():
        day_server_cnt[serv.cat] += 1
    server_cnt.append(day_server_cnt)

def get_day_vm_cnt():
    day_vm_cnt = [0, 0, 0, 0, 0]
    for vm in vm_list.values():
        day_vm_cnt[vm.cat] += 1
    vm_cnt.append(day_vm_cnt)

def get_day_server_empty_inuse_cnt():
    day_server_empty_cnt = 0
    for serv in server_list.values():
        if serv.vm_count == 0:
            day_server_empty_cnt += 1
    server_empty_cnt.append(day_server_empty_cnt)
    server_inuse_cnt.append(len(server_list) - day_server_empty_cnt)

def get_day_server_usage_cnt():
    day_usage_cnt = [0, 0, 0, 0, 0, 0]
    for serv in server_list.values():
        flag_cpu_high = serv.get_cpu_usage() > 0.7
        flag_cpu_low = serv.get_cpu_usage() < 0.1
        flag_memory_high = serv.get_memory_usage() > 0.7
        flag_memory_low = serv.get_memory_usage() < 0.1

        if flag_cpu_high:
            day_usage_cnt[0] += 1
        if flag_cpu_low:
            day_usage_cnt[1] += 1
        if flag_memory_high:
            day_usage_cnt[2] += 1
        if flag_memory_low:
            day_usage_cnt[3] += 1
        if flag_cpu_high and flag_memory_high:
            day_usage_cnt[4] += 1
        if flag_cpu_low and flag_memory_low:
            day_usage_cnt[5] += 1
        
    server_usage_cnt.append(day_usage_cnt)

def get_day_cpu_memory_usage():
    cpu_inuse = 0
    cpu_sum = 0
    memory_inuse = 0
    memory_sum = 0

    for serv in server_list.values():
        if serv.vm_count == 0:
            continue
        cpu_inuse += serv.cpu[0] + serv.cpu[1]
        cpu_sum += serv.cpu_src
        memory_inuse += serv.memory[0] + serv.memory[1]
        memory_sum += serv.memory_src
    
    cpu_usage.append(1 - float(cpu_inuse) / cpu_sum)
    memory_usage.append(1 - float(memory_inuse) / memory_sum)

def collect_data():
    in_f = open(sys.argv[1], 'r')

    # 读入PM信息
    N = int(in_f.readline().strip())
    for i in range(N):
        line = in_f.readline()
        line = line[1:-2]
        obj = line.split(',')

        model = obj[0].strip()
        cpu = int(obj[1].strip())
        memory = int(obj[2].strip())
        hardware_cost = int(obj[3].strip())
        energy_cost = int(obj[4].strip())
        server = Server(0, model, cpu, memory, hardware_cost, energy_cost)

        server_type_list[model] = server
    
    # 读入VM信息
    M = int(in_f.readline().strip())
    for i in range(M):
        line = in_f.readline()
        line = line[1:-2]
        obj = line.split(',')

        model = obj[0].strip()
        cpu = int(obj[1].strip())
        memory = int(obj[2].strip())
        dual = int(obj[3].strip())
        vm = VM(0, model, cpu, memory, bool(dual))

        vm_type_list[model] = vm
    
    T = int(in_f.readline().split()[0].strip())

    ans_f = open(sys.argv[2], 'r')

    # 处理天数
    global server_id_count
    for day in range(1, T + 1):
        day_hardware_cost = 0
        day_energy_cost = 0

        line = ans_f.readline()
        if len(line) == 0:
            break
        line = line[1:-2]
        obj = line.split(',')
        
        # 购买
        Q = int(obj[1].strip())
        for i in range(Q):
            line = ans_f.readline()
            line = line[1:-2]
            obj = line.split(',')

            model = obj[0].strip()
            count = int(obj[1].strip())

            for i in range(count):
                server: Server = server_type_list[model].copy(server_id_count)
                server_id_count += 1
                server_list[server.id] = server

                day_hardware_cost += server.hardware_cost

        line = ans_f.readline()
        line = line[1:-2]
        obj = line.split(',')
        
        # 迁移
        W = int(obj[1].strip())
        for i in range(W):
            line = ans_f.readline()
            line = line[1:-2]
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
            line = line[1:-2]
            obj = line.split(',')

            model = ""
            vm_id = -1
            if obj[0].strip() == 'add':
                model = obj[1].strip()
                vm_id = int(obj[2].strip())
            else:
                vm_id = int(obj[1].strip())
            
            if obj[0].strip() == 'add':
                line = ans_f.readline()
                line = line[1:-2]
                obj = line.split(',')
                
                vm: VM = copy.copy(vm_type_list[model])
                vm.id = vm_id
                vm_list[vm_id] = vm

                server_id = int(obj[0].strip())
                node = 2
                if not vm.dual:
                    node_char = obj[1].strip()
                    if node_char == 'A':
                        node = 0
                    else:
                        node = 1

                server: Server = server_list[server_id]
                server.deploy(vm, node)
                server_list[server_id] = server
            else:
                vm = vm_list[vm_id]
                server_id = Server.vm_dict[vm_id][0]
                server: Server = server_list[server_id]
                server.remove(vm)
                server_list[server_id] = server
                del vm_list[vm_id]

        for serv in server_list.values():
            if serv.vm_count > 0:
                day_energy_cost += serv.energy_cost
            
        hardware_cost_list.append(day_hardware_cost)
        energy_cost_list.append(day_energy_cost)
        cost_list.append(day_energy_cost + day_hardware_cost)
        get_day_vm_cnt()

        get_day_server_cnt()
        get_day_server_empty_inuse_cnt()
        get_day_server_usage_cnt()
        get_day_cpu_memory_usage()

        print('day %d server_cnt %d vm_cnt %d cost %d'%(day, len(server_list), len(vm_list), day_energy_cost + day_hardware_cost))

    get_server_model_cnt()

def make_picture():
    FIG_CNT = 12

    fig, axs = plt.subplots(FIG_CNT)
    fig.set_figheight(4 * FIG_CNT)
    fig.set_figwidth(8)

    T = len(cost_list)
    day_data = []
    for i in range(1, T + 1):
        day_data.append(i)

    # 总开销折线图
    axs[0].plot(day_data, cost_list)
    axs[0].set_title('overall_cost')
    axs[0].set(xlabel='days', ylabel='cost')

    # 硬件开销折线图
    axs[1].plot(day_data, hardware_cost_list)
    axs[1].set_title('hardware_cost')
    axs[1].set(xlabel='days', ylabel='cost')   

    # 能耗开销折线图
    axs[2].plot(day_data, energy_cost_list)
    axs[2].set_title('energy_cost')
    axs[2].set(xlabel='days', ylabel='cost')  

    # 服务器型号饼状图
    model_data = []
    model_cnt_data = []
    for model, cnt in server_model_cnt.items():
        model_data.append(model + ' (%d, %d)'%(server_type_list[model].cpu_src, server_type_list[model].memory_src))
        model_cnt_data.append(cnt)
    axs[3].pie(model_cnt_data, labels=model_data, autopct='%1.1f%%')
    axs[3].set_title('server_model_cnt')

    # 4种cat的服务器折线图
    cat_cnt_data = [[], [], [], [], [], []]
    for list in server_cnt:
        cnt_sum = 0
        for i in range(5):
            cat_cnt_data[i].append(list[i])
            cnt_sum += list[i]
        cat_cnt_data[5].append(cnt_sum)
    for i in range(5):
        axs[4].plot(day_data, cat_cnt_data[i], label=str(i))
    axs[4].plot(day_data, cat_cnt_data[5], label='sum')
    axs[4].legend(loc='upper left')
    axs[4].set_title('server_cnt')
    axs[4].set(xlabel='days', ylabel='count')

    # 空闲、在用服务器折线图
    axs[5].plot(day_data, server_empty_cnt, label='empty')
    axs[5].plot(day_data, server_inuse_cnt, label='in use')
    axs[5].legend(loc='upper left')
    axs[5].set_title('server_empty_cnt')
    axs[5].set(xlabel='days', ylabel='count')

    # CPU占用高低服务器折线图
    cpu_aspect_cnt = [[], []]
    for list in server_usage_cnt:
        cpu_aspect_cnt[0].append(list[0])
        cpu_aspect_cnt[1].append(list[1])
    axs[6].plot(day_data, cpu_aspect_cnt[0], label='cpu_high')
    axs[6].plot(day_data, cpu_aspect_cnt[1], label='cpu_low')
    axs[6].legend(loc='upper left')
    axs[6].set_title('cpu_aspect_cnt')
    axs[6].set(xlabel='days', ylabel='count')

    # 内存占用高低服务器折线图
    memory_aspect_cnt = [[], []]
    for list in server_usage_cnt:
        memory_aspect_cnt[0].append(list[2])
        memory_aspect_cnt[1].append(list[3])
    axs[7].plot(day_data, memory_aspect_cnt[0], label='memory_high')
    axs[7].plot(day_data, memory_aspect_cnt[1], label='memory_low')
    axs[7].legend(loc='upper left')
    axs[7].set_title('memory_aspect_cnt')
    axs[7].set(xlabel='days', ylabel='count')

    # CPU+内存占用高低服务器折线图
    both_aspect_cnt = [[], []]
    for list in server_usage_cnt:
        both_aspect_cnt[0].append(list[4])
        both_aspect_cnt[1].append(list[5])
    axs[8].plot(day_data, both_aspect_cnt[0], label='both_high')
    axs[8].plot(day_data, both_aspect_cnt[1], label='both_low')
    axs[8].legend(loc='upper left')
    axs[8].set_title('both_aspect_cnt')
    axs[8].set(xlabel='days', ylabel='count')

    # CPU占用率折线图
    axs[9].plot(day_data, cpu_usage)
    axs[9].set_title('cpu_usage')
    axs[9].set(xlabel='days', ylabel='percent')

    # 内存占用率折线图
    axs[10].plot(day_data, memory_usage)
    axs[10].set_title('memory_usage')
    axs[10].set(xlabel='days', ylabel='percent')

    # VM数量折线图
    cat_cnt_data = [[], [], [], [], [], []]
    for list in vm_cnt:
        cnt_sum = 0
        for i in range(5):
            cat_cnt_data[i].append(list[i])
            cnt_sum += list[i]
        cat_cnt_data[5].append(cnt_sum)
    for i in range(5):
        axs[11].plot(day_data, cat_cnt_data[i], label=str(i))
    axs[11].plot(day_data, cat_cnt_data[5], label='sum')
    axs[11].legend(loc='upper left')
    axs[11].set_title('vm_cnt')
    axs[11].set(xlabel='days', ylabel='count')

    plt.savefig('result.jpg', dpi=300)
    # plt.show()

if __name__ == '__main__':
    collect_data()
    make_picture()
