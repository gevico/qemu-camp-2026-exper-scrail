/*
 * QEMU Educational GPGPU Device
 *
 * Copyright (c) 2024-2025
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/units.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "qapi/error.h"
#include "hw/pci/pci.h"
#include "hw/pci/msi.h"
#include "hw/pci/msix.h"
#include "hw/core/qdev-properties.h"
#include "migration/vmstate.h"

#include "gpgpu.h"
#include "gpgpu_core.h"

static void gpgpu_reset(DeviceState *dev);

static uint64_t gpgpu_read_dev_info (void *opaque, hwaddr addr) {
    GPGPUState *s = GPGPU(OBJECT(opaque));
    uint32_t val = 0;
    switch (addr) {
        case GPGPU_REG_DEV_ID:
            val = GPGPU_DEV_ID_VALUE;
            break;
        case GPGPU_REG_DEV_VERSION:
            val = GPGPU_DEV_VERSION_VALUE;
            break;
        case GPGPU_REG_DEV_CAPS:
            val = (uint8_t)s->num_cus;
            val |= ((uint8_t)s->warps_per_cu) << 8;
            val |= ((uint8_t)s->warp_size) << 16;
            break;
        case GPGPU_REG_VRAM_SIZE_HI:
            val = extract64(s->vram_size, 32, 32);
            break;
        case GPGPU_REG_VRAM_SIZE_LO:
            val = extract64(s->vram_size, 0, 32);
            break;
    }
    return val;
}

static void gpgpu_write_dev_info (void *opaque, hwaddr addr, uint64_t val) {
    return ;
}

static uint64_t gpgpu_read_global_ctrl (void *opaque, hwaddr addr) {
    GPGPUState *s = GPGPU(OBJECT(opaque));
    uint32_t val = 0;
    switch (addr) {
        case GPGPU_REG_GLOBAL_CTRL:
            val = s->global_ctrl;
            break;
        case GPGPU_REG_GLOBAL_STATUS:
            val = s->global_status;
            break;
        case GPGPU_REG_ERROR_STATUS:
            val = s->error_status;
            break;
        default:
            g_assert_not_reached();
            break;
    }
    return val;
}

static void gpgpu_write_global_ctrl (void *opaque, hwaddr addr, uint64_t val) {
    GPGPUState *s = GPGPU(OBJECT(opaque));
    
    switch (addr) {
        case GPGPU_REG_GLOBAL_CTRL:
            if (val & GPGPU_CTRL_RESET) {
                gpgpu_reset(opaque);
                return;
            }
            s->global_ctrl = (val & GPGPU_CTRL_ENABLE);
            break;
        case GPGPU_REG_GLOBAL_STATUS:
            // No write permission
            break;
        case GPGPU_REG_ERROR_STATUS:
            // 每个字段都是写1清除, 直接p & ~q
            s->error_status &= ~(val & GPGPU_ERR_MASK);
            break;
        default:
            g_assert_not_reached();
            break;
    }
}

static uint64_t gpgpu_read_irq_ctrl (void *opaque, hwaddr addr) {
    GPGPUState *s = GPGPU(OBJECT(opaque));
    uint32_t val = 0;
    switch (addr) {
        case GPGPU_REG_IRQ_ENABLE:
            val = s->irq_enable;
            break;
        case GPGPU_REG_IRQ_STATUS:
            val = s->irq_status;
            break;
        default:
            break;
    }
    return val;
}

static void gpgpu_write_irq_ctrl (void *opaque, hwaddr addr, uint64_t val) {
    // IRQ_ENABLE寄存器字段含义未知,推测为最低位1代表使能
    // IRQ_STATUS BIT0 内核完成 BIT1 DMA完成 BIT3 错误
    GPGPUState *s = GPGPU(OBJECT(opaque));
    switch (addr) {
        case GPGPU_REG_IRQ_ENABLE:
            s->irq_enable = val;
            break;
        case GPGPU_REG_IRQ_ACK:
            // TODO: 中断处理
            s->irq_status &= ~(val & 0b111);
            break;
        default:
            break;
    }
}

static uint64_t gpgpu_read_kernel_dispatch (void *opaque, hwaddr addr) {
    GPGPUState *s = GPGPU(OBJECT(opaque));
    uint32_t val = 0;
    switch (addr) {
        case GPGPU_REG_KERNEL_ADDR_LO:
            val = extract64(s->kernel.kernel_addr, 0, 32);
            break;
        case GPGPU_REG_KERNEL_ADDR_HI:
            val = extract64(s->kernel.kernel_addr, 32, 32);
            break;

        case GPGPU_REG_KERNEL_ARGS_LO:
            val = extract64(s->kernel.kernel_args, 0, 32);
            break;
        case GPGPU_REG_KERNEL_ARGS_HI:
            val = extract64(s->kernel.kernel_args, 32, 32);
            break;

        case GPGPU_REG_GRID_DIM_X:
            val = s->kernel.grid_dim[0];
            break;
        case GPGPU_REG_GRID_DIM_Y:
            val = s->kernel.grid_dim[1];
            break;
        case GPGPU_REG_GRID_DIM_Z:
            val = s->kernel.grid_dim[2];
            break;

        case GPGPU_REG_BLOCK_DIM_X:
            val = s->kernel.block_dim[0];
            break;
        case GPGPU_REG_BLOCK_DIM_Y:
            val = s->kernel.block_dim[1];
            break;
        case GPGPU_REG_BLOCK_DIM_Z:
            val = s->kernel.block_dim[2];
            break;
        case GPGPU_REG_SHARED_MEM_SIZE:
            val = s->kernel.shared_mem_size;
            break;

        default:
            break;
    }
    return val;
}

static bool check_dim_no_zero(GPGPUState *s) {
    bool no_zero = true;
    for(int i = 0; i < 3; i++) {
        if (s->kernel.grid_dim[i] == 0 || s->kernel.block_dim[i] == 0) {
            no_zero = false;
            break;
        }
    }
    return no_zero;
}

static bool check_addr_limit(GPGPUState *s) {
    return s->kernel.kernel_addr < s->vram_size && s->kernel.kernel_args < s->vram_size;
}

static void gpgpu_write_kernel_dispatch (void *opaque, hwaddr addr, uint64_t val) {
    GPGPUState *s = GPGPU(OBJECT(opaque));
    switch (addr) {
        case GPGPU_REG_KERNEL_ADDR_LO:
            s->kernel.kernel_addr = deposit64(s->kernel.kernel_addr, 0, 32, val);
            break;
        case GPGPU_REG_KERNEL_ADDR_HI:
            s->kernel.kernel_addr = deposit64(s->kernel.kernel_addr, 32, 32, val);
            break;

        case GPGPU_REG_KERNEL_ARGS_LO:
            s->kernel.kernel_args = deposit64(s->kernel.kernel_args, 0, 32, val);
            break;
        case GPGPU_REG_KERNEL_ARGS_HI:
            s->kernel.kernel_args = deposit64(s->kernel.kernel_args, 32, 32, val);
            break;

        case GPGPU_REG_GRID_DIM_X:
            s->kernel.grid_dim[0] = val;
            break;
        case GPGPU_REG_GRID_DIM_Y:
            s->kernel.grid_dim[1] = val;
            break;
        case GPGPU_REG_GRID_DIM_Z:
            s->kernel.grid_dim[2] = val;
            break;

        case GPGPU_REG_BLOCK_DIM_X:
            s->kernel.block_dim[0] = val;
            break;
        case GPGPU_REG_BLOCK_DIM_Y:
            s->kernel.block_dim[1] = val;
            break;
        case GPGPU_REG_BLOCK_DIM_Z:
            s->kernel.block_dim[2] = val;
            break;
        case GPGPU_REG_SHARED_MEM_SIZE:
            s->kernel.shared_mem_size = val;
            break;
        case GPGPU_REG_DISPATCH:
            /* gurad 判断
               - 设备未使能
               - 正在忙
               - Grid Block维度为零
               - 内核地址越界
            */
            if (!(s->global_ctrl & GPGPU_CTRL_ENABLE) 
                || s->global_status == GPGPU_STATUS_BUSY
                || !check_dim_no_zero(s) 
                || !check_addr_limit(s)
                ) {
                s->error_status = GPGPU_ERR_INVALID_CMD;
                break;
            }
            s->global_status = GPGPU_STATUS_BUSY;
            // 触发内核执行
            gpgpu_core_exec_kernel(s);
            //TODO: 增加timer 并在回调处理状态设置
            s->global_status = GPGPU_STATUS_READY;
            s->irq_status |= GPGPU_IRQ_KERNEL_DONE;
        default:
            break;
    }
}

