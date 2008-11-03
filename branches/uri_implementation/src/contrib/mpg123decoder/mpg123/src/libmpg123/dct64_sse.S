/*
	dct64_sse: MMX/SSE optimized dct64

	copyright 2006-2007 by Zuxy Meng <zuxy.meng@gmail.com> / the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by the mysterious higway for MMX (apparently)
	then developed into SSE opt by Zuxy Meng, also building on Romain Dolbeau's AltiVec
	Both have agreed to distribution under LGPL 2.1 .

	Transformed back into standalone asm, with help of
	gcc -S -DHAVE_CONFIG_H -I.  -march=pentium3 -O3 -Wall -pedantic -fno-strict-aliasing -DREAL_IS_FLOAT -c -o dct64_sse.{S,c}

	Original comment from MPlayer source follows:
*/

/*
 * Discrete Cosine Tansform (DCT) for SSE
 * based upon code from mp3lib/dct64.c, mp3lib/dct64_altivec.c
 * and mp3lib/dct64_MMX.c
 */

#include "mangle.h"

#ifndef __APPLE__
	.section	.rodata
#else
	.data
#endif
	ALIGN16
	/* .type	nnnn, @object
	   .size	nnnn, 16 */
nnnn:
	.long	-2147483648
	.long	-2147483648
	.long	-2147483648
	.long	-2147483648
	ALIGN16
	/* .type	ppnn, @object
	   .size	ppnn, 16 */
ppnn:
	.long	0
	.long	0
	.long	-2147483648
	.long	-2147483648
	ALIGN16
	/* .type	pnpn, @object
	   .size	pnpn, 16 */
pnpn:
	.long	0
	.long	-2147483648
	.long	0
	.long	-2147483648
	ALIGN4
	/* .type	one.4748, @object
	   .size	one.4748, 4 */
one.4748:
	.long	1065353216

	.text
	ALIGN16,,15
.globl ASM_NAME(dct64_sse)
	/* .type	ASM_NAME(dct64_sse), @function */
ASM_NAME(dct64_sse):
	pushl	%ebp
	movl	%esp, %ebp
	/* stack from ebp: 0=ebp 4=back 8=arg0 12=arg1 16=arg2 */
#define ARG(n) (8+n*4)(%ebp)
	andl	$-16, %esp /* align the stack at 16 bytes */
	subl	$256, %esp /* reserve space for local b1 and b2 */
	pushl	%ebx
/* stack from esp: 0=ebx 4...131=b2 132...259=b1 */
#define B1OFF 132
#define B2OFF 4
#define B1(n) (B1OFF+n)(%esp)
#define B2(n) (B2OFF+n)(%esp)

	movl	ARG(2), %eax
	movl	ARG(0), %ecx
#APP
/* for (i = 0; i < 0x20 / 2; i += 4) cycle 1 */
	movaps    ASM_NAME(costab_mmxsse), %xmm3
	shufps    $27, %xmm3, %xmm3
	MOVUAPS    (%eax), %xmm1
	movaps    %xmm1, %xmm4
	MOVUAPS    112(%eax), %xmm2
	shufps    $27, %xmm4, %xmm4
	movaps    %xmm2, %xmm0
	shufps    $27, %xmm0, %xmm0
	addps     %xmm0, %xmm1
	movaps    %xmm1, B1(0)
	subps     %xmm2, %xmm4
	mulps     %xmm3, %xmm4
	movaps    %xmm4, B1(112)
	
#NO_APP
	movl	ARG(1), %ebx
#APP
/* for (i = 0; i < 0x20 / 2; i += 4) cycle 2 */
	movaps    ASM_NAME(costab_mmxsse)+16, %xmm3
	shufps    $27, %xmm3, %xmm3
	MOVUAPS    16(%eax), %xmm1
	movaps    %xmm1, %xmm4
	MOVUAPS    96(%eax), %xmm2
	shufps    $27, %xmm4, %xmm4
	movaps    %xmm2, %xmm0
	shufps    $27, %xmm0, %xmm0
	addps     %xmm0, %xmm1
	movaps    %xmm1, B1(16)
	subps     %xmm2, %xmm4
	mulps     %xmm3, %xmm4
	movaps    %xmm4, B1(96)
	
