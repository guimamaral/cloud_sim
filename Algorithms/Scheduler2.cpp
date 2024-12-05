//
//  Scheduler.cpp
//  CloudSim
//
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
//
// P-MAPPER ALGORITHM

#include "Scheduler.hpp"
#include <algorithm>
#include <limits>

static Scheduler s;
static bool migrating = false;
static unsigned active_machines = 16;
unsigned GetMachineUtilization(MachineId_t machine_id) {
    unsigned utilization = 0; 
    for (VMId_t vm_id : Scheduler.vms) {
        VMInfo_t vm_info = VM_GetInfo(vm_id);
        if (vm_info.machine_id == machine_id) {
            utilization += vm_info.active_tasks.size();
        }
    }
    return utilization;
}
bool SortMachines(MachineId_t a, MachineId_t b) {
    return Machine_GetInfo(a).memory_size < Machine_GetInfo(b).memory_size;
}

void Scheduler::Init() {
    // Find the parameters of the clusters
    // Get the total number of machines
    // For each machine:
    //      Get the type of the machine
    //      Get the memory of the machine
    //      Get the number of CPUs
    //      Get if there is a GPU or not
    //
    SimOutput("Scheduler::Init(): Total number of machines is " + to_string(Machine_GetTotal()), 3);
    SimOutput("Scheduler::Init(): Initializing scheduler", 1);
    unsigned total_machines = Machine_GetTotal();
    for(unsigned i = 0; i < total_machines; i++) {
        MachineId_t machine_id = MachineId_t(i);
        machines.push_back(machine_id);
    }
    // Sort machines by energy consumption
    std::sort(machines.begin(), machines.end(), SortMachines);
}

void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    // Update your data structure. The VM now can receive new tasks
}

void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    // P-Mapper algorithm

    bool task_gpu_capable = IsTaskGPUCapable(task_id);
    unsigned task_memory = GetTaskMemory(task_id);
    VMType_t task_vm_type = RequiredVMType(task_id);
    SLAType_t task_sla = RequiredSLA(task_id);
    CPUType_t task_cpu = RequiredCPUType(task_id);

    unsigned total_machines = Machine_GetTotal();
    for (unsigned i = 0; i < total_machines; i++) {
        MachineId_t machine_id = machines[i];
        MachineInfo_t machine_info = Machine_GetInfo(machine_id);
        if (task_cpu != machine_info.cpu) {
            continue;
        }
        if (machine_info.s_state != S0) {
           Machine_SetState(machine_id, S0);
        }
        unsigned machine_utilization = GetMachineUtilization(machine_id);
        float memory_utilization = (float) machine_info.memory_used / machine_info.memory_size;
        float task_load_factor = (float) (task_memory + VM_MEMORY_OVERHEAD) / machine_info.memory_size;

        if (machine_utilization + 1 < machine_info.num_cpus * 50 && memory_utilization + task_load_factor < 1.0) {
            VMId_t vm_id = VM_Create(task_vm_type, task_cpu);
            vms.push_back(vm_id);
            VM_Attach(vm_id, machine_id);
            VM_AddTask(vm_id, task_id, MID_PRIORITY);
            return;
        } 
    }

    // Turn off unused machines
    for (MachineId_t machine_id : machines) {
        MachineInfo_t machine_info = Machine_GetInfo(machine_id);
        float machine_utilization = (float) machine_info.memory_used / machine_info.memory_size;
        if (machine_info.s_state != S5 && machine_utilization == 0.0) {
            Machine_SetState(machine_id, S5);
        }
    }
}

void Scheduler::PeriodicCheck(Time_t now) {
    // This method should be called from SchedulerCheck()
    // SchedulerCheck is called periodically by the simulator to allow you to monitor, make decisions, adjustments, etc.
    // Unlike the other invocations of the scheduler, this one doesn't report any specific event
    // Recommendation: Take advantage of this function to do some monitoring and adjustments as necessary
}

void Scheduler::Shutdown(Time_t time) {
    // Do your final reporting and bookkeeping here.
    // Report about the total energy consumed
    // Report about the SLA compliance
    // Shutdown everything to be tidy :-)
    for(auto & vm: vms) {
        VM_Shutdown(vm);
    }
    SimOutput("SimulationComplete(): Finished!", 4);
    SimOutput("SimulationComplete(): Time is " + to_string(time), 4);
}

static MachineId_t GetLeastUtilizedMachine() {
    float min_utilization = 100.0;
    MachineId_t least_utilized_machine;
    for (auto machine_id : s.machines) {
        MachineInfo_t machine_info = Machine_GetInfo(machine_id);
        float machine_utilization = (float) machine_info.memory_used / machine_info.memory_size;
        if (machine_utilization < min_utilization) {
            least_utilized_machine = machine_id;
        }
    }
    return least_utilized_machine;
}

