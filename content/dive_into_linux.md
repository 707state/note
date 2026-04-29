---
title: 从Linux 2.6搞明白Linux
author: jask
date: 2026-01-23
tags:
  - Linux
  - OS
series: Linux自我修养
---

# 启动

## startup

Linux 的启动流程在不同架构上不太一样，这里以 i386 为例。

计算机开机时，BIOS / UEFI 会把引导程序（比如 GRUB）加载到内存，GRUB 再加载 Linux 内核。在 i386 中，内核被加载后，第一个代码入口是 `arch/x86/kernel/head_32.S` 里的 `startup_32`。

```c
__HEAD
ENTRY(startup_32)
	cld
	/*
	 * Test KEEP_SEGMENTS flag to see if the bootloader is asking
	 * us to not reload segments
	 */
	testb	$(1<<6), BP_loadflags(%esi)
	jnz	1f

	cli
	movl	$__BOOT_DS, %eax
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %fs
	movl	%eax, %gs
	movl	%eax, %ss
	...
```

在 `startup` 阶段，要完成从实模式到保护模式的过渡，还需要进行早期硬件初始化，包括重新加载段寄存器、复制引导参数、初始化页表、设置虚拟内存映射、启用分页并跳转到内核代码的起始位置。最终会跳转到 `i386_start_kernel`，接下来再完成后续的内核初始化。

## start_kernel

在 `i386_start_kernel` 中，会根据具体的 `subarch` 选择对应的 `early_setup` 实现。

```c
/* Call the subarch specific early setup function */
switch (boot_params.hdr.hardware_subarch) {
case X86_SUBARCH_MRST:
	x86_mrst_early_setup();
	break;
default:
	i386_default_early_setup();
	break;
}

/*
 * At this point everything still needed from the boot loader
 * or BIOS or kernel text should be early reserved or marked not
 * RAM in e820. All other memory is free game.
 */

start_kernel();
```

在这段函数开头，会预留足够的内存，确保初始化期间的一些关键内存区域不会被覆盖。内核会根据引导参数里的 `hardware_subarch` 字段，执行特定硬件平台的初始化函数。

在此之后，所有必要的内存区域都被标记为预留或者不可用。内核已经完成了关键内存的预留，代码段、数据段、BSS 段以及必要的 RAM 都被保护起来，引导程序和 BIOS 相关内存也被标记为不可用，从而避免干扰。

`start_kernel` 负责执行大部分早期初始化工作，让内核进入可运行状态。这里面会执行大量 `xxx_init`，比如 `tick_init`、`boot_cpu_init` 等。

`start_kernel` 里最重要的几个函数是：

1. `setup_arch`

这一个函数负责把引导程序、BIOS、EFI 提供的原始信息转成内核可用的结构。系统物理内存的计算（`max_low_pfn_mapped`、`max_pfn_mapped`）、ACPI 解析、NUMA 节点拓扑发现，以及 APIC / IOAPIC 映射和 IRQ 数量探测，都是在这里完成的。

2. `build_all_zonelists` 和 `page_alloc_init`

在这两个函数之前，内存分配只能使用 `bootmem`。前者会对内存的物理区域进行划分和分组，后者则为多核系统的内存分配通知系统做准备。

```c
void __init page_alloc_init(void)
{
	hotcpu_notifier(page_alloc_cpu_notify, 0);
}

static int page_alloc_cpu_notify(struct notifier_block *self,
				 unsigned long action, void *hcpu)
{
	int cpu = (unsigned long)hcpu;

	if (action == CPU_DEAD || action == CPU_DEAD_FROZEN) {
		drain_pages(cpu);

		/*
		 * Spill the event counters of the dead processor
		 * into the current processors event counters.
		 * This artificially elevates the count of the current
		 * processor.
		 */
		vm_events_fold_cpu(cpu);

		/*
		 * Zero the differential counters of the dead processor
		 * so that the vm statistics are consistent.
		 *
		 * This is only okay since the processor is dead and cannot
		 * race with what we are doing.
		 */
		refresh_cpu_vm_stats(cpu);
	}
	return NOTIFY_OK;
}
```

