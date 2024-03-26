#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>
#include <debug.h>
#include <stddef.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
typedef int pid_t;
struct lock filesys_lock;
void syscall_init (void);
/* Projects 2 and later. */
void halt (void); //NO_RETURN
void exit (int status);// NO_RETURN
pid_t fork (const char *thread_name,const struct intr_frame *f);
int exec (const char *file);
int wait (pid_t);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned length);
int write (int fd, const void *buffer, unsigned length);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

int dup2(int oldfd, int newfd);

/* Project 3 and optionally project 4. */
void *mmap (void *addr, size_t length, int writable, int fd, unsigned int offset);
void munmap (void *addr);

#endif /* userprog/syscall.h */
