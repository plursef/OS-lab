/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 3: Your code here.
	user_mem_assert(curenv, s, len, PTE_U | PTE_P);
	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	env_destroy(e);
	return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.
	struct Env* env;
	int errno;
	// call env_alloc to create a blank env with kernel's page mappings
	if ((errno = env_alloc(&env, curenv->env_id)) < 0) {
		cprintf("[%08x]sys_exofork failed: %e\n", curenv->env_id, errno);
		return errno;
	}
	// status is set to ENV_NOT_RUNNABLE
	// and register set is copied from the current enviroment
	env->env_status = ENV_NOT_RUNNABLE;
	env->env_tf = curenv->env_tf;
	// but tweaked with return value 0
	env->env_tf.tf_regs.reg_eax = 0;

	return env->env_id;

	// LAB 4: Your code here.
	// panic("sys_exofork not implemented");
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.
	struct Env* env = NULL;
	int errno;
	// ensure valid env
	if ((errno = envid2env(envid, &env, 1)) < 0) {
		cprintf("environment envid %08x doesn't currently exist\n", envid);
		return errno;
	}
	if (status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE) {
		cprintf("[%08x]sys_env_set_status: can't set invalid status(%d)\n", envid, status);
		return -E_INVAL;
	}
	// set enviroment status and return 0 on successfully complete
	env->env_status = status;
	return 0;

	// LAB 4: Your code here.
	panic("sys_env_set_status not implemented");
}

// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3), interrupts enabled, and IOPL of 0.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	// LAB 5: Your code here.
	// Remember to check whether the user has supplied us with a good
	// address!
	struct Env* env = NULL;
	int errno;
	// ensure valid env
	if ((errno = envid2env(envid, &env, 1)) < 0) {
		cprintf("environment envid %08x doesn't currently exist\n", envid);
		return errno;
	}
	// check if tf is a valid user space address
	user_mem_assert(env, tf, sizeof(struct Trapframe), PTE_U | PTE_P);
	// set enviroment trapframe and return 0 on successfully complete
	env->env_tf = *tf;
	// ensure that user environments always run at code
	env->env_tf.tf_cs |= 3;
	env->env_tf.tf_eflags |= FL_IF;
	env->env_tf.tf_eflags &= ~FL_IOPL_MASK;
	return 0;
	
	// panic("sys_env_set_trapframe not implemented");
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
	struct Env* env = NULL;
	int errno;
	// ensure valid env
	// check permission by setting third argument to 1, as it's 
	// a dangerous call
	if ((errno = envid2env(envid, &env, 1)) < 0) {
		cprintf("environment envid %08x doesn't currently exist\n", envid);
		return errno;
	}
	// set enviroment pgfault upcall and return 0 on successfully complete
	env->env_pgfault_upcall = func;
	return 0;
	// panic("sys_env_set_pgfault_upcall not implemented");
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!

	struct Env* env = NULL;
	int errno;
	// ensure valid env
	if ((errno = envid2env(envid, &env, 1)) < 0) {
		cprintf("environment envid %08x doesn't currently exist\n", envid);
		return errno;
	}
	// ensure valid va
	if ((uintptr_t)va >= UTOP || ((uintptr_t)va & (PGSIZE - 1))) {
		cprintf("[%08x]sys_page_alloc: can't alloc at invalid virtual address(%p)\n", envid, va);
		return -E_INVAL;
	}
	// ensure valid perm
	if (perm & ~PTE_SYSCALL || ((perm & (PTE_U|PTE_P)) != (PTE_U|PTE_P))) {
		cprintf("[%08x]sys_page_alloc: can't alloc with invalid perm, which violates permission restrictions, PTE_P | PTE_U must be covered, and only 4 bits could be set(PTE_AVAIL | PTE_P | PTE_W | PTE_U)\n", envid, va);
		return -E_INVAL;

	}
	// Now we can try alloc
	struct PageInfo *pp = page_alloc(ALLOC_ZERO);
	// if no enough memory
	if (!pp) {
		cprintf("[%08x]sys_page_alloc: no enough memory for alloc\n", envid);
		return -E_NO_MEM;
	}
	// insert this pp into 'va'
	if ((errno = page_insert(env->env_pgdir, pp, va, perm | PTE_P | PTE_U)) < 0) {
		cprintf("[%08x]sys_page_alloc: no enough memory for alloc page table\n", envid);
		// pp->pp_ref is always 0 because page_alloc doesn't add it
		page_free(pp);
		return -E_NO_MEM;
	}
	// We have done necessary works, just return 0 to report a good day
	return 0;
	// LAB 4: Your code here.
	// comment this panic for my confidence
	// panic("sys_page_alloc not implemented");
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// ensure valid perm
	if ( (perm & (PTE_U|PTE_P))  !=  (PTE_U|PTE_P)  ) return -E_INVAL;
	if ( (perm | PTE_SYSCALL) != PTE_SYSCALL  ) return -E_INVAL;
	// ensure valid va
	if ((uintptr_t)srcva >= UTOP || PGOFF(srcva) != 0) return -E_INVAL;
	if ((uintptr_t)dstva >= UTOP || PGOFF(dstva) != 0) return -E_INVAL;

	struct Env *srcenv, *dstenv;
	// ensure valid env
	if (envid2env(srcenvid, &srcenv, 1)<0 || envid2env(dstenvid, &dstenv, 1)<0) return -E_BAD_ENV;

	pte_t *src_pte;  
	struct PageInfo *pp = page_lookup(srcenv->env_pgdir, srcva, &src_pte);
	// if srcva is not mapped in srcenvid's address space, return -E_INVAL
	if (pp==NULL) return -E_INVAL;
	// -E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's address space.
	if (((*src_pte & PTE_W) == 0) && ((perm & PTE_W) == PTE_W )) return -E_INVAL;
	int errno = page_insert(dstenv->env_pgdir, pp, dstva, perm);
	// if no memory
	if(errno < 0) return errno;
	return 0;
	// LAB 4: Your code here.
	// panic("sys_page_map not implemented");
}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().
	struct Env* env = NULL;
	int errno;
	// ensure valid env
	if ((errno = envid2env(envid, &env, 1)) < 0) {
		cprintf("environment envid %08x doesn't currently exist\n", envid);
		return errno;
	}

	// ensure valid va
	if ((uintptr_t)va >= UTOP || ((uintptr_t)va & (PGSIZE - 1))) {
		cprintf("[%08x]sys_page_alloc: can't alloc at invalid virtual address(%p)\n", envid, va);
		return -E_INVAL;
	}
	page_remove(env->env_pgdir, va);
	// return 0 on success
	return 0;
	// LAB 4: Your code here.
	// panic("sys_page_unmap not implemented");
}

// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// Returns 0 on success, < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in the
//		current environment's address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
	// LAB 4: Your code here.
	struct Env* dstenv = NULL;
	int errno;
	// ensure valid env
	if ((errno = envid2env(envid, &dstenv, 0)) < 0) {
		// cprintf("environment envid %08x doesn't currently exist\n", envid);
		return errno;
	}
	// ensure dstenv is currently blocked in sys_ipc_recv
	if (!dstenv->env_ipc_recving) {
		// cprintf("[%08x]sys_ipc_try_send: target env %08x is not currently blocked in sys_ipc_recv\n", curenv->env_id, envid);
		return -E_IPC_NOT_RECV;
	}
	bool send_page = false;
	dstenv->env_ipc_perm = 0;
	// if sender wants to send a page
	if ((uintptr_t)srcva < UTOP) {
		// ensure valid va
		if (PGOFF(srcva)) {
			// cprintf("[%08x]sys_ipc_try_send: can't send page at invalid virtual address(%p)\n", curenv->env_id, srcva);
			return -E_INVAL;
		}
		// ensure valid perm
		if ((perm & (PTE_U|PTE_P)) != (PTE_U|PTE_P)) {
			// cprintf("[%08x]sys_ipc_try_send: can't send page with invalid perm\n", curenv->env_id);
			return -E_INVAL;
		}
		if (perm & ~PTE_SYSCALL) {
			// cprintf("[%08x]sys_ipc_try_send: can't send page with invalid perm\n", curenv->env_id);
			return -E_INVAL;
		}
		// ensure srcva is mapped in current enviroment's address space
		pte_t *src_pte;
		struct PageInfo *pp = page_lookup(curenv->env_pgdir, srcva, &src_pte);
		if (pp == NULL) {
			// cprintf("[%08x]sys_ipc_try_send: can't send page at unmapped virtual address(%p)\n", curenv->env_id, srcva);
			return -E_INVAL;
		}
		// ensure write permission
		if ((perm & PTE_W) && !(*src_pte & PTE_W)) {
			// cprintf("[%08x]sys_ipc_try_send: can't send page with invalid perm\n", curenv->env_id);
			return -E_INVAL;
		}
		// if receiver want to recv page
		if ((uintptr_t)dstenv->env_ipc_dstva < UTOP) {
			// try to map this page into dstenv's address space at dstenv->env_ipc_dstva
			if ((errno = page_insert(dstenv->env_pgdir, pp, dstenv->env_ipc_dstva, perm)) < 0) {
				// cprintf("[%08x]sys_ipc_try_send: no enough memory to map page into target env %08x\n", curenv->env_id, envid);
				return errno;
			}
			// indicate that we have sent a page
			send_page = true;
		}
			
	}
	// then just set ipc fields of dstenv
	dstenv->env_ipc_recving = false;
	dstenv->env_ipc_from = curenv->env_id;
	dstenv->env_ipc_value = value;
	dstenv->env_ipc_perm = send_page ? perm : 0;
	// mark dstenv runnable again
	dstenv->env_status = ENV_RUNNABLE;
	dstenv->env_tf.tf_regs.reg_eax = 0; // return 0
	return 0;
	// panic("sys_ipc_try_send not implemented");
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
	// LAB 4: Your code here.
	if ((uintptr_t)dstva < UTOP && ((uintptr_t)dstva & (PGSIZE - 1))) {
		// cprintf("[%08x]sys_ipc_recv: can't recv at invalid virtual address(%p)\n", curenv->env_id, dstva);
		return -E_INVAL;
	}
	// set env_ipc_recving and env_ipc_dstva fields of struct Env
	curenv->env_ipc_recving = true;
	curenv->env_ipc_dstva = dstva;
	// mark yourself not runnable
	curenv->env_status = ENV_NOT_RUNNABLE;
	// then give up the CPU
	sched_yield();
	// sched_yield not return, but we return 0 to satisfy the compiler and my mind
	return 0;
	// panic("sys_ipc_recv not implemented");
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.

	// panic("syscall not implemented");

	switch (syscallno) {
	case SYS_cputs:
		sys_cputs((const char *)a1, (size_t)a2);
		return 0;
	case SYS_cgetc:
		return sys_cgetc();
	case SYS_getenvid:
		return sys_getenvid();
	case SYS_env_destroy:
		return sys_env_destroy((envid_t)a1);
	case SYS_page_alloc:
		return sys_page_alloc((envid_t)a1, (void *)a2, (int)a3);
	case SYS_page_map:
		return sys_page_map((envid_t)a1, (void *)a2, (envid_t)a3, (void *)a4, (int)a5);
	case SYS_page_unmap:
		return sys_page_unmap((envid_t)a1, (void *)a2);
	case SYS_exofork:
		return sys_exofork();
	case SYS_env_set_status:
		return sys_env_set_status((envid_t)a1, (int)a2);
	case SYS_env_set_trapframe:
		return sys_env_set_trapframe((envid_t)a1, (struct Trapframe *)a2);
	case SYS_env_set_pgfault_upcall:
		return sys_env_set_pgfault_upcall((envid_t)a1, (void *)a2);
	case SYS_ipc_try_send:
		return sys_ipc_try_send((envid_t)a1, (uint32_t)a2, (void *)a3, (unsigned)a4);
	case SYS_ipc_recv:
		return sys_ipc_recv((void *)a1);
	case SYS_yield:
		sys_yield(); return 0; // sys_yield never returns, but we return 0 to satisfy the compiler and my mind
	default:
		return -E_INVAL;
	}
}

