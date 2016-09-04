# Thread Pool in C
A library to create thread pool with the capability of increasing/decreasing thread numbers dynamically accordance with strategy.

### How to use?

Create a thread pool

```
    // prepare thread pool strategy
    tp_strategy_t strategy = {
        .min = 5,   // minimum thread numbers
        .max = 10,  // maximum thread numbers
        .inc = 3,   // increase numbers of thread when all threads are running
        .des = 3,   // decrease numbers of thread when reach low water level
        .low = 2    // low water level, when idle threads is greater than "des" + "low",
                    // "des" idle threads will be dropped.
    };

    // create thread pool
    tp_context_t* tp_ctxt = create_thread_pool(&strategy);
```

Running a job in thread pool

```
    int ret = run_job_in_tp(tp_ctxt, worker, args);
```

Destroy a thread pool

```
    int ret = destroy_thread_pool(tp_ctxt);
```

For detail example, please refer to test program, [thread_pool_test.c](https://github.com/yhyu/thread_pool_c/blob/master/thread_pool_test.c).
