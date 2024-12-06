//
//  Scheduler.cpp
//  CloudSim
//
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
//
// Balanced Workload Allocation Algorithm

#include "Scheduler.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>

static Scheduler Scheduler;
static bool migrating = false;
static unsigned active_machines = 16;

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

        // MachineInfo_t info = Machine_GetInfo(machine_id);
        // MachineState_t s_state = info.s_state;
        // if (s_state != S5) {
        //     unsigned num_cpus = info.num_cpus;
        //     unsigned mem_size = info.memory_size;
        //     bool gpu = info.gpus;
        //     CPUType_t cpu = info.cpu;
        // }

    }
}

void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    // Update your data structure. The VM now can receive new tasks
}

bool SortMachines(MachineId_t a, MachineId_t b) {
    return Machine_GetEnergy(a) < Machine_GetEnergy(b);
}

unsigned GetTotalTaskMemory(VMId_t vm_id) {
    // Retrieve information about the specified VM
    VMInfo_t vm_info = VM_GetInfo(vm_id);

    // Initialize total memory
    unsigned total_memory = 0;

    // Iterate through all tasks in the VM
    for (TaskId_t task_id : vm_info.active_tasks) {
        // Retrieve task information
        TaskInfo_t task_info = GetTaskInfo(task_id);

        // Add the required memory for this task to the total
        total_memory += task_info.required_memory;
    }

    return total_memory;
}

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


void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    // Greedy Algorithm
    bool task_gpu_capable = IsTaskGPUCapable(task_id);
    unsigned task_memory = GetTaskMemory(task_id);
    VMType_t task_vm_type = RequiredVMType(task_id);
    SLAType_t task_sla = RequiredSLA(task_id);
    CPUType_t task_cpu = RequiredCPUType(task_id);

    //sort machines by energy consumption (ascending)
    std::sort(machines.begin(), machines.end(), SortMachines);

    //Assign each task to the machine that minimizes the difference in utilization across all machines
    // Variables to track the best machine
    MachineId_t best_machine = (MachineId_t)-1;
    float min_imbalance = std::numeric_limits<float>::max();

    // Iterate over machines to find the one that minimizes utilization imbalance
    for (MachineId_t machine_id : machines) {
        MachineInfo_t machine_info = Machine_GetInfo(machine_id);

        // Skip incompatible machines
        if (machine_info.cpu != task_cpu) continue;

        // Skip machines that cannot accommodate the task
        float current_utilization = (float)machine_info.memory_used / machine_info.memory_size;
        float task_load_factor = (float)(task_memory + VM_MEMORY_OVERHEAD) / machine_info.memory_size;
        if (current_utilization + task_load_factor > 1.0) continue;

        // Simulate placing the task on this machine and calculate the imbalance
        float potential_utilization = current_utilization + task_load_factor;

        // Calculate utilization imbalance across all machines
        float imbalance = CalculateUtilizationImbalance(machine_id, potential_utilization);

        // Update the best machine if this machine improves balance
        if (imbalance < min_imbalance) {
            min_imbalance = imbalance;
            best_machine = machine_id;
        }
    }

    // Place the task on the best machine
    if (best_machine != (MachineId_t)-1) {
        MachineInfo_t best_machine_info = Machine_GetInfo(best_machine);

        // Turn on the machine if it's off
        if (best_machine_info.s_state == S5) {
            Machine_SetState(best_machine, S0);
        }

        // Create a new VM and assign the task
        VMId_t vm_id = VM_Create(task_vm_type, task_cpu);
        VM_Attach(vm_id, best_machine);
        VM_AddTask(vm_id, task_id, MID_PRIORITY);
        vms.push_back(vm_id); // Track active VMs
    } else {
        // Handle SLA violation
        // SimOutput("SLA violation: Unable to place task " + to_string(task_id), 0);
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

void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {
    // VMId_t target_vm = (VMId_t)-1;
    // MachineId_t machine_id = (MachineId_t)-1;

    // // Find the VM hosting the completed task
    // for (auto &vm_id : vms) {
    //     VMInfo_t vm_info = VM_GetInfo(vm_id);
    //     auto it = std::find(vm_info.active_tasks.begin(), vm_info.active_tasks.end(), task_id);
    //     if (it != vm_info.active_tasks.end()) {
    //         target_vm = vm_id;
    //         machine_id = vm_info.machine_id;

    //         // Remove the task from the VM's active tasks
    //         vm_info.active_tasks.erase(it);
    //         break;
    //     }
    // }

    // if (target_vm == (VMId_t)-1 || machine_id == (MachineId_t)-1) {
    //     // Task not found; no further action
    //     SimOutput("TaskComplete(): Task not found in any VM.", 2);
    //     return;
    // }

    // SimOutput("TaskComplete(): Task " + to_string(task_id) + " completed at time " + to_string(now), 4);

    // // Check if the VM is empty and can be shut down
    // VMInfo_t vm_info = VM_GetInfo(target_vm);
    // if (vm_info.active_tasks.empty()) {
    //     VM_Shutdown(target_vm);
    //     vms.erase(std::remove(vms.begin(), vms.end(), target_vm), vms.end());
    //     SimOutput("TaskComplete(): VM " + to_string(target_vm) + " shut down.", 4);
    // }

    // // Check if the machine is underutilized or idle
    // unsigned machine_utilization = GetMachineUtilization(machine_id);
    // if (machine_utilization == 0) {
    //     // Machine is idle; turn it off
    //     Machine_SetState(machine_id, S5);
    //     SimOutput("TaskComplete(): Machine " + to_string(machine_id) + " turned off due to idleness.", 4);
    // } else if (machine_utilization < 0.2) {
    //     // Machine is underutilized; try to balance the workload
    //     VMId_t smallest_vm = GetSmallestVMOnMachine(machine_id);
    //     MachineId_t best_machine = FindBestMachineForVM(smallest_vm);

    //     if (best_machine != (MachineId_t)-1 && best_machine != machine_id) {
    //         // Migrate the VM to the best machine
    //         VM_Migrate(smallest_vm, best_machine);
    //         SimOutput("TaskComplete(): Migrated VM " + to_string(smallest_vm) + " from Machine " + to_string(machine_id) +
    //                   " to Machine " + to_string(best_machine), 4);
    //     }
    // }

    SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) + " is complete at " + to_string(now), 4);
}


