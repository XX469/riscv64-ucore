#### 练习1：分配并初始化一个进程控制块（需要编码）

alloc_proc函数（位于kern/process/proc.c中）负责分配并返回一个新的struct proc_struct结构，用于存储新建立的内核线程的管理信息。ucore需要对这个结构进行最基本的初始化，你需要完成这个初始化过程。

> 【提示】在alloc_proc函数的实现中，需要初始化的proc_struct结构中的成员变量至少包括：state/pid/runs/kstack/need_resched/parent/mm/context/tf/cr3/flags/name。

请在实验报告中简要说明你的设计实现过程。请回答如下问题：

- 请说明proc_struct中`struct context context`和`struct trapframe *tf`成员变量含义和在本实验中的作用是啥？（提示通过看代码和编程调试可以判断出来）



**设计实现过程：**该函数对一个进程进行最基本的初始化，因此大部分变量赋0即可，只有一些特殊的变量设置了特殊的值：

- `proc->state = PROC_UNINIT`			设置进程为“初始”态
- `proc->pid = -1`                                   设置进程pid的未初始化值
- `proc->cr3 = boot_cr3`                       使用内核页目录表的基址

proc_struct中`struct context context`和`struct trapframe *tf`成员变量含义和在本实验中的作用：

- `struct context context` ：当前进程执行的上下文，即几个关键寄存器的值，用于实现进程切换，能够还原之前进程的运行状态
- `struct trapframe *tf` ：保存了进程的中断帧。当进程从用户空间跳进内核空间的时候，进程的执行状态被保存在了中断帧中（注意这里需要保存的执行状态数量不同于上下文切换）。系统调用可能会改变用户寄存器的值，我们可以通过调整中断帧来使得系统调用返回特定的值



#### 练习2：为新创建的内核线程分配资源（需要编码）

创建一个内核线程需要分配和设置好很多资源。kernel_thread函数通过调用**do_fork**函数完成具体内核线程的创建工作。do_kernel函数会调用alloc_proc函数来分配并初始化一个进程控制块，但alloc_proc只是找到了一小块内存用以记录进程的必要信息，并没有实际分配这些资源。ucore一般通过do_fork实际创建新的内核线程。do_fork的作用是，创建当前内核线程的一个副本，它们的执行上下文、代码、数据都一样，但是存储位置不同。因此，我们**实际需要"fork"的东西就是stack和trapframe**。在这个过程中，需要给新内核线程分配资源，并且复制原进程的状态。你需要完成在kern/process/proc.c中的do_fork函数中的处理过程。它的大致执行步骤包括：

- 调用alloc_proc，首先获得一块用户信息块。
- 为进程分配一个内核栈。
- 复制原进程的内存管理信息到新进程（但内核线程不必做此事）
- 复制原进程上下文到新进程
- 将新进程添加到进程列表
- 唤醒新进程
- 返回新进程号

请在实验报告中简要说明你的设计实现过程。请回答如下问题：

- 请说明ucore是否做到给每个新fork的线程一个唯一的id？请说明你的分析和理由。



在`do_fork`函数中，通过调用`get_pid`函数为每个新fork的线程创建id，该函数通过遍历进程链表，并维护 `last_pid` 和 `next_safe` 变量来确保分配给新进程的 ID 是唯一的。如果 `last_pid` 已经被使用或者在已有进程的范围内，它会找到一个可用的 ID，并确保不会与已有的进程 ID 冲突。`next_safe` 变量用于确保在分配新进程 ID 时避免使用已经被占用的 ID，并且能够在已有进程 ID 的范围内找到下一个可用的 ID。它主要用于在遍历进程列表时记录当前已分配的进程 ID 中的最小可用值。



#### 练习3：编写proc_run 函数（需要编码）

proc_run用于将指定的进程切换到CPU上运行。它的大致执行步骤包括：

- 检查要切换的进程是否与当前正在运行的进程相同，如果相同则不需要切换。
- 禁用中断。你可以使用`/kern/sync/sync.h`中定义好的宏`local_intr_save(x)`和`local_intr_restore(x)`来实现关、开中断。
- 切换当前进程为要运行的进程。
- 切换页表，以便使用新进程的地址空间。`/libs/riscv.h`中提供了`lcr3(unsigned int cr3)`函数，可实现修改CR3寄存器值的功能。
- 实现上下文切换。`/kern/process`中已经预先编写好了`switch.S`，其中定义了`switch_to()`函数。可实现两个进程的context切换。
- 允许中断。

请回答如下问题：

- 在本实验的执行过程中，创建且运行了几个内核线程？



2个，一个是0号进程idleproc，表示空闲进程。一个是1号进程initproc，作用是显示“Hello World”，表示能够正常工作。



#### 扩展练习 Challenge：

- 说明语句`local_intr_save(intr_flag);....local_intr_restore(intr_flag);`是如何实现开关中断的？



**禁用中断：**

相关代码：

```c
#define local_intr_save(x)    do { x = __intr_save(); } while (0)

static inline bool __intr_save(void) {
    if (read_csr(sstatus) & SSTATUS_SIE) {
        intr_disable();
        return 1;
    }
    return 0;
}

void intr_disable(void) { clear_csr(sstatus, SSTATUS_SIE); }
```

- `local_intr_save(x)` 是一个宏定义，用于保存中断状态并禁用中断。在该宏中循环调用了一个内联函数 `__intr_save()` 来实现中断的保存与禁用。变量下用于保存当前中断状态
- `__intr_save`函数首先使用read_csr函数读取当前CPU状态寄存器并检查其中的SIE（Supervisor Interrupt Enable）位，判断当前中断是否被允许。如果为真，说明中断被允许，则调用`intr_disable()` 函数来禁用中断，具体来说是。否则直接返回。
- `intr_disable`调用了 `clear_csr()` 函数，该函数的作用是清除寄存器的指定位。在这里，`clear_csr()` 函数被用于清除 SSTATUS 寄存器中的 `SSTATUS_SIE` 位，将其置为 0，从而禁用了中断。通过操作这个位，该函数禁用了当前 CPU 的中断功能，即使有中断请求触发，CPU 也不会响应中断。



**允许中断：**

相关代码：

```c
#define local_intr_restore(x)  __intr_restore(x);

static inline void __intr_restore(bool flag) {
    if (flag) {
        intr_enable();
    }
}

void intr_enable(void) { set_csr(sstatus, SSTATUS_SIE); }
```

- `local_intr_restore(x)`是一个宏定义，直接调用了一次`__intr_restore(x)`来恢复中断
- `__intr_restore(bool flag)`函数用于恢复中断，首先根据传入的参数判断当前中断是否允许，如果当前状态为禁用中断，则调用`intr_enable()`函数实现允许中断。
- `intr_enable`重新将 SSTATUS 寄存器中的`SSTATUS_SIE` 置位

