# Linux waitqueue/kernel thread demo (KERNEL_VERSION(4,4,0))

Kernel threads are independent context which only runs in kernel mode.  Please
NOTE the flags @CLONE_VM and @CLONE_UNTRACED.
	
 _do_fork(flags|CLONE_VM|CLONE_UNTRACED, (unsigned long)fn,
	(unsigned long)arg, NULL, NULL, 0);

The CLONE_VM flag avoids the duplication of the page tables of the calling
process: this duplication would be a waste of time and memory, because the new
kernel thread will not access the User Mode address space anyway. The
CLONE_UNTRACED flag ensures that no process will be able to trace the new
kernel thread, even if the calling process is being traced.

This example demonstrates a linux kernel thread join, which is otherwise not
implemented as an API. i.e how to wait for completion of an independent  kernel
thread which exits by calling do_exit().
