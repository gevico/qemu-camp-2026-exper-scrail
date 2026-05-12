#pragma once
#include "decode.h"

//RV LP
#define LP_BINARY(f) \
    f(fcvt_s_bf16) f(fcvt_bf16_s) \
    f(fcvt_s_e4m3) f(fcvt_e4m3_s) f(fcvt_s_e5m2) f(fcvt_e5m2_s) \
    f(fcvt_s_e2m1) f(fcvt_e2m1_s)

// RVF
#define FLOAT_BINARY(f) \
    f(flw) f(fsw) \
    f(fsqrt_s) \
    f(fcvt_w_s) f(fcvt_wu_s) f(fcvt_s_w) f(fcvt_s_wu) \
    f(fmv_x_w) f(fmv_w_x) f(fclass_s)

#define FLOAT_TERNARY(f) \
    f(fadd_s) f(fsub_s) f(fmul_s) f(fdiv_s) \
    f(fsgnj_s) f(fsgnjn_s) f(fsgnjx_s) \
    f(fmin_s) f(fmax_s) \
    f(feq_s) f(flt_s) f(fle_s) \
    f(fmadd_s) f(fmsub_s) f(fnmsub_s) f(fnmadd_s)

// RVI + RVF
#define INSTR_NULLARY(f) \
    f(inv) f(ebreak) f(p_ret)

#define INSTR_UNARY(f)

#define INSTR_BINARY(f) \
    f(lui) f(auipc) f(jal) \
    f(lw) f(sw) f(lh) f(lb) f(lhu) f(lbu) f(sh) f(sb) \
    FLOAT_BINARY(f) \
    LP_BINARY(f)

#define INSTR_TERNARY(f) \
    f(add) f(sub) f(sll) f(slt) f(sltu) f(xor) f(srl) f(sra) f(or) f(and) \
    f(addi) f(slti) f(sltiu) f(xori) f(ori) f(andi) f(slli) f(srli) f(srai) \
    f(beq) f(bne) f(blt) f(bge) f(bltu) f(bgeu) f(jalr) \
    FLOAT_TERNARY(f)

#define INSTR_TERNARY_CSR(f) \
    f(csrrw) f(csrrs) f(csrrc) f(csrrwi) f(csrrsi) f(csrrci)

def_all_EXEC_ID();