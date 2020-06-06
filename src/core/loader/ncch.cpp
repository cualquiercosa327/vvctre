// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <codecvt>
#include <cstring>
#include <locale>
#include <memory>
#include <vector>
#include <fmt/format.h>
#include "common/logging/log.h"
#include "common/string_util.h"
#include "common/swap.h"
#include "core/core.h"
#include "core/file_sys/ncch_container.h"
#include "core/file_sys/title_metadata.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/fs/fs_user.h"
#include "core/loader/ncch.h"
#include "core/loader/smdh.h"
#include "core/memory.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Loader namespace

namespace Loader {

static const u64 UPDATE_MASK = 0x0000000e00000000;

FileType AppLoader_NCCH::IdentifyType(FileUtil::IOFile& file) {
    u32 magic;
    file.Seek(0x100, SEEK_SET);
    if (file.ReadArray<u32>(&magic, 1) != 1) {
        return FileType::Unknown;
    }

    if (magic == MakeMagic('N', 'C', 'S', 'D')) {
        return FileType::CCI;
    }

    if (magic == MakeMagic('N', 'C', 'C', 'H')) {
        return FileType::CXI;
    }

    return FileType::Unknown;
}

std::pair<std::optional<u32>, ResultStatus> AppLoader_NCCH::LoadKernelSystemMode() {
    if (!is_loaded) {
        ResultStatus res = base_ncch.Load();
        if (res != ResultStatus::Success) {
            return std::make_pair(std::optional<u32>{}, res);
        }
    }

    // Set the system mode as the one from the exheader.
    return std::make_pair(overlay_ncch->exheader_header.arm11_system_local_caps.system_mode.Value(),
                          ResultStatus::Success);
}

ResultStatus AppLoader_NCCH::LoadExec(std::shared_ptr<Kernel::Process>& process) {
    using Kernel::CodeSet;

    if (!is_loaded)
        return ResultStatus::ErrorNotLoaded;

    std::vector<u8> code;
    u64_le program_id;
    if (ResultStatus::Success == ReadCode(code) &&
        ResultStatus::Success == ReadProgramId(program_id)) {
        std::string process_name = Common::StringFromFixedZeroTerminatedBuffer(
            (const char*)overlay_ncch->exheader_header.codeset_info.name, 8);

        std::shared_ptr<CodeSet> codeset =
            Core::System::GetInstance().Kernel().CreateCodeSet(process_name, program_id);

        codeset->CodeSegment().offset = 0;
        codeset->CodeSegment().addr = overlay_ncch->exheader_header.codeset_info.text.address;
        codeset->CodeSegment().size =
            overlay_ncch->exheader_header.codeset_info.text.num_max_pages * Memory::PAGE_SIZE;

        codeset->RODataSegment().offset =
            codeset->CodeSegment().offset + codeset->CodeSegment().size;
        codeset->RODataSegment().addr = overlay_ncch->exheader_header.codeset_info.ro.address;
        codeset->RODataSegment().size =
            overlay_ncch->exheader_header.codeset_info.ro.num_max_pages * Memory::PAGE_SIZE;

        // TODO(yuriks): Not sure if the bss size is added to the page-aligned .data size or just
        //               to the regular size. Playing it safe for now.
        u32 bss_page_size = (overlay_ncch->exheader_header.codeset_info.bss_size + 0xFFF) & ~0xFFF;
        code.resize(code.size() + bss_page_size, 0);

        codeset->DataSegment().offset =
            codeset->RODataSegment().offset + codeset->RODataSegment().size;
        codeset->DataSegment().addr = overlay_ncch->exheader_header.codeset_info.data.address;
        codeset->DataSegment().size =
            overlay_ncch->exheader_header.codeset_info.data.num_max_pages * Memory::PAGE_SIZE +
            bss_page_size;

        // Apply patches now that the entire codeset (including .bss) has been allocated
        const ResultStatus patch_result = overlay_ncch->ApplyCodePatch(code);
        if (patch_result != ResultStatus::Success && patch_result != ResultStatus::ErrorNotUsed)
            return patch_result;

        codeset->entrypoint = codeset->CodeSegment().addr;
        codeset->memory = std::move(code);

        process = Core::System::GetInstance().Kernel().CreateProcess(std::move(codeset));

        // Attach a resource limit to the process based on the resource limit category
        process->resource_limit =
            Core::System::GetInstance().Kernel().ResourceLimit().GetForCategory(
                static_cast<Kernel::ResourceLimitCategory>(
                    overlay_ncch->exheader_header.arm11_system_local_caps.resource_limit_category));

        // Set the default CPU core for this process
        process->ideal_processor =
            overlay_ncch->exheader_header.arm11_system_local_caps.ideal_processor;

        // Copy data while converting endianness
        using KernelCaps = std::array<u32, ExHeader_ARM11_KernelCaps::NUM_DESCRIPTORS>;
        KernelCaps kernel_caps;
        std::copy_n(overlay_ncch->exheader_header.arm11_kernel_caps.descriptors, kernel_caps.size(),
                    begin(kernel_caps));
        process->ParseKernelCaps(kernel_caps.data(), kernel_caps.size());

        s32 priority = overlay_ncch->exheader_header.arm11_system_local_caps.priority;
        u32 stack_size = overlay_ncch->exheader_header.codeset_info.stack_size;

        // On real HW this is done with FS:Reg, but we can be lazy
        auto fs_user =
            Core::System::GetInstance().ServiceManager().GetService<Service::FS::FS_USER>(
                "fs:USER");
        fs_user->Register(process->process_id, process->codeset->program_id, filepath);

        process->Run(priority, stack_size);
        return ResultStatus::Success;
    }
    return ResultStatus::Error;
}

void AppLoader_NCCH::ParseRegionLockoutInfo() {
    std::vector<u8> smdh_buffer;
    if (ReadIcon(smdh_buffer) == ResultStatus::Success && smdh_buffer.size() >= sizeof(SMDH)) {
        SMDH smdh;
        memcpy(&smdh, smdh_buffer.data(), sizeof(SMDH));
        u32 region_lockout = smdh.region_lockout;
        constexpr u32 REGION_COUNT = 7;
        std::vector<u32> regions;
        for (u32 region = 0; region < REGION_COUNT; ++region) {
            if (region_lockout & 1) {
                regions.push_back(region);
            }
            region_lockout >>= 1;
        }
        auto cfg = Service::CFG::GetModule(Core::System::GetInstance());
        ASSERT_MSG(cfg, "CFG Module missing!");
        cfg->SetPreferredRegionCodes(regions);
    }
}

ResultStatus AppLoader_NCCH::Load(std::shared_ptr<Kernel::Process>& process) {
    u64_le ncch_program_id;

    if (is_loaded)
        return ResultStatus::ErrorAlreadyLoaded;

    ResultStatus result = base_ncch.Load();
    if (result != ResultStatus::Success) {
        return result;
    }

    ReadProgramId(ncch_program_id);
    std::string program_id{fmt::format("{:016X}", ncch_program_id)};

    LOG_INFO(Loader, "Program ID: {}", program_id);

    update_ncch.OpenFile(Service::AM::GetTitleContentPath(Service::FS::MediaType::SDMC,
                                                          ncch_program_id | UPDATE_MASK));
    result = update_ncch.Load();
    if (result == ResultStatus::Success) {
        overlay_ncch = &update_ncch;
    }

    is_loaded = true; // Set state to loaded

    result = LoadExec(process); // Load the executable into memory for booting
    if (ResultStatus::Success != result) {
        return result;
    }

    Core::System::GetInstance().ArchiveManager().RegisterSelfNCCH(*this);

    ParseRegionLockoutInfo();

    return ResultStatus::Success;
}

ResultStatus AppLoader_NCCH::IsExecutable(bool& out_executable) {
    Loader::ResultStatus result = overlay_ncch->Load();
    if (result != Loader::ResultStatus::Success)
        return result;

    out_executable = overlay_ncch->ncch_header.is_executable != 0;
    return ResultStatus::Success;
}

ResultStatus AppLoader_NCCH::ReadCode(std::vector<u8>& buffer) {
    return overlay_ncch->LoadSectionExeFS(".code", buffer);
}

ResultStatus AppLoader_NCCH::ReadIcon(std::vector<u8>& buffer) {
    return overlay_ncch->LoadSectionExeFS("icon", buffer);
}

ResultStatus AppLoader_NCCH::ReadBanner(std::vector<u8>& buffer) {
    return overlay_ncch->LoadSectionExeFS("banner", buffer);
}

ResultStatus AppLoader_NCCH::ReadLogo(std::vector<u8>& buffer) {
    return overlay_ncch->LoadSectionExeFS("logo", buffer);
}

ResultStatus AppLoader_NCCH::ReadProgramId(u64& out_program_id) {
    ResultStatus result = base_ncch.ReadProgramId(out_program_id);
    if (result != ResultStatus::Success)
        return result;

    return ResultStatus::Success;
}

ResultStatus AppLoader_NCCH::ReadExtdataId(u64& out_extdata_id) {
    ResultStatus result = base_ncch.ReadExtdataId(out_extdata_id);
    if (result != ResultStatus::Success)
        return result;

    return ResultStatus::Success;
}

ResultStatus AppLoader_NCCH::ReadRomFS(std::shared_ptr<FileSys::RomFSReader>& romfs_file) {
    return base_ncch.ReadRomFS(romfs_file);
}

ResultStatus AppLoader_NCCH::DumpRomFS(const std::string& target_path) {
    return base_ncch.DumpRomFS(target_path);
}

ResultStatus AppLoader_NCCH::ReadUpdateRomFS(std::shared_ptr<FileSys::RomFSReader>& romfs_file) {
    ResultStatus result = update_ncch.ReadRomFS(romfs_file);

    if (result != ResultStatus::Success)
        return base_ncch.ReadRomFS(romfs_file);

    return ResultStatus::Success;
}

ResultStatus AppLoader_NCCH::DumpUpdateRomFS(const std::string& target_path) {
    u64 program_id;
    ReadProgramId(program_id);
    update_ncch.OpenFile(
        Service::AM::GetTitleContentPath(Service::FS::MediaType::SDMC, program_id | UPDATE_MASK));
    return update_ncch.DumpRomFS(target_path);
}

} // namespace Loader
