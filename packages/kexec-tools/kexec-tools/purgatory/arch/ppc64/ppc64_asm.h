/*
 * ppc64_asm.h - common defines for PPC64 assembly parts
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */

/*
 * ABIv1 requires dot symbol while ABIv2 does not.
 */
#if defined(_CALL_ELF) && _CALL_ELF == 2
#define DOTSYM(a)	a
#else
#define GLUE(a,b)	a##b
#define DOTSYM(a)	GLUE(.,a)
#endif
