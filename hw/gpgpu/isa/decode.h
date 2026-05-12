#pragma once
#include "common.h"
#include "isa-def.h"
#include "../gpgpu_core.h"
#include "../gpgpu.h"

typedef struct {
    union {
        reg_t *preg;
        word_t imm;
        sword_t simm;
    };

} Operand;

typedef struct Decode {
    void (*EHelper)(struct Decode *);
    Operand dest, src1, src2;

    ISADecodeInfo isa;
    // 指向当前执行的Lane，以及后端内存空间vram_ptr, SIMT上下文(GPGPUState)
    GPGPULane *lane;
    GPGPUState *state;
} Decode;

#define id_src1 (&s->src1)
#define id_src2 (&s->src2)
#define id_dest (&s->dest)

#define INSTR_LIST(f) INSTR_NULLARY(f) /*INSTR_UNARY(f)*/ INSTR_BINARY(f) INSTR_TERNARY(f) INSTR_TERNARY_CSR(f)

#define INSTR_CNT(name) + 1
#define TOTAL_INSTR (0 MAP(INSTR_LIST, INSTR_CNT))

enum { BASE_COUNTER = __COUNTER__ };
#define def_EXEC_ID(name) \
    enum { concat(EXEC_ID_, name) = __COUNTER__ - BASE_COUNTER -1 };
#define def_all_EXEC_ID() MAP(INSTR_LIST, def_EXEC_ID)

#define def_THelper(name) \
    static inline int concat(table_, name) (Decode *s)
#define def_THelper_arity(name, arity) \
  def_THelper(name) { return concat(EXEC_ID_, name); }
#define def_THelper_nullary(name) def_THelper_arity(name, 0)
#define def_THelper_unary(name)   def_THelper_arity(name, 1)
#define def_THelper_binary(name)  def_THelper_arity(name, 2)
#define def_THelper_ternary(name) def_THelper_arity(name, 3)

#define def_all_THelper() \
  MAP(INSTR_NULLARY, def_THelper_nullary) \
  MAP(INSTR_UNARY,   def_THelper_unary  ) \
  MAP(INSTR_BINARY,  def_THelper_binary ) \
  MAP(INSTR_TERNARY, def_THelper_ternary) \
  MAP(INSTR_TERNARY_CSR, def_THelper_ternary)

#define def_DHelper(name) void concat(decode_, name) (Decode *s)

static inline def_DHelper(empty) {}

#define CASE_ENTRY(idx, id, tab) case idx: id(s); return tab(s);
#define IDTAB(idx, id, tab) CASE_ENTRY(idx, concat(decode_, id), concat(table_, tab))
#define TAB(idx, tab) IDTAB(idx, empty, tab)
#define EMPTY(idx) TAB(idx, inv)

#define def_EHelper(name) static inline void concat(exec_, name) (Decode *s)


static inline uint32_t vram_load(Decode *s, uint32_t addr, int size) {
    uint32_t val = 0;
    if (addr >= GPGPU_CORE_CTRL_BASE && addr < GPGPU_CORE_CTRL_BASE + 0x1000) {
        switch (addr) {
            case GPGPU_CORE_CTRL_THREAD_ID_X: val = s->state->simt.thread_id[0]; break;
            case GPGPU_CORE_CTRL_THREAD_ID_Y: val = s->state->simt.thread_id[1]; break;
            case GPGPU_CORE_CTRL_THREAD_ID_Z: val = s->state->simt.thread_id[2]; break;
            case GPGPU_CORE_CTRL_BLOCK_ID_X : val = s->state->simt.block_id[0]; break;
            case GPGPU_CORE_CTRL_BLOCK_ID_Y : val = s->state->simt.block_id[1]; break;
            case GPGPU_CORE_CTRL_BLOCK_ID_Z : val = s->state->simt.block_id[2]; break;
            case GPGPU_CORE_CTRL_BLOCK_DIM_X: val = s->state->kernel.block_dim[0]; break;
            case GPGPU_CORE_CTRL_BLOCK_DIM_Y: val = s->state->kernel.block_dim[1]; break;
            case GPGPU_CORE_CTRL_BLOCK_DIM_Z: val = s->state->kernel.block_dim[2]; break;
            case GPGPU_CORE_CTRL_GRID_DIM_X : val = s->state->kernel.grid_dim[0]; break;
            case GPGPU_CORE_CTRL_GRID_DIM_Y : val = s->state->kernel.grid_dim[1]; break;
            case GPGPU_CORE_CTRL_GRID_DIM_Z : val = s->state->kernel.grid_dim[2]; break;
            default: val = 0;
        }
    } else {
        memcpy(&val, s->state->vram_ptr + addr, size);
    }
    return val;
}

static inline void vram_store(Decode *s, uint32_t addr, uint32_t val, int size) {
    if (addr < s->state->vram_size) {
        printf("WARP lane=%d addr=0x%x val=%u\n", (int)(s->state->simt.lane_id), addr, val);
        memcpy(s->state->vram_ptr + addr, &val, size);
    }
    // CTRL只读
}


int isa_fetch_decode(Decode *s);