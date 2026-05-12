#pragma once
#include "decode.h"


/**********  COMPUTE  ***********/
def_EHelper(lui) {
    *s->dest.preg = s->src1.simm;
}

def_EHelper(add) {
    *s->dest.preg = *s->src1.preg + *s->src2.preg;
}

def_EHelper(sub) {
    *s->dest.preg = *s->src1.preg - *s->src2.preg;
}

def_EHelper(sll) {
    *s->dest.preg = *s->src1.preg << (*s->src2.preg & 0x1F);
}

def_EHelper(srl) {
    *s->dest.preg = *s->src1.preg >> (*s->src2.preg & 0x1F);
}

def_EHelper(sra) {
    *s->dest.preg = (uint32_t)((int32_t)*s->src1.preg >> (*s->src2.preg & 0x1F));
}

def_EHelper(slt) {
    *s->dest.preg = ((int32_t)*s->src1.preg < (int32_t)*s->src2.preg) ? 1 : 0;
}

def_EHelper(sltu) {
    *s->dest.preg = (*s->src1.preg < *s->src2.preg) ? 1 : 0;
}

def_EHelper(xor) {
    *s->dest.preg = *s->src1.preg ^ *s->src2.preg;
}

def_EHelper(or) {
    *s->dest.preg = *s->src1.preg | *s->src2.preg;
}

def_EHelper(and) {
    *s->dest.preg = *s->src1.preg & *s->src2.preg;
}

def_EHelper(addi) {
    *s->dest.preg = *s->src1.preg + s->src2.imm;
}

def_EHelper(slli) {
    *s->dest.preg = *s->src1.preg << (s->src2.imm & 0x1F);
}

def_EHelper(srli) {
    *s->dest.preg = *s->src1.preg >> (s->src2.imm & 0x1F);
}

def_EHelper(srai) {
    *s->dest.preg = (uint32_t)((int32_t)*s->src1.preg >> (s->src2.imm & 0x1F));
}

def_EHelper(slti) {
    *s->dest.preg = ((int32_t)*s->src1.preg < (int32_t)s->src2.imm) ? 1 : 0;
}

def_EHelper(sltiu) {
    *s->dest.preg = (*s->src1.preg < (uint32_t)s->src2.imm) ? 1 : 0;
}

def_EHelper(xori) {
    *s->dest.preg = *s->src1.preg ^ s->src2.imm;
}

def_EHelper(ori) {
    *s->dest.preg = *s->src1.preg | s->src2.imm;
}

def_EHelper(andi) {
    *s->dest.preg = *s->src1.preg & s->src2.imm;
}

def_EHelper(auipc) {
    *s->dest.preg = s->src1.imm;
}

/**********  LDST  ***********/
def_EHelper(lw) {
    uint32_t addr = *s->src1.preg + s->src2.imm;
    uint32_t val = vram_load(s, addr, 4);
    *s->dest.preg = val;
}

def_EHelper(lh) {
    uint32_t addr = *s->src1.preg + s->src2.imm;
    uint16_t val = vram_load(s, addr, 2);
    *s->dest.preg = (uint32_t)(int16_t)val;
}

def_EHelper(lb) {
    uint32_t addr = *s->src1.preg + s->src2.imm;
    uint8_t val = vram_load(s, addr, 1);
    *s->dest.preg = (uint32_t)(int8_t)val;
}

def_EHelper(lhu) {
    uint32_t addr = *s->src1.preg + s->src2.imm;
    uint16_t val = vram_load(s, addr, 2);
    *s->dest.preg = val;
}

def_EHelper(lbu) {
    uint32_t addr = *s->src1.preg + s->src2.imm;
    uint8_t val = vram_load(s, addr, 1);
    *s->dest.preg = val;
}

def_EHelper(sw) {
    uint32_t val = *s->dest.preg;
    uint32_t addr = *s->src1.preg + s->src2.imm;
    vram_store(s, addr, val, 4);
}

def_EHelper(sh) {
    uint16_t val = *s->dest.preg;
    uint32_t addr = *s->src1.preg + s->src2.imm;
    vram_store(s, addr, val, 2);
}

def_EHelper(sb) {
    uint8_t val = *s->dest.preg;
    uint32_t addr = *s->src1.preg + s->src2.imm;
    vram_store(s, addr, val, 1);
}

/**********  CONTROL  ***********/
def_EHelper(jal) {
    *s->dest.preg = s->src2.imm;    // rd = pc+4 (译码时捕获)
    s->lane->pc = s->src1.imm;       // pc = PC + offset
}

def_EHelper(jalr) {
    *s->dest.preg = s->lane->pc;    // rd = pc+4（主循环已前进）
    s->lane->pc = (*s->src1.preg + s->src2.imm) & ~1;
}