static uint64_t gpgpu_read_dma (void *opaque, hwaddr addr) {
    GPGPUState *s = GPGPU(OBJECT(opaque));
    uint32_t val = 0;
    switch (addr) {
        case GPGPU_REG_DMA_SRC_LO:
            val = extract64(s->dma.src_addr, 0, 32);
            break;
        case GPGPU_REG_DMA_SRC_HI:
            val = extract64(s->dma.src_addr, 32, 32);
            break;
        
        case GPGPU_REG_DMA_DST_LO:
            val = extract64(s->dma.dst_addr, 0, 32);
            break;
        case GPGPU_REG_DMA_DST_HI:
            val = extract64(s->dma.dst_addr, 32, 32);
            break;
        
        case GPGPU_REG_DMA_SIZE:
            val = s->dma.size;
            break;

        case GPGPU_REG_DMA_CTRL:
            val = s->dma.ctrl;
            break;
        
        case GPGPU_REG_DMA_STATUS:
            val = s->dma.status;
            break;
        default:
            break;
    }
    return val;
}

static void gpgpu_write_dma (void *opaque, hwaddr addr, uint64_t val) {
    GPGPUState *s = GPGPU(OBJECT(opaque));
    PCIDevice *pdev = PCI_DEVICE(OBJECT(opaque));
    switch (addr) {
        case GPGPU_REG_DMA_SRC_LO:
            s->dma.src_addr = deposit64(s->dma.src_addr, 0, 32, val);
            break;
        case GPGPU_REG_DMA_SRC_HI:
            s->dma.src_addr = deposit64(s->dma.src_addr, 32, 32, val);
            break;
        
        case GPGPU_REG_DMA_DST_LO:
            s->dma.dst_addr = deposit64(s->dma.dst_addr, 0, 32, val);
            break;
        case GPGPU_REG_DMA_DST_HI:
            s->dma.dst_addr = deposit64(s->dma.dst_addr, 32, 32, val);
            break;
        
        case GPGPU_REG_DMA_SIZE:
            s->dma.size = val;
            break;

        case GPGPU_REG_DMA_CTRL:
            s->dma.ctrl = val;
            int dir = val & GPGPU_DMA_DIR_MASK;

            // 处理START
            // 同步传输
            if(s->dma.ctrl & GPGPU_DMA_START) {
                if (dir == GPGPU_DMA_DIR_FROM_VRAM) {
                    pci_dma_write(pdev, s->dma.dst_addr, s->vram_ptr + s->dma.src_addr, s->dma.size);
                } else if (dir == GPGPU_DMA_DIR_TO_VRAM) {
                    pci_dma_read(pdev, s->dma.src_addr, s->vram_ptr + s->dma.dst_addr, s->dma.size);
                }
            }

            //1ms定时
            timer_mod(s->dma_timer, qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL) + 1);
            break;

        default:
            break;
    }
}

