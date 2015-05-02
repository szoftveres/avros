#include "pm.h"
#include "es.h"
#include "dm.h"
#include "kernel.h"
#include "queue.h"

#include "sys.h"

/*
============================================================
*/

typedef struct pm_task_s {
    QUEUE_HEADER
    pid_t               pid;
    q_head_t            chunk_q;
    union {
        pid_t           waitfor;   /* Waits for this pid to die */
        int             exitcode;  /* Exit code of zombie */
    } param;
    struct pm_task_s*   parent;
} pm_task_t;

#define PM_PIDOF(p) ((p) ? ((p)->pid) : (NULL))

/*
 *
 */

static q_head_t task_q;
static q_head_t wait_q;
static q_head_t zombie_q;

#define CLIENT         ((pm_task_t*)(Q_FIRST(task_q)))

/*
 * pid: task's own pid in kernel
 */

static pm_task_t*
pm_newtask (pid_t pid, pm_task_t* parent) {
    pm_task_t* pt;    
    pt = (pm_task_t*) kmalloc(sizeof(pm_task_t));
    if(!pt){
        return (NULL);
    }    
    memset(pt, 0x00, sizeof(pm_task_t)); 
    pt->pid = pid;
    q_init(&(pt->chunk_q));
    pt->parent = parent;    
    return (pt);   
}

/*
 *
 */

static pm_task_t*
pm_findbypid (pid_t pid) {
    pm_task_t *it = (pm_task_t*)Q_FIRST(task_q);
    while (it) {
        if(PM_PIDOF(it) == pid){
            break;
        }
        it = (pm_task_t*)Q_NEXT(it);
    } 
    return (it);
}

/**
 * Check whether ptsk is the child of CLIENT
 */
static q_item_t*
pm_haschild (q_head_t* que UNUSED, q_item_t* ptsk) {
    if(((pm_task_t*)ptsk)->parent != CLIENT) {
		return NULL;
    }
    return (ptsk);
}

/**
 * Check whether ptsk is the child of CLIENT, and CLIENT is 
 * waiting for it (ar any)
 */
static q_item_t*
pm_findzombie (q_head_t* que UNUSED, q_item_t* ptsk) {
    if(((pm_task_t*)ptsk)->parent != CLIENT) {
        return NULL;
    }
    if(((CLIENT->param.waitfor != PM_PIDOF(((pm_task_t*)ptsk))) 
        && (CLIENT->param.waitfor != TASK_ANY))){
    	return (NULL);
	}
    return (ptsk);
}

/**
 * Check whether ptsk is the child of CLIENT and make it orphan
 */
static q_item_t*
pm_makechildorphan (q_head_t* que UNUSED, q_item_t* ptsk) {
    if(((pm_task_t*)ptsk)->parent != CLIENT) {
        return (NULL);
    }
    ((pm_task_t*)ptsk)->parent = (NULL); /* the task has no parent anymore */
    return (NULL);
}

/**
 * Check whether ptsk is the child of CLIENT and remove it
 */
static q_item_t*
pm_removechild (q_head_t* que, q_item_t* ptsk) {
    if(((pm_task_t*)ptsk)->parent != CLIENT) {
        return (NULL);
    }
    kfree(Q_REMV(que, ptsk));
    return (NULL);
}

/**
 * check whether ptsk is the parent of CLIENT
 */
static q_item_t*
pm_findmyparent (q_head_t* que UNUSED, q_item_t* ptsk) {
    if(CLIENT->parent != ((pm_task_t*)ptsk)) {
        return (NULL);
    }
    return (ptsk);
}

/*
============================================================
*/

typedef struct mem_chunk_s {
    QUEUE_HEADER
    void*               ptr;
} mem_chunk_t;

/*
 *
 */

