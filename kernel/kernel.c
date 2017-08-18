#include <avr/interrupt.h>
#include <string.h>
#include "../lib/queue.h"
#include "hal.h"
#include "kernel.h"


/*
 * kernel globals
 */

static char*                kernel_sp;      /* Kernel SP */

static q_head_t             queue[TASK_PRIO_QUEUE_MAX];
static q_head_t             current_q;
static q_head_t             blocked_q;

unsigned int                eventcode;     /* kernel event code */

#define CURRENT         ((task_t*)(Q_FIRST(current_q)))

/*
 * TASK
 */

typedef struct task_s {     /* 11 bytes */
    QUEUE_HEADER            /* 4 byte */
    char*           sp;            /* stack pointer */
    char*           sb;            /* stack bottom */
    unsigned char   prio;          /* task priority */
    unsigned char   flags;         /* task flags */
    char            page;          /* page (-1 = invalid) */
} task_t;


#define TASK_FLAG_IRQDIS        (0x01)
#define TASK_FLAG_USE_PAGES     (0x02)

/*
 * kernel call codes
 */

#define KCALL_YIELD             0x00
#define KCALL_GETPID            0x01

#define KCALL_IRQEN             0x10
#define KCALL_IRQDIS            0x11

#define KCALL_CREATETASK        0x20
#define KCALL_ALLOCATESTACK     0x21
#define KCALL_SETUPTASK         0x22
#define KCALL_STARTTASK         0x23
#define KCALL_STOPTASK          0x24
#define KCALL_DELETETASK        0x25
#define KCALL_EXITTASK          0x26

#define KCALL_MALLOC            0x30
#define KCALL_FREE              0x31

#define KCALL_SEND              0x40
#define KCALL_SENDREC           0x41
#define KCALL_RECEIVE           0x42

#define KCALL_WAITEVENT         0x50


/*
 *
 */

void switchtokernel (void) __attribute__ ((naked));
void switch_from_kernel (void) __attribute__ ((naked));


void switchtokernel (void) {
    SAVE_CONTEXT();
    CURRENT->sp = GET_SP();
    SET_SP(kernel_sp);
    RESTORE_CONTEXT();
    RETURN();
}


void switch_from_kernel (void) {
    SAVE_CONTEXT();
    kernel_sp = GET_SP();
    SET_SP(CURRENT->sp);
    if (CURRENT->flags & TASK_FLAG_IRQDIS) {
        /* Don't turn on interrupts  */
        RESTORE_CONTEXT();
        RETURN();
    } else {
        /* Enable interrupts after return */
        RESTORE_CONTEXT();
        RETI();
    }
}


/*
 *
 */

static void
init_task_queues (void) {
    int i;
    for (i = 0; i < TASK_PRIO_QUEUE_MAX; i++) {
        q_init(&queue[i]);
    }
    q_init(&current_q);
    q_init(&blocked_q);
    return;
};

/*
 * Allocate memory for new task
 */

static task_t*
newtask (void) {
    task_t* ptsk;
    ptsk = (task_t*) malloc(sizeof(task_t));
    if (ptsk) {
        memset(ptsk, 0, sizeof(task_t));
    }
    return (ptsk);
}

/*
 * idle task
 * This task runs 99.9% of the time, so it shoud really not do anything
 */
static void
idle_task (void* args UNUSED) {
    while (1) {
        /* Let's save on the electricity bill */
        cpu_sleep();
    }
}


static void
do_pushstack (task_t* task, char val) {
    *(task->sp) = val;
    task->sp -= 1;
}


static char*
do_allocatestack (task_t* task, size_t size) {
    if (!size) {
        return NULL;
    }
    /* cpu_context + exit fn */
    size += (sizeof(cpu_context_t) +
             sizeof(void (*)(void)));
    task->sb = malloc(size);
    if (task->sb) {
        task->sp = task->sb + size - 1;
    }
    return (task->sb);
}

/*
static void
do_setstack (task_t* task, char* ptr, size_t size) {

    if (!ptr || !size) {
        return;
    }
    task->sb = NULL;
    task->sp = ptr + size - 1;
    return;
}
*/


/*
 * Push CPU context for a task
 */
