#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#ifdef USERPROG
#include "userprog/process.h"

#endif 
#include "threads/fixed_point.c"

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of processes in THREAD_BLOCKED state, that is, processes
   that are sleeping. */
static struct list blocked_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* HW2 */
static int load_avg;

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;
bool thread_aging;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);
  list_init (&blocked_list); // initiate blocked list

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
  initial_thread->recent_cpu = 0; // float 0 == int 0
  initial_thread->nice = 0;

}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void //^^^^^^^^^^^^^^ So this runs every tick. this is where "Waking func" should be called.
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL) {
    user_ticks++;
  }
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  enum intr_level old_level;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  // Initialize fd_table
  /*
  t->fd_table = palloc_get_multiple(PAL_ZERO,2);
  if (t->fd_table == NULL) {
    palloc_free_page(t);
    return TID_ERROR;
  }
  */

  /* Prepare thread for first run by initializing its stack.
     Do this atomically so intermediate values for the 'stack' 
     member cannot be observed. */
  old_level = intr_disable ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  intr_set_level (old_level);

  /* Add to run queue. */
  thread_unblock (t);

  if (t->priority > thread_get_priority())
    thread_yield(); // 새로 생성된 스레드 t가 current thread보다 priority가 높으면 cpu 양보

  return tid;
}

/* 스레드들의 priority를 비교하는 함수. priority of a > b이면 true 반환 */
bool compare_thread_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
  return list_entry (a, struct thread, elem)->priority > list_entry (b, struct thread, elem)->priority;
}

bool compare_donation_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
return list_entry (a, struct thread, donation_elem)->priority > list_entry (b, struct thread, donation_elem)->priority;
} 



/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  //list_push_back (&ready_list, &t->elem);
  
  list_insert_ordered (&ready_list, &t->elem, compare_thread_priority, 0); // priority 순으로 ready list에 삽입
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif
  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  printf("schedule---\n");
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();


  if (cur != idle_thread) 
    //list_push_back (&ready_list, &cur->elem);
    list_insert_ordered (&ready_list, &cur->elem, compare_thread_priority, 0);  // priority 순으로 ready list에 삽입
  cur->status = THREAD_READY;


  schedule ();
  intr_set_level (old_level);
}

/* faster wake_tick is true for the sorting */
bool
wake_time_compare(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
  const struct thread *a_th = list_entry(a, struct thread, elem);
  const struct thread *b_th = list_entry(b, struct thread, elem);
  // Compare ticks to sort the blocked_list
  return a_th->wake_tick < b_th->wake_tick;
}

/* Yields the CPU.  The current thread is put to sleep. */
void
thread_yield_sleep (int64_t wake_tick_value) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ()); 

  old_level = intr_disable ();

  ASSERT (intr_get_level () == INTR_OFF); // Interrupts must be turned on

  if (cur != idle_thread) {
    cur->wake_tick = wake_tick_value; // Set current thread's wake_tick to wake_tick_value
    cur->status = THREAD_BLOCKED; // Block current thread
    list_insert_ordered(&blocked_list, &cur->elem, wake_time_compare, NULL);
    // Insert current thread to blocked_list, shortest wake_tick fisrt
    schedule();
  }
 
  intr_set_level (old_level);
}