static uint64_t gpgpu_read_simt_context (void *opaque, hwaddr addr) {
    GPGPUState *s = GPGPU(OBJECT(opaque));
    uint32_t val = 0;
    switch (addr) {
        case GPGPU_REG_THREAD_ID_X:
            val = s->simt.thread_id[0];
            break;
        case GPGPU_REG_THREAD_ID_Y:
            val = s->simt.thread_id[1];
            break;
        case GPGPU_REG_THREAD_ID_Z:
            val = s->simt.thread_id[2];
            break;
        
        case GPGPU_REG_BLOCK_ID_X:
            val = s->simt.block_id[0];
            break;
        case GPGPU_REG_BLOCK_ID_Y:
            val = s->simt.block_id[1];
            break;
        case GPGPU_REG_BLOCK_ID_Z:
            val = s->simt.block_id[2];
            break;

        case GPGPU_REG_WARP_ID:
            val = s->simt.warp_id;
            break;
        
        case GPGPU_REG_LANE_ID:
            val = s->simt.lane_id;
            break;
        default:
            break;
    }
    return val;
}

static void gpgpu_write_simt_context (void *opaque, hwaddr addr, uint64_t val) {
    GPGPUState *s = GPGPU(OBJECT(opaque));
    switch (addr) {
        case GPGPU_REG_THREAD_ID_X:
            s->simt.thread_id[0] = val;
            break;
        case GPGPU_REG_THREAD_ID_Y:
            s->simt.thread_id[1] = val;
            break;
        case GPGPU_REG_THREAD_ID_Z:
            s->simt.thread_id[2] = val;
            break;
        
        case GPGPU_REG_BLOCK_ID_X:
            s->simt.block_id[0] = val; 
            break;
        case GPGPU_REG_BLOCK_ID_Y:
            s->simt.block_id[1] = val; 
            break;
        case GPGPU_REG_BLOCK_ID_Z:
            s->simt.block_id[2] = val; 
            break;

        case GPGPU_REG_WARP_ID:
            s->simt.warp_id = val;
            break;
        
        case GPGPU_REG_LANE_ID:
            s->simt.lane_id = val;
            break;
        default:
            break;
    }
}

