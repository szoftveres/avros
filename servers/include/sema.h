#ifndef _SEMA_H_
#define _SEMA_H_


#define SEMA_MUTEX (1)

typedef struct sema_s* sema;


void semasrv (void* args);

pid_t setsemapid (pid_t pid);

sema createsema(unsigned int val);
void deletesema(sema m);
void waitsema(sema m);
void signalsema(sema m);
unsigned int getsema(sema s);


#endif
