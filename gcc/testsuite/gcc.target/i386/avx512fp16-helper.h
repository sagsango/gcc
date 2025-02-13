/* This file is used for emulation of avx512fp16 runtime tests. To
   verify the correctness of _Float16 type calculation, the idea is
   convert _Float16 to float and do emulation using float instructions. 
   _Float16 type should not be emulate or check by itself.  */

#include "avx512f-helper.h"
#ifndef AVX512FP16_HELPER_INCLUDED
#define AVX512FP16_HELPER_INCLUDED

#ifdef DEBUG
#include <string.h>
#endif
#include <math.h>
#include <limits.h>
#include <float.h>

/* Useful macros.  */
#define NOINLINE __attribute__((noinline,noclone))
#define _ROUND_NINT (_MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC)
#define AVX512F_MAX_ELEM 512 / 32

/* Structure for _Float16 emulation  */
typedef union
{
  __m512          zmm;
  __m512h         zmmh;
  __m256          ymm[2];
  __m256h         ymmh[2];
  __m256i         ymmi[2];
  __m128h         xmmh[4];
  unsigned short  u16[32];
  unsigned int    u32[16];
  float           f32[16];
  _Float16        f16[32];
} V512;

/* Global variables.  */
V512 src1, src2, src3;
int n_errs = 0;

/* Helper function for packing/unpacking ph operands. */
void NOINLINE 
unpack_ph_2twops(V512 src, V512 *op1, V512 *op2)
{
    V512 v1;

    op1->zmm = _mm512_cvtph_ps(src.ymmi[0]);
    v1.ymm[0] = _mm512_extractf32x8_ps(src.zmm, 1);
    op2->zmm = _mm512_cvtph_ps(v1.ymmi[0]);
}

V512 NOINLINE
pack_twops_2ph(V512 op1, V512 op2)
{
    V512 v1, v2, v3;

    v1.ymmi[0] = _mm512_cvtps_ph(op1.zmm, _MM_FROUND_TO_NEAREST_INT);
    v2.ymmi[0] = _mm512_cvtps_ph(op2.zmm, _MM_FROUND_TO_NEAREST_INT);

    v3.zmm = _mm512_insertf32x8(v1.zmm, v2.ymm[0], 1);

    return v3;
}

/* Helper function used for result debugging */
#ifdef DEBUG
void NOINLINE
display_ps(const void *p, const char *banner, int n_elems)
{
    int i;
    V512 *v = (V512*)p;

    if (banner) {
        printf("%s", banner);
    }

    for (i = 15; i >= n_elems; i--) {
        printf(" --------");
        if (i == 8) {
            printf("\n");
            if (banner) {
                printf("%*s", (int)strlen(banner), "");
            }
        }
    }

    for (; i >= 0; i--) {
        printf(" %x", v->u32[i]);
        if (i == 8) {
            printf("\n");
            if (banner) {
                printf("%*s", (int)strlen(banner), "");
            }
        }
    }
    printf("\n");
}
#endif

/* Functions/macros used for init/result checking.
   Only check components within AVX512F_LEN.  */
#define TO_STRING(x) #x
#define STRINGIFY(x) TO_STRING(x)
#define NAME_OF(NAME) STRINGIFY(INTRINSIC (NAME))

#define CHECK_RESULT(res, exp, size, intrin) \
  check_results ((void*)res, (void*)exp, size,\
		 NAME_OF(intrin))

/* To evaluate whether result match _Float16 precision,
   only the last bit of real/emulate result could be
   different.  */
void NOINLINE
check_results(void *got, void *exp, int n_elems, char *banner)
{
    int i;
    V512 *v1 = (V512*)got;
    V512 *v2 = (V512*)exp;

    for (i = 0; i < n_elems; i++) {
        if (v1->u16[i] != v2->u16[i] &&
            ((v1->u16[i] > (v2->u16[i] + 1)) ||
             (v1->u16[i] < (v2->u16[i] - 1)))) {

#ifdef DEBUG
            printf("ERROR: %s failed at %d'th element: %x(%f) != %x(%f)\n",
                   banner ? banner : "", i,
                   v1->u16[i], *(float *)(&v1->u16[i]),
                   v2->u16[i], *(float *)(&v2->u16[i]));
            display_ps(got, "got:", n_elems);
            display_ps(exp, "exp:", n_elems);
#endif
            n_errs++;
            break;
        }
    }
}

/* Functions for src/dest initialization */
void NOINLINE
init_src()
{
    V512 v1, v2, v3, v4;
    int i;

    for (i = 0; i < AVX512F_MAX_ELEM; i++) {
        v1.f32[i] = -i + 1;
        v2.f32[i] = i * 0.5f;
        v3.f32[i] = i * 2.5f;
        v4.f32[i] = i - 0.5f;

        src3.u32[i] = (i + 1) * 10;
    }

    src1 = pack_twops_2ph(v1, v2);
    src2 = pack_twops_2ph(v3, v4);
}

void NOINLINE
init_dest(V512 * res, V512 * exp)
{
    int i;
    V512 v1;

    for (i = 0; i < AVX512F_MAX_ELEM; i++) {
        v1.f32[i] = 12 + 0.5f * i;
    }
    *res = *exp = pack_twops_2ph(v1, v1);
}

#define EMULATE(NAME) EVAL(emulate_, NAME, AVX512F_LEN)

#endif /* AVX512FP16_HELPER_INCLUDED */

/* Macros for AVX512VL Testing. Include V512 component usage
   and mask type for emulation. */

#if AVX512F_LEN == 256
#undef HF
#undef SF
#undef NET_MASK 
#undef MASK_VALUE 
#undef ZMASK_VALUE 
#define NET_MASK 0xffff
#define MASK_VALUE 0xcccc
#define ZMASK_VALUE 0xfcc1
#define HF(x) x.ymmh[0]
#define SF(x) x.ymm[0]
#elif AVX512F_LEN == 128
#undef HF
#undef SF
#undef NET_MASK 
#undef MASK_VALUE 
#undef ZMASK_VALUE 
#define NET_MASK 0xff
#define MASK_VALUE 0xcc
#define ZMASK_VALUE 0xc1
#define HF(x) x.xmmh[0]
#define SF(x) x.xmm[0]
#else
#define NET_MASK 0xffffffff
#define MASK_VALUE 0xcccccccc
#define ZMASK_VALUE 0xfcc1fcc1
#define HF(x) x.zmmh
#define SF(x) x.zmm
#endif

