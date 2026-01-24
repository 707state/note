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

Linux的启动流程在不同的架构上不太一样, 这里以i386为例.

计算机开机时, BIOS/UEFI把引导程序(比如GRUB)加载到内存, GRUB会加载Linux内核, 在i386中, 内核被加载时, 第一个代码入口是arch/x86/kernel/head\_32.S里面的startup\_32.

![startup_32](images/linux26/startup_32.png)

在startup阶段, 要完成从实模式到保护模式的过渡, 还需要进行早期的硬件初始化, 包括: 重新加载段寄存器, 复制引导参数, 初始化页表, 设置虚拟内存映射, 启用分页并跳转到内核代码的起始位置. 最终会跳转到i386\_start\_kernel函数. 接下来会完成后续的内核初始化.


## start\_kernel

在i386\_start\_kernel函数中会根据具体的subarch选择对应的early\_setup实现.

![start_kernel](images/linux26/start_kernel.png)

在这段函数的开头会预留足够的内存, 确保初始化期间的一些关键内存区域不会被覆盖, 内核会根据引导参数hardware\_subarch字段执行特定硬件平台的初始化函数.

在此之后, 所有必要的内存区域都被标记为了预留或者不可用, 内核已经完成了内存预留(代码段, 数据段, BSS段还有RAM都足够), 引导程序和BIOS相关的内存也被标记为不可用, 避免了干扰.

start\_kernel函数负责执行大部分早期的初始化工作来让内核正常运行, 里面执行了大量的xxx\_init操作, 比如tick\_init, boot\_cpu\_init等等.

start\_kernel里面最重要的几个函数是:

1. setup\_arch:

这一个函数负责把引导程序/BIOS/EFI提供的原始信息变为内核可用的结构, 把系统的物理内存计算出来(max\_low\_pfn\_mapped, max\_pfn\_mapped), ACPI解析, NUMA节点拓扑发现等等都在这里完成. 还有APIC/IOAPIC映射和IRQ数量探测.

2. build\_all\_zonelists和page\_alloc\_init:

在这两个函数之前内存分配只能使用bootmem, 而在这两个函数前者会对内存的物理区域进行划分和分组, 后者则为多核系统的内存分配通知系统做准备.

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
可以看到这里面注册的回调函数会在CPU状态改变时触发.

3. trap\_init和init\_IRQ

trap\_init会为特殊的中断号设置对应的中断服务例程.

![trap_init](images/linux26/trap_init.png)

在i386下,这些例程都被定义在entry\_32.S中.

init\_IRQ会完成分配和设置IRQ向量, 并且完成APIC和IOAPIC相关的初始化.

4. sched\_init

sched\_init要把内核的调度器(包括调度队列, 调度策略, 任务组和运行队列)初始化.

- 初始化了公平调度（CFS）和实时调度（RT）的相关数据结构。

- 配置了 CPU 负载、调度器亲和性、CPU 的调度队列（rq）。

- 为系统中的每个 CPU 设置了默认的调度策略和任务负载。

- 配置了内核空闲线程、任务组、调度类和性能监控。

5. rcu\_init

Read-Copy-Update是Linux内核里面处理同步的重要机制.

![rcu_init](images/linux26/rcu_init.png)

open\_softirq为内核注册了软中断, cpu\_notifier会在CPU状态变更时通知RCPU进行必要的修改.

6. rest\_init

rest\_init里面最重要的部分是kernel\_thread(kernel\_init). 这个函数启动了内核线程并执行系统初始化任务. 在kernel\_init最后会执行init\_post, 这里会执行run\_init\_process, 通过kernel\_execve把init进程跑起来.

![run_init_process](images/linux26/run_init_process.png)

在2.6内核中, 除了init进程是内核启动的, 还有kthreadadd这个PID为2的任务也是内核启动的. 主要管理待创建的内核线程.
