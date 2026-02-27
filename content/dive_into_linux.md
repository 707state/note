---
title: 从Linux 2.6搞明白Linux
author: jask
date: 2026-01-23
tags:
  - Linux
  - OS
---

# 启动

## startup

Linux的启动流程在不同的架构上不太一样，这里以i386为例。

计算机开机时，BIOS/UEFI把引导程序(比如GRUB)加载到内存，GRUB会加载Linux内核，在i386中，内核被加载时，第一个代码入口是arch/x86/kernel/head\_32.S里面的startup\_32。

![startup_32](images/linux26/startup_32.png)

在startup阶段，要完成从实模式到保护模式的过渡，还需要进行早期的硬件初始化，包括：重新加载段寄存器，复制引导参数，初始化页表，设置虚拟内存映射，启用分页并跳转到内核代码的起始位置，最终会跳转到i386\_start\_kernel函数。接下来会完成后续的内核初始化。


## start\_kernel

在i386\_start\_kernel函数中会根据具体的subarch选择对应的early\_setup实现。

![start_kernel](images/linux26/start_kernel.png)

在这段函数的开头会预留足够的内存，确保初始化期间的一些关键内存区域不会被覆盖，内核会根据引导参数hardware\_subarch字段执行特定硬件平台的初始化函数。

在此之后, 所有必要的内存区域都被标记为了预留或者不可用，内核已经完成了内存预留(代码段、数据段、BSS段还有RAM都足够)，引导程序和BIOS相关的内存也被标记为不可用，避免了干扰。

start\_kernel函数负责执行大部分早期的初始化工作来让内核正常运行，里面执行了大量的xxx\_init操作，比如tick\_init, boot\_cpu\_init等等。

start\_kernel里面最重要的几个函数是:

1. setup\_arch:

这一个函数负责把引导程序/BIOS/EFI提供的原始信息变为内核可用的结构，把系统的物理内存计算出来(max\_low\_pfn\_mapped、max\_pfn\_mapped)，ACPI解析，NUMA节点拓扑发现等等都在这里完成，还有APIC/IOAPIC映射和IRQ数量探测。

2. build\_all\_zonelists和page\_alloc\_init:

在这两个函数之前内存分配只能使用bootmem，而在这两个函数前者会对内存的物理区域进行划分和分组，后者则为多核系统的内存分配通知系统做准备。

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
可以看到这里面注册的回调函数会在CPU状态改变时触发。

3. trap\_init和init\_IRQ

trap\_init会为特殊的中断号设置对应的中断服务例程。

![trap_init](images/linux26/trap_init.png)

在i386下，这些例程都被定义在entry\_32.S中。

init\_IRQ会完成分配和设置IRQ向量，并且完成APIC和IOAPIC相关的初始化。

4. sched\_init

sched\_init要把内核的调度器(包括调度队列, 调度策略, 任务组和运行队列)初始化。

- 初始化了公平调度（CFS）和实时调度（RT）的相关数据结构。

- 配置了 CPU 负载、调度器亲和性、CPU 的调度队列（rq）。

- 为系统中的每个 CPU 设置了默认的调度策略和任务负载。

- 配置了内核空闲线程、任务组、调度类和性能监控。

5. rcu\_init

Read-Copy-Update是Linux内核里面处理同步的重要机制。

![rcu_init](images/linux26/rcu_init.png)

open\_softirq为内核注册了软中断，cpu\_notifier会在CPU状态变更时通知RCPU进行必要的修改。

6. rest\_init

rest\_init里面最重要的部分是kernel\_thread(kernel\_init)。这个函数启动了内核线程并执行系统初始化任务。在kernel\_init最后会执行init\_post，这里会执行run\_init\_process，通过kernel\_execve把init进程跑起来。

![run_init_process](images/linux26/run_init_process.png)

在2.6内核中，除了init进程是内核启动的，还有kthreadadd这个PID为2的任务也是内核启动的。主要管理待创建的内核线程。

# 进程/线程

Linux创建一个新进程的方式是fork+exec，尽管POSIX对于各个系统的原语进行了封装，Unix-Like的系统基本上都不提供直接的spawn操作。

Linux采用Copy-On-Write的方式实现fork，fork本身的开销就只有复制父进程的page tables和创建一个unique child pid，而不包括立刻复制物理内存页。在fork后子进程向内存修改之前，父子进程的page table都指向同一批物理页。只有当子进程修改内存时才会发生物理页的复制。

Unix是鼓励迅速的进程创建的，也就是fork + exec的共同使用。exec会丢弃当前进程的整个用户态空间，加载ELF并建立新的page tables。根本没有必要复制父进程物理页。 所以Linux的COW减少了fork+exec时不必要的内存开销。