unsigned GetTotalTaskMemoryForVM(VMInfo_t vm) {
    unsigned total = 0;
    for (TaskId_t task_id : vm.active_tasks) {
        TaskInfo_t task_info = GetTaskInfo(task_id);
        total += task_info.required_memory;
    }
    return total;
}

static VMId_t GetSmallestWorkload(MachineId_t machine_id) {
    unsigned min_workload = 4294967295;
    VMId_t smallest_workload = -1;
    for (VMId_t vm_id : s.vms) {
        cout << vm_id << endl;
        VMInfo_t vm_info = VM_GetInfo(vm_id);
        unsigned vm_total_task_memory = GetTotalTaskMemoryForVM(vm_info);
        if (vm_info.machine_id == machine_id &&
            vm_total_task_memory < min_workload) {
            smallest_workload = vm_id;
        }
    }
    return smallest_workload;
}

void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {
    MachineId_t least_utilized_machine = GetLeastUtilizedMachine();
    VMId_t smallest_workload_on_machine = GetSmallestWorkload(least_utilized_machine);
    if (smallest_workload_on_machine == -1) {
        return;
    }
    VMInfo_t vm_info = VM_GetInfo(smallest_workload_on_machine);
    unsigned num_machines = Machine_GetTotal();
    for (int i = num_machines - 1; i >= 0; i--) {
        MachineId_t machine_id = machines[i];
        MachineInfo_t machine_info = Machine_GetInfo(machine_id);
        if (vm_info.cpu != machine_info.cpu) {
            continue;
        }
        float machine_utilization = (float) machine_info.memory_used / machine_info.memory_size;
        float task_load_factor = (float) (GetTotalTaskMemoryForVM(vm_info) + VM_MEMORY_OVERHEAD)
            / machine_info.memory_size;
        if (machine_utilization + task_load_factor < 1.0) {
            if (machine_info.s_state == S0) {
                VM_Migrate(smallest_workload_on_machine, machine_id);
                return;
            }
        }
    }
    SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) + " is complete at " + to_string(now), 4);
}
// Public interface below

void InitScheduler() {
    SimOutput("InitScheduler(): Initializing scheduler", 4);
    s.Init();
}

void HandleNewTask(Time_t time, TaskId_t task_id) {
    SimOutput("HandleNewTask(): Received new task " + to_string(task_id) + " at time " + to_string(time), 4);
    s.NewTask(time, task_id);
}

void HandleTaskCompletion(Time_t time, TaskId_t task_id) {
    SimOutput("HandleTaskCompletion(): Task " + to_string(task_id) + " completed at time " + to_string(time), 4);
    s.TaskComplete(time, task_id);
}

void MemoryWarning(Time_t time, MachineId_t machine_id) {
    // The simulator is alerting you that machine identified by machine_id is overcommitted
    SimOutput("MemoryWarning(): Overflow at " + to_string(machine_id) + " was detected at time " + to_string(time), 0);
}

void MigrationDone(Time_t time, VMId_t vm_id) {
    // The function is called on to alert you that migration is complete
    SimOutput("MigrationDone(): Migration of VM " + to_string(vm_id) + " was completed at time " + to_string(time), 4);
    s.MigrationComplete(time, vm_id);
    migrating = false;
}

void SchedulerCheck(Time_t time) {
    // This function is called periodically by the simulator, no specific event
    SimOutput("SchedulerCheck(): SchedulerCheck() called at " + to_string(time), 4);
    s.PeriodicCheck(time);
    // static unsigned counts = 0;
    // counts++;
    // if(counts == 10) {
    //     migrating = true;
    //     VM_Migrate(1, 9);
    // }
}

void SimulationComplete(Time_t time) {
    // This function is called before the simulation terminates Add whatever you feel like.
    cout << "SLA violation report" << endl;
    cout << "SLA0: " << GetSLAReport(SLA0) << "%" << endl;
    cout << "SLA1: " << GetSLAReport(SLA1) << "%" << endl;
    cout << "SLA2: " << GetSLAReport(SLA2) << "%" << endl;     // SLA3 do not have SLA violation issues
    cout << "Total Energy " << Machine_GetClusterEnergy() << "KW-Hour" << endl;
    cout << "Simulation run finished in " << double(time)/1000000 << " seconds" << endl;
    SimOutput("SimulationComplete(): Simulation finished at time " + to_string(time), 4);

    s.Shutdown(time);
}

void SLAWarning(Time_t time, TaskId_t task_id) {

}

void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    // Called in response to an earlier request to change the state of a machine
}

