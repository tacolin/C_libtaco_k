#include "kthread.h"

#define derror(a, b...) printk("[ERROR] %s(): "a"\n", __func__, ##b)
#define CHECK_IF(assertion, error_action, ...) \
{\
    if (assertion) \
    { \
        derror(__VA_ARGS__); \
        {error_action;} \
    }\
}

static int _thread_routine(void* input)
{
    struct thread* t = (struct thread*)input;
    int ret;

    CHECK_IF(input == NULL, return -1, "input is null");

    allow_signal(SIGINT);
    t->running = true;
    do
    {
        ret = t->func(t->arg);

    } while (!kthread_should_stop() && (ret == THREAD_CONT));
    t->running = false;
    return 0;
}

void thread_join(struct thread* thread_array, int num)
{
    int i, chk;
    struct thread* t;
    char name[20];

    CHECK_IF(thread_array == NULL, return, "thread_array is null");
    CHECK_IF(num <= 0, return, "num = %d invalid", num);

    for (i=0; i<num; i++)
    {
        snprintf(name, 20, "thread %d", i);
        t = &thread_array[i];
        t->task = kthread_run(_thread_routine, t, name);
        if (IS_ERR(t->task))
        {
            derror("thread[%d] kthread_create failed", i);
            chk = PTR_ERR(t->task);
            t->task = NULL;
        }
    }
    return;
}

void thread_exit(struct thread* thread_array, int num)
{
    int i;
    struct thread* t;

    CHECK_IF(thread_array == NULL, return, "thread_array is null");
    CHECK_IF(num <= 0, return, "num = %d invalid", num);

    for (i=0; i<num; i++)
    {
        t = &thread_array[i];
        if (t->task && t->running)
        {
            kill_pid(find_vpid(t->task->pid), SIGINT, 1);
            kthread_stop(t->task);
            t->task = NULL;
        }
    }
    return;
}
