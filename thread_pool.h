#include <pthread.h>

typedef void (*th_worker)(void* args);

typedef struct tp_strategy {
    int min;    // minimum thread numbers
    int max;    // maximum thread numbers
    int inc;    // increase numbers of thread when all threads are running
    int des;    // decrease numbers of thread when reach low water level
    int low;    // low water level, when idle threads is greater than "des" + "low",
                // "des" idle threads will be dropped.
} tp_strategy_t;

typedef struct tp_context {
    tp_strategy_t strategy;
    int size;               // pool size
    int active;             // active threads, should be equal to pool size
    int running;            // number of running threads
    int chan_r;             // communication channel: read size
    int chan_w;             // communication channel: write size
    pthread_mutex_t lock;   // mutex lock for the context
} tp_context_t;

typedef struct th_context {
    int destroy;
    th_worker worker;
    void* args;
} th_context_t;

tp_context_t* create_thread_pool(tp_strategy_t* strategy);

int destroy_thread_pool(tp_context_t* tp_ctxt);

int run_job_in_tp(tp_context_t* tp_ctxt, th_worker worker, void* args);
