#include <linux/module.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/jiffies.h>
#include <linux/kmod.h>
#include <linux/fs.h>

MODULE_LICENSE("GPL");

#define WIFEXITED(status)   (((status) & 0x7F) == 0)
#define WEXITSTATUS(status) (((status) & 0xFF00) >> 8)
#define WIFSIGNALED(status) (((signed char)(((status) & 0x7F) + 1) >> 1) > 0)
#define WTERMSIG(status)    ((status) & 0x7F)
#define WIFSTOPPED(status)  (((status) & 0xFF) == 0x7F)
#define WSTOPSIG(status)    (((status) & 0xFF00) >> 8)

int my_exec(void);
int my_fork(void *argc);
void handle_signal(int status);  


/* Kernel thread task structure */
static struct task_struct *task;

/* Structure for do_wait parameters */
struct wait_opts {
    enum pid_type wo_type;
    int wo_flags;
    struct pid *wo_pid;

    struct waitid_info *wo_info;
    int wo_stat;
    struct rusage *wo_rusage;

    wait_queue_entry_t child_wait;
    int notask_error;
};

/* External functions from kernel */
extern pid_t kernel_clone(struct kernel_clone_args *kargs);
extern int do_execve( struct filename *filename,
                      const char __user *const __user *__argv,
                      const char __user *const __user *__envp);
extern struct filename *getname_kernel(const char *filename);
extern long do_wait(struct wait_opts *wo);

/* Execute the test program in child process */
int my_exec(void)
{
    //const char path[] = "/home/vagrant/csc3150/source/program2/test";
	const char path[] = "/tmp/test";

    struct filename *file_name = getname_kernel(path);
    int result;  

    if (IS_ERR(file_name)) {
        printk("[program2] : Failed to get filename, error = %ld\n", PTR_ERR(file_name));
        return PTR_ERR(file_name);
    }

    /* Execute a test program */
    result = do_execve(file_name, NULL, NULL);
    if (result < 0) {
        printk("[program2] : Failed to execute program, error = %d\n", result);
        return result;
    }

    return 0;
}




/* Wait for child process termination */
void my_wait(pid_t pid)
{
    int status;
    struct wait_opts wo;
    struct pid *wo_pid = NULL;
    enum pid_type type;
    type = PIDTYPE_PID;
    wo_pid = find_get_pid(pid);


    /* Wait options setup */
    wo.wo_type = type;
    wo.wo_pid=wo_pid;
    wo.wo_flags=WEXITED|WSTOPPED;
    wo.wo_info=NULL;
    wo.wo_stat=&status;
    wo.wo_rusage=NULL;

	printk("[program2] : child process\n");

    do_wait(&wo);
    put_pid(wo_pid);

    handle_signal(wo.wo_stat); 
}


/* Handle signal from child process */
void handle_signal(int status)
{
    int signal;

    /* Signal descriptions */
    const char *signal_names[] = {
    "UNKNOWN",   // 0: No signal 0
    "SIGHUP",    // 1: Hangup
    "SIGINT",    // 2: Interrupt from keyboard
    "SIGQUIT",   // 3: Quit from keyboard
    "SIGILL",    // 4: Illegal instruction
    "SIGTRAP",   // 5: Trace/breakpoint trap
    "SIGABRT",   // 6: Abort signal
    "SIGBUS",    // 7: Bus error
    "SIGFPE",    // 8: Floating-point exception
    "SIGKILL",   // 9: Kill signal
    "SIGUSR1",   // 10: User-defined signal 1
    "SIGSEGV",   // 11: Segmentation fault
    "SIGUSR2",   // 12: User-defined signal 2
    "SIGPIPE",   // 13: Broken pipe
    "SIGALRM",   // 14: Timer signal from alarm
    "SIGTERM",   // 15: Termination signal
    "SIGSTKFLT", // 16: Stack fault
    "SIGCHLD",   // 17: Child stopped or terminated
    "SIGCONT",   // 18: Continue if stopped
    "SIGSTOP",   // 19: Stop process
    "SIGTSTP",   // 20: Stop typed at terminal
    "SIGTTIN",   // 21: Terminal input for background process
    "SIGTTOU",   // 22: Terminal output for background process
    "SIGURG",    // 23: Urgent condition on socket
    "SIGXCPU",   // 24: CPU time limit exceeded
    "SIGXFSZ",   // 25: File size limit exceeded
    "SIGVTALRM", // 26: Virtual alarm clock
    "SIGPROF",   // 27: Profiling timer expired
    "SIGWINCH",  // 28: Window resize signal
    "SIGIO",     // 29: I/O now possible
    "SIGPWR",    // 30: Power failure
    "SIGSYS"     // 31: Bad system call
};
    if (WIFSIGNALED(status)) {
        signal = WTERMSIG(status);  
        printk("[program2] : get %s signal\n", signal_names[signal]);
		printk("[program2] : child process terminated\n");
        printk("[program2] : The return signal is %d\n", signal);
    } else if (WIFSTOPPED(status)) {
        signal = WSTOPSIG(status);
        printk("[program2] : get SIGSTOP signal\n");
		printk("[program2] : child process stopped\n");
        printk("[program2] : the return signal is %d\n",
               signal);
    } else if (WIFEXITED(status)) {
        int exit_status = WEXITSTATUS(status);
        printk("[program2] : child process exited normally\n");
        printk("[program2] : the return status is %d\n",
               exit_status);}
    

}

//implement fork function
int my_fork(void *argc){
    int i;
    //set default sigaction for current process
    struct k_sigaction *k_action = &current->sighand->action[0];
    for(i=0;i<_NSIG;i++){
        k_action->sa.sa_handler = SIG_DFL;
        k_action->sa.sa_flags = 0;
        k_action->sa.sa_restorer = NULL;
        sigemptyset(&k_action->sa.sa_mask);
        k_action++;
    }
    
    /* fork a process using kernel_clone or kernel_thread */
    /* execute a test program in child process */
    struct kernel_clone_args clone_args
    = { 
		//.flags = SIGCHLD,
		.flags = ((lower_32_bits(SIGCHLD) | CLONE_VM | CLONE_UNTRACED) & ~CSIGNAL),
		.exit_signal = SIGCHLD,
		.stack = (unsigned long)&my_exec,
		.parent_tid = NULL,
		.child_tid = NULL
    };
    

    pid_t pid = kernel_clone(&clone_args);
    printk("[program2] : The child process has pid = %d\n", pid);
    printk("[program2] : This is the parent process, pid = %d\n", (int)current->pid);
    
    /* wait until child process terminates */
    my_wait(pid);
    
    return 0;
}

static int __init program2_init(void){

    printk("[program2] : Module_init\n");
    
    /* write your code here */
    
    /* create a kernel thread to run my_fork */
    printk("[program2] : module_init create kthread start\n");
    task = kthread_create(&my_fork, NULL, "MyThread");

    if (!IS_ERR(task))
    {
        printk("[program2] : module_init kthread start\n");
        wake_up_process(task);
    }
    return 0;
	do_exit(0);
}

static void __exit program2_exit(void){
    printk("[program2] : Module_exit\n");
}

module_init(program2_init);
module_exit(program2_exit);

