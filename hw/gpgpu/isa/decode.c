#include "decode.h"
#include "isa-all-instr.h"

def_all_THelper();

#define def_DopHelper(name) \
    void concat(decode_op_, name) (Decode *s, Operand *op, uint32_t val, bool flag)

static inline def_DopHelper(i) {
    op->imm = val;
}

static inline def_DopHelper(r) {
    bool load_val = flag;
    static word_t zero_null = 0;
    op->preg = (!load_val && val == 0) ? &zero_null: &(s->lane->gpr[val]);
}

static inline def_DHelper(I) {
    decode_op_r(s, id_src1, s->isa.instr.i.rs1, true);
    decode_op_i(s, id_src2, s->isa.instr.i.simm11_0, false);
    decode_op_r(s, id_dest, s->isa.instr.i.rd, false);
}

static inline def_DHelper(R) {
    decode_op_r(s, id_src1, s->isa.instr.r.rs1, true);
    decode_op_r(s, id_src2, s->isa.instr.r.rs2, true);
    decode_op_r(s, id_dest, s->isa.instr.r.rd, false);
}

static inline def_DHelper(S) {
    decode_op_r(s, id_src1, s->isa.instr.s.rs1, true);
    sword_t simm = (s->isa.instr.s.simm11_5 << 5) | s->isa.instr.s.imm4_0;
    decode_op_i(s, id_src2, simm, false);
    decode_op_r(s, id_dest, s->isa.instr.s.rs2, true);
}

static inline def_DHelper(B) {
    sword_t offset = (s->isa.instr.b.simm12 << 12) | (s->isa.instr.b.imm11 << 11) |
        (s->isa.instr.b.imm10_5 << 5) | (s->isa.instr.b.imm4_1 << 1);
    decode_op_i(s, id_dest, s->lane->pc + offset, true);
    decode_op_r(s, id_src1, s->isa.instr.b.rs1, true);
    decode_op_r(s, id_src2, s->isa.instr.b.rs2, true);
}

static inline def_DHelper(U) {
    decode_op_i(s, id_src1, s->isa.instr.u.imm31_12 << 12, true);
    decode_op_r(s, id_dest, s->isa.instr.u.rd, false);
}

static inline def_DHelper(J) {
    sword_t offset = (s->isa.instr.j.simm20 << 20) | (s->isa.instr.j.imm19_12 << 12) |
        (s->isa.instr.j.imm11 << 11) | (s->isa.instr.j.imm10_1 << 1);
    decode_op_i(s, id_src1, s->lane->pc + offset, true);
    decode_op_r(s, id_dest, s->isa.instr.j.rd, false);
    id_src2->imm = s->lane->pc + 4;
}

static inline def_DHelper(auipc) {
    decode_U(s);
    id_src1->imm += s->lane->pc;
}

static inline def_DHelper(csr) {
    decode_op_r(s, id_src1, s->isa.instr.i.rs1, true);
    decode_op_i(s, id_src2, s->isa.instr.csr.csr, true);
    decode_op_r(s, id_dest, s->isa.instr.i.rd, false);
}

def_THelper(load) {
    switch (s->isa.instr.i.funct3) {
        TAB(0, lb)  TAB(1, lh)  TAB(2, lw)
        TAB(4, lbu) TAB(5, lhu)
    }
    return EXEC_ID_inv;
}

def_THelper(store) {
    switch (s->isa.instr.i.funct3) {
        TAB(0, sb)  TAB(1, sh)  TAB(2, sw)
    }
    return EXEC_ID_inv;
}

def_THelper(op_imm) {
    if (s->isa.instr.r.funct7 == 32) {
        switch (s->isa.instr.r.funct3) { TAB(5, srai) }    
    }
    switch (s->isa.instr.i.funct3) {
        // NEMU对addi进行进一步判断，区分：rs1=0即加载立即数 li imm、imm=0即移动寄存器值mv rd rs1、和完整语义的addi
        TAB(0, addi)  TAB(1, slli)  TAB(2, slti) TAB(3, sltiu)
        TAB(4, xori)  TAB(5, srli)  TAB(6, ori)  TAB(7, andi)
    }
    return EXEC_ID_inv;
}

