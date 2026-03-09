#include "userprog/slowdown.h"

#include "lib/debug.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"

/* Enabled? */
static bool slowdown_active = false;

/* Interrupt function called on every instruction. */
static void trap_interrupt(struct intr_frame *args UNUSED)
{
	/* Just calling the trap is slow enough. We could spin on a call to barrier () here
	 * if we need slower execution. */
}

/* Old syscall handler. */
intr_handler_func *syscall_handler;

/* Intercept the syscall handler. */
static void trap_syscall(struct intr_frame *args)
{
	slowdown_enable();
	syscall_handler(args);
	slowdown_disable();
}

/* Initialize the slowdown system for system calls in the system. */
void slowdown_init(void)
{
	slowdown_active = true;

	/* userprog/exception.c might have registered this interrupt already, take it over.
	 */
	intr_clear_int(0x01);

	/* Add our registration. */
	intr_register_int(0x01, 0, INTR_OFF, trap_interrupt, "Single-step trap");

	/* Install our custom hook for the syscall handler as well. */
	syscall_handler = intr_bypass_int(0x30, trap_syscall);
}

/* Enable slowdown for this thread. */
void slowdown_enable(void)
{
	if (!slowdown_active) {
		return;
	}

	/* Set the trap flag so that we get an interrupt on every instruction. Only
	 * applicable to the current thread. */
	asm volatile("pushfl; movl %%esp, %%eax; orl $0x0100, (%%eax); popfl;" : : : "eax");
}

/* Disable slowdown for this thread. */
void slowdown_disable(void)
{
	if (!slowdown_active) {
		return;
	}

	/* Set the trap flag so that we get an interrupt on every instruction. Only
	 * applicable to the current thread. */
	asm volatile("pushfl; movl %%esp, %%eax; andl $0xFFFFFEFF, (%%eax); popfl;" : : : "eax");
}

/* Check if slowdown is enabled for this thread. */
bool slowdown_enabled(void)
{
	int flags;
	asm volatile("pushfl; pop %0;" : "=g"(flags));
	return flags & 0x0100;
}
