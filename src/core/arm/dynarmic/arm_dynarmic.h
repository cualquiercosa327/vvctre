// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <dynarmic/A32/a32.h>
#include "common/common_types.h"
#include "core/arm/arm_interface.h"
#include "core/arm/skyeye_common/armstate.h"

namespace Memory {
struct PageTable;
class MemorySystem;
} // namespace Memory

namespace Core {
class System;
}

class DynarmicUserCallbacks;

class ARM_Dynarmic final : public ARM_Interface {
public:
    ARM_Dynarmic(Core::System* system, Memory::MemorySystem& memory, PrivilegeMode initial_mode);
    ~ARM_Dynarmic() override;

    void Run() override;
    void Step() override;

    void SetPC(u32 pc) override;
    u32 GetPC() const override;
    u32 GetReg(int index) const override;
    void SetReg(int index, u32 value) override;
    u32 GetVFPReg(int index) const override;
    void SetVFPReg(int index, u32 value) override;
    u32 GetVFPSystemReg(VFPSystemRegister reg) const override;
    void SetVFPSystemReg(VFPSystemRegister reg, u32 value) override;
    u32 GetCPSR() const override;
    void SetCPSR(u32 cpsr) override;
    u32 GetCP15Register(CP15Register reg) override;
    void SetCP15Register(CP15Register reg, u32 value) override;

    std::unique_ptr<ThreadContext> NewContext() const override;
    void SaveContext(const std::unique_ptr<ThreadContext>& arg) override;
    void LoadContext(const std::unique_ptr<ThreadContext>& arg) override;

    void PrepareReschedule() override;

    void ClearInstructionCache() override;
    void InvalidateCacheRange(u32 start_address, std::size_t length) override;
    void PageTableChanged() override;

private:
    friend class DynarmicUserCallbacks;
    Core::System& system;
    Memory::MemorySystem& memory;
    std::unique_ptr<DynarmicUserCallbacks> cb;
    std::unique_ptr<Dynarmic::A32::Jit> MakeJit();

    Dynarmic::A32::Jit* jit = nullptr;
    Memory::PageTable* current_page_table = nullptr;
    std::map<Memory::PageTable*, std::unique_ptr<Dynarmic::A32::Jit>> jits;
    std::shared_ptr<ARMul_State> interpreter_state;
};