def_THelper(op) {
    // 0 add sub
    // 5 srl sra
    if (s->isa.instr.r.funct7 == 32) {
        switch (s->isa.instr.r.funct3) {
            TAB(0, sub) TAB(5, sra) 
        }
    }
    switch (s->isa.instr.r.funct3) {
        TAB(0, add) TAB(1, sll) TAB(2, slt) TAB(3, sltu) 
        TAB(4, xor) TAB(5, srl) TAB(6, or)  TAB(7, and) 
    }

    return  EXEC_ID_inv;
}

def_THelper(branch) {
    switch (s->isa.instr.i.funct3) {
        TAB(0, beq)   TAB(1, bne)
        TAB(4, blt)   TAB(5, bge)   TAB(6, bltu)   TAB(7, bgeu)
    } 
    return EXEC_ID_inv;
}

def_THelper(system) {
    switch (s->isa.instr.i.funct3) {
        TAB(0, ebreak)
        TAB(1, csrrw)  TAB(2, csrrs)    TAB(3, csrrc) 
        TAB(5, csrrwi) TAB(6, csrrsi)   TAB(7, csrrci)
    }
    return EXEC_ID_inv;
}

def_THelper(jalr_dispatch) {
  if (s->isa.instr.i.rd == 0 && id_src2->imm == 0) {
    if (s->isa.instr.i.rs1 == 1) return table_p_ret(s);
  }
  return table_jalr(s);
}

// RVF
static inline def_DopHelper(fr) {
    op->preg = &(s->lane->fpr[val]);
}

static inline def_DHelper(FR) {
    decode_op_fr(s, id_src1, s->isa.instr.fp.rs1, false);
    decode_op_fr(s, id_src2, s->isa.instr.fp.rs2, false);
    decode_op_fr(s, id_dest, s->isa.instr.fp.rd, false);
}

static inline def_DHelper(R4) {
    decode_op_fr(s, id_src1, s->isa.instr.fp.rs1, false);
    decode_op_fr(s, id_src2, s->isa.instr.fp.rs2, false);
    decode_op_fr(s, id_dest, s->isa.instr.fp.rd,  false);
    // rs3 is decoded at exec.h
}

static inline def_DHelper(FLOAD) {
    decode_op_r(s, id_src1, s->isa.instr.i.rs1, false);
    decode_op_i(s, id_src2, s->isa.instr.i.simm11_0, false);
    decode_op_fr(s, id_dest, s->isa.instr.i.rd, false);
}

static inline def_DHelper(FSTORE) {
    decode_op_r(s, id_src1, s->isa.instr.s.rs1, true);
    sword_t simm = (s->isa.instr.s.simm11_5 << 5) | s->isa.instr.s.imm4_0;
    decode_op_i(s, id_src2, simm, false);
    decode_op_fr(s, id_dest, s->isa.instr.s.rs2, false);
}

static inline def_DHelper(fr2r){
  decode_op_fr(s, id_src1, s->isa.instr.fp.rs1, true);
  decode_op_fr(s, id_src2, s->isa.instr.fp.rs2, true);
  decode_op_r(s, id_dest, s->isa.instr.fp.rd, false);
}

static inline def_DHelper(r2fr){
  decode_op_r(s, id_src1, s->isa.instr.fp.rs1, true);
  decode_op_r(s, id_src2, s->isa.instr.fp.rs2, true);
  decode_op_fr(s, id_dest, s->isa.instr.fp.rd, false);
}

def_THelper(fload) {
    switch (s->isa.instr.i.funct3) {
        TAB(0b010, flw)
    }
    return EXEC_ID_inv;
}

def_THelper(fstore) {
    switch (s->isa.instr.s.funct3) {
        TAB(0b010, fsw)
    }
    return EXEC_ID_inv;
}

def_THelper(fsgnj_dispatch) {
    switch (s->isa.instr.fp.rm) {
        TAB(0, fsgnj_s)
        TAB(1, fsgnjn_s)
        TAB(2, fsgnjx_s)
    }
    return EXEC_ID_inv;
}

def_THelper(fminmax_dispatch) {
    switch (s->isa.instr.fp.rm) {
        TAB(0, fmin_s)
        TAB(1, fmax_s)
    }
    return EXEC_ID_inv;
}

