/* ****************************************************************************
 *
 *                 Host runtime - heap of the SDK allocator
 *
 * ****************************************************************************
 * lib_malloc.c manages the RAM between the linker-script symbols
 * __malloc_start__ and __malloc_end__ (both aligned to 8 bytes). On the host
 * they bound a static 64 KiB region. i686 PE prefixes C symbols with an
 * underscore, hence ___malloc_start__ / ___malloc_end__.
 * (lib_malloc's malloc/free are __wrap_ versions; libc malloc is unaffected.)
 */

	.bss
	.balign 8
	.globl	___malloc_start__
___malloc_start__:
	.space	65536
	.globl	___malloc_end__
___malloc_end__:
