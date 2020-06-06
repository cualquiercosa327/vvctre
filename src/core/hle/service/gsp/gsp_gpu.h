// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/result.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class SharedMemory;
} // namespace Kernel

namespace Service::GSP {

/// GSP interrupt ID
enum class InterruptId : u8 {
    PSC0 = 0x00,
    PSC1 = 0x01,
    PDC0 = 0x02, // Seems called every vertical screen line
    PDC1 = 0x03, // Seems called every frame
    PPF = 0x04,
    P3D = 0x05,
    DMA = 0x06,
};

/// GSP command ID
enum class CommandId : u32 {
    REQUEST_DMA = 0x00,
    /// Submits a commandlist for execution by the GPU.
    SUBMIT_GPU_CMDLIST = 0x01,

    // Fills a given memory range with a particular value
    SET_MEMORY_FILL = 0x02,

    // Copies an image and optionally performs color-conversion or scaling.
    // This is highly similar to the GameCube's EFB copy feature
    SET_DISPLAY_TRANSFER = 0x03,

    // Conceptionally similar to SET_DISPLAY_TRANSFER and presumable uses the same hardware path
    SET_TEXTURE_COPY = 0x04,
    /// Flushes up to 3 cache regions in a single command.
    CACHE_FLUSH = 0x05,
};

/// GSP thread interrupt relay queue
struct InterruptRelayQueue {
    // Index of last interrupt in the queue
    u8 index;
    // Number of interrupts remaining to be processed by the userland code
    u8 number_interrupts;
    // Error code - zero on success, otherwise an error has occurred
    u8 error_code;
    u8 padding1;

    u32 missed_PDC0;
    u32 missed_PDC1;

    InterruptId slot[0x34]; ///< Interrupt ID slots
};
static_assert(sizeof(InterruptRelayQueue) == 0x40, "InterruptRelayQueue struct has incorrect size");

struct FrameBufferInfo {
    u32 active_fb; // 0 = first, 1 = second
    u32 address_left;
    u32 address_right;
    u32 stride;   // maps to 0x1EF00X90 ?
    u32 format;   // maps to 0x1EF00X70 ?
    u32 shown_fb; // maps to 0x1EF00X78 ?
    u32 unknown;
};
static_assert(sizeof(FrameBufferInfo) == 0x1c, "Struct has incorrect size");

struct FrameBufferUpdate {
    BitField<0, 1, u8> index;    // Index used for GSP::SetBufferSwap
    BitField<0, 1, u8> is_dirty; // true if GSP should update GPU framebuffer registers
    u16 pad1;

    FrameBufferInfo framebuffer_info[2];

    u32 pad2;
};
static_assert(sizeof(FrameBufferUpdate) == 0x40, "Struct has incorrect size");
// TODO: Not sure if this padding is correct.
// Chances are the second block is stored at offset 0x24 rather than 0x20.
#ifndef _MSC_VER
static_assert(offsetof(FrameBufferUpdate, framebuffer_info[1]) == 0x20,
              "FrameBufferInfo element has incorrect alignment");
#endif

/// GSP command
struct Command {
    BitField<0, 8, CommandId> id;

    union {
        struct {
            u32 source_address;
            u32 dest_address;
            u32 size;
        } dma_request;

        struct {
            u32 address;
            u32 size;
            u32 flags;
            u32 unused[3];
            u32 do_flush;
        } submit_gpu_cmdlist;

        struct {
            u32 start1;
            u32 value1;
            u32 end1;

            u32 start2;
            u32 value2;
            u32 end2;

            u16 control1;
            u16 control2;
        } memory_fill;

        struct {
            u32 in_buffer_address;
            u32 out_buffer_address;
            u32 in_buffer_size;
            u32 out_buffer_size;
            u32 flags;
        } display_transfer;

        struct {
            u32 in_buffer_address;
            u32 out_buffer_address;
            u32 size;
            u32 in_width_gap;
            u32 out_width_gap;
            u32 flags;
        } texture_copy;

        struct {
            struct {
                u32 address;
                u32 size;
            } regions[3];
        } cache_flush;

        u8 raw_data[0x1C];
    };
};
static_assert(sizeof(Command) == 0x20, "Command struct has incorrect size");

/// GSP shared memory GX command buffer header
struct CommandBuffer {
    union {
        u32 hex;

        // Current command index. This index is updated by GSP module after loading the command
        // data, right before the command is processed. When this index is updated by GSP module,
        // the total commands field is decreased by one as well.
        BitField<0, 8, u32> index;

