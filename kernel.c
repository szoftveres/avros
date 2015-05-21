#include <avr/interrupt.h>
#include <string.h>
#include "kernel.h"
#include "sys.h"
#include "queue.h"

/*
 * ADD TASK
 */

typedef union addtask_u {
    struct {
        unsigned char   prio;           /* priority */
    } ask;
    struct {
        pid_t           pid;            /* pid */
    } ans;
} addtask_t;

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
 * LAUNCH TASK
 */

typedef struct launchtask_s {
    pid_t           pid;            /* pid */
    void(*ptr)(void* args);          /* task entry */
    void* args;                      /* task entry */
    size_t          stack;          /* stack size */
} launchtask_t;

/*
 * CREATE_STACK
 */

typedef union createstack_u {
    struct {
        pid_t           pid;            /* pid */
        size_t          size;           /* size */
    } ask;
    struct {
        char*           ptr;            /* ptr */
    } ans;
} createstack_t;

/*
 * FILL_STACK
 */

typedef struct pushstack_s {
    pid_t           pid;            /* pid */
    char*           ptr;            /* custom stack */
    size_t          size;           /* size */
} pushstack_t;

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
        addtask_t       addtask;        /* add task */
        createstack_t   createstack;    /* create stack */
        pushstack_t     pushstack;      /* push data on stack */
        launchtask_t    launchtask;     /* launch task */
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
    char*            sp;            /* stack pointer */
    char*            sb;            /* stack bottom */
    kcall_t        kcall;       /* kernel call parameters */
    unsigned char    prio;          /* task priority */
    char             flags;         /* task flags */
} task_t;


#define TASK_FLAG_IRQDIS    (0x01)

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

    KRNL_ADDTASK,
    KRNL_DELETETASK,

    KRNL_STARTTASK,
    KRNL_STOPTASK,

    KRNL_LAUNCHTASK,
    KRNL_EXITTASK,

    KRNL_CREATESTACK,
    KRNL_PUSHSTACK,

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
    if(ptsk){
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
k_stack_push (task_t* tp, char val) {
    *(tp->sp) = val;
    tp->sp -= 1;
}

/*
 * Create initial CPU context for a task
 */

static char*
k_build_initial_stack (task_t* newtask, void (*tp)(void* args), void* args, size_t stack) {

    cpu_context_t* ctxt;
    size_t real_size;

    real_size = stack + sizeof(cpu_context_t) + sizeof(void(*)(void));

    /* allocating stack */
	newtask->sb = malloc(real_size);
    if(!newtask->sb){
        return (NULL);
    }
    ctxt = (cpu_context_t*)(newtask->sb + stack);

    /* PUSH: post decrement, POP: pre-increment */
    newtask->sp = newtask->sb + real_size - 1;
    /* exit on return */
    k_stack_push(newtask, LOW(exittask));
    k_stack_push(newtask, HIGH(exittask));

    newtask->sp -= sizeof(cpu_context_t);
    ctxt->r24 = LOW(args);
    ctxt->r25 = HIGH(args);
    ctxt->retLow = LOW(tp);
    ctxt->retHigh = HIGH(tp);
    ctxt->r1 = 0;                                   /* GCC needs r1 to be 0x00 */
    return (newtask->sb);
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
 * Initialize idle task
 */  

static pid_t
initidle (void) {
    pid_t task = newtask();
    if (!task) {
        return (NULL);
    }
    if (!k_build_initial_stack(task, idle_task, NULL, 0x80)) {
        free(task);
        return (NULL);
    }
    task->prio = TASK_PRIO_IDLE;          /* Lowest priority */
    return (task);
}

/*
 * initialize the first user task
 */  

static pid_t
initusertask (void(*ptp)(void* args), void* args, size_t stack, unsigned char prio) {
    pid_t task = newtask();
    if(!task){
        return (NULL);
    }
    if(!k_build_initial_stack(task, ptp, args, stack)){
        free(task);
        return (NULL);
    }
    task->prio = prio; 
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

    wtask = initidle();
    if(!wtask){
        return;
    }
    Q_END(&queue[wtask->prio], wtask); 

    wtask = initusertask(ptp, args, stack, prio);
    if(!wtask){
        return;
    }
    Q_END(&queue[wtask->prio], wtask);   /* Start with first user task */

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
		
		  case KRNL_ADDTASK:  /* Add a new task entry in the blocked queue */
			wtask = (pid_t) Q_FRONT(&blocked_q, newtask());
			if (wtask) {
                wtask->prio = CURRENT->kcall.addtask.ask.prio;
				wtask->sb = NULL;
			}
			CURRENT->kcall.addtask.ans.pid = wtask;
			break;
	 
		  case KRNL_STARTTASK:      /* Start a task */
			wtask = CURRENT->kcall.starttask.pid;
             /* put new task at the end of rdy queue */
            Q_FRONT(&queue[wtask->prio], Q_REMV(&blocked_q, wtask));
			break;

		  case KRNL_CREATESTACK:    /* Create new stack frame */
			wtask = CURRENT->kcall.createstack.ask.pid;
			wtask->sb = malloc(CURRENT->kcall.createstack.ask.size);
			wtask->sp = wtask->sb + CURRENT->kcall.createstack.ask.size - 1; 
			CURRENT->kcall.createstack.ans.ptr = wtask->sb;
			break;

		  case KRNL_PUSHSTACK:      /* Push data onto the stack */
			wtask = CURRENT->kcall.pushstack.pid;
            wtask->sp -= CURRENT->kcall.pushstack.size;
            memcpy((wtask->sp + 1), CURRENT->kcall.pushstack.ptr,
                   CURRENT->kcall.pushstack.size);
			break;

		  case KRNL_LAUNCHTASK:     /* Start a task with built-in context */
			wtask = CURRENT->kcall.launchtask.pid;
			if(!k_build_initial_stack(wtask,
                                      CURRENT->kcall.launchtask.ptr, 
                                      CURRENT->kcall.launchtask.args, 
                                      CURRENT->kcall.launchtask.stack)){
				continue; /* Sorry... */
			}
            Q_FRONT(&queue[wtask->prio], Q_REMV(&blocked_q, wtask));
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
addtask (unsigned char prio) {
    CURRENT->kcall.addtask.ask.prio = prio;
    CURRENT->kcall.code = (KRNL_ADDTASK);
    swtrap();
    return (CURRENT->kcall.addtask.ans.pid);
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

char*
createstack (pid_t pid, size_t size) {
    CURRENT->kcall.createstack.ask.pid = pid; 
    CURRENT->kcall.createstack.ask.size = size;
    CURRENT->kcall.code = (KRNL_CREATESTACK);
    swtrap();
    return (CURRENT->kcall.createstack.ans.ptr);
}

/*
 *
 */

void
pushstack (pid_t pid, char* ptr, size_t size) {
    CURRENT->kcall.pushstack.pid = pid;   
    CURRENT->kcall.pushstack.ptr = ptr;
    CURRENT->kcall.pushstack.size = size;
    CURRENT->kcall.code = (KRNL_PUSHSTACK);
    swtrap();
    return;
}

/*
 *
 */

void
launchtask (pid_t pid, void(*ptsk)(void* args), void* args, size_t stacksize) {
    CURRENT->kcall.launchtask.pid = pid;   
    CURRENT->kcall.launchtask.ptr = ptsk;
    CURRENT->kcall.launchtask.args = args;
    CURRENT->kcall.launchtask.stack = stacksize;
    CURRENT->kcall.code = (KRNL_LAUNCHTASK);
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

