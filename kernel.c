#include <avr/interrupt.h>
#include <string.h>
#include "kernel.h"
#include "sys.h"
#include "queue.h"

/*
 * ADD TASK
 */

typedef union cratetask_u {
    struct {
        unsigned char   prio;           /* priority */
        char            page;           /* priority */
    } ask;
    struct {
        pid_t           pid;            /* pid */
    } ans;
} cratetask_t;

/*
 * SEND-REC
 */

typedef union message_u {
    struct {
        pid_t           pid;            /* pid */
        void*           data;           /* data */
        size_t          len;            /* size */
    } ask;
    struct {
        pid_t           pid;            /* pid */
    } ans;
} message_t;

/*
 * MALLOC
 */

typedef union kmalloc_u {
    struct {
        size_t          size;           /* size */
    } ask;
    struct {
        void*           ptr;            /* ptr */
    } ans;
} kmalloc_t;

/*
 * DELETE TASK
 */

typedef struct deletetask_s {
    pid_t           pid;            /* pid */
} deletetask_t;

/*
 * STOP TASK 
 */

typedef struct stoptask_s {
    pid_t           pid;            /* pid */
} stoptask_t;

/*
 * RUN TASK
 */

typedef struct starttask_s {
    pid_t           pid;            /* pid */
} starttask_t;

/*
 * SETUP TASK
 */

typedef struct setuptask_s {
    pid_t           pid;            /* pid */
    void(*ptr)(void* args);         /* task entry */
    void* args;                     /* arguments */
    void(*exitfn)(void);            /* arguments */
} setuptask_t;


/*
 * CREATE_STACK
 */

typedef union allocatestack_u {
    struct {
        pid_t           pid;            /* pid */
        size_t          size;           /* size */
    } ask;
    struct {
        char*           ptr;            /* ptr */
    } ans;
} allocatestack_t;

/*
 * FREE
 */

typedef struct kfree_s {
    void*           ptr;            /* pointer */
} kfree_t;

/*
 * WAIT EVENT
 */

typedef struct waitevent_s {
    int             event;
} waitevent_t;

/*
 * GETPID
 */

typedef struct getpid_s {
    pid_t           pid;            /* pid */
} getpid_t;


/*
 * KERNEL CALL
 */

typedef struct kcall_s {
    int              code;     /* kernel call code */
    union { 
        kmalloc_t       kmalloc;        /* malloc */
        message_t       message;        /* send-receive */
        kfree_t         kfree;          /* free */
        cratetask_t       cratetask;        /* add task */
        allocatestack_t   allocatestack;    /* create stack */
        setuptask_t     setuptask;      /* setup task */
        stoptask_t      stoptask;       /* stop task */
        deletetask_t    deletetask;     /* delete task */
        starttask_t     starttask;      /* start task */
        waitevent_t     waitevent;      /* wait event */
        getpid_t        getpid;         /* Get pid */
    };
} kcall_t;



/*
 * kernel globals
 */

static pid_t                kerneltask;

static q_head_t             queue[TASK_PRIO_QUEUE_MAX];
static q_head_t             current_q;
static q_head_t             blocked_q;

static int                  eventcode;     /* kernel event code */

#define CURRENT         ((task_t*)(Q_FIRST(current_q)))

/*
 * TASK 
 */

typedef struct task_s {    
    QUEUE_HEADER
    char*           sp;            /* stack pointer */
    char*           sb;            /* stack bottom */
    kcall_t         kcall;         /* kernel call parameters */
    unsigned char   prio;          /* task priority */
    unsigned char   flags;         /* task flags */
    char            page;          /* page (-1 = invalid) */
} task_t;


#define TASK_FLAG_IRQDIS        (0x01)
#define TASK_FLAG_USE_PAGES     (0x02)

/*
 * kernel call codes
 */

enum { 
    KRNL_YIELD          = 0,

    KRNL_WAITEVENT,

    KRNL_MALLOC,
    KRNL_FREE,

    KRNL_SEND,
	KRNL_SENDREC,
    KRNL_RECEIVE,

    KRNL_CREATETASK,
    KRNL_ALLOCATESTACK,
    KRNL_SETUPTASK,
    KRNL_STARTTASK,
    KRNL_STOPTASK,
    KRNL_DELETETASK,    
    KRNL_EXITTASK,

    KRNL_IRQEN,
    KRNL_IRQDIS,