static void*
do_pmalloc (pm_task_t* ptsk, size_t size) {
    mem_chunk_t *chunk = (mem_chunk_t*) kmalloc(sizeof(mem_chunk_t));
    if(!chunk){
        return (NULL);
    }
    chunk->ptr = kmalloc(size);
    if(!chunk->ptr){
        kfree(chunk);
        return (NULL);
    }
    Q_FRONT(&(ptsk->chunk_q), chunk);  
    return (chunk->ptr);
}

/*
 *
 */

static mem_chunk_t*
findchunk (pm_task_t* ptsk, void* ptr) {
    mem_chunk_t *it;
    if (!ptsk || !ptr) {
        return (NULL);
    }
    it = (mem_chunk_t*) Q_FIRST(ptsk->chunk_q);
    while(it){
        if(it->ptr == ptr){
            break;
        }
        it = (mem_chunk_t*) Q_NEXT(it);
    }   
    return (it);
}

/*
 * Returns the removed task item, or NULL on error
 */

static void
do_pmfree (pm_task_t* ptsk, void* ptr) {
    mem_chunk_t* chunk = findchunk(ptsk, ptr);
    if (chunk && chunk->ptr) {
        kfree (chunk->ptr);
    }
    kfree(Q_REMV(&(ptsk->chunk_q), chunk));
    return;
}

/*
 *
 */

static q_item_t*
pm_delchunks (q_head_t* que, q_item_t* chunk) {
    if (((mem_chunk_t*)chunk)->ptr) {
        kfree(((mem_chunk_t*)chunk)->ptr);
    }
    kfree(Q_REMV(que, chunk));
    return NULL;
}

/*
============================================================
*/

int
argc (char* argv[]) {
    int i;
    for(i=0; argv && argv[i]; i++);
    return i;
}

/*
 *
 */

static size_t
pm_argstack_size (char* orgargv[]) {
    int i;
    size_t size = 0;
    for(i=0; orgargv && orgargv[i]; i++){
        size += (strlen(orgargv[i]) + 1);
    }
    size += (sizeof(char*) * (i + 1));
    return size;
}

/*
 * User task launcher frame
 */

static void
pm_task_launcher (int(*ptsk)(char**), char** argv) {
    mexit(ptsk(argv));
}

/*
 *
 */

static void
cook_argstack (char* bottom, char* orgargv[]) {
    int i;
    int num = argc(orgargv);
    char** newargv = (char**) bottom;
	bottom += (sizeof(char*) * (num + 1));
	for(i=0; (i < num) && (orgargv) && (orgargv[i]); i++){
        newargv[i] = bottom;
		strcpy(bottom, orgargv[i]);
		bottom += (strlen(orgargv[i]) + 1); //  string + `\0`
	}
    newargv[i] = NULL;
}

/*
 *
 */

static void
patch_argstack (char* bottom, char* newbottom) {
    char** argp = (char**) bottom;
    while(*argp){
        if(newbottom > bottom) {
            *argp += (newbottom - bottom);
        } else {
            *argp -= (bottom - newbottom);
        }
        argp++;
    }
}

/*
 *
 */

static pm_task_t*
push_cpucontext (pm_task_t* ptsk, size_t stacksize, int(*ptr)(char**), 
        char** argv) {
    char   *stack;
    char   *sb;    
    size_t argsize = pm_argstack_size(argv);

    stack = (char*) kmalloc(argsize);     
    if (!stack) {
        return (NULL);
    }
    cook_argstack(stack, argv);
    /* all data is saved, we can free the old stack */
    stoptask(PM_PIDOF(ptsk));
    sb = createstack(PM_PIDOF(ptsk), (sizeof(cpu_context_t) + argsize + stacksize));
	if (!sb) {
		return (NULL);
	}
    patch_argstack(stack, (sb + sizeof(cpu_context_t) + stacksize));
    pushstack(PM_PIDOF(ptsk), stack, argsize);
    kfree(stack);

    stack = (char*) kmalloc(sizeof(cpu_context_t));
    if (!stack) {
        return (NULL);
    } 

    ((cpu_context_t*)stack)->r1 = 0;
    ((cpu_context_t*)stack)->r22 = LOW((sb + sizeof(cpu_context_t) + stacksize));
    ((cpu_context_t*)stack)->r23 = HIGH((sb + sizeof(cpu_context_t) + stacksize));
    ((cpu_context_t*)stack)->r24 = LOW(ptr);
    ((cpu_context_t*)stack)->r25 = HIGH(ptr);
    ((cpu_context_t*)stack)->retHigh = HIGH(pm_task_launcher);
    ((cpu_context_t*)stack)->retLow = LOW(pm_task_launcher);

    pushstack(PM_PIDOF(ptsk), stack, sizeof(cpu_context_t));    
    kfree(stack);
    return (ptsk);
}


