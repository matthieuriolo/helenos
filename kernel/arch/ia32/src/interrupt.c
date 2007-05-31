/*
 * Copyright (c) 2001-2004 Jakub Jermar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @addtogroup ia32interrupt
 * @{
 */
/** @file
 */

#include <arch/interrupt.h>
#include <syscall/syscall.h>
#include <print.h>
#include <debug.h>
#include <panic.h>
#include <arch/drivers/i8259.h>
#include <func.h>
#include <cpu.h>
#include <arch/asm.h>
#include <mm/tlb.h>
#include <mm/as.h>
#include <arch.h>
#include <symtab.h>
#include <proc/thread.h>
#include <proc/task.h>
#include <synch/spinlock.h>
#include <arch/ddi/ddi.h>
#include <ipc/sysipc.h>
#include <interrupt.h>
#include <ddi/irq.h>

/*
 * Interrupt and exception dispatching.
 */

void (* disable_irqs_function)(uint16_t irqmask) = NULL;
void (* enable_irqs_function)(uint16_t irqmask) = NULL;
void (* eoi_function)(void) = NULL;

void decode_istate(istate_t *istate)
{
	char *symbol = get_symtab_entry(istate->eip);

	if (!symbol)
		symbol = "";

	if (CPU)
		printf("----------------EXCEPTION OCCURED (cpu%u)----------------\n", CPU->id);
	else
		printf("----------------EXCEPTION OCCURED----------------\n");
		
	printf("%%eip: %#lx (%s)\n", istate->eip, symbol);
	printf("ERROR_WORD=%#lx\n", istate->error_word);
	printf("%%cs=%#lx,flags=%#lx\n", istate->cs, istate->eflags);
	printf("%%eax=%#lx, %%ecx=%#lx, %%edx=%#lx, %%esp=%p\n", istate->eax, istate->ecx, istate->edx, &istate->stack[0]);
#ifdef CONFIG_DEBUG_ALLREGS
	printf("%%esi=%#lx, %%edi=%#lx, %%ebp=%#lx, %%ebx=%#lx\n", istate->esi, istate->edi, istate->ebp, istate->ebx);
#endif
	printf("stack: %#lx, %#lx, %#lx, %#lx\n", istate->stack[0], istate->stack[1], istate->stack[2], istate->stack[3]);
	printf("       %#lx, %#lx, %#lx, %#lx\n", istate->stack[4], istate->stack[5], istate->stack[6], istate->stack[7]);
}

static void trap_virtual_eoi(void)
{
	if (eoi_function)
		eoi_function();
	else
		panic("no eoi_function\n");

}

static void null_interrupt(int n, istate_t *istate)
{
	fault_if_from_uspace(istate, "unserviced interrupt: %d", n);

	decode_istate(istate);
	panic("unserviced interrupt: %d\n", n);
}

/** General Protection Fault. */
static void gp_fault(int n __attribute__((unused)), istate_t *istate)
{
	if (TASK) {
		count_t ver;
		
		spinlock_lock(&TASK->lock);
		ver = TASK->arch.iomapver;
		spinlock_unlock(&TASK->lock);
	
		if (CPU->arch.iomapver_copy != ver) {
			/*
			 * This fault can be caused by an early access
			 * to I/O port because of an out-dated
			 * I/O Permission bitmap installed on CPU.
			 * Install the fresh copy and restart
			 * the instruction.
			 */
			io_perm_bitmap_install();
			return;
		}
		fault_if_from_uspace(istate, "general protection fault");
	}

	decode_istate(istate);
	panic("general protection fault\n");
}

static void ss_fault(int n __attribute__((unused)), istate_t *istate)
{
	fault_if_from_uspace(istate, "stack fault");

	decode_istate(istate);
	panic("stack fault\n");
}

static void simd_fp_exception(int n __attribute__((unused)), istate_t *istate)
{
	uint32_t mxcsr;
	asm (
		"stmxcsr %0;\n"
		: "=m" (mxcsr)
	);
	fault_if_from_uspace(istate, "SIMD FP exception(19), MXCSR: %#zx",
	    (unative_t) mxcsr);

	decode_istate(istate);
	printf("MXCSR: %#lx\n", mxcsr);
	panic("SIMD FP exception(19)\n");
}

static void nm_fault(int n __attribute__((unused)), istate_t *istate __attribute__((unused)))
{
#ifdef CONFIG_FPU_LAZY     
	scheduler_fpu_lazy_request();
#else
	fault_if_from_uspace(istate, "fpu fault");
	panic("fpu fault");
#endif
}

#ifdef CONFIG_SMP
static void tlb_shootdown_ipi(int n __attribute__((unused)), istate_t *istate __attribute__((unused)))
{
	trap_virtual_eoi();
	tlb_shootdown_ipi_recv();
}
#endif

/** Handler of IRQ exceptions */
static void irq_interrupt(int n, istate_t *istate __attribute__((unused)))
{
	ASSERT(n >= IVT_IRQBASE);
	
	int inum = n - IVT_IRQBASE;
	bool ack = false;
	ASSERT(inum < IRQ_COUNT);
	ASSERT((inum != IRQ_PIC_SPUR) && (inum != IRQ_PIC1));
	
	irq_t *irq = irq_dispatch_and_lock(inum);
	if (irq) {
		/*
		 * The IRQ handler was found.
		 */
		 
		if (irq->preack) {
			/* Send EOI before processing the interrupt */
			trap_virtual_eoi();
			ack = true;
		}
		irq->handler(irq, irq->arg);
		spinlock_unlock(&irq->lock);
	} else {
		/*
		 * Spurious interrupt.
		 */
#ifdef CONFIG_DEBUG
		printf("cpu%u: spurious interrupt (inum=%d)\n", CPU->id, inum);
#endif
	}
	
	if (!ack)
		trap_virtual_eoi();
}

void interrupt_init(void)
{
	int i;
	
	for (i = 0; i < IVT_ITEMS; i++)
		exc_register(i, "null", (iroutine) null_interrupt);
	
	for (i = 0; i < IRQ_COUNT; i++) {
		if ((i != IRQ_PIC_SPUR) && (i != IRQ_PIC1))
			exc_register(IVT_IRQBASE + i, "irq", (iroutine) irq_interrupt);
	}
	
	exc_register(7, "nm_fault", (iroutine) nm_fault);
	exc_register(12, "ss_fault", (iroutine) ss_fault);
	exc_register(13, "gp_fault", (iroutine) gp_fault);
	exc_register(19, "simd_fp", (iroutine) simd_fp_exception);
	
#ifdef CONFIG_SMP
	exc_register(VECTOR_TLB_SHOOTDOWN_IPI, "tlb_shootdown", (iroutine) tlb_shootdown_ipi);
#endif
}

void trap_virtual_enable_irqs(uint16_t irqmask)
{
	if (enable_irqs_function)
		enable_irqs_function(irqmask);
	else
		panic("no enable_irqs_function\n");
}

void trap_virtual_disable_irqs(uint16_t irqmask)
{
	if (disable_irqs_function)
		disable_irqs_function(irqmask);
	else
		panic("no disable_irqs_function\n");
}

/** @}
 */
