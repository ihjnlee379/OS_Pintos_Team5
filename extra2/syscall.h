#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "lib/user/syscall.h"

void syscall_init (void);
void exit (int status);
int write (int fd, const void *buffer, unsigned size);
#endif /* userprog/syscall.h */
