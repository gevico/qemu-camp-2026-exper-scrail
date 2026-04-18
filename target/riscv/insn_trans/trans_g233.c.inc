#include "tcg/tcg.h"
// #define CONFIG_GEVICO_VEC

static bool trans_dma(DisasContext *ctx, arg_dma *a) {
    gen_helper_dma(tcg_env, tcg_constant_tl(a->rd), tcg_constant_tl(a->rs1),tcg_constant_tl(a->rs2));
    return true;
}

static bool trans_sort (DisasContext *ctx, arg_sort *a) {
    
    TCGv A_base_addr = get_gpr(ctx, a->rs1, EXT_NONE);
    TCGv K = get_gpr(ctx, a->rd, EXT_NONE);

    gen_helper_sort(tcg_env, A_base_addr, K);
    return true;
}

static bool trans_crush (DisasContext *ctx, arg_crush *a) {
    TCGv src_addr = get_gpr(ctx, a->rs1, EXT_NONE);
    TCGv dst_addr = get_gpr(ctx, a->rd, EXT_NONE);
    TCGv N = get_gpr(ctx, a->rs2, EXT_NONE);

    gen_helper_crush(tcg_env, dst_addr, src_addr, N);

    return true;
}

static bool trans_expand (DisasContext *ctx, arg_expand *a) {
    TCGv src_addr = get_gpr(ctx, a->rs1, EXT_NONE);
    TCGv dst_addr = get_gpr(ctx, a->rd, EXT_NONE);
    TCGv N = get_gpr(ctx, a->rs2, EXT_NONE);

    gen_helper_expand(tcg_env, dst_addr, src_addr, N);
    return true;
}

<<<<<<< HEAD
static bool trans_vdot (DisasContext *ctx, arg_vdot *a) {
    TCGv a_addr = get_gpr(ctx, a->rs1, EXT_NONE);
    TCGv b_addr = get_gpr(ctx, a->rs2, EXT_NONE);
    TCGv dest = dest_gpr(ctx, a->rd);
    gen_helper_vdot(dest, tcg_env, a_addr, b_addr);
    gen_set_gpr(ctx, a->rd, dest);
=======
static bool trans_gemm (DisasContext *ctx, arg_gemm *a) {
    return true;
}



static bool trans_vadd (DisasContext *ctx, arg_vadd *a) {
    return true;
}





static bool trans_vdot (DisasContext *ctx, arg_vdot *a) {
>>>>>>> a031fc3c (feat:dma sort crush expand)
    return true;
}

static bool trans_vrelu (DisasContext *ctx, arg_vrelu *a) {
<<<<<<< HEAD
    TCGv src_addr = get_gpr(ctx, a->rs1, EXT_NONE);
    TCGv N = get_gpr(ctx, a->rs2, EXT_NONE);
    TCGv dst_addr = get_gpr(ctx, a->rd, EXT_NONE);
    gen_helper_vrelu(tcg_env, dst_addr, src_addr, N);
    return true;
}
#ifndef CONFIG_GEVICO_VEC
static bool trans_vscale (DisasContext *ctx, arg_vscale *a) {
    TCGv src_addr = get_gpr(ctx, a->rs1, EXT_NONE);
    TCGv scale = get_gpr(ctx, a->rs2, EXT_NONE);
    TCGv dst_addr = get_gpr(ctx, a->rd, EXT_NONE);
    gen_helper_vscale(tcg_env, dst_addr, src_addr, scale);
    return true;
}
#else
static bool trans_vscale(DisasContext *ctx, arg_vscale *a)
{
    TCGv src_base = get_gpr(ctx, a->rs1, EXT_NONE);
    TCGv dst_base = get_gpr(ctx, a->rd, EXT_NONE);
    TCGv scale = get_gpr(ctx, a->rs2, EXT_NONE);
    TCGv_i64 scale_i64 = tcg_temp_new_i64();
    tcg_gen_extu_tl_i64(scale_i64, scale);
    // ld from src_base to env->?1
    tcg_gen_gvec_muls(MO_32, vreg_ofs(ctx, a->rd), vreg_ofs(ctx, a->rs1), scale_i64, MAXSZ(ctx), MAXSZ(ctx));
    //st from env->?2 ti dst_base
    // 3. 处理结尾清理和状态更新
    finalize_rvv_inst(ctx);
    return true;
}
#endif
static bool trans_vmax (DisasContext *ctx, arg_vmax *a) {
    TCGv src_addr = get_gpr(ctx, a->rs1, EXT_NONE);
    TCGv N = get_gpr(ctx, a->rs2, EXT_NONE);
    TCGv dest = dest_gpr(ctx, a->rd);
    gen_helper_vmax(dest, tcg_env, src_addr, N);
    gen_set_gpr(ctx, a->rd, dest);
    return true;
}

static bool trans_gemm (DisasContext *ctx, arg_gemm *a) {
    TCGv a_addr = get_gpr(ctx, a->rs1, EXT_NONE);
    TCGv b_addr = get_gpr(ctx, a->rs2, EXT_NONE);
    TCGv dst_addr = get_gpr(ctx, a->rd, EXT_NONE);
    gen_helper_gemm(tcg_env, dst_addr, a_addr, b_addr);
    return true;
}

static bool trans_vadd (DisasContext *ctx, arg_vadd *a) {
    TCGv a_addr = get_gpr(ctx, a->rs1, EXT_NONE);
    TCGv b_addr = get_gpr(ctx, a->rs2, EXT_NONE);
    TCGv dst_addr = get_gpr(ctx, a->rd, EXT_NONE);
    gen_helper_vadd(tcg_env, dst_addr, a_addr, b_addr);
=======
    return true;
}

static bool trans_vscale (DisasContext *ctx, arg_vscale *a) {
    return true;
}

static bool trans_vmax (DisasContext *ctx, arg_vmax *a) {
>>>>>>> a031fc3c (feat:dma sort crush expand)
    return true;
}