#pragma once
#include "decode.h"
#include "fpu/softfloat.h"

#define FP_POSTOP(lane) do { \
    (lane)->fcsr |= get_float_exception_flags(&(lane)->fp_status); \
    set_float_exception_flags(0, &(lane)->fp_status); \
} while (0)

static inline uint8_t set_rm(Decode *s) {
    uint8_t rm = s->isa.instr.fp.rm;
    set_float_rounding_mode(rm == 7 ? s->lane->fcsr >> 5: rm, &s->lane->fp_status);
    return rm;
}

static inline uint32_t get_rs3(Decode *s) {
    /* R4 格式中 rs3 在 bits 31:27，即 fp.funct5 */
    return s->isa.instr.fp.funct5;
}

/**********  LDST  ***********/
def_EHelper(flw) {
    uint32_t addr = *s->src1.preg + s->src2.imm;
    uint32_t val = vram_load(s, addr, 4);
    *s->dest.preg = val;
}

def_EHelper(fsw) {
    uint32_t val = *s->dest.preg;
    uint32_t addr = *s->src1.preg + s->src2.imm;
    vram_store(s, addr, val, 4);
}

/**********  计算 (Arithmetic)  ***********/
def_EHelper(fadd_s) {
    set_rm(s);
    float32 a = *s->src1.preg;
    float32 b = *s->src2.preg;
    *s->dest.preg = float32_add(a, b, &s->lane->fp_status);
    FP_POSTOP(s->lane);
}

def_EHelper(fsub_s) {
    set_rm(s);
    float32 a = *s->src1.preg;
    float32 b = *s->src2.preg;
    *s->dest.preg = float32_sub(a, b, &s->lane->fp_status);
    FP_POSTOP(s->lane);
}

def_EHelper(fmul_s) {
    set_rm(s);
    float32 a = *s->src1.preg;
    float32 b = *s->src2.preg;
    *s->dest.preg = float32_mul(a, b, &s->lane->fp_status);
    FP_POSTOP(s->lane);
}

def_EHelper(fdiv_s) {
    set_rm(s);
    float32 a = *s->src1.preg;
    float32 b = *s->src2.preg;
    *s->dest.preg = float32_div(a, b, &s->lane->fp_status);
    FP_POSTOP(s->lane);
}

def_EHelper(fsqrt_s) {
    set_rm(s);
    float32 a = *s->src1.preg;
    *s->dest.preg = float32_sqrt(a, &s->lane->fp_status);
    FP_POSTOP(s->lane);
}

/**********  MADD 类 (Fused Multiply-Add)  ***********/
def_EHelper(fmadd_s) {
    set_rm(s);
    float32 a = *s->src1.preg;
    float32 b = *s->src2.preg;
    uint32_t rs3_reg = get_rs3(s);
    float32 c = s->lane->fpr[rs3_reg];
    *s->dest.preg = float32_muladd(a, b, c, 0, &s->lane->fp_status);
    FP_POSTOP(s->lane);
}

def_EHelper(fmsub_s) {
    set_rm(s);
    float32 a = *s->src1.preg;
    float32 b = *s->src2.preg;
    uint32_t rs3_reg = get_rs3(s);
    float32 c = s->lane->fpr[rs3_reg];
    *s->dest.preg = float32_muladd(a, b, c, float_muladd_negate_c, &s->lane->fp_status);
    FP_POSTOP(s->lane);
}

def_EHelper(fnmsub_s) {
    set_rm(s);
    float32 a = *s->src1.preg;
    float32 b = *s->src2.preg;
    uint32_t rs3_reg = get_rs3(s);
    float32 c = s->lane->fpr[rs3_reg];
    *s->dest.preg = float32_muladd(a, b, c,
        float_muladd_negate_c | float_muladd_negate_product, &s->lane->fp_status);
    FP_POSTOP(s->lane);
}