/* Wake the thread when it is the right time. It should be called every tick to check */
void
wake_blocked_thread(int64_t tick) {
  
  if (list_empty(&blocked_list)) {
    return;
  }

  struct list_elem *wake_turn;
  struct thread *cur;
  wake_turn = list_begin(&blocked_list);
  cur = list_entry(wake_turn, struct thread, elem);
  
  while (cur->wake_tick <= tick) { // After current thread's wake_tick
    wake_turn = list_remove(&cur->elem); // Remove current thread from blocked_list
    thread_unblock(cur); // Unblock current thread
    if (list_empty(&blocked_list))
       return;
    cur = list_entry(wake_turn, struct thread, elem); // Move to the next thread
  }
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* ready list에서 priority가 가장 높은 thread와 current thread를 비교. ready list의 thread가 priority 높으면 cpu 양보 */
void
yield_to_max (void) {

  //struct thread *first;
  //first = list_entry (list_front (&ready_list), struct thread, elem);

  //if (list_empty(&ready_list))
  //  return;

  if (!list_empty(&ready_list) && thread_current ()->priority < list_entry (list_front (&ready_list), struct thread, elem)->priority) {
    thread_yield ();
  }
}

/* ready list sorting */
void ready_list_sort(void) {
  if(list_empty(&ready_list)) {
    return;
  }
  list_sort(&ready_list, compare_thread_priority, NULL);
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  if (thread_mlfqs)
    return;

  thread_current ()->initial_priority = new_priority;
  change_priority();
  yield_to_max(); // priority 변경이 일어났는지 체크하고, 변경이 일어났다면 yield
}

void change_priority(void) {
  struct thread *t = thread_current();
  t->priority = t->initial_priority;

  if (!list_empty(&t->donation_list)) {
    list_sort(&t->donation_list, compare_donation_priority, NULL); // sorting 필요?? 필요하다면 donation element들을 비교하는 함수를 추가 구현해야 하는지 아니면 compare thread priority 함수로 sorting 할 수 있는지 확인필요
    struct thread *first = list_entry(list_front(&t->donation_list), struct thread, donation_elem);
    if (first->priority > t->priority) { // donation 리스트에서 가장 priority가 높은 thread와 현재 thread 비교. 높은 값을 현재 thread의 priority로 설정
      t->priority = first->priority;
      ready_list_sort();
    }
    if(t->waiting_for_this_lock != NULL) {
      set_priority_for_lock_holder(t->waiting_for_this_lock, DEFAULT_DEPTH - 1, DEFAULT_BOOL_DEPTH);
    }
  }
}

int calc_priority(int recent_cpu, int nice) {
  int result = substract_fixed_and_fixed(substract_fixed_and_fixed(convert_to_fixed_point(PRI_MAX), divide_fixed_and_int(recent_cpu, 4)), multiply_fixed_and_int(convert_to_fixed_point(nice), 2));
  result = convert_to_int_to_nearest(result);
  if (result > PRI_MAX) {
    result = PRI_MAX;
  }
  else if (result < PRI_MIN) {
    result = PRI_MIN;
  }
  return result;
}



/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) 
{
  enum intr_level before = intr_disable();     
  thread_current()->nice = nice;
  thread_current()->priority = calc_priority(thread_current()->recent_cpu, thread_current()->nice);
  intr_set_level(before);
  yield_to_max(); // 알아서 시간 지나면 피드백 되니 이게 없어야 할수도
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  enum intr_level before = intr_disable();  //  threads/interrupt.h에 정의                               
  int result = thread_current()->nice; 
  intr_set_level(before);
  return result;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  enum intr_level before = intr_disable();         
  int result = convert_to_int_to_nearest(multiply_fixed_and_int(load_avg, 100));
  intr_set_level(before);
  return result; 

}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  enum intr_level before = intr_disable(); 
  // return convert_to_int_to_nearest(multiply_fixed_and_int(thread_current()->recent_cpu, 100));
  int result = convert_to_int_to_nearest(multiply_fixed_and_int(thread_current()->recent_cpu, 100)); 
  intr_set_level(before);
  return result; 

}

void
set_priority(void) {
  int max_priority = -1;
  struct thread *t;
  struct thread *r;
  struct list_elem *e;
  if(REFRESH_TOTAL_PRIORITY ) {
    for (e=list_begin(&all_list); e!=list_end(&all_list); e=list_next(e)) {
        t = list_entry (e, struct thread, allelem);
        t->priority = calc_priority(t->recent_cpu,t->nice);
    }
  
    if (!list_empty(&ready_list)) {
        r = list_entry(list_front(&ready_list), struct thread, elem);
        max_priority = r->priority;
    }
  
    if (thread_current()->priority < max_priority)
      intr_yield_on_return();
  }
  else {
    t = thread_current();
    if (t == idle_thread) { 
        return;
      }
    t->priority = calc_priority (t->recent_cpu, t->nice);
  }
}