static void
do_setuptask (task_t* task,
              void (*tp)(void* args),
              void* args,
              void (*exitfn)(void)) {

    cpu_context_t* ctxt;

    do_pushstack(task, LOW(exittask));
    do_pushstack(task, HIGH(exittask));
    if (exitfn) {
        do_pushstack(task, LOW(exitfn));
        do_pushstack(task, HIGH(exitfn));
    }

    task->sp -= sizeof(cpu_context_t);

    ctxt = (cpu_context_t*)(task->sp + 1);

    ctxt->r24 = LOW(args);
    ctxt->r25 = HIGH(args);
    ctxt->retLow = LOW(tp);
    ctxt->retHigh = HIGH(tp);
    ctxt->r1 = 0;       /* GCC needs r1 to be 0x00 */

    return;
}


/*
 * Check whether a task waits for an event
 */

static q_item_t*
dispatchevent (q_head_t* que UNUSED, q_item_t* ptsk) {
    if (GET_KCALLCODE(GET_CTXT((task_t*)ptsk)) != (KCALL_WAITEVENT)) {
        return (NULL);
    }
    if (!((GETP0(GET_CTXT((task_t*)ptsk))) & (eventcode))) {
        return (NULL);
    }
    return (ptsk);
}

/*
 * Message processing and delivery
 */

static int
delivermsg (task_t* rcvr_task, task_t* sndr_task) {
    cpu_context_t *rcvr_ctxt = GET_CTXT(rcvr_task);
    cpu_context_t *sndr_ctxt = GET_CTXT(sndr_task);

    /* Check if rendez-vous criteria is satisfied */
    if ((GET_KCALLCODE(sndr_ctxt) != KCALL_SEND) &&
        (GET_KCALLCODE(sndr_ctxt) != KCALL_SENDREC)) {
        return (0);
    }
    if (GET_KCALLCODE(rcvr_ctxt) != KCALL_RECEIVE) {
        return (0);
    }
    if ((task_t*)GETP0(sndr_ctxt) != rcvr_task) {
        return (0);
    }
    if (((task_t*)GETP0(rcvr_ctxt) != sndr_task) &&
        ((task_t*)GETP0(rcvr_ctxt) != TASK_ANY)) {
        return (0);
    }
    /* Copy the msg */
    memcpy((void*)GETP1(rcvr_ctxt),
           (void*)GETP1(sndr_ctxt),
           (size_t)GETP2(rcvr_ctxt));
    /* Success, copy the sender's pid to the rcvr */
    SETP0(rcvr_ctxt, sndr_task);
    return (1);
}


static q_item_t*
sendmsg (q_head_t* que UNUSED, q_item_t* ptsk) {
    if (delivermsg((task_t*)ptsk, CURRENT)) {
        return (ptsk);
    }
    return (NULL);
}


static q_item_t*
receivemsg (q_head_t* que UNUSED, q_item_t* ptsk) {
    if (delivermsg(CURRENT, (task_t*)ptsk)) {
        return (ptsk);
    }
    return (NULL);
}

/*
 * scheduler
 */

static void
scheduler (void) {
    int prio;

    if (CURRENT) {
        return;
    }
    for (prio = 0; prio < TASK_PRIO_QUEUE_MAX; prio++) {
        if (Q_FIRST(queue[prio])) {
            Q_FRONT(&current_q, Q_REMV(&queue[prio], Q_FIRST(queue[prio])));
            break;
        }
    }
    /* CURRENT must be set by here */
    return;
}

/*
 * initialize the first user task
 */

static pid_t
inittask (void(*ptp)(void* args), void* args, size_t stack, unsigned char prio) {
    pid_t task = newtask();
    if (!task) {
        return (NULL);
    }
    if (!do_allocatestack (task, stack)) {
        return (NULL);
    }
    do_setuptask(task, ptp, args, NULL);
    task->prio = prio;
    Q_END(&queue[task->prio], task);
    return (task);
}

/*
 * Kernel initialization and main kernel loop
 * The first user task is creted from the parameter function
 */
