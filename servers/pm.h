#ifndef _PM_H_
#define _PM_H_

#include "../kernel/kernel.h"

/*
 *
 */



void pm (void* args);

pid_t setpmpid (pid_t pid);

/*
 *
 */
pid_t spawntask (int(*ptsk)(char**), size_t stacksize, char** argv);

int execv (char* name, char** argv);

pid_t wait (int* code);

pid_t waitpid (pid_t p, int* code);

void mexit (int code);

void* pmmalloc (size_t size);

void pmfree (void* ptr);

int argc (char** argv);

#endif
