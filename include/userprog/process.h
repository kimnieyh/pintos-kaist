#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

int process_create_initd (const char *file_name);
int process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (int);
void process_exit (void);
void process_activate (struct thread *next);
bool
lazy_load_segment (struct page *page, void *aux);
#endif /* userprog/process.h */