int count_ready_threads() {
  int result = 0;
  /*
  if(!list_empty(&all_list)) {
    struct list_elem *e = list_begin (&all_list);
    while (e!=list_end(&all_list)) { // current thread의 donation 리스트 순회
      struct thread *t = list_entry (e, struct thread, allelem);
      if((t->status == THREAD_RUNNING && t != idle_thread) || t->status == THREAD_READY) {
        result++;
      }
      e = list_next(e);
    }
  }
  */
  result = result + list_size(&ready_list);
  if (thread_current() != idle_thread) {
    result = result + 1;
  }
  return result;
}

int calc_load_avg() {
  return add_fixed_and_fixed(divide_fixed_and_int(multiply_fixed_and_int(load_avg, 59), 60), divide_fixed_and_int(convert_to_fixed_point(count_ready_threads()), 60));
}

int calc_recent_cpu(int recent_cpu, int nice) {
  return add_fixed_and_int(multiply_fixed_and_fixed(divide_fixed_and_fixed(multiply_fixed_and_int(load_avg, 2), add_fixed_and_int(multiply_fixed_and_int(load_avg, 2), 1)), recent_cpu), nice);
}

void refresh_all_load_avg_recent_cpu_priority(int bool_refresh) {
  load_avg = calc_load_avg();
  //enum intr_level before = intr_disable();
  if(!list_empty(&all_list)) {
    struct list_elem *e = list_begin (&all_list);
    while (e!=list_end(&all_list)) { // current thread의 donation 리스트 순회
      struct thread *t = list_entry (e, struct thread, allelem);
      if (t == idle_thread) {
        continue;
      }
      t->recent_cpu = calc_recent_cpu(t->recent_cpu, t->nice);
      if(bool_refresh) {
        t->priority = calc_priority(t->recent_cpu, t->nice);
      }
      e = list_next(e);
    }
  }
  //intr_set_level(before);
}

void recent_cpu_increase() {
  if(thread_current() != idle_thread) {
    thread_current()->recent_cpu += add_fixed_and_int(thread_current()->recent_cpu, 1);
  }
}

void
update_load_avg (void)
{
  int ready_threads = list_size(&ready_list); // ready threads 개수

  if (thread_current() != idle_thread) // running 상태
    ready_threads += 1;

  load_avg = divide_fixed_and_int(add_fixed_and_int(multiply_fixed_and_int(load_avg, 59), ready_threads), 60); // 최근 1분 동안 수행 가능한 프로세스의 평균 개수 = load_avg = (59/60) * load_avg + (1/60) * ready_threads
}

void
update_recent_cpu (void)
{
  if (thread_current() == idle_thread)
    return;

  struct thread *t;
  struct list_elem *e;
  
  for (e=list_begin(&all_list); e!=list_end(&all_list); e=list_next(e)) {
    t = list_entry (e, struct thread, allelem);
    if (t != idle_thread) {
      t->recent_cpu = add_fixed_and_int(multiply_fixed_and_fixed(divide_fixed_and_fixed((multiply_fixed_and_int(load_avg, 2)), (add_fixed_and_int(multiply_fixed_and_int(load_avg, 2), 1))), t->recent_cpu), t->nice);  // recent cpu = (2 * load_avg) / (2 * load_avg + 1) * recent_cpu + nice

}
}
}
/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  int i;
  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->magic = THREAD_MAGIC;
  list_push_back (&all_list, &t->allelem);

  /* HW2 */
  t->initial_priority = priority;
  list_init(&t->donation_list); 
  t->waiting_for_this_lock =NULL;

  t->recent_cpu = running_thread()->recent_cpu; // 현재 실행되고 있는 thread에서 children 생성
  t->nice = running_thread()->nice;

#ifdef USERPROG
  t->parent = running_thread();
  sema_init(&(t->sema_child), 0);
  sema_init(&(t->sema_mem), 0);
  sema_init(&(t->exec_lock), 0);
  list_init(&(t->child));
  list_push_back(&(running_thread()->child), &(t->child_e));
#endif
  for (i = 0; i < 128; i++) {                                                         
      t->fd_table[i] = NULL;                                                                
  } 
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));
 
  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