def_EHelper(fnmadd_s) {
    set_rm(s);
    float32 a = *s->src1.preg;
    float32 b = *s->src2.preg;
    uint32_t rs3_reg = get_rs3(s);
    float32 c = s->lane->fpr[rs3_reg];
    *s->dest.preg = float32_muladd(a, b, c, float_muladd_negate_product, &s->lane->fp_status);
    FP_POSTOP(s->lane);
}

/**********  类型转换 (Conversion)  ***********/
def_EHelper(fcvt_w_s) {
    set_rm(s);
    float32 a = *s->src1.preg;
    *s->dest.preg = float32_to_int32(a, &s->lane->fp_status);
    FP_POSTOP(s->lane);
}

def_EHelper(fcvt_wu_s) {
    set_rm(s);
    float32 a = *s->src1.preg;
    *s->dest.preg = float32_to_uint32(a, &s->lane->fp_status);
    FP_POSTOP(s->lane);
}

def_EHelper(fcvt_s_w) {
    set_rm(s);
    int32_t a = *s->src1.preg;
    *s->dest.preg = int32_to_float32(a, &s->lane->fp_status);
    FP_POSTOP(s->lane);
}

def_EHelper(fcvt_s_wu) {
    set_rm(s);
    uint32_t a = *s->src1.preg;
    *s->dest.preg = uint32_to_float32(a, &s->lane->fp_status);
    FP_POSTOP(s->lane);
}
// typedef enum {
//     FP32,
//     BF16,
//     E5M2,
//     E4M3,
//     E2M1
// } FloatType;

// typedef struct {
//     FloatType type;
//     int exp_bits;
//     int mant_bits;
//     int exp_bias;
//     bool has_inf;
//     bool has_nan;
// } FloatFmt;

// typedef enum {
//     IS_ZERO,
//     IS_SUBN,
//     IS_NORM,
//     IS_INF,
//     IS_NAN,
// } FPClass;

// #define FMT_FP32 ((FloatFmt){ FP32, 8,23, 127,  true,  true })
// #define FMT_BF16 ((FloatFmt){ BF16, 8, 7, 127,  true,  true })
// #define FMT_E5M2 ((FloatFmt){ E5M2, 5, 2,  15,  true,  true })
// #define FMT_E4M3 ((FloatFmt){ E4M3, 4, 3,   7, false,  true })
// #define FMT_E2M1 ((FloatFmt){ E2M1, 2, 1,   1, false, false })

// static inline uint32_t pack(uint32_t sign, int32_t exp, uint32_t mant, FloatFmt fmt) {
//     uint32_t mask_e = (1 << fmt.exp_bits) - 1;
//     uint32_t mask_m = (1 << fmt.mant_bits) - 1;
//     return ((sign & 1) << (fmt.mant_bits + fmt.exp_bits)) | ((exp&mask_e) << fmt.mant_bits) | (mant & mask_m);
// }

// static inline uint32_t pack_fp32(uint32_t sign, int32_t exp, uint32_t mant) {
//     return pack(sign, exp, mant, FMT_FP32);
// }

// static inline uint32_t convert_lp_fp32(uint32_t raw, FloatFmt fmt) {
//     const int m_size = fmt.mant_bits;
//     const int e_size = fmt.exp_bits;
//     const int e_max = (1 << e_size) -1;

//     uint32_t sign = extract32(raw, m_size + e_size, 1);
//     int32_t exp = extract32(raw, m_size, e_size);
//     uint32_t mant = extract32(raw, 0, m_size);

//     if (exp == e_max) { //指数全1 
//         if (mant == 0 && fmt.has_inf) {// lp有Inf且是
//             return pack_fp32(sign, 0xFF, 0);
//         }
//         if ((fmt.type == E4M3 && mant == 0b111) || 
//             (fmt.type != E4M3 && mant != 0  && fmt.has_nan)
//         ) { // 特判E4M3全1mant才是NaN，其余mant!=0即可
//             return pack_fp32(sign, 0xFF, mant << (FMT_FP32.mant_bits - m_size));
//         }
//     } 

