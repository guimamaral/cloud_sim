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
        SimOutput("SLA violation: Unable to place task " + to_string(task_id), 0);
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
    // Do any bookkeeping necessary for the data structures
    // Decide if a machine is to be turned off, slowed down, or VMs to be migrated according to your policy
    // This is an opportunity to make any adjustments to optimize performance/energy

    SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) + " is complete at " + to_string(now), 4);
}

// Public interface below

static Scheduler Scheduler;

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

