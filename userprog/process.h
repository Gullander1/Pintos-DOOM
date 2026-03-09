#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

typedef tid_t pid_t;
pid_t process_execute(char const *cmd_line);
int process_wait(int);
void process_exit(int status);
void process_cleanup(void);
void process_activate(void);

#endif /* userprog/process.h */