/* for (i = 0; i < 0x20 / 2; i += 4) cycle 3 */
	movaps    ASM_NAME(costab_mmxsse)+32, %xmm3
	shufps    $27, %xmm3, %xmm3
	MOVUAPS    32(%eax), %xmm1
	movaps    %xmm1, %xmm4
	MOVUAPS    80(%eax), %xmm2
	shufps    $27, %xmm4, %xmm4
	movaps    %xmm2, %xmm0
	shufps    $27, %xmm0, %xmm0
	addps     %xmm0, %xmm1
	movaps    %xmm1, B1(32)
	subps     %xmm2, %xmm4
	mulps     %xmm3, %xmm4
	movaps    %xmm4, B1(80)
	
/* for (i = 0; i < 0x20 / 2; i += 4) cycle 4 */
	movaps    ASM_NAME(costab_mmxsse)+48, %xmm3
	shufps    $27, %xmm3, %xmm3
	MOVUAPS    48(%eax), %xmm1
	movaps    %xmm1, %xmm4
	MOVUAPS    64(%eax), %xmm2
	shufps    $27, %xmm4, %xmm4
	movaps    %xmm2, %xmm0
	shufps    $27, %xmm0, %xmm0
	addps     %xmm0, %xmm1
	movaps    %xmm1, B1(48)
	subps     %xmm2, %xmm4
	mulps     %xmm3, %xmm4
	movaps    %xmm4, B1(64)
	
	movaps    B1(0), %xmm1
	movaps    B1(16), %xmm3
	movaps    B1(32), %xmm4
	movaps    B1(48), %xmm6
	movaps    %xmm1, %xmm7
	shufps    $27, %xmm7, %xmm7
	movaps    %xmm3, %xmm5
	shufps    $27, %xmm5, %xmm5
	movaps    %xmm4, %xmm2
	shufps    $27, %xmm2, %xmm2
	movaps    %xmm6, %xmm0
	shufps    $27, %xmm0, %xmm0
	addps     %xmm0, %xmm1
	movaps    %xmm1, B2(0)
	addps     %xmm2, %xmm3
	movaps    %xmm3, B2(16)
	subps     %xmm4, %xmm5
	movaps    %xmm5, B2(32)
	subps     %xmm6, %xmm7
	movaps    %xmm7, B2(48)
	
	movaps    B1(64), %xmm1
	movaps    B1(80), %xmm3
	movaps    B1(96), %xmm4
	movaps    B1(112), %xmm6
	movaps    %xmm1, %xmm7
	shufps    $27, %xmm7, %xmm7
	movaps    %xmm3, %xmm5
	shufps    $27, %xmm5, %xmm5
	movaps    %xmm4, %xmm2
	shufps    $27, %xmm2, %xmm2
	movaps    %xmm6, %xmm0
	shufps    $27, %xmm0, %xmm0
	addps     %xmm0, %xmm1
	movaps    %xmm1, B2(64)
	addps     %xmm2, %xmm3
	movaps    %xmm3, B2(80)
	subps     %xmm4, %xmm5
	movaps    %xmm5, B2(96)
	subps     %xmm6, %xmm7
	movaps    %xmm7, B2(112)
	
	movaps    B2(32), %xmm0
	movaps    B2(48), %xmm1
	movaps    ASM_NAME(costab_mmxsse)+64, %xmm4
	xorps     %xmm6, %xmm6
	shufps    $27, %xmm4, %xmm4
	mulps     %xmm4, %xmm1
	movaps    ASM_NAME(costab_mmxsse)+80, %xmm2
	xorps     %xmm7, %xmm7
	shufps    $27, %xmm2, %xmm2
	mulps     %xmm2, %xmm0
	movaps    %xmm0, B2(32)
	movaps    %xmm1, B2(48)
	movaps    B2(96), %xmm3
	mulps     %xmm2, %xmm3
	subps     %xmm3, %xmm6
	movaps    %xmm6, B2(96)
	movaps    B2(112), %xmm5
	mulps     %xmm4, %xmm5
	subps     %xmm5, %xmm7
	movaps    %xmm7, B2(112)
	
	movaps    ASM_NAME(costab_mmxsse)+96, %xmm0
	shufps    $27, %xmm0, %xmm0
	movaps    nnnn, %xmm5
	movaps    %xmm5, %xmm6
	
	movaps    B2(0), %xmm2
	movaps    B2(16), %xmm3
	movaps    %xmm2, %xmm4
	xorps     %xmm5, %xmm6
	shufps    $27, %xmm4, %xmm4
	movaps    %xmm3, %xmm1
	shufps    $27, %xmm1, %xmm1
	addps     %xmm1, %xmm2
	movaps    %xmm2, B1(0)
	subps     %xmm3, %xmm4
	xorps     %xmm6, %xmm4
	mulps     %xmm0, %xmm4
	movaps    %xmm4, B1(16)
	
	movaps    B2(32), %xmm2
	movaps    B2(48), %xmm3
	movaps    %xmm2, %xmm4
	xorps     %xmm5, %xmm6
	shufps    $27, %xmm4, %xmm4
	movaps    %xmm3, %xmm1
	shufps    $27, %xmm1, %xmm1
	addps     %xmm1, %xmm2
	movaps    %xmm2, B1(32)
	subps     %xmm3, %xmm4
	xorps     %xmm6, %xmm4
	mulps     %xmm0, %xmm4
	movaps    %xmm4, B1(48)
	
	movaps    B2(64), %xmm2
	movaps    B2(80), %xmm3
	movaps    %xmm2, %xmm4
	xorps     %xmm5, %xmm6
	shufps    $27, %xmm4, %xmm4
	movaps    %xmm3, %xmm1
	shufps    $27, %xmm1, %xmm1
	addps     %xmm1, %xmm2
	movaps    %xmm2, B1(64)
	subps     %xmm3, %xmm4
	xorps     %xmm6, %xmm4
	mulps     %xmm0, %xmm4
	movaps    %xmm4, B1(80)
	
	movaps    B2(96), %xmm2
	movaps    B2(112), %xmm3
	movaps    %xmm2, %xmm4
	xorps     %xmm5, %xmm6
	shufps    $27, %xmm4, %xmm4
	movaps    %xmm3, %xmm1
	shufps    $27, %xmm1, %xmm1
	addps     %xmm1, %xmm2
	movaps    %xmm2, B1(96)
	subps     %xmm3, %xmm4
	xorps     %xmm6, %xmm4
	mulps     %xmm0, %xmm4
	movaps    %xmm4, B1(112)
	
	movss     one.4748, %xmm1
	movss     ASM_NAME(costab_mmxsse)+112, %xmm0
	movaps    %xmm1, %xmm3
	unpcklps  %xmm0, %xmm3
	movss     ASM_NAME(costab_mmxsse)+116, %xmm2
	movaps    %xmm1, %xmm0
	unpcklps  %xmm2, %xmm0
	unpcklps  %xmm3, %xmm0
	movaps    ppnn, %xmm2
	
	movaps    B1(0), %xmm3
	movaps    %xmm3, %xmm4
	shufps    $20, %xmm4, %xmm4
	shufps    $235, %xmm3, %xmm3
	xorps     %xmm2, %xmm3
	addps     %xmm3, %xmm4
	mulps     %xmm0, %xmm4
	movaps    %xmm4, B2(0)
	movaps    B1(16), %xmm6
	movaps    %xmm6, %xmm5
	shufps    $27, %xmm5, %xmm5
	xorps     %xmm2, %xmm5
	addps     %xmm5, %xmm6
	mulps     %xmm0, %xmm6
	movaps    %xmm6, B2(16)
	
	movaps    B1(32), %xmm3
	movaps    %xmm3, %xmm4
	shufps    $20, %xmm4, %xmm4
	shufps    $235, %xmm3, %xmm3
	xorps     %xmm2, %xmm3
	addps     %xmm3, %xmm4
	mulps     %xmm0, %xmm4
	movaps    %xmm4, B2(32)
	movaps    B1(48), %xmm6
	movaps    %xmm6, %xmm5
	shufps    $27, %xmm5, %xmm5
	xorps     %xmm2, %xmm5
	addps     %xmm5, %xmm6
	mulps     %xmm0, %xmm6
	movaps    %xmm6, B2(48)
	
	movaps    B1(64), %xmm3
	movaps    %xmm3, %xmm4
	shufps    $20, %xmm4, %xmm4
	shufps    $235, %xmm3, %xmm3
	xorps     %xmm2, %xmm3
	addps     %xmm3, %xmm4
	mulps     %xmm0, %xmm4
	movaps    %xmm4, B2(64)
	movaps    B1(80), %xmm6
	movaps    %xmm6, %xmm5
	shufps    $27, %xmm5, %xmm5
	xorps     %xmm2, %xmm5
	addps     %xmm5, %xmm6
	mulps     %xmm0, %xmm6
	movaps    %xmm6, B2(80)
	
	movaps    B1(96), %xmm3
	movaps    %xmm3, %xmm4
	shufps    $20, %xmm4, %xmm4
	shufps    $235, %xmm3, %xmm3
	xorps     %xmm2, %xmm3
	addps     %xmm3, %xmm4
	mulps     %xmm0, %xmm4
	movaps    %xmm4, B2(96)
	movaps    B1(112), %xmm6
	movaps    %xmm6, %xmm5
	shufps    $27, %xmm5, %xmm5
	xorps     %xmm2, %xmm5
	addps     %xmm5, %xmm6
	mulps     %xmm0, %xmm6
	movaps    %xmm6, B2(112)
	
	movss     ASM_NAME(costab_mmxsse)+120, %xmm0
	movaps    %xmm1, %xmm2
	movaps    %xmm0, %xmm7
	unpcklps  %xmm1, %xmm2
	unpcklps  %xmm0, %xmm7
	movaps    pnpn, %xmm0
	unpcklps  %xmm7, %xmm2
	
	movaps    B2(32), %xmm1
	movaps    %xmm1, %xmm3
	shufps    $224, %xmm3, %xmm3
	shufps    $181, %xmm1, %xmm1
	xorps     %xmm0, %xmm1
	addps     %xmm1, %xmm3
	mulps     %xmm2, %xmm3
	movaps    %xmm3, B1(32)
	movaps    B2(48), %xmm4
	movaps    %xmm4, %xmm5
	shufps    $224, %xmm5, %xmm5
	shufps    $181, %xmm4, %xmm4
	xorps     %xmm0, %xmm4
	addps     %xmm4, %xmm5
	mulps     %xmm2, %xmm5
	movaps    %xmm5, B1(48)
	
	movaps    B2(64), %xmm1
	movaps    %xmm1, %xmm3
	shufps    $224, %xmm3, %xmm3
	shufps    $181, %xmm1, %xmm1
	xorps     %xmm0, %xmm1
	addps     %xmm1, %xmm3
	mulps     %xmm2, %xmm3
	movaps    %xmm3, B1(64)
	movaps    B2(80), %xmm4
	movaps    %xmm4, %xmm5
	shufps    $224, %xmm5, %xmm5
	shufps    $181, %xmm4, %xmm4
	xorps     %xmm0, %xmm4
	addps     %xmm4, %xmm5
	mulps     %xmm2, %xmm5
	movaps    %xmm5, B1(80)
	
	movaps    B2(96), %xmm1
	movaps    %xmm1, %xmm3
	shufps    $224, %xmm3, %xmm3
	shufps    $181, %xmm1, %xmm1
	xorps     %xmm0, %xmm1
	addps     %xmm1, %xmm3
	mulps     %xmm2, %xmm3
	movaps    %xmm3, B1(96)
	movaps    B2(112), %xmm4
	movaps    %xmm4, %xmm5
	shufps    $224, %xmm5, %xmm5
	shufps    $181, %xmm4, %xmm4
	xorps     %xmm0, %xmm4
	addps     %xmm4, %xmm5
	mulps     %xmm2, %xmm5
	movaps    %xmm5, B1(112)
	