// Public interface below


float Scheduler::CalculateUtilizationImbalance(MachineId_t simulated_machine, float simulated_utilization) {
    std::vector<float> utilizations;

    // Collect utilization data for all machines
    for (MachineId_t machine_id : machines) {
        if (machine_id == simulated_machine) {
            utilizations.push_back(simulated_utilization); // Use simulated utilization for this machine
        } else {
            MachineInfo_t machine_info = Machine_GetInfo(machine_id);
            utilizations.push_back((float)machine_info.memory_used / machine_info.memory_size);
        }
    }

    // Calculate mean utilization
    float sum = std::accumulate(utilizations.begin(), utilizations.end(), 0.0f);
    float mean = sum / utilizations.size();

    // Calculate standard deviation
    float squared_diff_sum = 0.0f;
    for (float utilization : utilizations) {
        squared_diff_sum += (utilization - mean) * (utilization - mean);
    }
    float std_dev = std::sqrt(squared_diff_sum / utilizations.size());

    return std_dev;
}

// MachineId_t Scheduler::FindBestMachineForVM(VMId_t vm_id) {
//     MachineId_t best_machine = (MachineId_t)-1;
//     float min_utilization = std::numeric_limits<float>::max();

//     VMInfo_t vm_info = VM_GetInfo(vm_id);
//     unsigned vm_memory = GetTotalTaskMemory(vm_id);

//     for (MachineId_t machine_id : machines) {
//         if (machine_id == vm_info.machine_id) continue; // Skip current machine

//         MachineInfo_t machine_info = Machine_GetInfo(machine_id);
//         float current_utilization = (float)machine_info.memory_used / machine_info.memory_size;
//         float new_utilization = current_utilization + ((float)vm_memory / machine_info.memory_size);

//         if (new_utilization < 1.0 && new_utilization < min_utilization) {
//             min_utilization = new_utilization;
//             best_machine = machine_id;
//         }
//     }

//     return best_machine;
// }


// VMId_t Scheduler::GetSmallestVMOnMachine(MachineId_t machine_id) {
//     unsigned min_workload = std::numeric_limits<unsigned>::max();
//     VMId_t smallest_vm = (VMId_t)-1;

//     for (VMId_t vm_id : vms) {
//         VMInfo_t vm_info = VM_GetInfo(vm_id);
//         if (vm_info.machine_id == machine_id) {
//             unsigned vm_memory = GetTotalTaskMemory(vm_id);
//             if (vm_memory < min_workload) {
//                 min_workload = vm_memory;
//                 smallest_vm = vm_id;
//             }
//         }
//     }
//     return smallest_vm;
// }



void InitScheduler() {
    SimOutput("InitScheduler(): Initializing scheduler", 4);
    Scheduler.Init();
}

void HandleNewTask(Time_t time, TaskId_t task_id) {
    SimOutput("HandleNewTask(): Received new task " + to_string(task_id) + " at time " + to_string(time), 4);
    Scheduler.NewTask(time, task_id);
}

void HandleTaskCompletion(Time_t time, TaskId_t task_id) {
    SimOutput("HandleTaskCompletion(): Task " + to_string(task_id) + " completed at time " + to_string(time), 4);
    Scheduler.TaskComplete(time, task_id);
}

void MemoryWarning(Time_t time, MachineId_t machine_id) {
    // The simulator is alerting you that machine identified by machine_id is overcommitted
    SimOutput("MemoryWarning(): Overflow at " + to_string(machine_id) + " was detected at time " + to_string(time), 0);
}

void MigrationDone(Time_t time, VMId_t vm_id) {
    // The function is called on to alert you that migration is complete
    SimOutput("MigrationDone(): Migration of VM " + to_string(vm_id) + " was completed at time " + to_string(time), 4);
    Scheduler.MigrationComplete(time, vm_id);
    migrating = false;
}

void SchedulerCheck(Time_t time) {
    // This function is called periodically by the simulator, no specific event
    SimOutput("SchedulerCheck(): SchedulerCheck() called at " + to_string(time), 4);
    Scheduler.PeriodicCheck(time);
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

    Scheduler.Shutdown(time);
}

void SLAWarning(Time_t time, TaskId_t task_id) {

}

void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    // Called in response to an earlier request to change the state of a machine
}

//assisted by ChatGPT