//     if (exp == 0 && mant == 0) { //Zeros
//         return pack_fp32(sign, 0, 0);
//     }

//     uint32_t res_exp, res_mant;
//     if (exp == 0) { // 非规格数
//         int lz = __builtin_clz(mant) - (32 - m_size); //额外减掉int32带来的额外前导零
//             int shift = lz +1;
//             res_exp = (1 - fmt.exp_bias - lz) + FMT_FP32.exp_bias; //计算lp格式的非规格指数基础值 额外减去mant到第一位1的位数
//             res_mant = (mant << (FMT_FP32.mant_bits - m_size + shift) ) & 0x7FFFFF; // 左移(23-m_size)的基础，然后额外移动去mant到第一位1的位数+1，然后掩码
//     } else {
//         //规格化数
//         res_exp = exp - fmt.exp_bias + FMT_FP32.exp_bias;
//         res_mant = mant << (FMT_FP32.mant_bits - m_size);
//     }
//     return pack_fp32(sign, res_exp, res_mant);
// }

// static inline uint32_t convert_fp32_lp(uint32_t raw, FloatFmt fmt, uint8_t rm, GPGPULane* lane) {
//     // 1. 提取 FP32 字段
//     const int m_size = FMT_FP32.mant_bits;
//     const int e_size = FMT_FP32.exp_bits;
//     const int e_max = (1 << e_size) -1;

//     uint32_t sign = extract32(raw, m_size + e_size, 1);
//     int32_t exp = extract32(raw, m_size, e_size);
//     uint32_t mant = extract32(raw, 0, m_size);

//     // 计算目标格式的最大表示值 (以 FP32 的指数衡量)
//     int32_t lp_max_exp = ((1 << fmt.exp_bits) - 1) - fmt.exp_bias + FMT_FP32.exp_bias;

//     // 2. 特殊值与饱和处理
//     if (exp == e_max) { // 输入是 Inf 或 NaN
//         if (fmt.has_inf) return pack(sign, (1 << fmt.exp_bits) - 1, (mant ? 1 : 0), fmt);
//         // 不支持 Inf 的格式饱和到最大值
//         return pack(sign, (1 << fmt.exp_bits) - 1, (1 << fmt.mant_bits) - 1, fmt);
//     }

//     // 3. 溢出饱和判断
//     if (exp > lp_max_exp) {
//         // 饱和到该格式能表示的最大有限值
//         return pack(sign, (1 << fmt.exp_bits) - 1, (1 << fmt.mant_bits) - 1, fmt);
//     }

//     // 4. 正常转换与舍入 (此处简化为截断，实际建议用 RNE)
//     int32_t new_exp = exp - 127 + fmt.exp_bias;
//     uint32_t new_mant = mant >> (23 - fmt.mant_bits);

//     if (new_exp <= 0) { /* 处理下溢为非规格化或零... */ return (sign << (fmt.exp_bits + fmt.mant_bits)); }

//     return pack(sign, new_exp, new_mant, fmt);
// }
def_EHelper(fcvt_s_bf16) {
    set_rm(s);
    *s->dest.preg = bfloat16_to_float32(*s->src1.preg, &s->lane->fp_status);
    FP_POSTOP(s->lane);
}

def_EHelper(fcvt_bf16_s) {
    set_rm(s);
    *s->dest.preg = float32_to_bfloat16(*s->src1.preg, &s->lane->fp_status);
    FP_POSTOP(s->lane);
}

def_EHelper(fcvt_s_e5m2) {
    // *s->dest.preg = convert_lp_fp32(*s->src1.preg, FMT_E5M2);
    set_rm(s);
    bfloat16 bf_val = float8_e5m2_to_bfloat16(*s->src1.preg, &s->lane->fp_status);
    *s->dest.preg = bfloat16_to_float32(bf_val, &s->lane->fp_status);
    FP_POSTOP(s->lane);
}

