// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <utility>
#include "audio_core/dsp_interface.h"
#include "audio_core/hle/hle.h"
#include "audio_core/lle/lle.h"
#include "common/logging/log.h"
#include "common/texture.h"
#include "core/arm/arm_interface.h"
#ifdef ARCHITECTURE_x86_64
#include "core/arm/dynarmic/arm_dynarmic.h"
#endif
#include "core/arm/dyncom/arm_dyncom.h"
#include "core/cheats/cheats.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/custom_tex_cache.h"
#include "core/gdbstub/gdbstub.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"
#include "core/hw/hw.h"
#include "core/loader/loader.h"
#include "core/movie.h"
#include "core/settings.h"
#include "video_core/video_core.h"

namespace Core {

/*static*/ System System::s_instance;

System::ResultStatus System::RunLoop(bool tight_loop) {
    status = ResultStatus::Success;
    if (!cpu_core) {
        return ResultStatus::ErrorNotInitialized;
    }

    if (GDBStub::IsServerEnabled()) {
        Kernel::Thread* thread = kernel->GetThreadManager().GetCurrentThread();
        if (thread && cpu_core) {
            cpu_core->SaveContext(thread->context);
        }
        GDBStub::HandlePacket();

        // If the loop is halted and we want to step, use a tiny (1) number of instructions to
        // execute. Otherwise, get out of the loop function.
        if (GDBStub::GetCpuHaltFlag()) {
            if (GDBStub::GetCpuStepFlag()) {
                tight_loop = false;
            } else {
                return ResultStatus::Success;
            }
        }
    }

    // If we don't have a currently active thread then don't execute instructions,
    // instead advance to the next event and try to yield to the next thread
    if (kernel->GetThreadManager().GetCurrentThread() == nullptr) {
        LOG_TRACE(Core_ARM11, "Idling");
        timing->Idle();
        timing->Advance();
        PrepareReschedule();
    } else {
        timing->Advance();
        if (tight_loop) {
            cpu_core->Run();
        } else {
            cpu_core->Step();
        }
    }

    if (GDBStub::IsServerEnabled()) {
        GDBStub::SetCpuStepFlag(false);
    }

    Reschedule();

    if (reset_requested.exchange(false)) {
        Reset();
    } else if (shutdown_requested.exchange(false)) {
        return ResultStatus::ShutdownRequested;
    }

    return status;
}

System::ResultStatus System::Load(Frontend::EmuWindow& emu_window, const std::string& filepath) {
    if (!FileUtil::Exists(filepath)) {
        return ResultStatus::ErrorFileNotFound;
    }

    app_loader = Loader::GetLoader(filepath);
    if (!app_loader) {
        LOG_CRITICAL(Core, "Unsupported file format");
        return ResultStatus::ErrorLoader_ErrorUnsupportedFormat;
    }
    std::pair<std::optional<u32>, Loader::ResultStatus> system_mode =
        app_loader->LoadKernelSystemMode();

    if (system_mode.second != Loader::ResultStatus::Success) {
        LOG_CRITICAL(Core, "Failed to determine system mode (Error {})!",
                     static_cast<int>(system_mode.second));

        switch (system_mode.second) {
        case Loader::ResultStatus::ErrorEncrypted:
            return ResultStatus::ErrorLoader_ErrorEncrypted;
        case Loader::ResultStatus::ErrorInvalidFormat:
            return ResultStatus::ErrorLoader_ErrorUnsupportedFormat;
        default:
            return ResultStatus::ErrorSystemMode;
        }
    }

    ASSERT(system_mode.first);

    ResultStatus init_result{Init(emu_window, *system_mode.first)};
    if (init_result != ResultStatus::Success) {
        LOG_CRITICAL(Core, "Failed to initialize system (Error {})!",
                     static_cast<u32>(init_result));
        System::Shutdown();
        return init_result;
    }

    std::shared_ptr<Kernel::Process> process;
    const Loader::ResultStatus load_result{app_loader->Load(process)};
    kernel->SetCurrentProcess(process);
    if (Loader::ResultStatus::Success != load_result) {
        LOG_CRITICAL(Core, "Failed to load ROM (Error {})!", static_cast<u32>(load_result));
        System::Shutdown();

        switch (load_result) {
        case Loader::ResultStatus::ErrorEncrypted:
            return ResultStatus::ErrorLoader_ErrorEncrypted;
        case Loader::ResultStatus::ErrorInvalidFormat:
            return ResultStatus::ErrorLoader_ErrorUnsupportedFormat;
        default:
            UNREACHABLE();
        }
    }
    cheat_engine = std::make_shared<Cheats::CheatEngine>(*this);
    perf_stats = std::make_unique<PerfStats>();
    custom_tex_cache = std::make_unique<Core::CustomTexCache>();
    if (Settings::values.custom_textures) {
        FileUtil::CreateFullPath(fmt::format("{}textures/{:016X}/",
                                             FileUtil::GetUserPath(FileUtil::UserPath::LoadDir),
                                             Kernel().GetCurrentProcess()->codeset->program_id));
        custom_tex_cache->FindCustomTextures();
    }
    if (Settings::values.preload_textures) {
        custom_tex_cache->PreloadTextures();
    }
    status = ResultStatus::Success;
    m_emu_window = &emu_window;
    m_filepath = filepath;

    // Reset counters and set time origin to current frame
    perf_stats->BeginSystemFrame();

    return status;
}

void System::PrepareReschedule() {
    cpu_core->PrepareReschedule();
    reschedule_pending = true;
}

void System::Reschedule() {
    if (!reschedule_pending) {
        return;
    }

    reschedule_pending = false;
    kernel->GetThreadManager().Reschedule();
}

System::ResultStatus System::Init(Frontend::EmuWindow& emu_window, u32 system_mode) {
    memory = std::make_unique<Memory::MemorySystem>();
    timing = std::make_unique<Timing>();

    kernel = std::make_unique<Kernel::KernelSystem>(
        *memory, *timing, [this] { PrepareReschedule(); }, system_mode);

    if (Settings::values.use_cpu_jit) {
#ifdef ARCHITECTURE_x86_64
        cpu_core = std::make_shared<ARM_Dynarmic>(this, *memory);
#else
        cpu_core = std::make_shared<ARM_DynCom>(this, *memory, USER32MODE);
        LOG_WARNING(Core, "CPU JIT requested, but Dynarmic not available");
#endif
    } else {
        cpu_core = std::make_shared<ARM_DynCom>(this, *memory, USER32MODE);
    }

    kernel->SetCPU(cpu_core);

    if (Settings::values.enable_dsp_lle) {
        dsp_core = std::make_shared<AudioCore::DspLle>(*memory,
                                                       Settings::values.enable_dsp_lle_multithread);
    } else {
        dsp_core = std::make_shared<AudioCore::DspHle>(*memory);
    }

    memory->SetDSP(*dsp_core);

    dsp_core->SetSink(Settings::values.audio_sink_id, Settings::values.audio_device_id);

    service_manager = std::make_shared<Service::SM::ServiceManager>(*this);
    archive_manager = std::make_unique<Service::FS::ArchiveManager>(*this);

    HW::Init(*memory);
    Service::Init(*this);
    GDBStub::DeferStart();

    VideoCore::Init(emu_window, *memory);

    return ResultStatus::Success;
}

RendererBase& System::Renderer() {
    return *VideoCore::g_renderer;
}

Service::SM::ServiceManager& System::ServiceManager() {
    return *service_manager;
}

const Service::SM::ServiceManager& System::ServiceManager() const {
    return *service_manager;
}

Service::FS::ArchiveManager& System::ArchiveManager() {
    return *archive_manager;
}

const Service::FS::ArchiveManager& System::ArchiveManager() const {
    return *archive_manager;
}

Kernel::KernelSystem& System::Kernel() {
    return *kernel;
}

const Kernel::KernelSystem& System::Kernel() const {
    return *kernel;
}

Timing& System::CoreTiming() {
    return *timing;
}

const Timing& System::CoreTiming() const {
    return *timing;
}

Memory::MemorySystem& System::Memory() {
    return *memory;
}

const Memory::MemorySystem& System::Memory() const {
    return *memory;
}

Cheats::CheatEngine& System::CheatEngine() {
    return *cheat_engine;
}

const Cheats::CheatEngine& System::CheatEngine() const {
    return *cheat_engine;
}

Core::CustomTexCache& System::CustomTexCache() {
    return *custom_tex_cache;
}

const Core::CustomTexCache& System::CustomTexCache() const {
    return *custom_tex_cache;
}

void System::RegisterMiiSelector(std::shared_ptr<Frontend::MiiSelector> mii_selector) {
    registered_mii_selector = std::move(mii_selector);
}

void System::RegisterSoftwareKeyboard(std::shared_ptr<Frontend::SoftwareKeyboard> swkbd) {
    registered_swkbd = std::move(swkbd);
}

void System::Shutdown() {
    GDBStub::Shutdown();
    VideoCore::Shutdown();
    perf_stats.reset();
    cheat_engine.reset();
    archive_manager.reset();
    service_manager.reset();
    dsp_core.reset();
    cpu_core.reset();
    kernel.reset();
    timing.reset();
    app_loader.reset();
}

void System::Reset() {
    Shutdown();
    Load(*m_emu_window, m_filepath);
}

} // namespace Core