static uint64_t gpgpu_read_sync (void *opaque, hwaddr addr) {
    GPGPUState *s = GPGPU(OBJECT(opaque));
    uint32_t val = 0;
    switch (addr) {
        case GPGPU_REG_THREAD_MASK:
            val = s->simt.thread_mask;
            break;
        default:
            break;
    }
    return val;
}

static void gpgpu_write_sync (void *opaque, hwaddr addr, uint64_t val) {
    GPGPUState *s = GPGPU(OBJECT(opaque));
    switch (addr) {
        case GPGPU_REG_BARRIER:
            s->simt.barrier_count++;
            break;
        case GPGPU_REG_THREAD_MASK:
            s->simt.thread_mask = val;
            break;
        default:
            break;
    }
}

static uint64_t gpgpu_read_msix_table (void *opaque, hwaddr addr) {
    // GPGPUState *s = GPGPU(OBJECT(opaque));
    // uint32_t val = 0;
    // switch (addr) {
    //     // case :

    //         break;
    //     default:
    //         break;
    // }
    // return val;
    return 0;
}

static void gpgpu_write_msix_table (void *opaque, hwaddr addr, uint64_t val) {
    (void)opaque;
    (void)addr;
    (void)val;
}

static uint64_t gpgpu_read_msix_pba (void *opaque, hwaddr addr) {
    // GPGPUState *s = GPGPU(OBJECT(opaque));
    // uint32_t val = 0;
    // switch (addr) {
    //     // case :

    //         break;
    //     default:
    //         break;
    // }
    // return val;
    return 0;
}

static void gpgpu_write_msix_pba (void *opaque, hwaddr addr, uint64_t val) {
    (void)opaque;
    (void)addr;
    (void)val;
}

