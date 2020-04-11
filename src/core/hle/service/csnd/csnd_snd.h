// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::CSND {

enum class Encoding : u8 {
    Pcm8 = 0,
    Pcm16 = 1,
    Adpcm = 2,
    Psg = 3,
};

enum class LoopMode : u8 {
    Manual = 0,  // Play block 1 endlessly ignoring the size
    Normal = 1,  // Play block 1 once, then repeat with block 2. Block size is reloaded every time a
                 // new block is started
    OneShot = 2, // Play block 1 once and stop
    ConstantSize = 3, // Similar to Normal, but only load block size once at the beginning
};

struct AdpcmState {
    s16 predictor = 0;
    u8 step_index = 0;
};

struct Channel {
    PAddr block1_address = 0;
    PAddr block2_address = 0;
    u32 block1_size = 0;
    u32 block2_size = 0;
    AdpcmState block1_adpcm_state;
    AdpcmState block2_adpcm_state;
    bool block2_adpcm_reload = false;
    u16 left_channel_volume = 0;
    u16 right_channel_volume = 0;
    u16 left_capture_volume = 0;
    u16 right_capture_volume = 0;
    u32 sample_rate = 0;
    bool linear_interpolation = false;
    LoopMode loop_mode = LoopMode::Manual;
    Encoding encoding = Encoding::Pcm8;
    u8 psg_duty = 0;
};

class CSND_SND final : public ServiceFramework<CSND_SND> {
public:
    explicit CSND_SND(Core::System& system);
    ~CSND_SND() = default;

private:
    void Initialize(Kernel::HLERequestContext& ctx);
    void Shutdown(Kernel::HLERequestContext& ctx);
    void ExecuteCommands(Kernel::HLERequestContext& ctx);
    void ExecuteType1Commands(Kernel::HLERequestContext& ctx);
    void AcquireSoundChannels(Kernel::HLERequestContext& ctx);
    void ReleaseSoundChannels(Kernel::HLERequestContext& ctx);
    void AcquireCapUnit(Kernel::HLERequestContext& ctx);
    void ReleaseCapUnit(Kernel::HLERequestContext& ctx);
    void FlushDataCache(Kernel::HLERequestContext& ctx);
    void StoreDataCache(Kernel::HLERequestContext& ctx);
    void InvalidateDataCache(Kernel::HLERequestContext& ctx);
    void Reset(Kernel::HLERequestContext& ctx);

    Core::System& system;

    std::shared_ptr<Kernel::Mutex> mutex = nullptr;
    std::shared_ptr<Kernel::SharedMemory> shared_memory = nullptr;

    static constexpr u32 MaxCaptureUnits = 2;
    std::array<bool, MaxCaptureUnits> capture_units = {false, false};

    static constexpr u32 ChannelCount = 32;
    std::array<Channel, ChannelCount> channels;

    u32 master_state_offset = 0;
    u32 channel_state_offset = 0;
    u32 capture_state_offset = 0;
    u32 type1_command_offset = 0;

    u32 acquired_channel_mask = 0;
};

/// Initializes the CSND_SND Service
void InstallInterfaces(Core::System& system);

} // namespace Service::CSND