def_EHelper(fcvt_e5m2_s) {
    set_rm(s);
    *s->dest.preg = float32_to_float8_e5m2(*s->src1.preg,true, &s->lane->fp_status);
    FP_POSTOP(s->lane);
}

def_EHelper(fcvt_s_e4m3) {
    // *s->dest.preg = convert_lp_fp32(*s->src1.preg, FMT_E4M3);
    set_rm(s);
    bfloat16 bf_val = float8_e4m3_to_bfloat16(*s->src1.preg, &s->lane->fp_status);
    *s->dest.preg = bfloat16_to_float32(bf_val, &s->lane->fp_status);
    FP_POSTOP(s->lane);
}

def_EHelper(fcvt_e4m3_s) {
    set_rm(s);
    *s->dest.preg = float32_to_float8_e4m3(*s->src1.preg, true, &s->lane->fp_status);
    FP_POSTOP(s->lane);
}

def_EHelper(fcvt_s_e2m1) {
    // *s->dest.preg = convert_lp_fp32(*s->src1.preg, FMT_E2M1);
    set_rm(s);
    float8_e4m3 f8_val = float4_e2m1_to_float8_e4m3(*s->src1.preg, &s->lane->fp_status);
    bfloat16 bf_val = float8_e4m3_to_bfloat16(f8_val, &s->lane->fp_status);
    *s->dest.preg = bfloat16_to_float32(bf_val, &s->lane->fp_status);
    FP_POSTOP(s->lane);
}

static inline uint32_t float32_to_float4_e2m1(float32 raw,float_status * s) {
    uint32_t sign = extract32(raw, 31, 1);
    uint32_t exp = extract32(raw, 23, 8);
    uint32_t mant = extract32(raw, 0, 23);

    union { uint32_t u; float f; } pun = { .u = raw & 0x7FFFFFFF };
    float abs_f = pun.f;

    static const float tbl[] = {
        0.0f,
        0.5f,
        1.0f,
        1.5f,
        2.0f,
        3.0f,
        4.0f,
        6.0f
    };

    // Inf NaN
    if (unlikely(exp == 0xFF)) {
        if (mant != 0) { // NaN
            float_raise(float_flag_invalid, s);
            return (sign << 3) | 0b111;
        }
        float_raise(float_flag_overflow |float_flag_inexact, s);
        return (sign << 3) | 0b111;
    }
    
    // 零
    if (unlikely(abs_f == 0.0f)) {
        return sign << 3;
    }

    // 超过最大值 6.0 直接饱和 
    if (abs_f >= 6.0f) {
        if (abs_f > 6.0f) float_raise(float_flag_overflow | float_flag_inexact, s);
        return (sign << 3) | 7;
    }
    
    // 寻找区间abs_f in [ tbl[idx], tbl[idx+1] )
    int idx;
    for (idx = 0; idx < 7; idx++) {
        if (abs_f < tbl[idx+1]) {
            break;
        }
    }

    if (abs_f == tbl[idx]) return (sign << 3) | idx;

    bool up = false;
    float mid = (tbl[idx] + tbl[idx + 1]) / 2.0f;
    switch (s->float_rounding_mode) {
        case float_round_nearest_even:
            if (abs_f > mid) up = true;
            else if (abs_f == mid) up = (idx & 1); /* 向上找偶数: idx=1(0.5)->idx=2(1.0) */
            break;
        case float_round_ties_away:
            up = (abs_f >= mid);
            break;
        case float_round_up:
            up = !sign;
            break;
        case float_round_down:
            up = sign;
            break;
        case float_round_to_zero:
            up = false;
            break;
        default: abort();
    }
    if (up) idx++;
    float_raise(float_flag_inexact, s);

    if (idx == 1 || idx == 0) {
        float_raise(float_flag_underflow, s);
    }
    return (sign << 3) | idx;
}