可以看到，这里注册的回调函数会在 CPU 状态改变时触发。

3. `trap_init` 和 `init_IRQ`

`trap_init` 会为特殊的中断号设置对应的中断服务例程。

```c
void __init trap_init(void)
{
	int i;

	set_intr_gate(0, &divide_error);
	set_intr_gate_ist(1, &debug, DEBUG_STACK);
	set_intr_gate_ist(2, &nmi, NMI_STACK);
	/* int3 can be called from all */
	set_system_intr_gate_ist(3, &int3, DEBUG_STACK);
	/* int4 can be called from all */
	set_system_intr_gate(4, &overflow);
	set_intr_gate(5, &bounds);
	set_intr_gate(6, &invalid_op);
	set_intr_gate(7, &device_not_available);
	set_intr_gate_ist(8, &double_fault, DOUBLEFAULT_STACK);
	set_intr_gate(9, &coprocessor_segment_overrun);
	set_intr_gate(10, &invalid_TSS);
	set_intr_gate(11, &segment_not_present);
	set_intr_gate_ist(12, &stack_segment, STACKFAULT_STACK);
	set_intr_gate(13, &general_protection);
	set_intr_gate(14, &page_fault);
	set_intr_gate(15, &spurious_interrupt_bug);
	set_intr_gate(16, &coprocessor_error);
	set_intr_gate(17, &alignment_check);
	set_intr_gate(19, &simd_coprocessor_error);
	/* Reserve all the builtin and the syscall vector: */
	for (i = 0; i < FIRST_EXTERNAL_VECTOR; i++)
		set_bit(i, used_vectors);
	/*
	 * Should be a barrier for any external CPU state:
	 */
	cpu_init();
	x86_init.irqs.trap_init();
}
```

在 i386 下，这些例程都定义在 `entry_32.S` 中。

`init_IRQ` 会完成 IRQ 向量的分配和设置，并完成 APIC / IOAPIC 相关初始化。

4. `sched_init`

`sched_init` 要把内核调度器初始化起来，包括调度队列、调度策略、任务组和运行队列。

- 初始化公平调度（CFS）和实时调度（RT）的相关数据结构。
- 配置 CPU 负载、调度器亲和性以及各 CPU 的调度队列（`rq`）。
- 为系统中的每个 CPU 设置默认调度策略和任务负载。
- 配置内核空闲线程、任务组、调度类和性能监控。

5. `rcu_init`

Read-Copy-Update 是 Linux 内核中处理同步的重要机制。

```c
void __init rcu_init(void)
{
	int cpu;

	rcu_bootup_announce();
	RCU_INIT_FLAVOR(&rcu_sched_state, rcu_sched_data);
	RCU_INIT_FLAVOR(&rcu_bh_state, rcu_bh_data);
	__rcu_init_preempt();
	open_softirq(RCU_SOFTIRQ, rcu_process_callbacks);
	/*
	 * We don't need protection against CPU-hotplug here because
	 * this is called early in boot, before either interrupts
	 * or the scheduler are operational.
	 */
	cpu_notifier(rcu_cpu_notify, 0);
	for_each_online_cpu(cpu)
		rcu_cpu_notify(NULL, CPU_UP_PREPARE, (void *)(long)cpu);
}
```

`open_softirq` 为内核注册了软中断，`cpu_notifier` 会在 CPU 状态变更时通知 RCU 进行必要处理。

6. `rest_init`

`rest_init` 里面最重要的部分是 `kernel_thread(kernel_init)`。这个函数会启动内核线程并执行系统初始化任务。在 `kernel_init` 最后会执行 `init_post`，其中再调用 `run_init_process`，通过 `kernel_execve` 把 `init` 进程跑起来。

```c
static void run_init_process(char *init_filename)
{
	argv_init[0] = init_filename;
	kernel_execve(init_filename, argv_init, envp_init);
}
```

