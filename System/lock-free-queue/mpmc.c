#include "mpmc.h"
#include <asm-generic/errno-base.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// 如果对于轮转不清楚,可以手动模拟一下
int queue_init(mpmc_queue_t *q, size_t capacity) {
    if (capacity < 2 || (capacity & (capacity - 1)) != 0) {
        return -1;
    }

    q->buffer_mask = capacity - 1;
    q->buffer = (cell_t*)malloc(sizeof(cell_t) * capacity);
    if (!q->buffer) {
        return -1;
    }

    for (int index = 0; index < capacity; ++index) {
        atomic_init(&q->buffer[index].sequence_number, index);
    }

    atomic_init(&q->enqueue_pos, 0);
    atomic_init(&q->dequeue_pos, 0);

    return 0;
}

// 可以加上重试次数或者随机sleep来解决竞争比较激烈的时候的问题.
int queue_enqueue(mpmc_queue_t *q, void *data) {
    cell_t* cell;
    // 这里相当于是一个快照
    size_t pos = atomic_load_explicit(&q->enqueue_pos, memory_order_relaxed);

    for (;;) {
        cell = &(q->buffer[pos & q->buffer_mask]);
        // ensure after this clause we have right status of seq
        size_t seq = atomic_load_explicit(&cell->sequence_number, memory_order_acquire);

        intptr_t diff = (intptr_t)seq - (intptr_t)pos;

        if (diff == 0) {
            // 先CAS更改全局序列号为pos + 1 然后出来
            if(atomic_compare_exchange_weak_explicit(&q->enqueue_pos, &pos, pos + 1, 
                memory_order_relaxed, memory_order_relaxed)) {
                    break;
            }
        } else if (diff < 0) {
            // 当前pos很大,但是seq还没有来得及被消费
            return 0;
        }
        else{
            // 中间被其他的生产者抢走了
            pos = atomic_load_explicit(&q->enqueue_pos, memory_order_relaxed);
        }
    }

    // 一定需要在buffer写入之后才能更新seq
    cell->data = data;
    // 更改cell序列号为pos + 1
    atomic_store_explicit(&cell->sequence_number, pos + 1, memory_order_release);
    return 1;
} 

// 释放dequeue_pos的cell位置
// 可以模拟一下这个过程,我最近需要释放的就是seq = dequeue_pos + 1
int queue_dequeue(mpmc_queue_t *q, void **data) {
    if (!data) {
        return -1;
    }
    cell_t* cell;
    size_t pos = atomic_load_explicit(&q->dequeue_pos, memory_order_relaxed);

    for (;;) {
        cell = &q->buffer[pos & q->buffer_mask];
        // 保证获取的seq是准确的
        size_t seq = atomic_load_explicit(&cell->sequence_number, memory_order_acquire);

        intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);

        // CPU分支预测可能大概率走这里的if else.
        if (diff == 0) {
            if (atomic_compare_exchange_weak_explicit(&q->dequeue_pos, &pos, pos + 1, 
                memory_order_relaxed, memory_order_relaxed)) {
                    break;
                }
        } else if (diff < 0) {
            // 队列满了
            return 0;
        } else {
            pos = atomic_load_explicit(&q->dequeue_pos, memory_order_relaxed);
        }
    }

    // 如果没有前面的acquire,可能代码会直接跳转到这里执行
    *data = cell->data;
    // 这里加上一整圈即可
    // 最精妙的就在于生产者消费者的序列号问题.
    // 生产者每次会加一,消费者每次竞争的位置就会刚好落后生产者1
    // 每次消费者消费结束之后会给数字整个加上一轮
    atomic_store_explicit(&cell->sequence_number, pos + q->buffer_mask + 1, memory_order_release);
    return 1;
} 

int queue_destroy(mpmc_queue_t* q) {
    if (q == NULL) {
        errno = EINVAL;
        return -1;
    }

    free(q->buffer);
    q->buffer = NULL;
    q->buffer_mask = 0;

    return 0;
}


