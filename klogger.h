#ifndef _KLOGGER_H_
#define _KLOGGER_H_

#include <linux/time.h>

#include "kqueue.h"
#include "kudp.h"

#define LOG_OK (0)
#define LOG_FAIL (-1)

#define LOG_FILE_PATH_SIZE (108)

#define LOG_PRINTK (0x001)
#define LOG_UDP    (0x010)
#define LOG_FILE   (0x100)

#define LOG_DEFAULT_LV (LOG_INFO)

#define log_print(logger, level, msg, param...)\
{\
    if (logger_getlevel(logger) >= level)\
    {\
        struct timeval __tv;\
        char* __tmp;\
        do_gettimeofday(&__tv);\
        __tmp = kasprintf(GFP_ATOMIC, "(%03lds,%03ldms): "msg, __tv.tv_sec%1000, __tv.tv_usec/1000, ##param); \
        logger_push(logger, __tmp);\
    }\
}

typedef enum
{
    LOG_INVALID = 0,
    LOG_FATAL,
    LOG_ERR,
    LOG_WARN,
    LOG_INFO,
    LOG_V1,
    LOG_V2,
    LOG_V3,

    LOG_LEVELS

} log_lv_t;

struct logger;

struct logger* logger_create(int type, char* dstip, int dstport, char* filepath);
void logger_release(struct logger* l);

void logger_run(struct logger* l);
void logger_break(struct logger* l);

void logger_push(struct logger* l, char* str);

void logger_setlevel(struct logger* l, log_lv_t lv);
log_lv_t logger_getlevel(struct logger* l);

#endif //_KLOGGER_H_