    KRNL_GETPID,
    
};

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
newtask (void)
{
    task_t* ptsk;
    ptsk = (task_t*) malloc(sizeof(task_t));
    if (ptsk) {
        memset(ptsk, 0, sizeof(task_t));
    }
    return (ptsk);
}

/*
 * idle task
 */

static void
idle_task (void* args UNUSED) {
    while (1) {
        cpu_sleep();
    }
}

/*
 * push a byte on the stack of the task and set its SP accordingly
 */

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
 * Push CPU context for a task
 */

static void
do_setuptask (task_t* task, void (*tp)(void* args), void* args, void (*exitfn)(void)) {

    cpu_context_t* ctxt;

    do_pushstack(task, LOW(exitfn ? exitfn : exittask));
    do_pushstack(task, HIGH(exitfn ? exitfn : exittask));

    task->sp -= sizeof(cpu_context_t);

    ctxt = (cpu_context_t*)(task->sp + 1);
 
    ctxt->r24 = LOW(args);
    ctxt->r25 = HIGH(args);
    ctxt->retLow = LOW(tp);
    ctxt->retHigh = HIGH(tp);
    ctxt->r1 = 0;                                   /* GCC needs r1 to be 0x00 */
    return;
}



/*
 * Switch to kernel task 
 */

void switchtokernel (void) __attribute__ ((naked));

void switchtokernel (void) {
    SAVE_CONTEXT();
    CURRENT->sp = GET_SP();
    SET_SP(kerneltask->sp);
    RESTORE_CONTEXT();
    RETURN();
}

#define SET_EVENTCODE_AND_JMP_TO_HANDLER(x)     \
    do {                                        \
        asm("\n\tpush   r24"                    \
            "\n\tpush   r25"::);                \
        eventcode = (x);                        \
        asm("\n\tpop    r25"                    \
            "\n\tpop    r24"                    \
            "\n\tjmp switchtokernel\n\t"::);    \
    } while(0)

/**
 * software trap
 */
void swtrap (void) __attribute__ ((naked));
void swtrap (void) {
    LOCK();
    SET_EVENTCODE_AND_JMP_TO_HANDLER(EVENT_NONE);
}

/**
 * timer1 interrupt handler
 */
ISR (TIMER1_OVF_vect) __attribute__ ((signal, naked));
ISR (TIMER1_OVF_vect) { /* GIE cleared automatically */
    SET_EVENTCODE_AND_JMP_TO_HANDLER(EVENT_TIMER1OVF);
}

/**
 * USART0 RX COMPLETE interrupt handler
 */
ISR (USART0_RX_vect) __attribute__ ((signal, naked));
ISR (USART0_RX_vect) { /* GIE cleared automatically */
    SET_EVENTCODE_AND_JMP_TO_HANDLER(EVENT_USART0RX);
}

/**
 * USART0 TX COMPLETE interrupt handler
 */
ISR (USART0_TX_vect) __attribute__ ((signal, naked));
ISR (USART0_TX_vect) { /* GIE cleared automatically */
    SET_EVENTCODE_AND_JMP_TO_HANDLER(EVENT_USART0TX);
}

/**
 * USART1 RX COMPLETE interrupt handler
 */
ISR (USART1_RX_vect) __attribute__ ((signal, naked));
ISR (USART1_RX_vect) { /* GIE cleared automatically */
    SET_EVENTCODE_AND_JMP_TO_HANDLER(EVENT_USART1RX);
}

/**
 * USART1 TX COMPLETE interrupt handler
 */
ISR (USART1_TX_vect) __attribute__ ((signal, naked));
ISR (USART1_TX_vect) { /* GIE cleared automatically */
    SET_EVENTCODE_AND_JMP_TO_HANDLER(EVENT_USART1TX);
}

/*
 * Switch to user task 
 */

void switch_from_kernel (void) __attribute__ ((naked));

