#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

#define REFRESH_TOTAL_PRIORITY 1        /* HW2 */
#define BOOL_REFRESH_PRIORITY 0
#define FIXED_POINT_MASK 0x4000

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    int initial_priority;                 /* Initial priority */ // 초기 priority, 이건 안바뀜
    //지금 여기에 어떤lock으로부터 얼마의 priority를 받는지에 대한 정보 필요
    //이 정보는 lock으로부터 priority donation을 받을 시 갱신
    struct list donation_list; // multiple 여기에 donation_elem
    struct list_elem donation_elem; // elem로 안쓰고 따로 필요한지는 잘 모르겠으나 allelem로 따로 있으니 일단은 생성
    struct lock *waiting_for_this_lock;          // nested을 위해서 추후 lock 갱신이 필요할 때를 대비해서

    int recent_cpu;                 /* fixed point */
    int nice;                            /* int */


    int64_t wake_tick;                  /* Wake the thread at this tick */
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* HW1 - Alarm clock */
void thread_yield_sleep (int64_t wake_tick_value);
bool wake_time_compare(const struct list_elem *a, const struct list_elem *b, void *aux);
void wake_blocked_thread(int64_t tick);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

/* Hw2 - Priority Scheduling */
void yield_to_max(void);
bool compare_thread_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
bool compare_donation_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
void change_priority(void);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
void set_priority(void);
int calc_priority(int recent_cpu, int nice);
int count_ready_threads(void);
int calc_load_avg(void);
int calc_recent_cpu(int recent_cpu, int nice);
void refresh_all_load_avg_recent_cpu_priority(int bool_refresh);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void ready_list_sort(void);

int convert_to_fixed_point(int n);
int convert_to_int_to_zero(int x);
int convert_to_int_to_nearest(int x);

int add_fixed_and_fixed(int x, int y);
int add_fixed_and_int(int x, int n);
int substract_fixed_and_fixed(int x, int y);
int substract_fixed_and_int(int x, int n);
int multiply_fixed_and_fixed(int x, int y);
int multiply_fixed_and_int(int x, int n);
int divide_fixed_and_fixed(int x, int y);
int divide_fixed_and_int(int x, int n);

#endif /* threads/thread.h */
