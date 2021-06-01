#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);
void check_vaddr (const void *vaddr);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void check_vaddr (const void *vaddr) {
  if (!is_user_vaddr(vaddr)) {
    exit(-1);
  }
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
 // hex_dump(f->esp, f->esp, 100, 1);
  //printf("syscall num : %d\n", *(uint32_t *)(f->esp));
  switch (*(uint32_t *)(f->esp)) {
    case SYS_HALT:
      break;
    case SYS_EXIT:
      //printf("SYS_EXIT\n");
      check_vaddr(f->esp+4);
      exit(*(uint32_t *)(f->esp+4));
      printf("exit done\n");
      break;
    case SYS_EXEC:
      check_vaddr(f->esp+4);
      f->eax = exec((const char *)*(uint32_t *)(f->esp+4));
      break;
    case SYS_WAIT:
      check_vaddr(f->esp+4);
      f->eax = wait((pid_t)*(uint32_t *)(f->esp+4));
      break;
    case SYS_CREATE:
      break;
    case SYS_REMOVE:
      break;
    case SYS_OPEN:
      break;
    case SYS_FILESIZE:
      break;
    case SYS_READ:
      break;
    case SYS_WRITE:
      //printf("SYS_WRITE\n");
      f->eax = write((int)*(uint32_t *)(f->esp+4), (void *)*(uint32_t *)(f->esp+8), (unsigned)*((uint32_t *)(f->esp+12)));
      break;
    case SYS_SEEK:
      break;
    case SYS_TELL:
      break;
    case SYS_CLOSE:
      break; }
    //printf ("system call!\n");
    //thread_exit ();
}

void halt (void) {
  shutdown_power_off();
}

void exit (int status) {
  printf("%s: exit(%d)\n", thread_name(), status);
  thread_exit();
  printf("thread_exit() done\n");
}

pid_t exec (const char *first_word) {
  return process_execute(first_word);
}

int wait (pid_t pid) {
   return process_wait(pid);
}

int read (int fd, void* buffer, unsigned size) {
  int i;
  if (fd == 0) {
    for (i = 0; i < size; i++) {
      if (((char *)buffer)[i] == '\0') {
        break;
      }
    }
  }
  return i;
}

int write (int fd, const void *buffer, unsigned size) {
  struct file *f;
  struct thread *t = thread_current();
  //printf("fd == %d\n", fd);
  if (fd == 1) {
    putbuf(buffer, size);
    //printf("buffer : %s\n", buffer);
    return size;
  }
  return -1;
}