void switch_from_kernel (void) {
    SAVE_CONTEXT();
    kerneltask->sp = GET_SP();
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
 * Check whether a task waits for an event
 */

static q_item_t*
dispatchevent (q_head_t* que UNUSED, q_item_t* ptsk) {
    if ((((task_t*)ptsk)->kcall.code) != (KRNL_WAITEVENT)) {
        return (NULL);
    }
    if (!((((task_t*)ptsk)->kcall.waitevent.event) & (eventcode))) {
        return (NULL);     
    }
    return (ptsk);
}

/*
 * Send message
 */

static q_item_t*
managesend (q_head_t* que UNUSED, q_item_t* ptsk) {
    if (((task_t*)ptsk)->kcall.code != KRNL_RECEIVE) {
        return (NULL);
    }
    if (CURRENT->kcall.message.ask.pid != ((task_t*)ptsk)) {
        return (NULL);
    }
    if((((task_t*)ptsk)->kcall.message.ask.pid != CURRENT) && 
        (((task_t*)ptsk)->kcall.message.ask.pid != TASK_ANY)){
        return (NULL);
    }
    memcpy(((task_t*)ptsk)->kcall.message.ask.data, 
		   CURRENT->kcall.message.ask.data, 
           ((task_t*)ptsk)->kcall.message.ask.len);
	((task_t*)ptsk)->kcall.message.ans.pid = CURRENT;
    return (ptsk); // Waiting task will continue running
}

/*
 * Receive message
 */

static q_item_t*
managereceive (q_head_t* que UNUSED, q_item_t* ptsk) {
    if (((task_t*)ptsk)->kcall.code != KRNL_SEND 
            && ((task_t*)ptsk)->kcall.code != KRNL_SENDREC) {
        return (NULL);
    }
    if(((task_t*)ptsk)->kcall.message.ask.pid != CURRENT){
        return (NULL);
    }
    if((CURRENT->kcall.message.ask.pid != ((task_t*)ptsk)) &&
        (CURRENT->kcall.message.ask.pid != TASK_ANY)) {
        return (NULL);    
    }
    /* copy the message */
    memcpy(CURRENT->kcall.message.ask.data, 
           ((task_t*)ptsk)->kcall.message.ask.data, 
           CURRENT->kcall.message.ask.len);
    CURRENT->kcall.message.ans.pid = ((task_t*)ptsk);
    return (ptsk);
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
    return;
}

/*
 * initialize kernel task data structure
 */  

static pid_t
initkrnl (void) {
    eventcode = EVENT_NONE;
    kerneltask = newtask();
    return (kerneltask);
}

/*
 * initialize the first user task
 */  

static pid_t
inittask (void(*ptp)(void* args), void* args, size_t stack, unsigned char prio) {
    pid_t task = newtask();
    if(!task){
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
 * Kernel initialization and kernel task loop starts here
 * The parameter function will be created as the first user task
 */ 

void
kernel (void(*ptp)(void* args), void* args, size_t stack, unsigned char prio) {
    pid_t       wtask;              /* task we work on */
    pid_t       old;

    LOCK();    
    init_task_queues();

    if(!initkrnl()){
        return;
    }

    inittask(idle_task, NULL, 0x80, TASK_PRIO_IDLE);
    inittask(ptp, args, stack, prio);

    while (1) {
        scheduler(); 
        switch_from_kernel();
        /* kernel entry point */
		old = CURRENT;

		/* handle event */
        if (eventcode != EVENT_NONE) {
            /* Check whether any task waits for this event */
            wtask = (pid_t) q_forall(&blocked_q, dispatchevent);
            if (wtask) {
                if ((wtask->kcall.waitevent.event) & (PREEMPT_ON_EVENT)) {
                    /* Do preemption */
                    Q_END(&queue[old->prio], Q_REMV(&current_q, CURRENT));
                } else {
                    Q_FRONT(&queue[old->prio], Q_REMV(&current_q, CURRENT));
                }
                /* Handler comes first */
                Q_FRONT(&current_q, Q_REMV(&blocked_q, wtask));
                wtask->kcall.waitevent.event = eventcode;
            }
			continue;
		}
		
		/* handle kernel call*/
		switch (CURRENT->kcall.code) {  
		
		  case KRNL_CREATETASK:  /* Add a new task entry in the blocked queue */
			wtask = (pid_t) Q_FRONT(&blocked_q, newtask());
			if (wtask) {
                wtask->prio = CURRENT->kcall.cratetask.ask.prio;
                wtask->page = CURRENT->kcall.cratetask.ask.page;
				wtask->sb = NULL;
			}
			CURRENT->kcall.cratetask.ans.pid = wtask;
			break;

		  case KRNL_ALLOCATESTACK:    /* Create new stack frame */
			wtask = CURRENT->kcall.allocatestack.ask.pid;
            /* cpu_context + exit fn */
            CURRENT->kcall.allocatestack.ans.ptr = 
                do_allocatestack(wtask, CURRENT->kcall.allocatestack.ask.size);
			break;

          case KRNL_SETUPTASK:
			wtask = CURRENT->kcall.setuptask.pid;
			do_setuptask(wtask,
                         CURRENT->kcall.setuptask.ptr, 
                         CURRENT->kcall.setuptask.args, 
                         CURRENT->kcall.setuptask.exitfn);
			break;

		  case KRNL_STARTTASK:      /* Start a task */
			wtask = CURRENT->kcall.starttask.pid;
             /* put new task at the end of rdy queue */
            Q_FRONT(&queue[wtask->prio], Q_REMV(&blocked_q, wtask));
            //Q_FRONT(&queue[old->prio], Q_REMV(&current_q, CURRENT));
			break;

		  case KRNL_STOPTASK:       /* Delete the stack of a blocked task */
			wtask = CURRENT->kcall.stoptask.pid;
			if (wtask->sb) {
				free(wtask->sb);
                wtask->sb = NULL;
			}			
			break;
	  
		  case KRNL_DELETETASK:     /* Delete a blocked task */
			free((void*)Q_REMV(&blocked_q, CURRENT->kcall.deletetask.pid));
			break;

		  case KRNL_EXITTASK:       /* Current task exits */
			free(CURRENT->sb);
			free(Q_REMV(&current_q, CURRENT));
			break; 

		  case KRNL_IRQEN:          /* Enable interrupts */
			CURRENT->flags &= ~(TASK_FLAG_IRQDIS);
			break;

		  case KRNL_IRQDIS:         /* Disable interrupts */
			CURRENT->flags |= (TASK_FLAG_IRQDIS);
			break;

		  case KRNL_WAITEVENT:      /* Block task until an event occurs */
            Q_FRONT(&blocked_q, Q_REMV(&current_q, CURRENT));
			break;              

		  case KRNL_MALLOC:         /* Allocate memory */
          
			CURRENT->kcall.kmalloc.ans.ptr = 
                    malloc(CURRENT->kcall.kmalloc.ask.size);
			break;            

		  case KRNL_FREE:           /* Free memory */
			free(CURRENT->kcall.kfree.ptr);
			break;

          case KRNL_GETPID:         /* Get pid */
			CURRENT->kcall.getpid.pid = CURRENT;
			break;

		  case KRNL_SEND:           /* Send a message */
			wtask = (task_t*) q_forall(&blocked_q, managesend);
			if (!wtask) {
                /* cannot deliver, block sending task */
                Q_FRONT(&blocked_q, Q_REMV(&current_q, CURRENT));
			} else {
                Q_FRONT(&queue[wtask->prio], Q_REMV(&blocked_q, wtask));
            }
			break;

		  case KRNL_SENDREC:        /* Send a message, then receive */
			wtask = (task_t*) q_forall(&blocked_q, managesend);
			if(wtask){
                /* msg delivered, put CURRENT in RCV state */
                CURRENT->kcall.code = KRNL_RECEIVE;
                Q_FRONT(&queue[wtask->prio], Q_REMV(&blocked_q, wtask));
            }
            Q_FRONT(&blocked_q, Q_REMV(&current_q, CURRENT));
			break;

		  case KRNL_RECEIVE:        /* Receive a message */
			wtask = (task_t*) q_forall(&blocked_q, managereceive);
			if(!wtask){
                /* no sender, block receiving task */
				Q_FRONT(&blocked_q, Q_REMV(&current_q, CURRENT));
			} else {
                if (wtask->kcall.code == KRNL_SENDREC) {
                    /* msg received, put sender in RCV state */
                    wtask->kcall.code = KRNL_RECEIVE;
                } else {
                    Q_FRONT(&queue[wtask->prio], Q_REMV(&blocked_q, wtask));
                }
            }
			break;       

		  case KRNL_YIELD:          /* Let other tasks running */
            Q_END(&queue[old->prio], Q_REMV(&current_q, CURRENT));

		  default:
			break;
		}
    }
    return;
}

/*
================================================================================
*/

/*
 * PID of the current task
 */

pid_t
getpid (void) {
    CURRENT->kcall.code = (KRNL_GETPID);
    swtrap();
    return (CURRENT->kcall.getpid.pid);
}

/*
 * Release the CPU
 */

void
yield (void) {
    CURRENT->kcall.code = (KRNL_YIELD);
    swtrap();
    return;
}

/*
 * 
 */

int
waitevent (int event) {
    CURRENT->kcall.waitevent.event = event;
    CURRENT->kcall.code = (KRNL_WAITEVENT);
    swtrap();
    return (CURRENT->kcall.waitevent.event);
}

/*
 *
 */

void*
kmalloc (size_t size) {
    CURRENT->kcall.kmalloc.ask.size = size;
    CURRENT->kcall.code = (KRNL_MALLOC);
    swtrap();
    return (CURRENT->kcall.kmalloc.ans.ptr);
}

/*
 *
 */

void
kfree (void* ptr) {
    CURRENT->kcall.kfree.ptr = ptr;
    CURRENT->kcall.code = (KRNL_FREE);
    swtrap();
    return;
}

/*
 *
 */

pid_t
send (pid_t dest, void* msg) {
    CURRENT->kcall.message.ask.pid = dest;
    CURRENT->kcall.message.ask.data = msg;
    CURRENT->kcall.code = (KRNL_SEND);
    swtrap();
    return  (CURRENT->kcall.message.ans.pid);
}

/*
 *
 */

pid_t
sendrec (pid_t tsk, void* msg, size_t len) {
    CURRENT->kcall.message.ask.pid = tsk;
    CURRENT->kcall.message.ask.data = msg;
    CURRENT->kcall.message.ask.len = len;
    CURRENT->kcall.code = (KRNL_SENDREC);
    swtrap();
    return (CURRENT->kcall.message.ans.pid);
}

/*
 *
 */

pid_t
receive (pid_t src, void* msg, size_t len) {
    CURRENT->kcall.message.ask.pid = src;
    CURRENT->kcall.message.ask.data = msg;
    CURRENT->kcall.message.ask.len = len;
    CURRENT->kcall.code = (KRNL_RECEIVE);
    swtrap();
    return (CURRENT->kcall.message.ans.pid);
}
    
/*
 *
 */

pid_t
cratetask (unsigned char prio, char page) {
    CURRENT->kcall.cratetask.ask.prio = prio;
    CURRENT->kcall.cratetask.ask.page = page;
    CURRENT->kcall.code = (KRNL_CREATETASK);
    swtrap();
    return (CURRENT->kcall.cratetask.ans.pid);
}

/*
 *
 */

char*
allocatestack (pid_t pid, size_t size) {
    CURRENT->kcall.allocatestack.ask.pid = pid; 
    CURRENT->kcall.allocatestack.ask.size = size;
    CURRENT->kcall.code = (KRNL_ALLOCATESTACK);
    swtrap();
    return (CURRENT->kcall.allocatestack.ans.ptr);
}

/*
 *
 */

void
setuptask (pid_t pid,
           void(*ptsk)(void* args),
           void* args,
           void(*exitfn)(void)) {

    CURRENT->kcall.setuptask.pid = pid;   
    CURRENT->kcall.setuptask.ptr = ptsk;
    CURRENT->kcall.setuptask.args = args;
    CURRENT->kcall.setuptask.exitfn = exitfn;
    CURRENT->kcall.code = (KRNL_SETUPTASK);
    swtrap();
    return;
}

/*
 *
 */

void
starttask (pid_t pid) {
    CURRENT->kcall.starttask.pid = pid;
    CURRENT->kcall.code = (KRNL_STARTTASK);
    swtrap();
    return;
}

/*
 *
 */

void
stoptask(pid_t pid) {
    CURRENT->kcall.stoptask.pid = pid;
    CURRENT->kcall.code = (KRNL_STOPTASK);
    swtrap();
    return;
}

/*
 *
 */

void
deletetask(pid_t pid) {
    CURRENT->kcall.deletetask.pid = pid;
    CURRENT->kcall.code = (KRNL_DELETETASK);
    swtrap();
    return;
}

/*
 *
 */

void
exittask(void) {
    CURRENT->kcall.code = (KRNL_EXITTASK);
    swtrap();
    /* NEVER REACHED */
    return;
}

/*
 *
 */

void
kirqen(void) {
    CURRENT->kcall.code = (KRNL_IRQEN);
    swtrap();
    return;
}

/*
 *
 */

void
kirqdis(void) {
    CURRENT->kcall.code = (KRNL_IRQDIS);
    swtrap();
    return;
}

