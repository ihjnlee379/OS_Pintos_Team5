#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "lib/user/syscall.h"

void syscall_init (void);
void halt (void);
void exit (int status);
bool create(const char *f, unsigned size);
bool remove(const char *f);
int open(const char *f);
int filesiz(int fd);
void seek(int fd, unsigned offset);
unsigned tell(int fd);
void close (int fd);
pid_t exec (const char *first_word);
int write (int fd, const void *buffer, unsigned size);
int read (int fd, void* buffer, unsigned size);
#endif /* userprog/syscall.h */