/*
============================================================
*/

/*
 * PM COMMANDS
 */

enum {
    PM_REG,
    PM_SPAWN,
    PM_EXEC,
    PM_WAIT,
    PM_EXIT,
    PM_MALLOC,
    PM_FREE,
};

/*
 * SPAWN 
 */

typedef struct ask_spawn_s {
    int(*ptp)(char**);              /* task entry */
    size_t  stack;                  /* stack size */
	char**  argv;                   /* argv */
} ask_spawn_t;

typedef struct ans_spawn_s {
    pid_t pid;                      /* pid */
} ans_spawn_t;

typedef union spawn_u {
    ask_spawn_t ask;                /* ask */
    ans_spawn_t ans;                /* answer */
} spawn_t;

/*
 * EXEC
 */

typedef struct ask_exec_s {
    char*           name;           /* prg name */
    char**          argv;           /* argv */
} ask_exec_t;

typedef struct ans_exec_s {
} ans_exec_t;

typedef union exec_u {
    ask_exec_t   ask;               /* ask */
    ans_exec_t   ans;               /* answer */
} exec_t;

/*
 * EXIT
 */

typedef struct exit_s {
    int code;                       /* exit code */
} exit_t;

/*
 * WAIT
 */

typedef struct ask_wait_s {
    pid_t pid;                      /* pid */
} ask_wait_t;

typedef struct ans_wait_s {
    pid_t pid;                      /* pid */
    int   code;                     /* exit code */
} ans_wait_t;

typedef union wait_s {
    ask_wait_t ask;                 /* ask */
    ans_wait_t ans;                 /* answer */
} wait_t;

/*
 * MALLOC
 */

typedef struct ask_pmmalloc_s {
    size_t          size;           /* size */
} ask_pmmalloc_t;

typedef struct ans_pmmalloc_s {
    void*           ptr;            /* ptr */
} ans_pmmalloc_t;

typedef union pmmalloc_u {
    ask_pmmalloc_t   ask;           /* ask */
    ans_pmmalloc_t   ans;           /* answer */
} pmmalloc_t;


/*
 * FREE
 */

typedef struct pmfree_s {
    void*           ptr;            /* pointer */
} pmfree_t;


/*
 * KILL
 */

typedef struct kill_s {
    pid_t pid;                      /* pid */
} kill_t;

/*
 * REGISTER
 */

typedef struct register_s {
    pid_t pid;                      /* pid */
} register_t;

/*
 * PM CALL
 */

typedef union pm_param_u {
    spawn_t         spawn;          /* spawn */
    exec_t          exec;           /* reg prg */
    exit_t          exit;           /* exit */
    wait_t          wait;           /* wait */
    pmmalloc_t      malloc;         /* malloc */
    register_t      reg;            /* register */
    pmfree_t        free;           /* free */
} pm_param_t;

/*
 *
 */

typedef struct pmmsg_s {
    int cmd;
    pm_param_t param;
} pmmsg_t;

/*
============================================================
*/