def_THelper(fcvt_w_s_dispatch) {
    switch (s->isa.instr.fp.rs2) {
        TAB(0, fcvt_w_s)
        TAB(1, fcvt_wu_s)
    }
    return EXEC_ID_inv;
}

def_THelper(fmvxw_class_dispatch) {
    switch (s->isa.instr.fp.rm) {
        TAB(0, fmv_x_w)
        TAB(1, fclass_s) 
    }
    return EXEC_ID_inv;
}

def_THelper(f_cmp_dispatch) {
    switch (s->isa.instr.fp.rm) {
        TAB(0, fle_s)
        TAB(1, flt_s)
        TAB(2, feq_s)
    }
    return EXEC_ID_inv;
}

def_THelper(fcvt_s_w_dispatch) {
    switch (s->isa.instr.fp.rs2) {
        TAB(0, fcvt_s_w)
        TAB(1, fcvt_s_wu)
    }
    return EXEC_ID_inv;
}

def_THelper(bf16_dispatch) {
    switch (s->isa.instr.fp.rs2) {
        TAB(1, fcvt_bf16_s)
        TAB(0, fcvt_s_bf16)
    }
    return EXEC_ID_inv;
}

def_THelper(fp8_dispatch) {
    switch (s->isa.instr.fp.rs2) {
        TAB(1, fcvt_e4m3_s)
        TAB(0, fcvt_s_e4m3)
        TAB(3, fcvt_e5m2_s)
        TAB(2, fcvt_s_e5m2)
    }
    return EXEC_ID_inv;
}

def_THelper(fp4_dispatch) {
    switch (s->isa.instr.fp.rs2) {
        TAB(1, fcvt_e2m1_s)
        TAB(0, fcvt_s_e2m1)
    }
    return EXEC_ID_inv;
}

def_THelper(op_fp) {
    switch (s->isa.instr.r.funct7) {
        IDTAB(0x22, FR, bf16_dispatch)
        IDTAB(0x24, FR, fp8_dispatch)
        IDTAB(0x26, FR, fp4_dispatch)
    }
    switch (s->isa.instr.fp.funct5) {
        IDTAB(000, FR, fadd_s) IDTAB(001, FR, fsub_s) IDTAB(002, FR, fmul_s) IDTAB(003, FR, fdiv_s)
        IDTAB(013, FR, fsqrt_s)
        IDTAB(004, FR, fsgnj_dispatch)
        IDTAB(005, FR, fminmax_dispatch)
        IDTAB(030, fr2r, fcvt_w_s_dispatch)
        IDTAB(034, fr2r, fmvxw_class_dispatch)
        IDTAB(024, fr2r, f_cmp_dispatch)
        IDTAB(032, r2fr, fcvt_s_w_dispatch)
        IDTAB(036, r2fr, fmv_w_x)

    }
    return EXEC_ID_inv;
}

def_THelper(main) {
    switch (s->isa.instr.i.opcode6_2) {
        IDTAB(000, I, load)
        IDTAB(010, S, store)
        IDTAB(004, I, op_imm) IDTAB(005, auipc, auipc)
        IDTAB(014, R, op)   IDTAB(015, U, lui)
        // jal jalr NEMU有进一步分发
        IDTAB(030, B, branch)   IDTAB(031, I, jalr_dispatch)
        IDTAB(033, J, jal)
        IDTAB(034, csr, system)

        // RVF
        IDTAB(001, FLOAD, fload)
        IDTAB(011, FSTORE, fstore)
        IDTAB(020, R4, fmadd_s)
        IDTAB(021, R4, fmsub_s)
        IDTAB(022, R4, fnmsub_s)
        IDTAB(023, R4, fnmadd_s)
        TAB(024, op_fp)
    }
    return table_inv(s);
}


#include "isa-exec.h"
#include "isa-exec-float.h"

#define FILL_EXEC_TABLE(name) [concat(EXEC_ID_, name)] = concat(exec_, name),

static const void *g_exec_table[TOTAL_INSTR] = {
    MAP(INSTR_LIST, FILL_EXEC_TABLE)};

int isa_fetch_decode(Decode *s) {
    int idx = table_main(s);
    s->EHelper = g_exec_table[idx];
    return idx;
}