#NO_APP
	flds	B1(40)
	movl	%esp, %edx
	addl	$B1OFF, %edx
	movl	%esp, %eax
	addl	$B2OFF, %eax
	fadds	B1(44)
	fstps	B1(40)
	flds	B1(56)
	fadds	B1(60)
	flds	B1(48)
	fadd	%st(1), %st
	fstps	B1(48)
	fadds	B1(52)
	fstps	B1(56)
	flds	B1(52)
	fadds	B1(60)
	fstps	B1(52)
	flds	B1(72)
	fadds	B1(76)
	fstps	B1(72)
	flds	B1(88)
	fadds	B1(92)
	flds	B1(80)
	fadd	%st(1), %st
	fstps	B1(80)
	fadds	B1(84)
	fstps	B1(88)
	flds	B1(84)
	fadds	B1(92)
	fstps	B1(84)
	flds	B1(104)
	fadds	B1(108)
	fstps	B1(104)
	flds	B1(120)
	fadds	B1(124)
	flds	B1(112)
	fadd	%st(1), %st
	fstps	B1(112)
	fadds	B1(116)
	fstps	B1(120)
	flds	B1(116)
	fadds	B1(124)
	fstps	B1(116)
#APP
	flds       ASM_NAME(costab_mmxsse)+120
	flds     (%eax)
	fadds   4(%eax)
	fistp 512(%ecx)
	flds     (%eax)
	fsubs   4(%eax)
	fmul  %st(1)
	fistp    (%ecx)
	flds   12(%eax)
	fsubs   8(%eax)
	fmul  %st(1)
	fist  256(%ebx)
	fadds  12(%eax)
	fadds   8(%eax)
	fistp 256(%ecx)
	flds   16(%eax)
	fsubs  20(%eax)
	fmul  %st(1)
	flds   28(%eax)
	fsubs  24(%eax)
	fmul  %st(2)
	fist  384(%ebx)
	fld   %st(0)
	fadds  24(%eax)
	fadds  28(%eax)
	fld   %st(0)
	fadds  16(%eax)
	fadds  20(%eax)
	fistp 384(%ecx)
	fadd  %st(2)
	fistp 128(%ecx)
	faddp %st(1)
	fistp 128(%ebx)
	flds   32(%edx)
	fadds  48(%edx)
	fistp 448(%ecx)
	flds   48(%edx)
	fadds  40(%edx)
	fistp 320(%ecx)
	flds   40(%edx)
	fadds  56(%edx)
	fistp 192(%ecx)
	flds   56(%edx)
	fadds  36(%edx)
	fistp  64(%ecx)
	flds   36(%edx)
	fadds  52(%edx)
	fistp  64(%ebx)
	flds   52(%edx)
	fadds  44(%edx)
	fistp 192(%ebx)
	flds   60(%edx)
	fist  448(%ebx)
	fadds  44(%edx)
	fistp 320(%ebx)
	flds   96(%edx)
	fadds 112(%edx)
	fld   %st(0)
	fadds  64(%edx)
	fistp 480(%ecx)
	fadds  80(%edx)
	fistp 416(%ecx)
	flds  112(%edx)
	fadds 104(%edx)
	fld   %st(0)
	fadds  80(%edx)
	fistp 352(%ecx)
	fadds  72(%edx)
	fistp 288(%ecx)
	flds  104(%edx)
	fadds 120(%edx)
	fld   %st(0)
	fadds  72(%edx)
	fistp 224(%ecx)
	fadds  88(%edx)
	fistp 160(%ecx)
	flds  120(%edx)
	fadds 100(%edx)
	fld   %st(0)
	fadds  88(%edx)
	fistp  96(%ecx)
	fadds  68(%edx)
	fistp  32(%ecx)
	flds  100(%edx)
	fadds 116(%edx)
	fld   %st(0)
	fadds  68(%edx)
	fistp  32(%ebx)
	fadds  84(%edx)
	fistp  96(%ebx)
	flds  116(%edx)
	fadds 108(%edx)
	fld   %st(0)
	fadds  84(%edx)
	fistp 160(%ebx)
	fadds  76(%edx)
	fistp 224(%ebx)
	flds  108(%edx)
	fadds 124(%edx)
	fld   %st(0)
	fadds  76(%edx)
	fistp 288(%ebx)
	fadds  92(%edx)
	fistp 352(%ebx)
	flds  124(%edx)
	fist  480(%ebx)
	fadds  92(%edx)
	fistp 416(%ebx)
	ffreep %st(0)
	
#NO_APP
	movzwl	(%ecx), %eax
	movw	%ax, (%ebx)
	popl	%ebx
	movl	%ebp, %esp
	popl	%ebp
	ret
	/* .size	ASM_NAME(dct64_sse), .-ASM_NAME(dct64_sse) */

/* Mark non-executable stack. */
#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif
