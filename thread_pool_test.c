/*
 * Copyright 2016 Edward Yu
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include "thread_pool.h"

typedef struct test_ctxt {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int val;
} test_ctxt_t;

static void worker(void* args)
{
    test_ctxt_t* ctxt = (test_ctxt_t*)args;
    pthread_mutex_lock(&ctxt->lock);
    while(!ctxt->val)
        pthread_cond_wait(&ctxt->cond, &ctxt->lock);
    --ctxt->val;
    pthread_mutex_unlock(&ctxt->lock);
}

static void finish_job(test_ctxt_t* ctxt, int size)
{
    pthread_mutex_lock(&ctxt->lock);
    ctxt->val = size;
    pthread_mutex_unlock(&ctxt->lock);
    pthread_cond_broadcast(&ctxt->cond);
}

static int start_jobs(tp_context_t* tp_ctxt, void* args, int size)
{
    int i = 0, ret = 0;
    for (i = 0; i < size; ++i) {
        ret = run_job_in_tp(tp_ctxt, worker, args);
        if (ret != 0)
            return ret;
    }
    return 0;
}

void test_basic()
{
    tp_strategy_t strategy = {
        .min = 5,
        .max = 10,
        .inc = 3,
        .des = 3,
        .low = 2
    };

    // test thread pool creation
    tp_context_t* tp_ctxt = create_thread_pool(&strategy);
    if (!tp_ctxt) {
        printf("%s, Fail: create_thread_pool fail.\n", __func__);
        return;
    }
    sleep(1);
    if (strategy.min != tp_ctxt->size || tp_ctxt->size != tp_ctxt->active) {
        printf("%s, Fail: pool size mismatch, expect %d, size %d, active %d\n",
            __func__, strategy.min, tp_ctxt->size, tp_ctxt->active);
        return;
    }

    // test thread pool consumption
    test_ctxt_t ctxt = {
        .lock = PTHREAD_MUTEX_INITIALIZER,
        .cond = PTHREAD_COND_INITIALIZER,
        .val = 0
    };
    int num_jobs = 7, num_th = 8; // 5 + 3
    int ret = start_jobs(tp_ctxt, &ctxt, num_jobs);
    if (0 != ret) {
        printf("%s, Fail: start jobs, ret %d (expect 0).\n", __func__, ret);
        return;
    }
    sleep(1);
    if (num_jobs != tp_ctxt->running) {
        printf("%s, Fail: number of jobs incorrect, expect %d, actual %d.\n", __func__, num_jobs, tp_ctxt->running);
        return;
    }
    if (num_th != tp_ctxt->size || num_th != tp_ctxt->active) {
        printf("%s, Fail: pool size mismatch, expect %d, size %d, active %d\n",
            __func__, num_th, tp_ctxt->size, tp_ctxt->active);
        return;
    }

    // finish strategy.des jobs
    finish_job(&ctxt, strategy.des);
    sleep(1);
    num_jobs -= strategy.des;
    if (num_jobs != tp_ctxt->running) {
        printf("%s, Fail: number of jobs incorrect, expect %d, actual %d.\n", __func__, num_jobs, tp_ctxt->running);
        return;
    }
    // not reach low level yet, pool size should be no change
    if (num_th != tp_ctxt->size || num_th != tp_ctxt->active) {
        printf("%s, Fail: pool size mismatch, expect %d, size %d, active %d\n",
            __func__, num_th, tp_ctxt->size, tp_ctxt->active);
        return;
    }

    // finish mode strategy.low + 1
    finish_job(&ctxt, strategy.low + 1);
    sleep(1);
    num_jobs -= (strategy.low + 1);
    if (num_jobs != tp_ctxt->running) {
        printf("%s, Fail: number of jobs incorrect, expect %d, actual %d.\n", __func__, num_jobs, tp_ctxt->running);
        return;
    }
    num_th -= strategy.des;
    if (num_th != tp_ctxt->size || num_th != tp_ctxt->active) {
        printf("%s, Fail: pool size mismatch, expect %d, size %d, active %d\n",
            __func__, num_th, tp_ctxt->size, tp_ctxt->active);
        return;
    }

    // finish all
    finish_job(&ctxt, num_jobs);
    sleep(1);
    num_jobs = 0;
    if (num_jobs != tp_ctxt->running) {
        printf("%s, Fail: number of jobs incorrect, expect %d, actual %d.\n", __func__, num_jobs, tp_ctxt->running);
        return;
    }
    // reach min threads, should not change
    if (num_th != tp_ctxt->size || num_th != tp_ctxt->active) {
        printf("%s, Fail: pool size mismatch, expect %d, size %d, active %d\n",
            __func__, num_th, tp_ctxt->size, tp_ctxt->active);
        return;
    }

    // test thread pool termination
    if (0 != destroy_thread_pool(tp_ctxt)) {
        printf("%s, Fail: destroy thread pool fail.\n", __func__);
        return;
    }
    printf("%s, Pass.\n", __func__);
}

int main(int argc, char *argv[])
{
    test_basic();
}