void
kernel (void(*ptp)(void* args), void* args, size_t stack, unsigned char prio) {
    LOCK();     /* In theory this is unnecessary */
    init_task_queues();
    inittask(idle_task, NULL, 0x80, TASK_PRIO_IDLE);
    inittask(ptp, args, stack, prio);
    eventcode = EVENT_NONE;

    while (1) {
        pid_t           wtask;
        pid_t           old;
        cpu_context_t   *ctxt;
        scheduler();
        switch_from_kernel();

        /* kernel re-entry point */

        old = CURRENT;
        ctxt = GET_CTXT(CURRENT);

        /* handle event */
        if (eventcode != EVENT_NONE) {
            /* Check whether any task waits for this event */
            wtask = (pid_t) q_forall(&blocked_q, dispatchevent);
            if (wtask) {
                if ((GETP0(GET_CTXT((task_t*)wtask))) & (PREEMPT_ON_EVENT)) {
                    /* Do preemption */
                    Q_END(&queue[old->prio], Q_REMV(&current_q, CURRENT));
                } else {
                    Q_FRONT(&queue[old->prio], Q_REMV(&current_q, CURRENT));
                }
                /* Handler comes first */
                Q_FRONT(&current_q, Q_REMV(&blocked_q, wtask));
                SETP0(GET_CTXT((task_t*)wtask), eventcode);
            }
            eventcode = EVENT_NONE;
            continue;
        }

        /* handle kernel call*/
        switch (GET_KCALLCODE(ctxt)) {

          case KCALL_CREATETASK:
            wtask = (pid_t) Q_FRONT(&blocked_q, newtask());
            if (wtask) {
                wtask->prio = (unsigned char)GETP0(ctxt);
                wtask->page = (char)GETP1(ctxt);
                wtask->sb = NULL;
            }
            SETP0(ctxt, wtask);
            break;

          case KCALL_ALLOCATESTACK:    /* Create new stack frame */
            SETP0(ctxt, do_allocatestack((task_t*)GETP0(ctxt), (size_t)GETP1(ctxt)));
            break;

          case KCALL_SETUPTASK:
            do_setuptask((task_t*)GETP0(ctxt),
                         (void (*)(void*))GETP1(ctxt),
                         (void*)GETP2(ctxt),
                         (void (*)(void))GETP3(ctxt));
            break;

          case KCALL_STARTTASK:      /* Start a task */
            wtask = (pid_t)GETP0(ctxt);
             /* put new task at the end of rdy queue */
            Q_END(&queue[wtask->prio], Q_REMV(&blocked_q, wtask));
            break;

          case KCALL_STOPTASK:       /* Delete the stack of a blocked task */
            wtask = (pid_t)GETP0(ctxt);
            if (wtask->sb) {
                free(wtask->sb);
                wtask->sb = NULL;
            }
            break;

          case KCALL_DELETETASK:     /* Delete a blocked task */
            free((void*)Q_REMV(&blocked_q, (pid_t)GETP0(ctxt)));
            break;

          case KCALL_EXITTASK:       /* Current task exits */
            free(CURRENT->sb);
            free(Q_REMV(&current_q, CURRENT));
            break;

          case KCALL_IRQEN:          /* Enable interrupts */
            CURRENT->flags &= ~(TASK_FLAG_IRQDIS);
            break;

          case KCALL_IRQDIS:         /* Disable interrupts */
            CURRENT->flags |= (TASK_FLAG_IRQDIS);
            break;

          case KCALL_WAITEVENT:      /* Block task until an event occurs */
            Q_END(&blocked_q, Q_REMV(&current_q, CURRENT));
            break;

          case KCALL_MALLOC:         /* Allocate memory */
            SETP0(ctxt, malloc((size_t)GETP0(ctxt)));
            break;

          case KCALL_FREE:           /* Free memory */
            free((void*)GETP0(ctxt));
            break;

          case KCALL_GETPID:         /* Get pid */
            SETP0(ctxt, CURRENT);
            break;

          case KCALL_SEND:           /* Send a message */
            wtask = (task_t*) q_forall(&blocked_q, sendmsg);
            if (!wtask) {
                /* cannot deliver, block sending task */
                Q_FRONT(&blocked_q, Q_REMV(&current_q, CURRENT));
            } else {
                Q_END(&queue[wtask->prio], Q_REMV(&blocked_q, wtask));
            }
            break;

          case KCALL_SENDREC:        /* Send a message, then receive */
            wtask = (task_t*) q_forall(&blocked_q, sendmsg);
            if (wtask) {
                /* msg delivered, put CURRENT in RCV state */
                SET_KCALLCODE(ctxt, KCALL_RECEIVE);
                Q_END(&queue[wtask->prio], Q_REMV(&blocked_q, wtask));
            }
            Q_FRONT(&blocked_q, Q_REMV(&current_q, CURRENT));
            break;

          case KCALL_RECEIVE:        /* Receive a message */
            wtask = (task_t*) q_forall(&blocked_q, receivemsg);
            if (!wtask) {
                /* no sender, block receiving task */
                Q_FRONT(&blocked_q, Q_REMV(&current_q, CURRENT));
            } else {
                if (GET_KCALLCODE(GET_CTXT(wtask)) == KCALL_SENDREC) {
                    /* msg received, put sender in RCV state */
                    SET_KCALLCODE(GET_CTXT(wtask), KCALL_RECEIVE);
                } else {
                    Q_END(&queue[wtask->prio], Q_REMV(&blocked_q, wtask));
                }
            }
            break;

          case KCALL_YIELD:          /* Let other tasks running */
            Q_END(&queue[old->prio], Q_REMV(&current_q, CURRENT));
            break;

          default:
            break;
        }
    }
    return;
}

