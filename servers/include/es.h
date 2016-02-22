#ifndef _ES_H_
#define _ES_H_


pid_t setespid (pid_t pid);

void es (void* args);

void es_regprg(char* name, int(*ptr)(char**), size_t stack);

void es_getprg(char* name, int(**ptr)(char**), size_t *stack);

#endif
