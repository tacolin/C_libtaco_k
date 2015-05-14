#ifndef _KTHREAD_H_
#define _KTHREAD_H_

#include <linux/kthread.h>

#define THREAD_EXIT (0)
#define THREAD_CONT (5566)

struct thread
{
    int (*func)(void* arg);
    void* arg;

    struct task_struct* task;
    bool  running;
};

void thread_join(struct thread* thread_array, int num);
void thread_exit(struct thread* thread_array, int num);

#endif //_KTHREAD_H_