在 2.6 内核中，除了 `init` 进程是内核启动的，还有 `kthreadd` 这个 PID 为 2 的任务也是内核启动的，主要负责管理待创建的内核线程。

# 进程 / 线程

Linux 创建一个新进程的方式是 `fork + exec`。尽管 POSIX 对各个系统的原语做了封装，但 Unix-like 系统基本上都不提供直接的 `spawn` 操作。

Linux 采用 Copy-On-Write 的方式实现 `fork`。`fork` 本身的开销主要是复制父进程的 page tables 和创建一个唯一的 child PID，而不包括立刻复制物理内存页。在 `fork` 之后、子进程真正修改内存之前，父子进程的 page table 都指向同一批物理页。只有当子进程修改内存时，才会发生物理页复制。

Unix 鼓励快速创建进程，也就是配合使用 `fork + exec`。`exec` 会丢弃当前进程的整个用户态空间，加载 ELF 并建立新的 page tables，因此根本没有必要预先复制父进程的物理页。所以 Linux 的 COW 减少了 `fork + exec` 时不必要的内存开销。

Linux 上 `fork` 是 `copy` 的一个特例，`vfork` 则是 `copy` 的另一种特例。当然三者最后都是调用内核的 `do_fork`。

## do_fork

```c
/*
 *  Ok, this is the main fork-routine.
 *
 * It copies the process, and if successful kick-starts
 * it and waits for it to finish using the VM if required.
 */
long do_fork(unsigned long clone_flags,
	      unsigned long stack_start,
	      struct pt_regs *regs,
	      unsigned long stack_size,
	      int __user *parent_tidptr,
	      int __user *child_tidptr)
{
	struct task_struct *p;
	int trace = 0;
	long nr;

	/*
	 * Do some preliminary argument and permissions checking before we
	 * actually start allocating stuff
	 */
	if (clone_flags & CLONE_NEWUSER) {
		if (clone_flags & CLONE_THREAD)
			return -EINVAL;
		/* hopefully this check will go away when userns support is
		 * complete
		 */
		if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SETUID) ||
				!capable(CAP_SETGID))
			return -EPERM;
	}

	/*
	 * We hope to recycle these flags after 2.6.26
	 */
	if (unlikely(clone_flags & CLONE_STOPPED)) {
		static int __read_mostly count = 100;

		if (count > 0 && printk_ratelimit()) {
			char comm[TASK_COMM_LEN];

			count--;
			printk(KERN_INFO "fork(): process `%s' used deprecated "
					"clone flags 0x%lx\n",
				get_task_comm(comm, current),
				clone_flags & CLONE_STOPPED);
		}
	}

	/*
	 * When called from kernel_thread, don't do user tracing stuff.
	 */
	if (likely(user_mode(regs)))
		trace = tracehook_prepare_clone(clone_flags);

	p = copy_process(clone_flags, stack_start, regs, stack_size,
			 child_tidptr, NULL, trace);
	/*
	 * Do this prior waking up the new thread - the thread pointer
	 * might get invalid after that point, if the thread exits quickly.
	 */
	if (!IS_ERR(p)) {
		struct completion vfork;

		trace_sched_process_fork(current, p);

		nr = task_pid_vnr(p);

		if (clone_flags & CLONE_PARENT_SETTID)
			put_user(nr, parent_tidptr);

		if (clone_flags & CLONE_VFORK) {
			p->vfork_done = &vfork;
			init_completion(&vfork);
		}

		audit_finish_fork(p);
		tracehook_report_clone(regs, clone_flags, nr, p);

		/*
		 * We set PF_STARTING at creation in case tracing wants to
		 * use this to distinguish a fully live task from one that
		 * hasn't gotten to tracehook_report_clone() yet.  Now we
		 * clear it and set the child going.
		 */
		p->flags &= ~PF_STARTING;

		if (unlikely(clone_flags & CLONE_STOPPED)) {
			/*
			 * We'll start up with an immediate SIGSTOP.
			 */
			sigaddset(&p->pending.signal, SIGSTOP);
			set_tsk_thread_flag(p, TIF_SIGPENDING);
			__set_task_state(p, TASK_STOPPED);
		} else {
			wake_up_new_task(p, clone_flags);
		}

		tracehook_report_clone_complete(trace, regs,
						clone_flags, nr, p);

		if (clone_flags & CLONE_VFORK) {
			freezer_do_not_count();
			wait_for_completion(&vfork);
			freezer_count();
			tracehook_report_vfork_done(p, nr);
		}
	} else {
		nr = PTR_ERR(p);
	}
	return nr;
}
```

这里面最重要的部分是 `copy_process`。

```c
static struct task_struct *copy_process(unsigned long clone_flags,
					unsigned long stack_start,
					struct pt_regs *regs,
					unsigned long stack_size,
					int __user *child_tidptr,
					struct pid *pid,
					int trace)
{
	...
	ret = dup_task_struct(current); // 创建新的内核栈、thread_info、task_struct，此时父子进程 pid 还没有区别
	...
	// 检查新进程是否会超出资源限制
	if (atomic_read(&p->real_cred->user->processes) >=
			task_rlimit(p, RLIMIT_NPROC)) {
		if (!capable(CAP_SYS_ADMIN) && !capable(CAP_SYS_RESOURCE) &&
		    p->real_cred->user != INIT_USER)
			goto bad_fork_free;
	}
	...
	// 还在检查
	retval = -EAGAIN;
	if (nr_threads >= max_threads)
		goto bad_fork_cleanup_count;

	if (!try_module_get(task_thread_info(p)->exec_domain->module))
		goto bad_fork_cleanup_count;
	...
	// 把子进程从父进程的按位拷贝快照里区分出来，也就是给子任务结构体赋值
	p->did_exec = 0;
	delayacct_tsk_init(p);  /* Must remain after dup_task_struct() */
	copy_flags(clone_flags, p);
	INIT_LIST_HEAD(&p->children);
	...
	init_sigpending(&p->pending);
	...
	p->utime = cputime_zero;
	p->stime = cputime_zero;
	...
	do_posix_clock_monotonic_gettime(&p->start_time);
	p->real_start_time = p->start_time;
	...
	task_io_accounting_init(&p->ioac);
	acct_clear_integrals(p);
	posix_cpu_timers_init(p);
	...
	sched_fork(p, clone_flags);
	...
	// 分配 pid
	if (pid != &init_struct_pid) {
		retval = -ENOMEM;
		pid = alloc_pid(p->nsproxy->pid_ns);
		if (!pid)
			goto bad_fork_cleanup_io;

		if (clone_flags & CLONE_NEWPID) {
			retval = pid_ns_prepare_proc(p->nsproxy->pid_ns);
			if (retval < 0)
				goto bad_fork_free_pid;
		}
	}

	p->pid = pid_nr(pid);
	p->tgid = p->pid;
	...
	// 根据 clone flags 决定资源是共享还是复制
	if ((retval = copy_semundo(clone_flags, p)))
		goto bad_fork_cleanup_audit;
	if ((retval = copy_files(clone_flags, p)))
		goto bad_fork_cleanup_semundo;
	if ((retval = copy_fs(clone_flags, p)))
		goto bad_fork_cleanup_files;
	if ((retval = copy_sighand(clone_flags, p)))
		goto bad_fork_cleanup_fs;
	if ((retval = copy_signal(clone_flags, p)))
		goto bad_fork_cleanup_sighand;
	if ((retval = copy_mm(clone_flags, p)))
		goto bad_fork_cleanup_signal;
	if ((retval = copy_namespaces(clone_flags, p)))
		goto bad_fork_cleanup_mm;
	if ((retval = copy_io(clone_flags, p)))
		goto bad_fork_cleanup_namespaces;
	...
	// 收尾，把新的 task 添加到系统、挂在各种链表 / 哈希表里，处理线程 / 进程组 / session 之间的关系
	return p;
}
```

`copy_process` 执行成功之后，`do_fork` 里父进程需要立即做 tracing 并获取 child PID，把子进程信息写回父进程的用户空间，还要通知 audit / tracing 子系统 clone 已经发生。根据 `clone_flags`，还要决定子任务是 `STOPPED` 还是 `RUNNABLE`（即 `wake_up_new_task`）。`vfork` 的逻辑稍微特殊一点，父进程要阻塞等待完成。

```c
p = copy_process(clone_flags, stack_start, regs, stack_size,
		 child_tidptr, NULL, trace);
