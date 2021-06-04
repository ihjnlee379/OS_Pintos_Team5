#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
//#include "userprog/syscall.h"
#include "filesys/off_t.h"

static void syscall_handler (struct intr_frame *);
void check_vaddr (const void *vaddr);
struct lock f_lock;


struct file {
  struct inode *inode;
  off_t pos;
  bool deny_write;
};

void
syscall_init (void) 
{
  lock_init(&f_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void check_vaddr (const void *vaddr) {
  if (!is_user_vaddr(vaddr)) {
    //printf("found\n");
    exit(-1);
  }
}

static void
syscall_handler (struct intr_frame *f)
{
/*
  if((f->esp) == NULL) {
    exit(-1);
  }
  printf("%x\n", f->esp);
  hex_dump(f->esp, f->esp, 100, 1);
  printf("%d\n", *(uint32_t *)f->esp);
*/
//printf("dfdfdf\n");
 // hex_dump(f->esp, f->esp, 100, 1);
  //printf("syscall num : %d\n", *(uint32_t *)(f->esp));
  switch (*(uint32_t *)(f->esp)) {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      //printf("SYS_EXIT\n");
      check_vaddr(f->esp+4);
      exit(*(uint32_t *)(f->esp+4));
      //printf("exit done\n");
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
      check_vaddr(f->esp+4);
      check_vaddr(f->esp+8);
      f->eax = create((const char *)*(uint32_t *)(f->esp+4), (unsigned)*(uint32_t *)(f->esp+8));
      break;
    case SYS_REMOVE:
      check_vaddr(f->esp+4);
      f->eax = remove((const char*)*(uint32_t *)(f->esp+4));
      break;
    case SYS_OPEN:
      check_vaddr(f->esp+4);
      f->eax = open((const char *)*(uint32_t *)(f->esp+4));
      break;
    case SYS_FILESIZE:
      check_vaddr(f->esp+4);
      f->eax = filesize((int)*(uint32_t *)(f->esp+4));
      break;
    case SYS_READ:
      //exit(-1);
      check_vaddr(f->esp+4);
      check_vaddr(f->esp+8);
      check_vaddr(f->esp+12);
      f->eax = read((int)*(uint32_t *)(f->esp + 4), (const void *)*(uint32_t *)(f->esp + 8), (unsigned)*((uint32_t *)(f->esp + 12)));
      break;
    case SYS_WRITE:
      check_vaddr(f->esp+4);
      check_vaddr(f->esp+8);
      check_vaddr(f->esp+12);
      //printf("SYS_WRITE\n");
      f->eax = write((int)*(uint32_t *)(f->esp+4), (void *)*(uint32_t *)(f->esp+8), (unsigned)*((uint32_t *)(f->esp+12)));
      break;
    case SYS_SEEK:
      check_vaddr(f->esp+4);
      check_vaddr(f->esp+8);
      seek((int)*(uint32_t *)(f->esp+4), (unsigned)*(uint32_t *)(f->esp+8));
      break;
    case SYS_TELL:
      check_vaddr(f->esp+4);
      f->eax = tell((int)*(uint32_t *)(f->esp+4));
      break;
    case SYS_CLOSE:
      check_vaddr(f->esp+4);
      close((int)*(uint32_t *)(f->esp+4));
      break; 
    default:
      exit(-1);
  }
}
    //printf ("system call!\n");
    //thread_exit ();

void halt (void) {
  shutdown_power_off();
}


void exit (int status) {
  printf("%s: exit(%d)\n", thread_name(), status);
  thread_exit();
  //printf("thread_exit() done\n");
}


pid_t exec (const char *first_word) {
  int result;
  if (first_word) {
    return -1;
  }
  lock_acquire(&f_lock); 
  result = process_execute(first_word);
  lock_release(&f_lock);
  return result;
}

 
int filesize(int fd) {
   if (thread_current()->fd_table[fd] == NULL) {
     exit (-1);
   }
   return file_length(thread_current()->fd_table[fd]);
}


int wait (pid_t pid) {
   return process_wait(pid);
}


int read (int fd, void *buffer, unsigned size) {
  int i;
  struct file *f;
  check_vaddr(buffer);
  lock_acquire(&f_lock);
  if (fd == 0) { 
    for (i = 0; i != size; i++) {
      if (((char *)buffer)[i] == '\0')
        break;
    } 
    lock_release(&f_lock);
    return i;
  }
  else { 
    if(!(f = thread_current()->fd_table[fd])) {
      lock_release(&f_lock);
      exit(-1);
    }
    if (size = file_read(f, buffer, size)) {
      lock_release(&f_lock);
      return size;
    }
    lock_release(&f_lock);
    return 0;
  }
}


int write (int fd, const void *buffer, unsigned size) {
  struct file *f;
  struct thread *t = thread_current();
  lock_acquire(&f_lock);
  if (fd == 1) {
    putbuf(buffer, size);
    lock_release(&f_lock);
    return size;
  }
  else if (fd > 2) {
    struct file *t_file = t->fd_table[fd];
    if (t_file == NULL) {
      lock_release(&f_lock);
      exit(-1);
    }
    if (t_file->deny_write) {
      file_deny_write(t_file);
    }
    lock_release(&f_lock);
    return file_write(t_file, buffer, size);
  }
  lock_release(&f_lock);
  return -1;
} 



bool create (const char *file, unsigned size) {
  if(file == NULL) {
    exit(-1);
    return -1;
  }
  return filesys_create(file, size);
}


int open (const char *file) {
  if (file == NULL) {
    exit(-1);
  }
  lock_acquire(&f_lock);
  struct file *f = filesys_open(file);
  if (f == NULL) {
    lock_release(&f_lock);
    return -1;
  }
  int i = 3;
  while(i < 128) {
    struct file *t_file = thread_current()->fd_table[i];
   
    if (t_file == NULL) {
      if (strcmp(thread_name(), file) == 0) {
        file_deny_write(f);
      }
      thread_current()->fd_table[i] = f;
      lock_release(&f_lock);
      return i;
    }
    i++;
  }
  lock_release(&f_lock);
  return -1;
}


void close(int fd) {
  struct file *f = thread_current()->fd_table[fd];
  if (f == NULL)
    exit(-1);
  file_close(f);
  thread_current()->fd_table[fd] = NULL;
}


void seek(int fd, unsigned offset) {
  if (thread_current()->fd_table[fd] == NULL)
    exit(-1);
  file_seek(thread_current()->fd_table[fd], offset);
}


unsigned tell(int fd) {
  if (thread_current()->fd_table[fd] == NULL)
    exit(-1);
  return file_tell(thread_current()->fd_table[fd]);
}


bool remove(const char *f) {
  if (f == NULL)
    exit (-1);
  return filesys_remove(f);
}
