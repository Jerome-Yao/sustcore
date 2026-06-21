/**
 * @file bootstrap.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief task bootstrap / preload / spec 装载
 * @version alpha-1.0.0
 * @date 2026-06-21
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/permission.h>
#include <exe/elfloader.h>
#include <sustcore/bootstrap.h>
#include <logger.h>
#include <mem/alloc.h>
#include <task/task.h>

#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <elf.h>

namespace task {
    namespace {
        [[nodiscard]]
        bool valid_cap_explain(CapIdx cap_idx, PayloadType cap_type,
                               const char *cap_desc) noexcept {
            return cap_idx != cap::null && cap_idx != cap::error &&
                   cap_type != PayloadType::NONE && cap_desc != nullptr &&
                   cap_desc[0] != '\0';
        }

        [[nodiscard]]
        bool valid_vaddr_explain(VirAddr vaddr,
                                 const char *vaddr_desc) noexcept {
            return vaddr.nonnull() && vaddr_desc != nullptr &&
                   vaddr_desc[0] != '\0';
        }

        [[nodiscard]]
        TaskSpec::BootstrapRecordData make_bootstrap_record(
            uint32_t type, const void *payload, size_t payload_size) {
            TaskSpec::BootstrapRecordData record{
                .type  = type,
                .bytes = std::vector<char>(sizeof(bsheader) + payload_size),
            };
            auto *header = reinterpret_cast<bsheader *>(record.bytes.data());
            header->size = record.bytes.size();
            header->type = type;
            if (payload_size != 0) {
                memcpy(record.bytes.data() + sizeof(bsheader), payload,
                       payload_size);
            }
            return record;
        }

    }  // namespace

    Result<CapIdx> TaskManager::preload(CapIdx image_cap, TaskSpec &spec,
                                        LoadPrm &prm) {
        auto create_res = cap::CHolderManager::inst().create_holder();
        if (!create_res.has_value()) {
            loggers::SUSTCORE::ERROR("创建CHolder失败! 错误码: %s",
                                     to_cstring(create_res.error()));
            unexpect_return(ErrCode::CREATION_FAILED);
        }
        auto holder = create_res.value();

        auto preload_res = preload_into(image_cap, holder, spec, prm);
        if (!preload_res.has_value()) {
            auto rm_res =
                cap::CHolderManager::inst().remove_holder(holder->id());
            assert(rm_res.has_value());
            propagate_return(preload_res);
        }
        return preload_res;
    }

    Result<CapIdx> TaskManager::preload_into(CapIdx image_cap,
                                             cap::CHolder *holder,
                                             TaskSpec &spec, LoadPrm &prm) {
        if (holder == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        auto gfp_res = GFP::get_free_page(1);
        if (!gfp_res.has_value()) {
            loggers::SUSTCORE::ERROR("无法为程序页表分配物理页");
            unexpect_return(ErrCode::CREATION_FAILED);
        }
        auto tmm = util::owner(new TaskMemoryManager(gfp_res.value()));

        auto cap_res = holder->lookup(image_cap);
        propagate(cap_res);
        if (cap_res.value()->payload()->type_id() != PayloadType::VFILE) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        if (!cap_res.value()->imply(perm::vfile::EXEC)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }

        spec.holder = holder;
        spec.tmm    = tmm;
        prm.src_path       = "<cap>";
        prm.image_file_cap = image_cap;
        return image_cap;
    }

    Result<void> TaskManager::validate_bootstrap_record(
        const TaskSpec::BootstrapRecordData &record) noexcept {
        if (record.bytes.size() < sizeof(bsheader)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto *header = reinterpret_cast<const bsheader *>(record.bytes.data());
        if (header->size != record.bytes.size() || header->type == 0) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        BootstrapRecordView view{};
        if (!bootstrap_make_view(header, view)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        if (header->type == boot::TYPE_CAPEXP) {
            BootstrapCapExplainView cap_explain{};
            if (!bootstrap_parse_cap_explain(view, cap_explain) ||
                !valid_cap_explain(cap_explain.cap_idx, cap_explain.cap_type,
                                   cap_explain.cap_desc))
            {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
        } else if (header->type == boot::TYPE_VADDREXP) {
            BootstrapVaddrExplainView vaddr_explain{};
            if (!bootstrap_parse_vaddr_explain(view, vaddr_explain) ||
                !valid_vaddr_explain(vaddr_explain.vaddr,
                                     vaddr_explain.vaddr_desc))
            {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
        }

        void_return();
    }

    Result<void> TaskManager::append_bootstrap_cap_explain_record(
        TaskSpec &spec, CapIdx cap_idx, PayloadType cap_type, b64 cap_perm,
        const char *cap_desc) {
        if (!valid_cap_explain(cap_idx, cap_type, cap_desc)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        BootstrapCapExplainPayloadHead head{
            .cap_idx  = cap_idx,
            .cap_type = cap_type,
            .cap_perm = cap_perm,
        };
        std::vector<char> payload(sizeof(head) + strlen(cap_desc) + 1);
        memcpy(payload.data(), &head, sizeof(head));
        memcpy(payload.data() + sizeof(head), cap_desc, strlen(cap_desc) + 1);
        spec.bsargv.push_back(make_bootstrap_record(boot::TYPE_CAPEXP,
                                                    payload.data(),
                                                    payload.size()));
        void_return();
    }

    Result<void> TaskManager::append_bootstrap_vaddr_explain_record(
        TaskSpec &spec, VirAddr vaddr, const char *vaddr_desc) {
        if (!valid_vaddr_explain(vaddr, vaddr_desc)) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        BootstrapVaddrExplainPayloadHead head{
            .vaddr = vaddr.arith(),
        };
        std::vector<char> payload(sizeof(head) + strlen(vaddr_desc) + 1);
        memcpy(payload.data(), &head, sizeof(head));
        memcpy(payload.data() + sizeof(head), vaddr_desc,
               strlen(vaddr_desc) + 1);
        spec.bsargv.push_back(
            make_bootstrap_record(boot::TYPE_VADDREXP, payload.data(),
                                  payload.size()));
        void_return();
    }

    Result<void> TaskManager::load_task_spec(
        CapIdx image_cap, cap::CHolder *holder,
        const std::vector<std::string> &argv,
        const std::vector<std::string> &envp,
        const std::vector<TaskSpec::BootstrapRecordData> &bsargv,
        TaskSpec &spec, LoadPrm &prm) {
        auto preload_res = holder == nullptr
                               ? preload(image_cap, spec, prm)
                               : preload_into(image_cap, holder, spec, prm);
        if (!preload_res.has_value()) {
            loggers::SUSTCORE::ERROR("预加载程序资源失败! 错误码: %s",
                                     to_cstring(preload_res.error()));
            propagate_return(preload_res);
        }

        auto load_res = loader::elf::load(spec, prm);
        if (!load_res.has_value()) {
            loggers::SUSTCORE::ERROR("加载ELF程序失败! 错误码: %s",
                                     to_cstring(load_res.error()));
            unexpect_return(ErrCode::CREATION_FAILED);
        }

        spec.argv = argv;
        spec.envp = envp;
        spec.bsargv.clear();
        spec.auxv.clear();
        for (const auto &record : bsargv) {
            auto validate_res = validate_bootstrap_record(record);
            propagate(validate_res);
            spec.bsargv.push_back(record);
        }
        void_return();
    }

    Result<void> TaskManager::load_linux_task_spec(
        CapIdx image_cap, cap::CHolder *holder, CapIdx subsystem_image_cap,
        const std::vector<std::string> &argv,
        const std::vector<std::string> &envp,
        const std::vector<TaskSpec::BootstrapRecordData> &bsargv,
        TaskSpec &spec, LoadPrm &prm) {
        if (holder == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        auto preload_res = preload_into(image_cap, holder, spec, prm);
        if (!preload_res.has_value()) {
            loggers::SUSTCORE::ERROR("预加载POSIX程序失败! 错误码: %s",
                                     to_cstring(preload_res.error()));
            propagate_return(preload_res);
        }

        auto load_linux_res = loader::elf::load_segments(spec, prm, true);
        if (!load_linux_res.has_value()) {
            loggers::SUSTCORE::ERROR("加载POSIX程序失败! 错误码: %s",
                                     to_cstring(load_linux_res.error()));
            unexpect_return(ErrCode::CREATION_FAILED);
        }

        VirAddr linux_entry = spec.entrypoint;
        LoadPrm subsystem_prm{
            .image_file_cap = subsystem_image_cap,
            .src_path       = "<cap>",
        };
        spec.entrypoint           = VirAddr(static_cast<addr_t>(0));
        spec.linuxproc_entrypoint = VirAddr(static_cast<addr_t>(0));
        auto load_subsystem_res =
            loader::elf::load_segments(spec, subsystem_prm, false);
        if (!load_subsystem_res.has_value()) {
            loggers::SUSTCORE::ERROR("加载POSIX子系统失败! 错误码: %s",
                                     to_cstring(load_subsystem_res.error()));
            unexpect_return(ErrCode::CREATION_FAILED);
        }

        spec.argv = argv;
        spec.envp = envp;
        spec.bsargv.clear();
        for (const auto &record : bsargv) {
            auto validate_res = validate_bootstrap_record(record);
            propagate(validate_res);
            spec.bsargv.push_back(record);
        }
        spec.auxv = {AT_NULL, 0};
        spec.linuxproc_entrypoint = linux_entry;
        void_return();
    }
}  // namespace task