/*
 * Do this prior waking up the new thread - the thread pointer
 * might get invalid after that point, if the thread exits quickly.
 */
if (!IS_ERR(p)) {
	struct completion vfork;

	trace_sched_process_fork(current, p);

	nr = task_pid_vnr(p);

	if (clone_flags & CLONE_PARENT_SETTID)
		put_user(nr, parent_tidptr);

	if (clone_flags & CLONE_VFORK) {
		p->vfork_done = &vfork;
		init_completion(&vfork);
	}

	audit_finish_fork(p);
	tracehook_report_clone(regs, clone_flags, nr, p);
```

有个常见说法是：`fork` 有两个返回值，一个在父进程返回，一个在子进程返回。

这个说法并不算准确。

先看父进程：

```c
p = copy_process(clone_flags, stack_start, regs, stack_size,
		 child_tidptr, NULL, trace);
/*
 * Do this prior waking up the new thread - the thread pointer
 * might get invalid after that point, if the thread exits quickly.
 */
if (!IS_ERR(p)) {
	struct completion vfork;

	trace_sched_process_fork(current, p);

	nr = task_pid_vnr(p);
	...
} else {
	nr = PTR_ERR(p);
}
return nr;
```

`task_pid_vnr` 会从父进程所在的 PID namespace 里获取子进程的 PID，成功时返回相应的 pid。这是第一个“返回值”。

那子进程的返回值是在哪里给的？

```c
int copy_thread(unsigned long clone_flags, unsigned long sp,
		unsigned long unused,
		struct task_struct *p, struct pt_regs *regs)
{
	int err;
	struct pt_regs *childregs;
	struct task_struct *me = current;

	childregs = ((struct pt_regs *)
			(THREAD_SIZE + task_stack_page(p))) - 1;
	*childregs = *regs;
	childregs->ax = 0;
	if (user_mode(regs))
		childregs->sp = sp;
	else
		childregs->sp = (unsigned long)childregs;
	p->thread.sp = (unsigned long)childregs;
	p->thread.sp0 = (unsigned long)(childregs + 1);
	p->thread.usersp = me->thread.usersp;
	set_tsk_thread_flag(p, TIF_FORK);
	p->thread.io_bitmap_ptr = NULL;
	savesegment(gs, p->thread.gsindex);
	p->thread.gs = p->thread.gsindex ? 0 : me->thread.gs;
	savesegment(fs, p->thread.fsindex);
	p->thread.fs = p->thread.fsindex ? 0 : me->thread.fs;
	savesegment(es, p->thread.es);
	savesegment(ds, p->thread.ds);
	err = -ENOMEM;
	memset(p->thread.ptrace_bps, 0, sizeof(p->thread.ptrace_bps));
	if (unlikely(test_tsk_thread_flag(me, TIF_IO_BITMAP))) {
		p->thread.io_bitmap_ptr = kmalloc(IO_BITMAP_BYTES, GFP_KERNEL);
		if (!p->thread.io_bitmap_ptr) {
			p->thread.io_bitmap_max = 0;
			return -ENOMEM;
		}
		memcpy(p->thread.io_bitmap_ptr, me->thread.io_bitmap_ptr,
				IO_BITMAP_BYTES);
		set_tsk_thread_flag(p, TIF_IO_BITMAP);
	}
	/*
	 * Set a new TLS for the child thread?
	 */
	if (clone_flags & CLONE_SETTLS) {
		err = do_arch_prctl(p, ARCH_SET_FS, childregs->r8);
		if (err)
			goto out;
	}
	clear_tsk_thread_flag(p, TIF_DS_AREA_MSR);
	p->thread.ds_ctx = NULL;
	clear_tsk_thread_flag(p, TIF_DEBUGCTLMSR);
	p->thread.debugctlmsr = 0;
	err = 0;
out:
	if (err && p->thread.io_bitmap_ptr) {
		kfree(p->thread.io_bitmap_ptr);
		p->thread.io_bitmap_max = 0;
	}
	return err;
}
```

子进程的返回发生在 `copy_thread` 里。在 `childregs = task_pt_regs(p)` 这一类逻辑中，会获取到子进程内核栈上的 `pt_regs`；接下来 `*childregs = *regs` 会复制父进程寄存器，而 `childregs->ax = 0` 则把子进程的返回值设置为 0。

在父进程执行完 `wake_up_new_task` 以后，子进程变成 runnable。调度器在某个时刻选择到这个子任务，然后 `context switch` 到子任务上，CPU 开始执行 `p->thread.ip` 指向的代码，最后通过 `ret_from_fork` 返回用户态。

```c
/*
 * A newly forked process directly context switches into this address.
 *
 * rdi: prev task we switched from
 */
ENTRY(ret_from_fork)
	DEFAULT_FRAME

	LOCK ; btr $TIF_FORK,TI_flags(%r8)

	push kernel_eflags(%rip)
	CFI_ADJUST_CFA_OFFSET 8
	popf					# reset kernel eflags
	CFI_ADJUST_CFA_OFFSET -8

	call schedule_tail			# rdi: 'prev' task parameter

	GET_THREAD_INFO(%rcx)

	RESTORE_REST

	testl $3, CS-ARGOFFSET(%rsp)		# from kernel_thread?
	je   int_ret_from_sys_call

	testl $_TIF_IA32, TI_flags(%rcx)	# 32-bit compat task needs IRET
	jnz  int_ret_from_sys_call

	RESTORE_TOP_OF_STACK %rdi, -ARGOFFSET
	jmp ret_from_sys_call			# go to the SYSRET fastpath

	CFI_ENDPROC
END(ret_from_fork)
```

这是 x86 上新创建的子进程 / 线程第一次被调度运行时的起点，由调度器切换到新任务时的 `thread.ip` 指向。

## vfork

`vfork` 相比 `fork`，最大的区别就是子进程不会拷贝 page table entries，而且会一直阻塞父进程，直到子进程执行完成。

## Threads

Linux 内核并不会特殊对待线程，底层都是一个 `task_struct`，基本上可以看作是共享部分资源的进程。

Linux 中有 Kernel Thread，提供了在内核中执行后台任务的能力，这些内核线程只会在内核态运行。

```c
/**
 * kthread_create - create a kthread.
 * @threadfn: the function to run until signal_pending(current).
 * @data: data ptr for @threadfn.
 * @namefmt: printf-style name for the thread.
 *
 * Description: This helper function creates and names a kernel
 * thread.  The thread will be stopped: use wake_up_process() to start
 * it.  See also kthread_run().
 *
 * When woken, the thread will run @threadfn() with @data as its
 * argument. @threadfn() can either call do_exit() directly if it is a
 * standalone thread for which noone will call kthread_stop(), or
 * return when 'kthread_should_stop()' is true (which means
 * kthread_stop() has been called).  The return value should be zero
 * or a negative error number; it will be passed to kthread_stop().
 *
 * Returns a task_struct or ERR_PTR(-ENOMEM).
 */
struct task_struct *kthread_create(int (*threadfn)(void *data),
				   void *data,
				   const char namefmt[],
				   ...)
{
	struct kthread_create_info create;

	create.threadfn = threadfn;
	create.data = data;
	init_completion(&create.done);

	spin_lock(&kthread_create_lock);
	list_add_tail(&create.list, &kthread_create_list);
	spin_unlock(&kthread_create_lock);

	wake_up_process(kthreadd_task);
	wait_for_completion(&create.done);

	if (!IS_ERR(create.result)) {
		struct sched_param param = { .sched_priority = 0 };
		va_list args;

		va_start(args, namefmt);
		vsnprintf(create.result->comm, sizeof(create.result->comm),
			  namefmt, args);
		va_end(args);
		/*
		 * root may have changed our (kthreadd's) priority or CPU mask.
		 * The kernel thread should not inherit these properties.
		 */
		sched_setscheduler_nocheck(create.result, SCHED_NORMAL, &param);
		set_cpus_allowed_ptr(create.result, cpu_all_mask);
	}
	return create.result;
}
```

这里会看到一个 `kthreadd_task`。这是由 `rest_init` 启动的第二个进程（PID 为 2）关联的 `task_struct`。为了添加一个 kernel thread 去执行任务，这里就需要唤醒 `kthreadd` 这个进程。

## exit

进程结束通常是自行退出，也就是主动调用 `exit`。即使不在 `main` 结尾显式写一个 `exit`，编译器和运行时也会在 `main` 返回后走到退出路径。

`exit` 的具体逻辑在 `do_exit` 中，大致流程如下：

```c
void do_exit(long code)
{
	struct task_struct *tsk = current;
	int group_dead;
	...
	// 设置 PF_EXITING
	exit_irq_thread();
	exit_signals(tsk);  /* sets PF_EXITING */
	...
	acct_update_integrals(tsk); // 统计信息
	...
	// 释放当前进程的 mm_struct
	tsk->exit_code = code;
	taskstats_exit(tsk, group_dead);
	exit_mm(tsk);
	...
	exit_sem(tsk);
	// 减少计数，计数归 0 时销毁对应资源
	exit_files(tsk);
	exit_fs(tsk);
	...
	exit_notify(tsk, group_dead); // 通知 parent process
	...
	exit_rcu();
	/* causes final put_task_struct in finish_task_switch(). */
	tsk->state = TASK_DEAD;
	schedule();
}
```

执行了 `do_exit` 之后，绝大部分资源都会被释放。如果这些资源只被当前任务使用，那么此时基本都已经进入回收流程；不过在 `do_exit` 之后，内核通常还会暂时保留 PID，因此进程会变成 zombie，并且不能再运行。只有在 `release_task` 被调用时，才会清理 PID。

```c
void release_task(struct task_struct * p)
{
	struct task_struct *leader;
	int zap_leader;
repeat:
	tracehook_prepare_release_task(p);
	/* don't need to get the RCU readlock here - the process is dead and
	 * can't be modifying its own credentials. But shut RCU-lockdep up */
	rcu_read_lock();
	atomic_dec(&__task_cred(p)->user->processes);
	rcu_read_unlock();

	proc_flush_task(p);

	write_lock_irq(&tasklist_lock);
	tracehook_finish_release_task(p);
	__exit_signal(p);

	/*
	 * If we are the last non-leader member of the thread
	 * group, and the leader is zombie, then notify the
	 * group leader's parent process. (if it wants notification.)
	 */
	zap_leader = 0;
	leader = p->group_leader;
	if (leader != p && thread_group_empty(leader) && leader->exit_state == EXIT_ZOMBIE) {
		BUG_ON(task_detached(leader));
		do_notify_parent(leader, leader->exit_signal);
		/*
		 * If we were the last child thread and the leader has
		 * exited already, and the leader's parent ignores SIGCHLD,
		 * then we are the one who should release the leader.
		 *
		 * do_notify_parent() will have marked it self-reaping in
		 * that case.
		 */
		zap_leader = task_detached(leader);

		/*
		 * This maintains the invariant that release_task()
		 * only runs on a task in EXIT_DEAD, just for sanity.
		 */
		if (zap_leader)
			leader->exit_state = EXIT_DEAD;
	}

	write_unlock_irq(&tasklist_lock);
	release_thread(p);
	call_rcu(&p->rcu, delayed_put_task_struct);

	p = leader;
	if (unlikely(zap_leader))
		goto repeat;
}
```

具体逻辑是：`__exit_signal` 会调用 `__unhash_process`，再进一步执行 `detach_pid`。
