/*
 * Copyright (c) Atmosphère-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stratosphere.hpp>
#include "fssrv_program_info.hpp"

namespace ams::fssrv::impl {

    namespace {

        constinit os::SdkMutex g_mutex;
        constinit bool g_initialized = false;

        constinit u64 g_initial_process_id_min = 0;
        constinit u64 g_initial_process_id_max = 0;

        constinit u64 g_current_process_id = 0;

        constinit std::aligned_storage<0x80>::type g_static_buffer_for_program_info_for_initial_process;

        template<typename T>
        class StaticAllocatorForProgramInfoForInitialProcess : public std::allocator<T> {
            public:
                StaticAllocatorForProgramInfoForInitialProcess() { /* ... */ }

                template<typename U>
                StaticAllocatorForProgramInfoForInitialProcess(const StaticAllocatorForProgramInfoForInitialProcess<U> &) { /* ... */ }

                template<typename U>
                struct rebind {
                    using other = StaticAllocatorForProgramInfoForInitialProcess<U>;
                };

                [[nodiscard]] T *allocate(::std::size_t n) {
                    AMS_ABORT_UNLESS(sizeof(T) * n <= sizeof(g_static_buffer_for_program_info_for_initial_process));
                    return reinterpret_cast<T *>(std::addressof(g_static_buffer_for_program_info_for_initial_process));
                }

                void deallocate(T *p, ::std::size_t n) {
                    AMS_UNUSED(p, n);
                }
        };

        constexpr const u32 FileAccessControlForInitialProgram[0x1C / sizeof(u32)]     = {0x00000001, 0x00000000, 0x80000000, 0x0000001C, 0x00000000, 0x0000001C, 0x00000000};
        constexpr const u32 FileAccessControlDescForInitialProgram[0x2C / sizeof(u32)] = {0x00000001, 0x00000000, 0x80000000, 0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF};

        ALWAYS_INLINE void InitializeInitialAndCurrentProcessId() {
            if (AMS_UNLIKELY(!g_initialized)) {
                std::scoped_lock lk(g_mutex);
                if (AMS_LIKELY(!g_initialized)) {
                    /* Get initial process id range. */
                    R_ABORT_UNLESS(svc::GetSystemInfo(std::addressof(g_initial_process_id_min), svc::SystemInfoType_InitialProcessIdRange, svc::InvalidHandle, svc::InitialProcessIdRangeInfo_Minimum));
                    R_ABORT_UNLESS(svc::GetSystemInfo(std::addressof(g_initial_process_id_max), svc::SystemInfoType_InitialProcessIdRange, svc::InvalidHandle, svc::InitialProcessIdRangeInfo_Maximum));

                    AMS_ABORT_UNLESS(0 < g_initial_process_id_min);
                    AMS_ABORT_UNLESS(g_initial_process_id_min <= g_initial_process_id_max);

                    /* Get current procss id. */
                    R_ABORT_UNLESS(svc::GetProcessId(std::addressof(g_current_process_id), svc::PseudoHandle::CurrentProcess));

                    /* Set initialized. */
                    g_initialized = true;
                }
            }
        }

    }

    std::shared_ptr<ProgramInfo> ProgramInfo::GetProgramInfoForInitialProcess() {
        static constinit os::SdkMutex s_mutex;
        static constinit bool s_initialized = false;
        static constinit std::shared_ptr<ProgramInfo> s_initial_program_info = nullptr;

        /* Ensure we've initialized the program info. */
        if (AMS_UNLIKELY(!s_initialized)) {
            std::scoped_lock lk(s_mutex);
            if (AMS_LIKELY(!s_initialized)) {
                class ProgramInfoHelper : public ProgramInfo {
                    public:
                        ProgramInfoHelper(const void *data, s64 data_size, const void *desc, s64 desc_size) : ProgramInfo(data, data_size, desc, desc_size) { /* ... */ }
                };

                s_initial_program_info = std::allocate_shared<ProgramInfoHelper>(StaticAllocatorForProgramInfoForInitialProcess<char>{}, FileAccessControlForInitialProgram, sizeof(FileAccessControlForInitialProgram), FileAccessControlDescForInitialProgram, sizeof(FileAccessControlDescForInitialProgram));
                s_initialized = true;
            }
        }

        return s_initial_program_info;
    }

    bool IsInitialProgram(u64 process_id) {
        /* Initialize/sanity check. */
        InitializeInitialAndCurrentProcessId();
        AMS_ABORT_UNLESS(g_initial_process_id_min > 0);

        /* Check process id in range. */
        return g_initial_process_id_min <= process_id && process_id <= g_initial_process_id_max;
    }

    bool IsCurrentProcess(u64 process_id) {
        /* Initialize. */
        InitializeInitialAndCurrentProcessId();

        return process_id == g_current_process_id;
    }

}
