// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/hle/kernel/event.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::BOSS {

class Module final {
public:
    explicit Module(Core::System& system);
    ~Module() = default;

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> boss, const char* name, u32 max_session);
        ~Interface() = default;

    protected:
        void InitializeSession(Kernel::HLERequestContext& ctx);
        void SetStorageInfo(Kernel::HLERequestContext& ctx);
        void UnregisterStorage(Kernel::HLERequestContext& ctx);
        void GetStorageInfo(Kernel::HLERequestContext& ctx);
        void RegisterPrivateRootCa(Kernel::HLERequestContext& ctx);
        void RegisterPrivateClientCert(Kernel::HLERequestContext& ctx);
        void GetNewArrivalFlag(Kernel::HLERequestContext& ctx);
        void RegisterNewArrivalEvent(Kernel::HLERequestContext& ctx);
        void SetOptoutFlag(Kernel::HLERequestContext& ctx);
        void GetOptoutFlag(Kernel::HLERequestContext& ctx);
        void RegisterTask(Kernel::HLERequestContext& ctx);
        void UnregisterTask(Kernel::HLERequestContext& ctx);
        void ReconfigureTask(Kernel::HLERequestContext& ctx);
        void GetTaskIdList(Kernel::HLERequestContext& ctx);
        void GetStepIdList(Kernel::HLERequestContext& ctx);
        void GetNsDataIdList(Kernel::HLERequestContext& ctx);
        void GetNsDataIdList1(Kernel::HLERequestContext& ctx);
        void GetNsDataIdList2(Kernel::HLERequestContext& ctx);
        void GetNsDataIdList3(Kernel::HLERequestContext& ctx);
        void SendProperty(Kernel::HLERequestContext& ctx);
        void SendPropertyHandle(Kernel::HLERequestContext& ctx);
        void ReceiveProperty(Kernel::HLERequestContext& ctx);
        void UpdateTaskInterval(Kernel::HLERequestContext& ctx);
        void UpdateTaskCount(Kernel::HLERequestContext& ctx);
        void GetTaskInterval(Kernel::HLERequestContext& ctx);
        void GetTaskCount(Kernel::HLERequestContext& ctx);
        void GetTaskServiceStatus(Kernel::HLERequestContext& ctx);
        void StartTask(Kernel::HLERequestContext& ctx);
        void StartTaskImmediate(Kernel::HLERequestContext& ctx);
        void CancelTask(Kernel::HLERequestContext& ctx);
        void GetTaskFinishHandle(Kernel::HLERequestContext& ctx);
        void GetTaskState(Kernel::HLERequestContext& ctx);
        void GetTaskResult(Kernel::HLERequestContext& ctx);
        void GetTaskCommErrorCode(Kernel::HLERequestContext& ctx);
        void GetTaskStatus(Kernel::HLERequestContext& ctx);
        void GetTaskError(Kernel::HLERequestContext& ctx);
        void GetTaskInfo(Kernel::HLERequestContext& ctx);
        void DeleteNsData(Kernel::HLERequestContext& ctx);
        void GetNsDataHeaderInfo(Kernel::HLERequestContext& ctx);
        void ReadNsData(Kernel::HLERequestContext& ctx);
        void SetNsDataAdditionalInfo(Kernel::HLERequestContext& ctx);
        void GetNsDataAdditionalInfo(Kernel::HLERequestContext& ctx);
        void SetNsDataNewFlag(Kernel::HLERequestContext& ctx);
        void GetNsDataNewFlag(Kernel::HLERequestContext& ctx);
        void GetNsDataLastUpdate(Kernel::HLERequestContext& ctx);
        void GetErrorCode(Kernel::HLERequestContext& ctx);
        void RegisterStorageEntry(Kernel::HLERequestContext& ctx);
        void GetStorageEntryInfo(Kernel::HLERequestContext& ctx);
        void SetStorageOption(Kernel::HLERequestContext& ctx);
        void GetStorageOption(Kernel::HLERequestContext& ctx);
        void StartBgImmediate(Kernel::HLERequestContext& ctx);
        void GetTaskProperty0(Kernel::HLERequestContext& ctx);
        void RegisterImmediateTask(Kernel::HLERequestContext& ctx);
        void SetTaskQuery(Kernel::HLERequestContext& ctx);
        void GetTaskQuery(Kernel::HLERequestContext& ctx);
        void InitializeSessionPrivileged(Kernel::HLERequestContext& ctx);
        void GetAppNewFlag(Kernel::HLERequestContext& ctx);
        void GetNsDataIdListPrivileged(Kernel::HLERequestContext& ctx);
        void GetNsDataIdListPrivileged1(Kernel::HLERequestContext& ctx);
        void SendPropertyPrivileged(Kernel::HLERequestContext& ctx);
        void DeleteNsDataPrivileged(Kernel::HLERequestContext& ctx);
        void GetNsDataHeaderInfoPrivileged(Kernel::HLERequestContext& ctx);
        void ReadNsDataPrivileged(Kernel::HLERequestContext& ctx);
        void SetNsDataNewFlagPrivileged(Kernel::HLERequestContext& ctx);
        void GetNsDataNewFlagPrivileged(Kernel::HLERequestContext& ctx);

    private:
        std::shared_ptr<Module> boss;

        u8 new_arrival_flag;
        u8 ns_data_new_flag;
        u8 ns_data_new_flag_privileged;
        u8 output_flag;
    };

private:
    std::shared_ptr<Kernel::Event> task_finish_event;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::BOSS