def_EHelper(p_ret) {
    s->lane->active = false;
}

def_EHelper(beq)  { if (*s->src1.preg == *s->src2.preg) s->lane->pc = s->dest.imm; }
def_EHelper(bne)  { if (*s->src1.preg != *s->src2.preg) s->lane->pc = s->dest.imm; }
def_EHelper(blt)  { if ((int32_t)*s->src1.preg <  (int32_t)*s->src2.preg) s->lane->pc = s->dest.imm; }
def_EHelper(bge)  { if ((int32_t)*s->src1.preg >= (int32_t)*s->src2.preg) s->lane->pc = s->dest.imm; }
def_EHelper(bltu) { if (*s->src1.preg <  *s->src2.preg) s->lane->pc = s->dest.imm; }
def_EHelper(bgeu) { if (*s->src1.preg >= *s->src2.preg) s->lane->pc = s->dest.imm; }

/**********  SYSTEM  ***********/
def_EHelper(ebreak) {
    s->lane->active = false;
}

static inline uint32_t csr_read(Decode *s, uint32_t addr) {
    switch (addr) {
        case CSR_MHARTID: return s->lane->mhartid;
        case CSR_FFLAGS:  return s->lane->fcsr & 0x1F;
        case CSR_FRM:     return (s->lane->fcsr >> 5) & 0x7;
        case CSR_FCSR:    return s->lane->fcsr & 0xFF;
        default:          return 0;
    }
}

// TODO: 需要review
static inline void csr_write(Decode *s, uint32_t addr, uint32_t val) {
    switch (addr) {
        case CSR_MHARTID: /* RO */ break;
        case CSR_FFLAGS:
            // s->lane->fcsr = (s->lane->fcsr & ~0x1F) | (val & 0x1F);
            FFLAGS_UPDATE(s->lane->fcsr, val);
            break;
        case CSR_FRM:
            // s->lane->fcsr = (s->lane->fcsr & ~0xE0) | ((val & 0x7) << 5);
            FRM_UPDATE(s->lane->fcsr, val);
            set_float_rounding_mode(val & 0x7, &s->lane->fp_status);
            break;
        case CSR_FCSR:
            s->lane->fcsr = val & 0xFF;
            set_float_rounding_mode((val >> 5) & 0x7, &s->lane->fp_status);
            set_float_exception_flags(val & 0x1F, &s->lane->fp_status);
            break;
    }
}

/* csrrw:  t = CSR; CSR = x[rs1];         x[rd] = t */
def_EHelper(csrrw) {
    *s->dest.preg = csr_read(s, s->src2.imm);
    if (*s->src1.preg != 0) {
        csr_write(s, s->src2.imm, *s->src1.preg);
    }
}

/* csrrs: t = CSR; CSR = t | x[rs1];      x[rd] = t */
def_EHelper(csrrs) {
    uint32_t t = csr_read(s, s->src2.imm);
    *s->dest.preg = t;
    if (*s->src1.preg != 0) {
        csr_write(s, s->src2.imm, t | *s->src1.preg);
    }
}

/* csrrc: t = CSR; CSR = t & ~x[rs1];     x[rd] = t */
def_EHelper(csrrc) {
    uint32_t t = csr_read(s, s->src2.imm);
    *s->dest.preg = t;
    if (*s->src1.preg != 0) {
        csr_write(s, s->src2.imm, t & ~(*s->src1.preg));
    }
}

/* csrrwi: t = CSR; CSR = zimm;           x[rd] = t */
def_EHelper(csrrwi) {
    *s->dest.preg = csr_read(s, s->src2.imm);
    uint32_t zimm = s->isa.instr.i.rs1;
    if (zimm != 0) {
        csr_write(s, s->src2.imm, zimm);
    }
}

/* csrrsi: t = CSR; CSR = t | zimm;       x[rd] = t */
def_EHelper(csrrsi) {
    uint32_t t = csr_read(s, s->src2.imm);
    *s->dest.preg = t;
    uint32_t zimm = s->isa.instr.i.rs1;
    if (zimm != 0) {
        csr_write(s, s->src2.imm, t | zimm);
    }
}

/* csrrci: t = CSR; CSR = t & ~zimm;      x[rd] = t */
def_EHelper(csrrci) {
    uint32_t t = csr_read(s, s->src2.imm);
    *s->dest.preg = t;
    uint32_t zimm = s->isa.instr.i.rs1;
    if (zimm != 0) {
        csr_write(s, s->src2.imm, t & ~zimm);
    }
}

def_EHelper(inv) {

}


/**********  FLOAT  ***********/
