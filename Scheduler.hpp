//
//  Scheduler.hpp
//  CloudSim
//
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
//

#ifndef Scheduler_hpp
#define Scheduler_hpp

#include <vector>

#include "Interfaces.h"

class Scheduler {
public:
    Scheduler()                 {}
    void Init();
    void MigrationComplete(Time_t time, VMId_t vm_id);
    void NewTask(Time_t now, TaskId_t task_id);
    void PeriodicCheck(Time_t now);
    void Shutdown(Time_t now);
    void TaskComplete(Time_t now, TaskId_t task_id);
    float CalculateUtilizationImbalance(MachineId_t simulated_machine, float simulated_utilization);
    VMId_t GetSmallestVMOnMachine(MachineId_t machine_id);
    MachineId_t FindBestMachineForVM(VMId_t vm_id);
    vector<VMId_t> vms;
    vector<MachineId_t> machines;
};



#endif /* Scheduler_hpp */
