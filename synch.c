/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}


bool compare_sema_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
  struct semaphore_elem *a_sema = list_entry(a, struct semaphore_elem, elem);
  struct semaphore_elem *b_sema = list_entry(b, struct semaphore_elem, elem);

  struct list *a_list = &(a_sema->semaphore.waiters);
  struct list *b_list = &(b_sema->semaphore.waiters);

  return list_entry(list_begin(a_list), struct thread, elem)->priority > list_entry(list_begin(b_list), struct thread, elem)->priority;

} // semaphore의 thread priority 비교 (가장 높은 priority의 thread를 가진 semaphore를 깨우기 위함). a > b이면 true

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0) 
    {
      //list_push_back (&sema->waiters, &thread_current ()->elem);
      list_insert_ordered (&sema->waiters, &thread_current ()->elem, compare_thread_priority, 0); //wake되는 thread의 priority 정렬
      thread_block ();
    }
  sema->value--;
  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) 
    {
      sema->value--;
      success = true; 
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (!list_empty (&sema->waiters)) {
    //if(!thread_mlfqs) {
      list_sort(&sema->waiters, &compare_thread_priority, NULL); // waiters list를 priority 순서대로 정렬
    //}
    thread_unblock (list_entry (list_pop_front (&sema->waiters),
                                struct thread, elem));
  }
  sema->value++;
  intr_set_level (old_level);
  yield_to_max(); // 더 높은 priority 가진 thread가 있다면 yield(preemption)
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) 
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) 
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) 
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) 
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));
  
  struct thread *cur = thread_current ();

  //lock holder가 없으면 holder가 NULL임
  if(lock->holder != NULL) {
    cur->waiting_for_this_lock = lock;
    //priority donation 필요 상황
      // 단계: init보다 높으면 일단 넣고나서 최종 priority 계산
      // thread_set_priority 때문에 그냥 다 넣어야할듯
      list_insert_ordered (&lock->holder->donation_list, &cur->donation_elem, compare_donation_priority, 0);  // priority

      if (!thread_mlfqs) {
        set_priority_for_lock_holder(lock, DEFAULT_DEPTH, DEFAULT_BOOL_DEPTH);
      }

      //donate(lock->holder); //
    // 단순히 이렇게 donation하면 이것이 추후에도 괜찮을지 판단 필요
    // (이것이 영구적인 효과일지 일시적인 효과일지 판단 필요) 
    // 아마 release에서 원상태로 돌려놓아야함

    // 기본적인 thread struct의 개편이 필요함
    // 대략 현재 priority, 초기 priority, priority donation 받게 되는 lock과 값
  }

  
  sema_down (&lock->semaphore);
  cur->waiting_for_this_lock = NULL;
  lock->holder = thread_current ();
}

/* Priority donation */
void
set_priority_for_lock_holder (struct lock *lock, int depth, int bool_depth) {
//depth 구현되어 있으나 굳이 구현해야 하나? bool_depth 추가해서 depth 기능 on/off
  if(bool_depth && depth <= 0) {
    return;
  }
  else {
    depth--;
  }
  int before = lock->holder->priority;
  lock->holder->priority = lock->holder->initial_priority;
  if(!list_empty(&lock->holder->donation_list)) {
    //시작 시 sorting을 하고 하면 편할 듯
    list_sort(&lock->holder->donation_list, compare_donation_priority, NULL); // 정렬(multipli donation 처리)
    struct thread *t = list_entry (list_begin(&lock->holder->donation_list), struct thread, donation_elem); // 정렬되어 있으니 앞에 있는 것 하나만 체크
    if(t->priority > lock->holder->priority) {
        lock->holder->priority = t->priority;
        ready_list_sort();
    }
    if(lock->holder->priority != before && lock->holder->waiting_for_this_lock != NULL) {
      set_priority_for_lock_holder(lock->holder->waiting_for_this_lock, depth, bool_depth); // (nest donation 처리)
    }
    /* 굳이 이렇게 전체를 확인해야하는걸까? 애초에 donation_list는 정렬되어 있는데?
    struct list_elem *e = list_begin (&lock->holder->donation_list);
    while (e!=list_end(&thread_current->donation_list) { // current thread의 donation 리스트 순회
      struct thread *t = list_entry (e, struct thread, donation_elem);
      if(t->priority > lock->holder->priority) {
        lock->holder->priority = t->priority;
      }
      e = list_next(e);
    }
    */

  }
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  success = sema_try_down (&lock->semaphore);
  if (success)
    lock->holder = thread_current ();
  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) 
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

  lock_remove(lock); // 애초에 순차적으로 지우니깐 이 결과도 sorted
  // lock release하면 현재 priority하고 만약 이게 또 다른 lock을 기다리면 그쪽에 priority 다시 갱신 필요
  if (!thread_mlfqs) {
    set_priority_for_lock_holder(lock, DEFAULT_DEPTH, DEFAULT_BOOL_DEPTH); // 이걸로 가능할듯 그럴려면 lock holder 아직은 초기화하면 안됨
  }
  lock->holder = NULL;
  sema_up (&lock->semaphore);
}

/* Remove from donation list */
void lock_remove(struct lock *l) {
// thread_current()->donation_list 가 empty이면 안됨
  if(list_empty(&thread_current()->donation_list)) {
    return;
  }
  struct list_elem *e = list_begin (&thread_current()->donation_list);

  while (e!=list_end(&thread_current()->donation_list)) { // current thread의 donation 리스트 순회
    struct thread *t = list_entry (e, struct thread, donation_elem);
    if (t->waiting_for_this_lock == l) // donation의 element가 lock을 소유했다면 리스트에서 삭제
      list_remove(e);
    e = list_next(e);
  }
}


/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}



/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0);
  //list_push_back (&cond->waiters, &waiter.elem);
  list_insert_ordered (&cond->waiters, &waiter.elem, compare_sema_priority, 0); 
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters)) {
    list_sort(&cond->waiters, compare_sema_priority, NULL); // waiters list를 priority 순서대로 정렬
    sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
  }
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}