/*
0x0000   - 0x00FF   dev_info
0x0100   - 0x01FF   global_ctrl
0x0200   - 0x02FF   irq_ctrl
0x0300   - 0x03FF   kernel_dispatch
0x0400   - 0x04FF   dma
0x1000   - 0x1FFF   simt_context
0x2000   - 0x2FFF   sync
0xF_E000 - 0xF_EFFF msix_table
0xF_F000 - 0xF_FFFF msix_pba

*/
/* TODO: Implement MMIO control register read */
static uint64_t gpgpu_ctrl_read(void *opaque, hwaddr addr, unsigned size)
{
    (void)size;

    if (addr < 0x0100) {
        /* 0x0000 - 0x00FF: dev_info */
        return gpgpu_read_dev_info(opaque, addr);
    } else if (addr < 0x0200) {
        /* 0x0100 - 0x01FF: global_ctrl */
        return gpgpu_read_global_ctrl(opaque, addr);
    } else if (addr < 0x0300) {
        /* 0x0200 - 0x02FF: irq_ctrl */
        return gpgpu_read_irq_ctrl(opaque, addr);
    } else if (addr < 0x0400) {
        /* 0x0300 - 0x03FF: kernel_dispatch */
        return gpgpu_read_kernel_dispatch(opaque, addr);
    } else if (addr < 0x0500) {
        /* 0x0400 - 0x04FF: dma */
        return gpgpu_read_dma(opaque, addr);
    } else if (addr < 0x1000) {
        /* 0x0500 - 0x0FFF: undefined */
        return 0;
    } else if (addr < 0x2000) {
        /* 0x1000 - 0x1FFF: simt_context */
        return gpgpu_read_simt_context(opaque, addr);
    } else if (addr < 0x3000) {
        /* 0x2000 - 0x2FFF: sync */
        return gpgpu_read_sync(opaque, addr);
    } else if (addr < 0xFE000) {
        /* 0x3000 - 0xFDFFF: undefined */
        return 0;
    } else if (addr < 0xFF000) {
        /* 0xFE000 - 0xFEFFF: msix_table */
        return gpgpu_read_msix_table(opaque, addr);
    } else if (addr < 0x100000) {
        /* 0xFF000 - 0xFFFFF: msix_pba */
        return gpgpu_read_msix_pba(opaque, addr);
    }
    /* >= 0x100000: undefined */
    return 0;
}

/* TODO: Implement MMIO control register write */
static void gpgpu_ctrl_write(void *opaque, hwaddr addr, uint64_t val,
                             unsigned size)
{
    (void)size;
    if (addr < 0x0100) {
        /* 0x0000 - 0x00FF: dev_info */
        gpgpu_write_dev_info(opaque, addr, val);
    } else if (addr < 0x0200) {
        /* 0x0100 - 0x01FF: global_ctrl */
        gpgpu_write_global_ctrl(opaque, addr, val);
    } else if (addr < 0x0300) {
        /* 0x0200 - 0x02FF: irq_ctrl */
        gpgpu_write_irq_ctrl(opaque, addr, val);
    } else if (addr < 0x0400) {
        /* 0x0300 - 0x03FF: kernel_dispatch */
        gpgpu_write_kernel_dispatch(opaque, addr, val);
    } else if (addr < 0x0500) {
        /* 0x0400 - 0x04FF: dma */
        gpgpu_write_dma(opaque, addr, val);
    } else if (addr < 0x1000) {
        /* 0x0500 - 0x0FFF: undefined */
        return;
    } else if (addr < 0x2000) {
        /* 0x1000 - 0x1FFF: simt_context */
        gpgpu_write_simt_context(opaque, addr, val);
    } else if (addr < 0x3000) {
        /* 0x2000 - 0x2FFF: sync */
        gpgpu_write_sync(opaque, addr, val);
    } else if (addr < 0xFE000) {
        /* 0x3000 - 0xFDFFF: undefined */
        return;
    } else if (addr < 0xFF000) {
        /* 0xFE000 - 0xFEFFF: msix_table */
        gpgpu_write_msix_table(opaque, addr, val);
    } else if (addr < 0x100000) {
        /* 0xFF000 - 0xFFFFF: msix_pba */
        gpgpu_write_msix_pba(opaque, addr, val);
    }
    /* >= 0x100000: undefined */
}

