#include <linux/file.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/types.h>

#include "klogger.h"

#define derror(a, b...) printk("[ERROR] %s(): "a"\n", __func__, ##b)
#define CHECK_IF(assertion, error_action, ...)\
{\
    if (assertion)\
    {\
        derror(__VA_ARGS__); \
        {error_action;} \
    }\
}

struct logger
{
    struct queue mq;
    int type;
    bool running;
    log_lv_t lv;

    struct udp udp;
    struct udp_addr remote;

    char filepath[LOG_FILE_PATH_SIZE];
    struct file* fp;
};

static void _clean_log(void* input)
{
    if (input) kfree(input);
}

struct logger* logger_create(int type, char* dstip, int dstport, char* filepath)
{
    struct logger* l = kzalloc(sizeof(struct logger), GFP_ATOMIC);

    CHECK_IF(l == NULL, return NULL, "l is null");

    queue_init(&l->mq, -1, _clean_log, QUEUE_FLAG_POP_BLOCK);
    l->type = type;
    l->lv = LOG_DEFAULT_LV;

    if (type & LOG_UDP)
    {
        CHECK_IF(dstip == NULL, goto _ERROR, "LOG_UDP dstip is null");
        CHECK_IF(dstport <= 0, goto _ERROR, "LOG_UDP dstport = %d invalid", dstport);

        udp_init(&l->udp, NULL, UDP_PORT_ANY);
        snprintf(l->remote.ipv4, INET_ADDRSTRLEN, "%s", dstip);
        l->remote.port = dstport;
    }

    if (type & LOG_FILE)
    {
        mm_segment_t oldfs = get_fs();

        CHECK_IF(filepath == NULL, goto _ERROR, "LOG_FILE filepath is null");

        snprintf(l->filepath, LOG_FILE_PATH_SIZE, "%s", filepath);

        set_fs(get_ds());
        l->fp = filp_open(filepath, O_RDWR | O_CREAT, 0644);
        set_fs(oldfs);

        CHECK_IF(IS_ERR(l->fp), goto _ERROR, "filp_open failed");
    }
    return l;

_ERROR:
    if (l) kfree(l);
    return NULL;
}

void logger_release(struct logger* l)
{
    CHECK_IF(l == NULL, return, "l is null");

    if (l->running) logger_break(l);

    if (l->type & LOG_UDP)
    {
        udp_uninit(&l->udp);
    }

    if (l->type & LOG_FILE)
    {
        mm_segment_t oldfs = get_fs();

        CHECK_IF(IS_ERR(l->fp), return, "l->fp is err");

        set_fs(get_ds());
        filp_close(l->fp, NULL);
        set_fs(oldfs);

        l->fp = NULL;
    }

    kfree(l);
    return;
}

void logger_run(struct logger* l)
{
    char* msg;

    CHECK_IF(l == NULL, return, "l is null");

    l->running = true;
    while (l->running)
    {
        msg = queue_pop(&l->mq);
        CHECK_IF(msg == NULL, break, "msg is null");

        if (l->type & LOG_PRINTK)
        {
            printk("%s", msg);
        }

        if (l->type & LOG_UDP)
        {
            udp_send(&l->udp, l->remote, msg, strlen(msg)+1);
        }

        if (l->type & LOG_FILE)
        {
            if (!IS_ERR(l->fp))
            {
                mm_segment_t oldfs = get_fs();

                set_fs(get_ds());
                l->fp->f_op->write(l->fp, msg, strlen(msg)+1, &l->fp->f_pos);
                set_fs(oldfs);
            }
            else
            {
                derror("l->fp is err");
                l->running = false;
            }
        }
        kfree(msg);
    }
    l->running = false;
    return;
}

void logger_break(struct logger* l)
{
    CHECK_IF(l == NULL, return, "l is null");
    l->running = false;
    log_print(l, LOG_INVALID, "stop\n");
}

void logger_setlevel(struct logger* l, log_lv_t lv)
{
    CHECK_IF(l == NULL, return, "l is null");
    l->lv = lv;
}

log_lv_t logger_getlevel(struct logger* l)
{
    CHECK_IF(l == NULL, return LOG_INVALID, "l is null");
    return l->lv;
}

void logger_push(struct logger* l, char* str)
{
    CHECK_IF(l == NULL, return, "l is null");
    CHECK_IF(str == NULL, return, "str is null");
    queue_push(&l->mq, str);
}
