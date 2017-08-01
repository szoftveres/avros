#ifndef _EX_H_
#define _EX_H_


pid_t setexpid (pid_t pid);

void ex (void* args);

void ex_regprg(char* name, int(*ptr)(char**), size_t stack);

void ex_getprg(char* name, int(**ptr)(char**), size_t *stack);

#endif