void
pm (void) {
    pid_t msg_client;
    pm_task_t* pm_task;
    
    pmmsg_t msg;

    q_init(&task_q);
    q_init(&wait_q);
    q_init(&zombie_q);

    while(1){
        msg_client = receive(TASK_ANY, &msg, sizeof(msg));
        Q_FRONT(&task_q, Q_REMV(&task_q, pm_findbypid(msg_client)));

        switch(msg.cmd){

          case PM_REG:
            pm_task = pm_newtask(msg.param.reg.pid, NULL);
            if(!Q_END(&task_q, pm_task)){
                continue;   /* Sorry... */
            }
            if(!dm_addtask(msg.param.reg.pid, NULL)){
                continue;
            }
            break;

          case PM_SPAWN:{
                pid_t task;
                task = addtask(TASK_PRIO_DFLT);
                if(!task){
                    continue;   /* Sorry... */
                }
                pm_task = pm_newtask(task, CLIENT);
                if(!Q_END(&task_q, pm_task)){
                    continue;   /* Sorry... */
                }                
                if (!dm_addtask(task, PM_PIDOF(CLIENT))){
                        continue;   /* Sorry... */
                }
                if(!push_cpucontext(pm_task, msg.param.spawn.ask.stack, 
                        msg.param.spawn.ask.ptp, msg.param.spawn.ask.argv)){
                    continue;   /* Sorry... */
                }
                starttask(task);
                msg.param.spawn.ans.pid = task;
            }
            break; // Reply to client

          case PM_EXEC:{
                int(*ptr)(char**);
                size_t stacksize;
                es_getprg(msg.param.exec.ask.name, &ptr, &stacksize);          
                if (!ptr) {
                    break;  /* Error, send reply */
                }
                if (!push_cpucontext(CLIENT, stacksize, ptr,
                        msg.param.exec.ask.argv)) {
                    /* nothing to do, task should be removed completely */
                    continue;
                }
                q_forall(&(CLIENT->chunk_q), pm_delchunks);
                starttask(msg_client);
                /* no reply */
                continue;
            }
            break;    

          case PM_EXIT: 
            q_forall(&(CLIENT->chunk_q), pm_delchunks);
            stoptask(PM_PIDOF(CLIENT));
            deletetask(PM_PIDOF(CLIENT)); 
            dm_deletetask(msg_client);

            /* Deleting zombie children */
            q_forall(&zombie_q, pm_removechild);
            /* Making other children orphan */
            q_forall(&wait_q, pm_makechildorphan);
            q_forall(&task_q, pm_makechildorphan);

	        if (!CLIENT->parent) {
                /* client is orphan, remove it */
                kfree(Q_REMV(&task_q, CLIENT));
                continue; /* No reply */
            }
            /* check whether parent task is in the waiting queue */
            pm_task = (pm_task_t*) q_forall(&wait_q, pm_findmyparent);
		    if (pm_task && 
                    ((pm_task->param.waitfor == PM_PIDOF(CLIENT)) || 
                    (pm_task->param.waitfor == TASK_ANY))) {
                /* parent is waiting for client (or any) */
                Q_END(&task_q, Q_REMV(&wait_q, pm_task));
	            msg.param.wait.ans.pid = PM_PIDOF(CLIENT); /* PID */
		        msg.param.wait.ans.code = CLIENT->param.exitcode; /* ExitCode */
                msg_client = PM_PIDOF(pm_task); /* Reply to parent */
                kfree(Q_REMV(&task_q, CLIENT)); /* Remove CLIENT */
            } else {
                /* parent is not waiting yet */
                CLIENT->param.exitcode = msg.param.exit.code;
                Q_END(&zombie_q, Q_REMV(&task_q, CLIENT));
                /* Client is now zombie */
			    continue; /* No reply */
		    }            
	        break;

          case PM_WAIT:
            CLIENT->param.waitfor = msg.param.wait.ask.pid;
            pm_task = (pm_task_t*) q_forall(&zombie_q, pm_findzombie);
	        if (pm_task) {	/* found a zombie child */
                msg.param.wait.ans.pid = PM_PIDOF(pm_task); /* PID */
		        msg.param.wait.ans.code = pm_task->param.exitcode; /*Exit Code*/
                kfree(Q_REMV(&zombie_q, pm_task)); /* Remove zombie child */
		        break; /* Reply to client */
	        }
            if (!q_forall(&task_q, pm_haschild) && 
                    !q_forall(&wait_q, pm_haschild)) {
                /* ERROR, no child at all */
                msg.param.wait.ans.pid = 0; /* PID */
		        msg.param.wait.ans.code = 0; /* Exit Code */
                continue; /* XXX */
            }
            /* Client is waiting */
            Q_END(&wait_q, Q_REMV(&task_q, CLIENT));
            continue; /* No reply */

          case PM_MALLOC:
            msg.param.malloc.ans.ptr = 
                    do_pmalloc(CLIENT, msg.param.malloc.ask.size);
            break;

          case PM_FREE:
            do_pmfree(CLIENT, msg.param.free.ptr);
            break;

          default:
            continue;
        }
        send(msg_client, &msg);
    }
}

