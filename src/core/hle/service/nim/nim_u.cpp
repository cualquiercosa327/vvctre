// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/event.h"
#include "core/hle/service/nim/nim_u.h"

namespace Service::NIM {

NIM_U::NIM_U(Core::System& system) : ServiceFramework("nim:u", 2) {
    const FunctionInfo functions[] = {
        {0x00010000, nullptr, "StartSysUpdate"},
        {0x00020000, nullptr, "GetUpdateDownloadProgress"},
        {0x00040000, nullptr, "FinishTitlesInstall"},
        {0x00050000, &NIM_U::CheckForSysUpdateEvent, "CheckForSysUpdateEvent"},
        {0x00090000, &NIM_U::CheckSysUpdateAvailable, "CheckSysUpdateAvailable"},
        {0x000A0000, nullptr, "GetState"},
        {0x000B0000, nullptr, "GetSystemTitleHash"},
        {0x00110000, &NIM_U::Unknown, "Unknown"},
    };
    RegisterHandlers(functions);

    nim_system_update_event =
        system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "NIM System Update Event");

    unknown_event = system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "NIM Unknown Event");
}

NIM_U::~NIM_U() = default;

void NIM_U::CheckForSysUpdateEvent(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x5, 0, 0); // 0x50000
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(nim_system_update_event);
    LOG_TRACE(Service_NIM, "called");
}

void NIM_U::CheckSysUpdateAvailable(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x9, 0, 0); // 0x90000

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(false); // No update available

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

void NIM_U::Unknown(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x11, 0, 0); // 0x00110000
    unknown_event->Signal();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(unknown_event);
}

} // namespace Service::NIM