        // Total commands to process, must not be value 0 when GSP module handles commands. This
        // must be <=15 when writing a command to shared memory. This is incremented by the
        // application when writing a command to shared memory, after increasing this value
        // TriggerCmdReqQueue is only used if this field is value 1.
        BitField<8, 8, u32> number_commands;
    };

    u32 unk[7];

    Command commands[0xF];
};
static_assert(sizeof(CommandBuffer) == 0x200, "CommandBuffer struct has incorrect size");

class GSP_GPU;

class SessionData : public Kernel::SessionRequestHandler::SessionDataBase {
public:
    SessionData();
    SessionData(GSP_GPU* gsp);
    ~SessionData();

    GSP_GPU* gsp;

    /// Event triggered when GSP interrupt has been signalled
    std::shared_ptr<Kernel::Event> interrupt_event;
    /// Thread index into interrupt relay queue
    u32 thread_id;
    /// Whether RegisterInterruptRelayQueue was called for this session
    bool registered = false;
};

class GSP_GPU final : public ServiceFramework<GSP_GPU, SessionData> {
public:
    explicit GSP_GPU(Core::System& system);
    ~GSP_GPU() = default;

    void ClientDisconnected(std::shared_ptr<Kernel::ServerSession> server_session) override;

    /**
     * Signals that the specified interrupt type has occurred to userland code
     * @param interrupt_id ID of interrupt that is being signalled
     */
    void SignalInterrupt(InterruptId interrupt_id);

    /**
     * Retrieves the framebuffer info stored in the GSP shared memory for the
     * specified screen index and thread id.
     * @param thread_id GSP thread id of the process that accesses the structure that we are
     * requesting.
     * @param screen_index Index of the screen we are requesting (Top = 0, Bottom = 1).
     * @returns FramebufferUpdate Information about the specified framebuffer.
     */
    FrameBufferUpdate* GetFrameBufferInfo(u32 thread_id, u32 screen_index);

private:
    /**
     * Signals that the specified interrupt type has occurred to userland code for the specified GSP
     * thread id.
     * @param interrupt_id ID of interrupt that is being signalled.
     * @param thread_id GSP thread that will receive the interrupt.
     */
    void SignalInterruptForThread(InterruptId interrupt_id, u32 thread_id);

    void WriteHWRegs(Kernel::HLERequestContext& ctx);
    void WriteHWRegsWithMask(Kernel::HLERequestContext& ctx);
    void ReadHWRegs(Kernel::HLERequestContext& ctx);
    void SetBufferSwap(Kernel::HLERequestContext& ctx);
    void FlushDataCache(Kernel::HLERequestContext& ctx);
    void InvalidateDataCache(Kernel::HLERequestContext& ctx);
    void SetLcdForceBlack(Kernel::HLERequestContext& ctx);
    void TriggerCmdReqQueue(Kernel::HLERequestContext& ctx);
    void SetAxiConfigQoSMode(Kernel::HLERequestContext& ctx);
    void RegisterInterruptRelayQueue(Kernel::HLERequestContext& ctx);
    void UnregisterInterruptRelayQueue(Kernel::HLERequestContext& ctx);
    void AcquireRight(Kernel::HLERequestContext& ctx);
    void ReleaseRight(Kernel::HLERequestContext& ctx);
    void ReleaseRight(const SessionData* session_data);
    void ImportDisplayCaptureInfo(Kernel::HLERequestContext& ctx);
    void StoreDataCache(Kernel::HLERequestContext& ctx);
    void SetLedForceOff(Kernel::HLERequestContext& ctx);

    /// Returns the session data for the specified registered thread id, or nullptr if not found.
    SessionData* FindRegisteredThreadData(u32 thread_id);

    u32 GetUnusedThreadId() const;

    std::unique_ptr<Kernel::SessionRequestHandler::SessionDataBase> MakeSessionData() override;

    Core::System& system;

    /// GSP shared memory
    std::shared_ptr<Kernel::SharedMemory> shared_memory;

    /// Thread id that currently has GPU rights or UINT32_MAX if none.
    u32 active_thread_id = UINT32_MAX;

    bool first_initialization = true;

    /// Maximum number of threads that can be registered at the same time in the GSP module.
    static constexpr u32 MaxGSPThreads = 4;

    /// Thread ids currently in use by the sessions connected to the GSPGPU service.
    std::array<bool, MaxGSPThreads> used_thread_ids = {false, false, false, false};

    friend class SessionData;
};

ResultCode SetBufferSwap(u32 screen_id, const FrameBufferInfo& info);

} // namespace Service::GSP
