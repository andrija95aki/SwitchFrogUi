/*
 * MIPS o32 five-, six-, and seven-argument syscall wrappers.
 *
 * Zig's glibc 2.19 cross-link includes libc_nonshared objects that reference
 * these hidden glibc helpers but omits their assembly objects. The wrappers
 * follow the GNU C Library implementation: the syscall number is the final
 * stack argument, arguments 5+ are already in the Linux o32 stack slots, and
 * a3 is returned in v1 as the error indicator.
 */

	.text
	.set	noreorder
	.set	nomips16

	.globl	__mips_syscall5
	.hidden	__mips_syscall5
	.ent	__mips_syscall5
__mips_syscall5:
	lw	$2, 20($29)
	syscall
	move	$3, $7
	jr	$31
	nop
	.end	__mips_syscall5

	.globl	__mips_syscall6
	.hidden	__mips_syscall6
	.ent	__mips_syscall6
__mips_syscall6:
	lw	$2, 24($29)
	syscall
	move	$3, $7
	jr	$31
	nop
	.end	__mips_syscall6

	.globl	__mips_syscall7
	.hidden	__mips_syscall7
	.ent	__mips_syscall7
__mips_syscall7:
	lw	$2, 28($29)
	syscall
	move	$3, $7
	jr	$31
	nop
	.end	__mips_syscall7