/*
 *
 */

static pid_t pmtask;

/*
 *
 */

pid_t
setpmpid (pid_t pid) {
    pmtask = pid;
    return (pmtask);    
}

/*
 * Spawn
 */

pid_t
spawntask (int(*ptsk)(char**), size_t stacksize, char** argv) {
    pmmsg_t msg;
    msg.cmd = PM_SPAWN;
    msg.param.spawn.ask.ptp = ptsk;
    msg.param.spawn.ask.stack = stacksize;
    msg.param.spawn.ask.argv = argv;
    sendrec(pmtask, &msg, sizeof(msg));
    return (msg.param.spawn.ans.pid);
}

/*
 * Execute
 */

int
execv (char* name, char** argv) {
    pmmsg_t msg;
    msg.cmd = PM_EXEC;
    msg.param.exec.ask.name = name;
    msg.param.exec.ask.argv = argv;
    sendrec(pmtask, &msg, sizeof(msg));
    return (-1);
}

/*
 *
 */

void
mexit (int code) {
    pmmsg_t msg;
    msg.cmd = PM_EXIT;
    msg.param.exit.code = code;
    sendrec(pmtask, &msg, sizeof(msg));
    /* NEVER REACHED */
    return;
}

/*
 *
 */

pid_t
wait (int* code) {
    pmmsg_t msg;
    msg.cmd = PM_WAIT;
	msg.param.wait.ask.pid = TASK_ANY;
    sendrec(pmtask, &msg, sizeof(msg));
    if(code){
        *code = msg.param.wait.ans.code;
    }
    return (msg.param.wait.ans.pid);
}

/*
 *
 */

pid_t
waitpid (pid_t p, int* code) {
    pmmsg_t msg;
    msg.cmd = PM_WAIT;
	msg.param.wait.ask.pid = p;
    sendrec(pmtask, &msg, sizeof(msg));
    if(code){
        *code = msg.param.wait.ans.code;
    }
    return (msg.param.wait.ans.pid);
}


/*
 *
 */

void*
pmmalloc (size_t size) {
    pmmsg_t msg;
    msg.cmd = PM_MALLOC;
    msg.param.malloc.ask.size = size;
    sendrec(pmtask, &msg, sizeof(msg));
    return (msg.param.malloc.ans.ptr);
}

/*
 *
 */

void
pmfree (void* ptr) {
    pmmsg_t msg;
    msg.cmd = PM_FREE;
    msg.param.free.ptr = ptr;
    sendrec(pmtask, &msg, sizeof(msg));
    return;
}

/*
 *
 */

void
pmreg (pid_t pid) {
    pmmsg_t msg;
    msg.cmd = PM_REG;
    msg.param.reg.pid = pid;
    sendrec(pmtask, &msg, sizeof(msg));
    return;
}

