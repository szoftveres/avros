#ifndef _TIMER_H_
#define _TIMER_H_

#include "kernel.h"

typedef struct time_s {
    char        sec;
    char        min;
    char        hour;
} time_t;


void timer (void);
pid_t settimerpid (pid_t pid);

void delay (int ticks);
void getuptime (time_t* time);
void settime (time_t* time);
void gettime (time_t* time);

#endif
