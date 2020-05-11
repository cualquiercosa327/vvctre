// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>
#include "core/hle/service/service.h"

namespace Kernel {
class Event;
class SharedMemory;
} // namespace Kernel

namespace Service::IR {

class BufferManager;
class ExtraHID;

/// An interface representing a device that can communicate with 3DS via ir:USER service
class IRDevice {
public:
    /**
     * A function object that implements the method to send data to the 3DS, which takes a vector of
     * data to send.
     */
    using SendFunc = std::function<void(const std::vector<u8>& data)>;

    explicit IRDevice(SendFunc send_func);
    virtual ~IRDevice();

    /// Called when connected with 3DS
    virtual void OnConnect() = 0;

    /// Called when disconnected from 3DS
    virtual void OnDisconnect() = 0;

    /// Called when data is received from the 3DS. This is invoked by the ir:USER send function.
    virtual void OnReceive(const std::vector<u8>& data) = 0;

protected:
    /// Sends data to the 3DS. The actual sending method is specified in the constructor
    void Send(const std::vector<u8>& data);

private:
    const SendFunc send_func;
};

/// Interface to "ir:USER" service
class IR_USER final : public ServiceFramework<IR_USER> {
public:
    explicit IR_USER(Core::System& system);
    ~IR_USER();

    void ReloadInputDevices();
    void SetCustomCirclePadProState(std::optional<std::tuple<float, float, bool, bool>> state);
    std::tuple<float, float, bool, bool> GetCirclePadProState();

private:
    void InitializeIrNopShared(Kernel::HLERequestContext& ctx);
    void RequireConnection(Kernel::HLERequestContext& ctx);
    void GetReceiveEvent(Kernel::HLERequestContext& ctx);
    void GetSendEvent(Kernel::HLERequestContext& ctx);
    void Disconnect(Kernel::HLERequestContext& ctx);
    void GetConnectionStatusEvent(Kernel::HLERequestContext& ctx);
    void FinalizeIrNop(Kernel::HLERequestContext& ctx);
    void SendIrNop(Kernel::HLERequestContext& ctx);
    void ReleaseReceivedData(Kernel::HLERequestContext& ctx);

    void PutToReceive(const std::vector<u8>& payload);

    std::shared_ptr<Kernel::Event> conn_status_event, send_event, receive_event;
    std::shared_ptr<Kernel::SharedMemory> shared_memory;
    IRDevice* connected_device{nullptr};
    std::unique_ptr<BufferManager> receive_buffer;
    std::unique_ptr<ExtraHID> extra_hid;
};

} // namespace Service::IR
