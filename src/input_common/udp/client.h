// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <tuple>
#include "common/common_types.h"
#include "common/thread.h"
#include "common/vector_math.h"

namespace InputCommon::CemuhookUDP {

constexpr u16 DEFAULT_PORT = 26760;
constexpr char DEFAULT_ADDR[] = "127.0.0.1";

class Socket;

namespace Response {
struct PadData;
struct PortInfo;
struct Version;
} // namespace Response

struct DeviceStatus {
    std::mutex update_mutex;
    std::tuple<Common::Vec3<float>, Common::Vec3<float>> motion_status;
    std::tuple<float, float, bool> touch_status;

    // Calibration data for scaling the device's touch area to 3ds
    struct CalibrationData {
        u16 min_x{};
        u16 min_y{};
        u16 max_x{};
        u16 max_y{};
    };
    std::optional<CalibrationData> touch_calibration;
};

class Client {
public:
    explicit Client(std::shared_ptr<DeviceStatus> status, const std::string& host = DEFAULT_ADDR,
                    u16 port = DEFAULT_PORT, u8 pad_index = 0, u32 client_id = 24872);

    ~Client();

    void ReloadSocket(const std::string& host = "127.0.0.1", u16 port = 26760, u8 pad_index = 0,
                      u32 client_id = 24872);

private:
    void OnVersion(Response::Version);
    void OnPortInfo(Response::PortInfo);
    void OnPadData(Response::PadData);
    void StartCommunication(const std::string& host, u16 port, u8 pad_index, u32 client_id);

    std::unique_ptr<Socket> socket;
    std::shared_ptr<DeviceStatus> status;
    std::thread thread;
    u64 packet_sequence = 0;
};

} // namespace InputCommon::CemuhookUDP
