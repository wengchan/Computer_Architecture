	.file	1 "mbq1.c"

 # GNU C 2.7.2.3 [AL 1.1, MM 40, tma 0.1] SimpleScalar running sstrix compiled by GNU C

 # Cc1 defaults:
 # -mgas -mgpOPT

 # Cc1 arguments (-G value = 8, Cpu = default, ISA = 1):
 # -quiet -dumpbase -O0 -o

gcc2_compiled.:
__gnu_compiled_c:
	.sdata
	.align	2
$LC0:
	.ascii	"%d\n\000"
	.text
	.align	2
	.globl	main

	.text

	.loc	1 5
	.ent	main
main:
	.frame	$fp,56,$31		# vars= 8, regs= 8/0, args= 16, extra= 0
	.mask	0xc03f0000,-4
	.fmask	0x00000000,0
	subu	$sp,$sp,56
	sw	$31,52($sp)
	sw	$fp,48($sp)
	sw	$21,44($sp)
	sw	$20,40($sp)
	sw	$19,36($sp)
	sw	$18,32($sp)
	sw	$17,28($sp)
	sw	$16,24($sp)
	move	$fp,$sp
	jal	__main
	li	$16,0x00000001		# 1
	li	$17,0x00000002		# 2
	li	$18,0x00000003		# 3
	li	$19,0x00000004		# 4
	li	$20,0x00000001		# 1
	move	$21,$0
	sw	$0,16($fp)
	li	$2,0x00989680		# 10000000
	sw	$2,20($fp)
$L2:
	bne	$20,$0,$L4
	j	$L3
$L4:
	addu	$16,$17,$18
	addu	$17,$16,$19
	addu	$18,$17,$19
	addu	$19,$18,$16
	addu	$16,$17,$18
	addu	$18,$18,$19
	addu	$19,$16,$17
	lw	$3,16($fp)
	addu	$2,$3,1
	move	$3,$2
	sw	$3,16($fp)
	move	$21,$0
	move	$21,$0
	lw	$2,16($fp)
	lw	$3,20($fp)
	slt	$20,$2,$3
	move	$21,$0
	move	$21,$0
	j	$L2
$L3:
	addu	$2,$16,$17
	addu	$3,$2,$18
	addu	$2,$3,$19
	la	$4,$LC0
	move	$5,$2
	jal	printf
	move	$2,$0
	j	$L1
$L1:
	move	$sp,$fp			# sp not trusted here
	lw	$31,52($sp)
	lw	$fp,48($sp)
	lw	$21,44($sp)
	lw	$20,40($sp)
	lw	$19,36($sp)
	lw	$18,32($sp)
	lw	$17,28($sp)
	lw	$16,24($sp)
	addu	$sp,$sp,56
	j	$31
	.end	main
