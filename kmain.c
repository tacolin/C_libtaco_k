#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include "kthread.h"
#include "klogger.h"
#include "kqueue.h"
#include "kudp.h"

MODULE_AUTHOR("tacolin");
MODULE_DESCRIPTION("KERNEL SPACE LOGGER");
MODULE_LICENSE("GPL");

#define dprint(a, b...) printk("%s(): "a"\n", __func__, ##b)

static struct thread  _t[2];
// static struct thread  _t[3];
// static struct queue   _q;
static struct logger* _l;

// static void _clean_msg(void* input)
// {
//     if (input) kfree(input);
// }

static int _logger(void* arg)
{
    struct logger* l = (struct logger*)arg;
    logger_run(l);
    return THREAD_EXIT;
}

// static int _producer(void* arg)
// {
//     struct queue* q = (struct queue*)arg;
//     int i;
//     char* msg;
//     for (i=0; i<20; i++)
//     {
//         msg = kzalloc(20, GFP_ATOMIC);
//         snprintf(msg, 20, "fuck you %d\n", i);
//         log_print(_l, LOG_INFO, "produce: %s", msg);
//         queue_push(q, msg);
//     }
//     return THREAD_EXIT;
// }

// static int _consumer(void* arg)
// {
//     struct queue* q = (struct queue*)arg;
//     char* msg;
//     QUEUE_FOREACH(q, msg)
//     {
//         log_print(_l, LOG_V1, "recv : %s", msg);
//         kfree(msg);
//     }
//     return THREAD_EXIT;
// }

// static int _consumer(void* arg)
// {
//     struct queue* q = (struct queue*)arg;
//     char* msg       = queue_pop(q);
//     if (msg == NULL)
//     {
//         return THREAD_EXIT;
//     }

//     printk("recv : %s", msg);
//     kfree(msg);
//     return THREAD_CONT;
// }

// static int __init klog_init(void)
// {
//     _l = logger_create(LOG_PRINTK | LOG_FILE | LOG_UDP, "192.168.8.115", 55555, "/home/mme/testlog.txt");

//     queue_init(&_q, 10, _clean_msg, QUEUE_FLAG_PUSH_BLOCK | QUEUE_FLAG_POP_BLOCK);

//     _t[0].func = _logger;
//     _t[0].arg  = _l;

//     _t[1].func = _producer;
//     _t[1].arg  = &_q;

//     _t[2].func = _consumer;
//     _t[2].arg  = &_q;

//     thread_join(_t, 3);

//     dprint("ok");
//     return 0;
// }

// static void __exit klog_exit(void)
// {
//     logger_break(_l);

//     thread_exit(_t, 3);

//     queue_clean(&_q);

//     logger_release(_l);

//     dprint("ok");
//     return;
// }

static int _recv_udp(void* arg)
{
    struct logger* lg = (struct logger*)arg;
    char buffer[256] = {0};
    struct udp udp;
    struct udp_addr remote;
    int recvlen;

    udp_init(&udp, NULL, 55555);

    while ((recvlen = udp_recv(&udp, buffer, 256, &remote)) > 0)
    {
        buffer[recvlen-1] = '\0';
        log_print(lg, LOG_INFO, "%s\n", buffer);
    }

    udp_uninit(&udp);
    return THREAD_EXIT;
}

static int __init ktaco_init(void)
{
    _l = logger_create(LOG_PRINTK, 0, 0, 0);

    _t[0].func = _logger;
    _t[0].arg = _l;

    _t[1].func = _recv_udp;
    _t[1].arg = _l;

    thread_join(_t, 2);

    dprint("ok");
    return 0;
}

static void __exit ktaco_exit(void)
{
    thread_exit(_t, 2);

    logger_break(_l);
    logger_release(_l);
    dprint("ok");
    return;
}

module_init(ktaco_init);
module_exit(ktaco_exit);
