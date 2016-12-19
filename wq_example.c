/**
 * wqdemo.c  - Linux kernel waitqueue example.
 *
 * Author - Sougata Santra <sougata.santra@gmail.com>
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/delay.h>

/*
 * Module wide variables.
 */
wait_queue_head_t wqdemo_wq;
struct task_struct *wqdemo_thread;
static atomic_t wqdemo_thread_started;

/*
 * wqdemo_thread_fn - runs in a separate scheduling context.
 */
static int wqdemo_thread_fn(void *data)
{
	atomic_set(&wqdemo_thread_started, 1);
	smp_mb__after_atomic();
	if (waitqueue_active(&wqdemo_wq))
		wake_up(&wqdemo_wq);
	/*
	 * Sleep for 10 seconds.
	 */
	msleep_interruptible(10000);
	pr_crit ("Woke up and inside wqdemo_thread_fn.");
	do_exit(0);
}

/**
 * Waitqueues are set of sleeping processes which are woken up by the kernel
 * when some condition becomes true.  It is a datastructure needed to track
 * process descriptors in state TASK_[UN]INTERRUPTIBLE states.  They are used
 * for interrupt handling, process synchronization etc.  The set is represented
 * with a head element of type @wait_queue_head_t and the elements of the set
 * are of type @wait_queue_head_t.
 *
 * The example below demonstarates the use of waitqueue for process
 * synchronization.
 */
static int __init init_wqdemo(void)
{

	init_waitqueue_head(&wqdemo_wq);
	wqdemo_thread = kthread_create(wqdemo_thread_fn, NULL, "wqdemod");
	if (IS_ERR(wqdemo_thread)) {
		pr_crit("Failed to start wqdemod kernel "
				"thread (error %ld).",
				-PTR_ERR(wqdemo_thread));
	} else {
		/*
		 * Take an extra reference on @wqdemo_thread so that while
		 * checking if the kernel thread has exited during module
		 * exit,  the @task_struct is not destroyed.
		 */
		get_task_struct(wqdemo_thread);
		wake_up_process(wqdemo_thread);
		/**
		 * The wait_event and wait_event_interruptible macros put the
		 * calling process to sleep on a wait queue until a given
		 * condition is verified. For instance, the wait_
		 * event(wq,condition) macro essentially yields the following
		 * fragment:
		 *
		 * DEFINE_WAIT(__wait);
		 * for (;;) {
		 * 	prepare_to_wait(&wq, &__wait, TASK_UNINTERRUPTIBLE);
		 * 	if (condition)
		 * 		break;
		 * 	schedule( );
		 * }
		 * finish_wait(&wq, &__wait);
		 *
		 * The DEFINE_WAIT() macro declares a new wait_queue_t variable
		 * and initializes it with the descriptor of the process
		 * currently executing on the CPU and the address of the
		 * autoremove_wake_function() wake-up function. This function
		 * invokes default_wake_function() to awaken the sleeping
		 * process, and then removes the wait queue element from the
		 * wait queue list.
		 *
		 * The prepare_to_wait() and prepare_to_wait_exclusive()
		 * functions set the process state to the value passed as the
		 * third parameter, then set the exclusive flag in the wait
		 * queue element respectively to 0 (nonexclusive) or 1
		 * (exclusive), and finally insert the wait queue element wait
		 * into the list of the wait queue head wq.
		 * As soon as the process is awakened, it executes the
		 * finish_wait() function, which sets again the process state
		 * to TASK_RUNNING (just in case the awaking condition
		 * becomes true before invoking schedule()), and removes the
		 * wait queue element from the wait queue list (unless this has
		 * already been done by the wake- up function).
		 *
		 * Here we use wait_event for the thread to start.
		 */
		wait_event(wqdemo_wq, atomic_read(&wqdemo_thread_started));
	}
	return 0;
}

static void __exit exit_wqdemo(void)
{
	struct completion *vfork;
	int ret;

	/*
	 * @wqdemo_thread is an independent kernel thread that will never have
	 * kthread_stop() called and might have already terminated by calling
	 * do_exit() but we are not sure if it has terminated so we need to
	 * ensure that before we exit the module.
	 * 
	 * The relationship between tasks_struct and kernel threads is that the
	 * struct kthread field @exited of type struct completion is pointed to
	 * by @vfork_done field of task_struct and set up when the kthread is
	 * created.
	 *
	 * #define __to_kthread(vfork)     \
	 *         container_of(vfork, struct kthread, exited)
	 *
	 * static inline struct kthread *to_kthread(struct task_struct *k)
	 * {
	 *	 return __to_kthread(k->vfork_done);
	 * }
	 *
	 * When the kthread exits by calling do_exit() or when someone
	 * calls kthread_stop() on the kthread which in turn causes the kthread
	 * function to return and call do_exit().  In both the cases the we need
	 * to wait for the @&kthread->exited to be completed() if the kthread
	 * is still present,  which is called from do_exit()->exit_mm()->
	 * mm_release()
	 */
	vfork = ACCESS_ONCE(wqdemo_thread->vfork_done);
	if (unlikely(vfork)) {
		wake_up_process(wqdemo_thread);
		wait_for_completion(vfork);
		pr_info("Waited for the thread to exit");
	}
	ret = wqdemo_thread->exit_code;
	/*
	 * Release the extra reference we took previously.
	 */
	put_task_struct(wqdemo_thread);
	pr_info("wqdemo_thread exited with status %d", ret);
}

MODULE_LICENSE("GPL");
module_init(init_wqdemo)
module_exit(exit_wqdemo)
