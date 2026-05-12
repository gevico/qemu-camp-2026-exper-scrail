/*
 * QEMU GPGPU - RISC-V SIMT Core Implementation
 *
 * Copyright (c) 2024-2025
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "gpgpu.h"
#include "gpgpu_core.h"
#include "isa/decode.h"

/* TODO: Implement warp initialization */
void gpgpu_core_init_warp(GPGPUWarp *warp, uint32_t pc,
                          uint32_t thread_id_base, const uint32_t block_id[3],
                          uint32_t num_threads,
                          uint32_t warp_id, uint32_t block_id_linear)
{
    memset(warp, 0, sizeof(*warp));
    warp->warp_id = warp_id;
    warp->thread_id_base = thread_id_base;
    warp->active_mask = (num_threads == 32) ? 0xFFFFFFFF : ((1 << num_threads) - 1); 
    for (int i = 0; i < 3; i++) {
        warp->block_id[i] = block_id[i];
    }
    for (int thread_id = 0; thread_id < num_threads; thread_id++) {
        GPGPULane *lane = &warp->lanes[thread_id];
        lane->pc = pc;
        lane->active = true;
        lane->mhartid = MHARTID_ENCODE(block_id_linear, warp_id, thread_id);

        lane->fcsr = 0;     //frm默认值 = RNE fflag = 0
        memset(&lane->fp_status, 0, sizeof(float_status));
        set_float_rounding_mode(float_round_nearest_even, &lane->fp_status);
    }
}

/* TODO: Implement warp execution (RV32I + RV32F interpreter) */
int gpgpu_core_exec_warp(GPGPUState *s, GPGPUWarp *warp, uint32_t max_cycles)
{
    Decode d;
    while (max_cycles--) {
        if (warp->active_mask == 0) {
            return 0;
        }
        // TODO: pc同步问题
        uint32_t warp_pc = warp->lanes[0].pc; 
        uint32_t inst;
        memcpy(&inst, &s->vram_ptr[warp_pc], 4);
        d.isa.instr.val = inst;
        d.state = s;
        

        for (int i = 0; i < GPGPU_WARP_SIZE; i++) {
            if (((1 << i) & warp->active_mask) == 0) {
                continue;
            }
            isa_fetch_decode(&d);
            
            // TODO: simt同步数值检查，考虑切换vram读取源，避免缓存不一致
            s->simt.warp_id = warp->warp_id;
            s->simt.lane_id = i;
            uint32_t tid = warp->thread_id_base + i;
            s->simt.thread_id[0] = tid % s->kernel.block_dim[0];
            s->simt.thread_id[1] = (tid / s->kernel.block_dim[0]) % s->kernel.block_dim[1];
            s->simt.thread_id[2] = tid / (s->kernel.block_dim[0] * s->kernel.block_dim[1]);

            d.lane = &warp->lanes[i];
            d.lane->pc += 4;
            d.EHelper(&d);
        }

        for (int i = 0; i < GPGPU_WARP_SIZE; i++) {
            if (!warp->lanes[i].active) {
                warp->active_mask = (warp->active_mask & ~(1 << i));
            }
        }
    }
    return -1;
}

/* TODO: Implement kernel dispatch and execution */
int gpgpu_core_exec_kernel(GPGPUState *s)
{
    /*
        1. init each warp
        2. exec each warp
        3.return 
    */
    int ret;
    GPGPUSIMTContext * simt = &s->simt;
    GPGPUWarp * warp = g_malloc0(sizeof(GPGPUWarp));
    uint32_t threads_per_block = s->kernel.block_dim[0] * s->kernel.block_dim[1] * s->kernel.block_dim[2];
    uint32_t warps_per_block = (threads_per_block + s->warp_size - 1) / s->warp_size;
    FOR_EACH_3D(b_idx, bx, by, bz, s->kernel.grid_dim) {
        simt->block_id[0] = bx;
        simt->block_id[1] = by;
        simt->block_id[2] = bz;

        for (uint32_t w_idx = 0; w_idx < warps_per_block; w_idx++) {
            uint32_t base = w_idx * s->warp_size;
            uint32_t num_threads = threads_per_block - base;
            if (num_threads > s->warp_size) {
                num_threads = s->warp_size;
            }
            gpgpu_core_init_warp(warp, s->kernel.kernel_addr, base, s->kernel.block_dim, num_threads, w_idx, b_idx);
            ret = gpgpu_core_exec_warp(s, warp, GPGPU_CORE_MAX_CYCLES);
            if (ret) {
                return -1;
            }
        }
    }
    g_free(warp);
    return 0;
}