/*
================================================================================
*/

pid_t
getpid (void) {
    register pid_t ret __asm__ ("r24");
    KERNEL_CALL(KCALL_GETPID);
    return (ret);
}


void
yield (void) {
    KERNEL_CALL(KCALL_YIELD);
    return;
}


int
waitevent (int event UNUSED) {
    register int ret __asm__ ("r24");
    KERNEL_CALL(KCALL_WAITEVENT);
    return (ret);
}


void*
kmalloc (size_t size UNUSED) {
    register void* ret __asm__ ("r24");
    KERNEL_CALL(KCALL_MALLOC);
    return (ret);
}


void
kfree (void* ptr UNUSED) {
    KERNEL_CALL(KCALL_FREE);
    return;
}


pid_t
send (pid_t dest UNUSED, void* msg UNUSED) {
    register pid_t ret __asm__ ("r24");
    KERNEL_CALL(KCALL_SEND);
    return (ret);
}


pid_t
sendrec (pid_t tsk UNUSED, void* msg UNUSED, size_t len UNUSED) {
    register pid_t ret __asm__ ("r24");
    KERNEL_CALL(KCALL_SENDREC);
    return (ret);
}


pid_t
receive (pid_t src UNUSED, void* msg UNUSED, size_t len UNUSED) {
    register pid_t ret __asm__ ("r24");
    KERNEL_CALL(KCALL_RECEIVE);
    return (ret);
}


pid_t
createtask (unsigned char prio UNUSED, char page UNUSED) {
    register pid_t ret __asm__ ("r24");
    KERNEL_CALL(KCALL_CREATETASK);
    return (ret);
}


char*
allocatestack (pid_t pid UNUSED, size_t size UNUSED) {
    register char* ret __asm__ ("r24");
    KERNEL_CALL(KCALL_ALLOCATESTACK);
    return (ret);
}


void
setuptask (pid_t pid UNUSED,
           void(*ptsk)(void* args) UNUSED,
           void* args UNUSED,
           void(*exitfn)(void) UNUSED) {

    KERNEL_CALL(KCALL_SETUPTASK);
    return;
}


void
starttask (pid_t pid UNUSED) {
    KERNEL_CALL(KCALL_STARTTASK);
    return;
}


void
stoptask(pid_t pid UNUSED) {
    KERNEL_CALL(KCALL_STOPTASK);
    return;
}


void
deletetask(pid_t pid UNUSED) {
    KERNEL_CALL(KCALL_DELETETASK);
    return;
}


void
exittask(void) {
    KERNEL_CALL(KCALL_EXITTASK);
    /* NEVER REACHED */
    return;
}


void
kirqen(void) {
    KERNEL_CALL(KCALL_IRQEN);
    return;
}


void
kirqdis(void) {
    KERNEL_CALL(KCALL_IRQDIS);
    return;
}


