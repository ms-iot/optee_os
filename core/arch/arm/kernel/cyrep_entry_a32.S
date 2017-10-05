/** @file
*
*  Copyright (c) Microsoft Corporation. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/
#include <arm32_macros.S>
#include <arm.h>
#include <asm.S>
#include <kernel/unwind.h>
#include <CyrepDef.h>

.extern optee_args

/*
 * void cyrep_memmove(void *dst, void *src, size_t sz)
 *
 * This is a simple non-optimized memmove which copies backwards or forwards
 * based on whether src and dst overlap or not.
 *
 * Make sure to preserve bootargs in r4-r7
 */
LOCAL_FUNC cyrep_memmove , :
UNWIND( .fnstart)
	cmp 	r2, #0						/* sz == 0 */
	beq		memmove_end
	cmp		r0, r1						/* src == dst */
	beq		memmove_end
	bhi		copy_backwards				/* copy backwards if dst > src */

copy_forwards_loop:
	/* use ARM auto post-indexing */
	ldrb	r3, [r1], #1				/* tmp = *src++ */
	strb	r3, [r0], #1				/* *dst++ = tmp */
	subs	r2, r2, #1					/* sz -= 1 */
	beq		memmove_end
	b		copy_forwards_loop

copy_backwards:
	add		r0, r2						/* dst += sz */
	add		r1, r2						/* src += sz */

copy_backwards_loop:
	/* use ARM auto pre-indexing */
	ldrb	r3, [r1, #1]!				/* tmp = *--src */
	strb	r3, [r0, #1]!				/* *--dst = tmp */
	subs	r2, r2, #1					/* sz -= 1 */
	beq		memmove_end
	b		copy_backwards_loop

memmove_end:
	bx		lr
UNWIND( .fnend)
END_FUNC cyrep_memmove

/*
 * void cyrep_entry(CyrepFwArgs *cyrep_args)
 *
 * Make sure to preserve bootargs in r4-r7
 */
FUNC cyrep_entry , :
UNWIND( .fnstart)
	cmp		r0, #0						/* cyrep_args != NULL */
	beq		cyrep_entry_end

	mov		r1, r0 						/* src */
	ldr		r0, =optee_args				/* dst */
	mov		r2, #CYREP_ARGS_MAX_SIZE	/* sz */
	bl 		cyrep_memmove				/* memmove(dst, src, sz) */

	mov		r0, #0						/* success */
	b		cyrep_entry_end

error:
	mov		r0, #-1						/* failure */
cyrep_entry_end:
	bx		lr
UNWIND( .fnend)
END_FUNC cyrep_entry