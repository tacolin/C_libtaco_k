#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>

#include "kqueue.h"

#define derror(a, b...) printk("[ERROR] %s(): "a"\n", __func__, ##b)
#define CHECK_IF(assertion, error_action, ...) \
{\
    if (assertion) \
    { \
        derror(__VA_ARGS__); \
        {error_action;} \
    }\
}

#define LOCK(q) down_interruptible(&q->lock)
#define UNLOCK(q) up(&q->lock)

int queue_init(struct queue* q, int depth, void (*cleanfn)(void*), int flag)
{
    CHECK_IF(q == NULL, return QUEUE_FAIL, "q is null");
    CHECK_IF(depth == 0, return QUEUE_FAIL, "depth is %d", depth);

    q->head = NULL;
    q->tail = NULL;
    q->depth = depth;
    q->num = 0;
    sema_init(&(q->num_sem), 0);
    q->cleanfn = cleanfn;
    sema_init(&q->lock, 1);
    q->flag = flag;
    if (depth > 0)
    {
        sema_init(&(q->empty_sem), depth);
    }
    else
    {
        sema_init(&(q->empty_sem), 0);
        q->flag &= ~QUEUE_FLAG_PUSH_BLOCK;
    }
    return QUEUE_OK;
}

void queue_clean(struct queue* q)
{
    struct queue_node* node;
    struct queue_node* next;
    CHECK_IF(q == NULL, return, "q is null");

    LOCK(q);
    node = q->head;
    while (node)
    {
        next = node->next;
        if (q->cleanfn)
        {
            q->cleanfn(node->data);
        }
        kfree(node);
        node = next;
    }
    q->num  = 0;
    q->head = NULL;
    q->tail = NULL;
    sema_init(&(q->num_sem), 0);
    if (q->depth > 0)
    {
        sema_init(&(q->empty_sem), q->depth);
    }
    else
    {
        sema_init(&(q->empty_sem), 0);
    }
    UNLOCK(q);
    return;
}

int queue_push(struct queue* q, void* data)
{
    struct queue_node* node;
    CHECK_IF(q == NULL, return QUEUE_FAIL, "q is null");
    CHECK_IF(data == NULL, return QUEUE_FAIL, "data is null");

    if (q->flag & QUEUE_FLAG_PUSH_BLOCK)
    {
        down_interruptible(&(q->empty_sem));
    }
    else
    {
        if ((q->depth > 0) && (q->num >= q->depth)) // queue full
        {
            return QUEUE_FAIL;
        }
    }

    node       = kzalloc(sizeof(struct queue_node), GFP_ATOMIC);
    node->data = data;

    LOCK(q);
    if (q->tail)
    {
        q->tail->next = node;
        q->tail       = node;
    }
    else
    {
        q->tail = node;
        q->head = node;
    }

    q->num++;

    if (q->flag & QUEUE_FLAG_POP_BLOCK)
    {
        up(&(q->num_sem));
    }
    UNLOCK(q);
    return QUEUE_OK;
}

void* queue_pop(struct queue* q)
{
    struct queue_node *node;
    void* data;

    CHECK_IF(q == NULL, return NULL, "q is null");

    if (q->flag & QUEUE_FLAG_POP_BLOCK)
    {
        down_interruptible(&(q->num_sem));
    }
    else if (q->num <= 0)
    {
        CHECK_IF(q->head, return NULL, "q is empty but head exists");
        CHECK_IF(q->tail, return NULL, "q is empty but tail exists");
        return NULL;
    }

    CHECK_IF(q->head == NULL, return NULL, "num = %d, but head is NULL", q->num);
    CHECK_IF(q->tail == NULL, return NULL, "num = %d, but tail is NULL", q->num);

    LOCK(q);
    node = q->head;
    if (node->next)
    {
        q->head = node->next;
    }
    else
    {
        q->head = NULL;
        q->tail = NULL;
    }
    q->num--;

    if (q->flag & QUEUE_FLAG_PUSH_BLOCK)
    {
        up(&(q->empty_sem));
    }
    UNLOCK(q);

    data = node->data;
    kfree(node);
    return data;
}

int queue_num(struct queue* q)
{
    CHECK_IF(q == NULL, return -1, "q is null");
    return q->num;
}
