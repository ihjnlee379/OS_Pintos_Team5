1번.
해당 주석 시점에서 thread의 유의미한 process는 전부 끝나고 추가 작업이 없는 idle한 상태로 바뀌어서 sleep을 하고 다음 time block에 wake을 해야하는 것인지 궁급합니다.
void
timer_sleep (int64_t ticks) 
{
  int64_t start = timer_ticks ();

  ASSERT (intr_get_level () == INTR_ON);

  //%%%%%%%%%%%%%%%%%%%%%Is this the point where meaningful processes stop and need to wait?
  int64_t end = start + ticks;
  if (end < timer_ticks()) //time block not finished yet
    thread_yield_sleep (end);
}



2.
해당 함수는 매 틱마다 지속적으로 실행되는 함수인지 궁금합니다.
static void
timer_interrupt (struct intr_frame *args UNUSED)
{
  ticks++; //%%%%%%%%%%%%%%%%%%%%%%%%%% Does this run everytime?
  wake_blocked_thread(timer_ticks());
  thread_tick ();
}



3.
AUX라는 것의 사용처 및 사용법이 궁금합니다.
bool
wake_time_compare(struct thread *a, struct thread *b, void *aux) { //%%%%%%%%%%%%%%%% What is this AUX thing?
  return a->wake_tick < b->wake_tick;
}



4, 5.
먼저 해당 if 구문의 역할과 cur 쓰레드가 idle_thread라는 것의 의미가 궁금합니다.
또한 아래에는 status를 직접 바꾸었는데 thread_block 함수를 사용하지 않고 이렇게 해도 되는지 궁금합니다.
/* Yields the CPU.  The current thread is put to sleep. */
void
thread_yield_sleep (int64_t wake_tick_value) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();


  if (cur != idle_thread) {   //%%%%%%%%%%%%%%%%%%%%%%% What is the purpose of this if statement?
    list_insert_ordered(&blocked_list, &cur->elem, wake_time_compare, NULL);   // fastest wake at first  
    //list_push_back (&blocked_list, &cur->elem);
    cur->wake_tick = wake_tick_value;
  }
  cur->status = THREAD_BLOCKED;
  //%%%%%%%%%%%%%%%%%%% Or Do I have to call thread_block()??

  schedule ();
  intr_set_level (old_level);
}



6.
list의 정보를 불러올 때 list_begin과 list_front를 쓰는 것의 근복적인 차이가 궁금합니다.
void
wake_blocked_thread(int64_t tick) {
  if (list_empty(&blocked_list)) {
    return;
  }
  struct thread *cur = list_entry(list_pop_front(&blocked_list), struct thread, elem); //%%%%%%%%%%%% What is the difference between list_front and list_begin?
  if (cur->wake_tick <= tick) {
    thread_unblock(cur);
  }
  else {
    list_push_front(&blocked_list, &cur->elem);
  }
}