static const MemoryRegionOps gpgpu_ctrl_ops = {
    .read = gpgpu_ctrl_read,
    .write = gpgpu_ctrl_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

/* TODO: Implement VRAM read */
static uint64_t gpgpu_vram_read(void *opaque, hwaddr addr, unsigned size)
{
    GPGPUState *s = GPGPU(OBJECT(opaque));
    uint64_t val = 0;
    memcpy(&val, s->vram_ptr + addr, size);
    return val;
}

/* TODO: Implement VRAM write */
static void gpgpu_vram_write(void *opaque, hwaddr addr, uint64_t val,
                             unsigned size)
{
    GPGPUState *s = GPGPU(OBJECT(opaque));
    memcpy(s->vram_ptr + addr, &val, size);
}

static const MemoryRegionOps gpgpu_vram_ops = {
    .read = gpgpu_vram_read,
    .write = gpgpu_vram_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
};

static uint64_t gpgpu_doorbell_read(void *opaque, hwaddr addr, unsigned size)
{
    (void)opaque;
    (void)addr;
    (void)size;
    return 0;
}

static void gpgpu_doorbell_write(void *opaque, hwaddr addr, uint64_t val,
                                 unsigned size)
{
    (void)opaque;
    (void)addr;
    (void)val;
    (void)size;
}

static const MemoryRegionOps gpgpu_doorbell_ops = {
    .read = gpgpu_doorbell_read,
    .write = gpgpu_doorbell_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

/* TODO: Implement DMA completion handler */
static void gpgpu_dma_complete(void *opaque)
{
    GPGPUState *s = GPGPU(opaque);
    
    s->dma.status = GPGPU_DMA_COMPLETE;
    s->irq_status |= GPGPU_IRQ_DMA_DONE;
    
    if (s->dma.ctrl & GPGPU_DMA_IRQ_ENABLE) {
        msix_notify(PCI_DEVICE(s), GPGPU_MSIX_VEC_DMA);
    }
}

/* TODO: Implement kernel completion handler */
static void gpgpu_kernel_complete(void *opaque)
{
    (void)opaque;
}

static void gpgpu_realize(PCIDevice *pdev, Error **errp)
{
    GPGPUState *s = GPGPU(pdev);
    uint8_t *pci_conf = pdev->config;

    pci_config_set_interrupt_pin(pci_conf, 1);

    s->vram_ptr = g_malloc0(s->vram_size);
    if (!s->vram_ptr) {
        error_setg(errp, "GPGPU: failed to allocate VRAM");
        return;
    }

    /* BAR 0: control registers */
    memory_region_init_io(&s->ctrl_mmio, OBJECT(s), &gpgpu_ctrl_ops, s,
                          "gpgpu-ctrl", GPGPU_CTRL_BAR_SIZE);
    pci_register_bar(pdev, 0,
                     PCI_BASE_ADDRESS_SPACE_MEMORY |
                     PCI_BASE_ADDRESS_MEM_TYPE_64,
                     &s->ctrl_mmio);

    /* BAR 2: VRAM */
    memory_region_init_io(&s->vram, OBJECT(s), &gpgpu_vram_ops, s,
                          "gpgpu-vram", s->vram_size);
    pci_register_bar(pdev, 2,
                     PCI_BASE_ADDRESS_SPACE_MEMORY |
                     PCI_BASE_ADDRESS_MEM_TYPE_64 |
                     PCI_BASE_ADDRESS_MEM_PREFETCH,
                     &s->vram);

    /* BAR 4: doorbell registers */
    memory_region_init_io(&s->doorbell_mmio, OBJECT(s), &gpgpu_doorbell_ops, s,
                          "gpgpu-doorbell", GPGPU_DOORBELL_BAR_SIZE);
    pci_register_bar(pdev, 4,
                     PCI_BASE_ADDRESS_SPACE_MEMORY,
                     &s->doorbell_mmio);

    if (msix_init(pdev, GPGPU_MSIX_VECTORS,
                  &s->ctrl_mmio, 0, 0xFE000,
                  &s->ctrl_mmio, 0, 0xFF000,
                  0, errp)) {
        g_free(s->vram_ptr);
        return;
    }

    msi_init(pdev, 0, 1, true, false, errp);

    s->dma_timer = timer_new_ms(QEMU_CLOCK_VIRTUAL, gpgpu_dma_complete, s);
    s->kernel_timer = timer_new_ms(QEMU_CLOCK_VIRTUAL,
                                   gpgpu_kernel_complete, s);

    s->global_status = GPGPU_STATUS_READY;
}

static void gpgpu_exit(PCIDevice *pdev)
{
    GPGPUState *s = GPGPU(pdev);

    timer_free(s->dma_timer);
    timer_free(s->kernel_timer);
    g_free(s->vram_ptr);
    msix_uninit(pdev, &s->ctrl_mmio, &s->ctrl_mmio);
    msi_uninit(pdev);
}

//TODO: 给出的reset实现与手册表述不一致，手册描述 ** VRAM不清零 **
static void gpgpu_reset(DeviceState *dev)
{
    GPGPUState *s = GPGPU(dev);

    s->global_ctrl = 0;
    s->global_status = GPGPU_STATUS_READY;
    s->error_status = 0;
    s->irq_enable = 0;
    s->irq_status = 0;
    memset(&s->kernel, 0, sizeof(s->kernel));
    memset(&s->dma, 0, sizeof(s->dma));
    memset(&s->simt, 0, sizeof(s->simt));
    timer_del(s->dma_timer);
    timer_del(s->kernel_timer);
    if (s->vram_ptr) {
        memset(s->vram_ptr, 0, s->vram_size);
    }
}

static const Property gpgpu_properties[] = {
    DEFINE_PROP_UINT32("num_cus", GPGPUState, num_cus,
                       GPGPU_DEFAULT_NUM_CUS),
    DEFINE_PROP_UINT32("warps_per_cu", GPGPUState, warps_per_cu,
                       GPGPU_DEFAULT_WARPS_PER_CU),
    DEFINE_PROP_UINT32("warp_size", GPGPUState, warp_size,
                       GPGPU_DEFAULT_WARP_SIZE),
    DEFINE_PROP_UINT64("vram_size", GPGPUState, vram_size,
                       GPGPU_DEFAULT_VRAM_SIZE),
};

static const VMStateDescription vmstate_gpgpu = {
    .name = "gpgpu",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_PCI_DEVICE(parent_obj, GPGPUState),
        VMSTATE_UINT32(global_ctrl, GPGPUState),
        VMSTATE_UINT32(global_status, GPGPUState),
        VMSTATE_UINT32(error_status, GPGPUState),
        VMSTATE_UINT32(irq_enable, GPGPUState),
        VMSTATE_UINT32(irq_status, GPGPUState),
        VMSTATE_END_OF_LIST()
    }
};

static void gpgpu_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->realize = gpgpu_realize;
    pc->exit = gpgpu_exit;
    pc->vendor_id = GPGPU_VENDOR_ID;
    pc->device_id = GPGPU_DEVICE_ID;
    pc->revision = GPGPU_REVISION;
    pc->class_id = GPGPU_CLASS_CODE;

    device_class_set_legacy_reset(dc, gpgpu_reset);
    dc->desc = "Educational GPGPU Device";
    dc->vmsd = &vmstate_gpgpu;
    device_class_set_props(dc, gpgpu_properties);
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo gpgpu_type_info = {
    .name          = TYPE_GPGPU,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(GPGPUState),
    .class_init    = gpgpu_class_init,
    .interfaces    = (InterfaceInfo[]) {
        { INTERFACE_PCIE_DEVICE },
        { }
    },
};

static void gpgpu_register_types(void)
{
    type_register_static(&gpgpu_type_info);
}

type_init(gpgpu_register_types)
