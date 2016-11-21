#ifndef _KERNEL_H_
#define _KERNEL_H_

#include <stdlib.h>  // malloc, free
#include <stdint.h>  // uint types
#include <string.h>  // memset


#define UNUSED      __attribute__ ((unused))


enum {
    TASK_PRIO_RT = 0,
    TASK_PRIO_HIGH,
    TASK_PRIO_DFLT,
    TASK_PRIO_LOW,
	TASK_PRIO_IDLE,

    TASK_PRIO_QUEUE_MAX   /* Must be the last regardless of the number of prios */
};

typedef struct task_s* pid_t;

/* EVENTS */
#define EVENT_NONE          (0x0000)
#define EVENT_TIMER1OVF     (0x0001)
#define EVENT_USART0RX      (0x0002)
#define EVENT_USART0TX      (0x0004)
#define EVENT_USART1RX      (0x0008)
#define EVENT_USART1TX      (0x0010)

#define PREEMPT_ON_EVENT    (0x8000)

/* PID */
#define TASK_ANY    ((pid_t)(0xFFFF))

/* DEFAULT STACK SIZE */
#define DEFAULT_STACK_SIZE  ((size_t)(160))

/* PAGE INVALID */
#define PAGE_INVALID  ((char)(-1))


/*
 *
 */

void kernel (void(*ptp)(void* args),
             void* args,
             size_t stack,
             unsigned char prio);

/*
 *
 */



void yield(void);

pid_t getpid(void);

int waitevent(int event);

pid_t send(pid_t dest, void* msg);

pid_t receive(pid_t src, void* msg, size_t len);

pid_t sendrec(pid_t src, void* msg, size_t len);

void* kmalloc (size_t size);

void kfree (void* ptr);


pid_t cratetask(unsigned char prio, char page);

char* allocatestack(pid_t pid, size_t size);

void setuptask(pid_t pid, void(*ptsk)(void* args), void* args, void(*exitfn)(void));

void starttask(pid_t pid);

void stoptask(pid_t pid);

void deletetask(pid_t pid);

void exittask(void);

void kirqen(void);

void kirqdis(void);




/*
 *
 */


#endif