def_EHelper(fcvt_e2m1_s) {
    set_rm(s);
    *s->dest.preg = float32_to_float4_e2m1(*s->src1.preg, &s->lane->fp_status);
    FP_POSTOP(s->lane);
    
}
/**********  移动 (Move)  ***********/
def_EHelper(fmv_x_w) {
    /* float → int 位操作不变，直接搬 */
    *s->dest.preg = *s->src1.preg;
}

def_EHelper(fmv_w_x) {
    /* int → float 位操作不变，直接搬 */
    *s->dest.preg = *s->src1.preg;
}

/**********  比较 (Compare)  ***********/
def_EHelper(feq_s) {
    float32 a = *s->src1.preg;
    float32 b = *s->src2.preg;
    *s->dest.preg = float32_eq_quiet(a, b, &s->lane->fp_status) ? 1 : 0;
    FP_POSTOP(s->lane);
}

def_EHelper(flt_s) {
    float32 a = *s->src1.preg;
    float32 b = *s->src2.preg;
    *s->dest.preg = float32_lt(a, b, &s->lane->fp_status) ? 1 : 0;
    FP_POSTOP(s->lane);
}

def_EHelper(fle_s) {
    float32 a = *s->src1.preg;
    float32 b = *s->src2.preg;
    *s->dest.preg = float32_le(a, b, &s->lane->fp_status) ? 1 : 0;
    FP_POSTOP(s->lane);
}

/**********  符号注入 (Sign Injection)  ***********/
/* fsgnj.s  rd, rs1, rs2: rd = {rs2[31], rs1[30:0]} */
def_EHelper(fsgnj_s) {
    uint32_t rs1 = *s->src1.preg;
    uint32_t rs2 = *s->src2.preg;
    *s->dest.preg = (rs1 & 0x7FFFFFFF) | (rs2 & 0x80000000);
}

/* fsgnjn.s rd, rs1, rs2: rd = {~rs2[31], rs1[30:0]} */
def_EHelper(fsgnjn_s) {
    uint32_t rs1 = *s->src1.preg;
    uint32_t rs2 = *s->src2.preg;
    *s->dest.preg = (rs1 & 0x7FFFFFFF) | ((~rs2) & 0x80000000);
}

/* fsgnjx.s rd, rs1, rs2: rd = {rs1[31] ^ rs2[31], rs1[30:0]} */
def_EHelper(fsgnjx_s) {
    uint32_t rs1 = *s->src1.preg;
    uint32_t rs2 = *s->src2.preg;
    *s->dest.preg = rs1 ^ (rs2 & 0x80000000);
}

/**********  极值 (Min/Max)  ***********/
def_EHelper(fmin_s) {
    float32 a = *s->src1.preg;
    float32 b = *s->src2.preg;
    *s->dest.preg = float32_min(a, b, &s->lane->fp_status);
    FP_POSTOP(s->lane);
}

def_EHelper(fmax_s) {
    float32 a = *s->src1.preg;
    float32 b = *s->src2.preg;
    *s->dest.preg = float32_max(a, b, &s->lane->fp_status);
    FP_POSTOP(s->lane);
}

/**********  分类 (Classify)  ***********/
def_EHelper(fclass_s) {
    float32 a = *s->src1.preg;
    uint32_t cls = 0;
    int exp = (a >> 23) & 0xFF;
    uint32_t mant = a & 0x7FFFFF;
    bool sign = (a >> 31) & 1;

    if (exp == 0xFF && mant == 0) {
        cls = sign ? BIT(0) : BIT(7);  /* inf */
    } else if (exp == 0xFF && mant != 0) {
        cls = (mant & 0x400000) ? BIT(9) : BIT(8);  /* qNaN / sNaN */
    } else if (exp == 0 && mant == 0) {
        cls = sign ? BIT(3) : BIT(4);  /* zero */
    } else if (exp == 0) {
        cls = sign ? BIT(2) : BIT(5);  /* subnormal */
    } else {
        cls = sign ? BIT(1) : BIT(6);  /* normal */
    }
    *s->dest.preg = cls;
}
