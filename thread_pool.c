#define _GNU_SOURCE

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "thread_pool.h"

static void* tp_worker(void* args);

// Get a waiting job, this is a blocking call.
//
// Return value:
//  = 0: Success
//  < 0: Fail
static int get_tp_job(tp_context_t* tp_ctxt, th_context_t* th_ctxt)
{
    int ret = read(tp_ctxt->chan_r, th_ctxt, sizeof(th_context_t));
    return ret == sizeof(th_context_t) ? 0 : -1;
}

// Issue a job to thread pool.
//
// Return value:
//  = 0: Success
//  < 0: Fail
static int tp_job_start(tp_context_t* tp_ctxt, th_context_t* th_ctxt)
{
    int ret = write(tp_ctxt->chan_w, th_ctxt, sizeof(th_context_t));
    return ret == sizeof(th_context_t) ? 0 : -1;
}

static int resize_tp(tp_context_t* tp_ctxt, int size)
{
    if (tp_ctxt->size == size)
        return 0;

    int i = 0;

    if (tp_ctxt->size < size) {
        for (i = tp_ctxt->size; i < size; ++i) {
            pthread_t thid; // need't keep track of thread id
            if (0 != pthread_create(&thid, NULL, tp_worker, tp_ctxt)) {
                perror("create thread fail:");
                break;
            }
        }
        tp_ctxt->size = i;
        return 0;
    }

    // below is (tp_ctxt->size > size)
    th_context_t th_ctxt = {.destroy = 1};
    for (i = 0; i < (tp_ctxt->size - size); ++i) {
        if (0 != tp_job_start(tp_ctxt, &th_ctxt))
            break;
    }
    tp_ctxt->size -= i;
    return 0;
}

static void tp_job_done(tp_context_t* tp_ctxt)
{
    int ret = 0;
    pthread_mutex_lock(&tp_ctxt->lock);
    do {
        // 1. decrease running threads
        --tp_ctxt->running;

        // 2. check low water level
        int low_level = tp_ctxt->size - tp_ctxt->strategy.des - tp_ctxt->strategy.low;
        if (tp_ctxt->running < low_level) {
            int resize = tp_ctxt->size - tp_ctxt->strategy.des;
            if (resize < tp_ctxt->strategy.min)
                resize = tp_ctxt->strategy.min;
            resize_tp(tp_ctxt, resize);
        }
    } while(0);
    pthread_mutex_unlock(&tp_ctxt->lock);
}

// Acquire an idle thread, this is a blocking call.
//
// Return value:
//  = 0: Success
//  > 1: Success, and increased pool size
//  < 0: Fail
static int tp_acquire_thread(tp_context_t* tp_ctxt)
{
    int ret = 0;
    pthread_mutex_lock(&tp_ctxt->lock);
    do {
        // 1. pre-occupy, i.e., increate running job but not more than pool size
        if (tp_ctxt->running < tp_ctxt->size) {
            ++tp_ctxt->running;
            break;
        }

        // 2. if 1. not available, try to increase pool size
        if (tp_ctxt->size >= tp_ctxt->strategy.max) {
            ret = -1;
            break;
        }
        int resize = tp_ctxt->size + tp_ctxt->strategy.inc;
        if (resize > tp_ctxt->strategy.max)
            resize = tp_ctxt->strategy.max;
        resize_tp(tp_ctxt, resize);
        if (tp_ctxt->running < tp_ctxt->size) {
            ++tp_ctxt->running;
            ret = 1;
            break;
        }
        ret = -1;
    } while(0);
    pthread_mutex_unlock(&tp_ctxt->lock);

    return ret;
}

static void* tp_worker(void* args)
{
    tp_context_t* tp_ctxt = (tp_context_t*)args;
    th_context_t ctxt = {0};
    pthread_mutex_lock(&tp_ctxt->lock);
    ++tp_ctxt->active;
    pthread_mutex_unlock(&tp_ctxt->lock);

    while(0 == get_tp_job(tp_ctxt, &ctxt)) {
        if (ctxt.destroy)
            break;
        if (ctxt.worker)
            ctxt.worker(ctxt.args);

        tp_job_done(tp_ctxt);
    }

    pthread_mutex_lock(&tp_ctxt->lock);
    --tp_ctxt->active;
    pthread_mutex_unlock(&tp_ctxt->lock);
}

tp_context_t* create_thread_pool(tp_strategy_t* strategy)
{
    tp_context_t* tp_ctxt = (tp_context_t*)malloc(sizeof(tp_context_t));
    if (!tp_ctxt)
        return NULL;
    memset(tp_ctxt, 0, sizeof(tp_context_t));
    memcpy(&tp_ctxt->strategy, strategy, sizeof(tp_strategy_t));

    int cpu_online = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_online <= 0)
        cpu_online = 1;

    if (0 == tp_ctxt->strategy.min)
        tp_ctxt->strategy.min = cpu_online;
    if (0 == tp_ctxt->strategy.max)
        tp_ctxt->strategy.max = 2 * cpu_online;

    // init context lock 
    pthread_mutex_init(&tp_ctxt->lock, NULL);

    // init communication channel
    int chan[2] = {0};
    if (0 != pipe(chan))
        return NULL; // TODO: free memory
    tp_ctxt->chan_r = chan[0];
    tp_ctxt->chan_w = chan[1];

    // create threads
    resize_tp(tp_ctxt, tp_ctxt->strategy.min);
    return tp_ctxt;
}

int destroy_thread_pool(tp_context_t* tp_ctxt)
{
    close(tp_ctxt->chan_w);
    close(tp_ctxt->chan_r);

    int i = 0, retry = 5;
    for (i = 0; i < retry; ++i) {
        if (0 == tp_ctxt->active)
            break;
        sleep(1);
    }
    if (i < retry) {
        pthread_mutex_destroy(&tp_ctxt->lock);
        free(tp_ctxt);
        return 0;
    }
    perror("Not all threads stopped");
    return -1;
}

int run_job_in_tp(tp_context_t* tp_ctxt, th_worker worker, void* args)
{
    th_context_t th_ctxt = {0, worker, args};
    int ret = tp_acquire_thread(tp_ctxt);
    if (ret >= 0)
        return tp_job_start(tp_ctxt, &th_ctxt);
    return ret;
}