Linux上fork是copy的一个特例, vfork则是copy的另一种特例。 当然三者都是调用内核的do\_fork。

## do\_fork

![do_fork](images/linux26/do_fork.png)

这里面最重要的部分是copy\_process。

```c
static struct task_struct *copy_process(unsigned long clone_flags,
                    unsigned long stack_start,
					struct pt_regs *regs,
					unsigned long stack_size,
					int __user *child_tidptr,
					struct pid *pid,
					int trace){
                    ...
    ret=dup_task_struct(current);// 创建新的内核栈、thread_info、task_struct，此时父子进程pid还没有区别
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
    // 分配pid
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
    //根据clone flags决定资源是共享还是复制
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
    // 收尾，把新的task添加到系统、挂在各种链表/哈希表里面、处理线程/进程组/session之间的关系.
    return p;
}
```

copy\_process执行成功之后，do\_fork里父进程需要立即trace并获取child pid，把子进程写回给父进程的用户空间，还要去通知audit/tracing子系统clone已经发生。根据clone\_flags还要决定子任务是STOP还是RUNNABLE(wake\_up\_new\_task)。vfork的逻辑稍微特殊一点，父进程要阻塞等待完成。

![after\_copy\_process](images/linux26/after_copy_process.png)

有个说法是：_fork有两个返回值，一个在父进程返回，一个在子进程返回_。

这个说法有失偏颇。

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

task\_pid\_vnr会从父进程的PID namespace里面获取到子进程的PID, 在成功是返回相应的pid。这是第一个返回值。

在哪里给子进程返回呢？

![copy_thread](images/linux26/copy_thread.png)

子进程的返回发生在copy\_thread里面，在childregs=task\_pt\_regs(p)这里获取到子进程内核栈上的pt\_regs，接下来\*childregs=\*regs则是复制父进程的寄存器，而在childregs->ax=0这里就把子进程的返回值设置为0。在父进程执行完wake\_up\_new\_task以后，子进程变成runnable的，调度器某个时刻选择子任务，然后context switch到子任务上，CPU开始执行p->thread.ip上的代码，最后通过通过ret\_from\_fork返回用户态。

![ret\_from\_fork](images/linux26/ret_from_fork.png)

这是x86上新创建的子进程/线程第一次被调度运行时的起点，由调度器切换到新任务时的thread.ip指向。

## vfork

vfork相比fork，最大的区别就是vfork的子进程不会拷贝page tables entries，而且会一直阻塞父进程知道执行完成。

## Threads

Linux内核并不会特殊对待线程，都是一个task\_struct，基本上可以看作是共享一些资源的进程。

Linux中有Kernel Thread，提供了内核中执行后台任务的能力，这些内核线程只会在内核态运行。

![kthread\_create](images/linux26/kthread_create.png)

这里会看到一个kthreadadd\_task，这是由rest\_init（参考前面启动阶段）启动的第二个进程（pid为2）关联的task\_struct，为了添加一个kernel thread去执行任务，在这里就需要唤醒kthreadadd这个进程。

## exit

进程的结束通常是自行退出，即主动调用exit（即使不在main结尾放一个exit，编译器也会在main返回后插入一个）。

exit的具体逻辑在do\_exit中，大致流程：

```c
void do_exit(long code){
    struct task_struct *tsk = current;
	int group_dead;
    ...
    // 设置PF_EXITING
    exit_irq_thread();
	exit_signals(tsk);  /* sets PF_EXITING */
    ...
    acct_update_integrals(tsk); // 统计信息
    ...
    // 释放当前进程的mm_struct
    tsk->exit_code = code;
	taskstats_exit(tsk, group_dead);
	exit_mm(tsk);
    ...
    exit_sem(tsk);
    // 减少计数，计数归0时销毁对应的资源
	exit_files(tsk);
	exit_fs(tsk);
    ...
    exit_notify(tsk, group_dead); // 通知parent process
    ...
    exit_rcu();
	/* causes final put_task_struct in finish_task_switch(). */
	tsk->state = TASK_DEAD;
	schedule();
}
```

执行了do\_exit之后，所有资源都会被释放，现在只有thread\_info还没被释放（如果所有的资源只被这个任务使用），在do\_exit之后内核还是会保留pid，但是进程会变成zombie进程并且不能运行了。只有在release\_task被调用时，才会清理PID。

![release\_task](images/linux26/release_task.png)

具体的逻辑是：\_\_exit\_signal调用\_\_unhash\_process来执行detach\_pid。
