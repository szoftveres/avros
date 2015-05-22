#include "pm.h"
#include "es.h"
#include "vfs.h"
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
    };
    struct pm_task_s*   parent;
} pm_task_t;

#define PM_PIDOF(p) ((p) ? ((p)->pid) : (NULL))

/*
 *
 */

static q_head_t task_q;
static q_head_t wait_q;
static q_head_t zombie_q;

#define PM_CLIENT       ((pm_task_t*)(Q_FIRST(task_q)))



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

typedef union spawn_u {
    struct {
        int(*ptp)(char**);              /* task entry */
        size_t  stack;                  /* stack size */
	    char**  argv;                   /* argv */
    } ask;
    struct {
        pid_t pid;                      /* pid */
    } ans;
} spawn_t;

/*
 * EXEC
 */

typedef union exec_u {
    struct {
        char*           name;           /* prg name */
        char**          argv;           /* argv */
    } ask;
    struct {
    } ans;
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

typedef union wait_s {
    struct {
        pid_t pid;                      /* pid */
    } ask;
    struct {
        pid_t pid;                      /* pid */
        int   code;                     /* exit code */
    } ans;
} wait_t;

/*
 * MALLOC
 */

typedef union pmmalloc_u {
    struct {
        size_t          size;           /* size */
    } ask;
    struct {
        void*           ptr;            /* ptr */
    } ans;
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
 *
 */

typedef struct pmmsg_s {
    int cmd;
    union {
        spawn_t         spawn;          /* spawn */
        exec_t          exec;           /* reg prg */
        exit_t          exit;           /* exit */
        wait_t          wait;           /* wait */
        pmmalloc_t      malloc;         /* malloc */
        register_t      reg;            /* register */
        pmfree_t        free;           /* free */
    };
} pmmsg_t;


/*
============================================================
*/



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
 * Check whether ptsk is the child of PM_CLIENT
 */
static q_item_t*
pm_haschild (q_head_t* que UNUSED, q_item_t* ptsk) {
    if(((pm_task_t*)ptsk)->parent != PM_CLIENT) {
		return NULL;
    }
    return (ptsk);
}

/**
 * Check whether ptsk is the child of PM_CLIENT, and PM_CLIENT is 
 * waiting for it (ar any)
 */
static q_item_t*
pm_findzombie (q_head_t* que UNUSED, q_item_t* ptsk) {
    if(((pm_task_t*)ptsk)->parent != PM_CLIENT) {
        return NULL;
    }
    if(((PM_CLIENT->waitfor != PM_PIDOF(((pm_task_t*)ptsk))) 
        && (PM_CLIENT->waitfor != TASK_ANY))){
    	return (NULL);
	}
    return (ptsk);
}

/**
 * Check whether ptsk is the child of PM_CLIENT and make it orphan
 */
static q_item_t*
pm_makechildorphan (q_head_t* que UNUSED, q_item_t* ptsk) {
    if(((pm_task_t*)ptsk)->parent != PM_CLIENT) {
        return (NULL);
    }
    ((pm_task_t*)ptsk)->parent = (NULL); /* the task has no parent anymore */
    return (NULL);
}

/**
 * Check whether ptsk is the child of PM_CLIENT and remove it
 */
static q_item_t*
pm_removechild (q_head_t* que, q_item_t* ptsk) {
    if(((pm_task_t*)ptsk)->parent != PM_CLIENT) {
        return (NULL);
    }
    kfree(Q_REMV(que, ptsk));
    return (NULL);
}

/**
 * check whether ptsk is the parent of PM_CLIENT
 */
static q_item_t*
pm_findmyparent (q_head_t* que UNUSED, q_item_t* ptsk) {
    if(PM_CLIENT->parent != ((pm_task_t*)ptsk)) {
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

static mem_chunk_t*
findchunk (pm_task_t* ptsk, void* ptr) {
    mem_chunk_t *it;
    if (!ptsk || !ptr) {
        return (NULL);
    }
    it = (mem_chunk_t*) Q_FIRST(ptsk->chunk_q);
    while (it) {
        if(it->ptr == ptr){
            break;
        }
        it = (mem_chunk_t*) Q_NEXT(it);
    }   
    return (it);
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
    for (i = 0; argv && argv[i]; i++);
    return i;
}

/*
 *
 */

static size_t
pm_argstack_size (char* orgargv[]) {
    int i;
    size_t size = 0;
    for( i = 0; orgargv && orgargv[i]; i++){
        size += (strlen(orgargv[i]) + 1);
    }
    size += (sizeof(char*) * (i + 1));
    return size;
}


/* ======================================================
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
        kfree(stack);
		return (NULL);
	}
    patch_argstack(stack, (sb + sizeof(cpu_context_t) + stacksize));
    pushstack(PM_PIDOF(ptsk), stack, argsize);
    kfree(stack);

    stack = (char*) kmalloc(sizeof(cpu_context_t));
    if (!stack) {
        return (NULL);
    }
     
    {
        char c;
        c = LOW(mexit);
        pushstack(PM_PIDOF(ptsk), &c, 1);
        c = HIGH(mexit);
        pushstack(PM_PIDOF(ptsk), &c, 1);
    }

    ((cpu_context_t*)stack)->r1 = 0;
    ((cpu_context_t*)stack)->r24 = LOW((sb + sizeof(cpu_context_t) + stacksize));
    ((cpu_context_t*)stack)->r25 = HIGH((sb + sizeof(cpu_context_t) + stacksize));
    ((cpu_context_t*)stack)->retHigh = HIGH(ptr);
    ((cpu_context_t*)stack)->retLow = LOW(ptr);

    pushstack(PM_PIDOF(ptsk), stack, sizeof(cpu_context_t));    
    kfree(stack);
    return (ptsk);
}



/*
============================================================
*/

int
do_reg (pmmsg_t* msg) {
    pm_task_t*  pm_task;

    pm_task = pm_newtask(msg->reg.pid, NULL);
    if(!Q_END(&task_q, pm_task)){
        return 0;   /* Sorry... */
    }
    if(!vfs_addtask(msg->reg.pid, NULL)){
        return 0;
    }
    return 1;
}

/*
*/

void
do_spawn (pmmsg_t* msg) {
    pid_t       task;
    pm_task_t*  pm_task;

    task = addtask(TASK_PRIO_DFLT);
    if (!task) {
        return;   /* Sorry... */
    }
    pm_task = pm_newtask(task, PM_CLIENT);
    if (!Q_END(&task_q, pm_task)) {
        return;   /* Sorry... */
    }                
    if (!vfs_addtask(task, PM_PIDOF(PM_CLIENT))){
        return;   /* Sorry... */
    }
    if (!push_cpucontext(pm_task, msg->spawn.ask.stack, 
        msg->spawn.ask.ptp, msg->spawn.ask.argv)) {
        return;   /* Sorry... */
    }
    starttask(task);
    msg->spawn.ans.pid = task;
    return;
}

/*
*/

int
do_exec (pmmsg_t* msg) {
    int(*ptr)(char**);
    size_t stacksize;

    es_getprg(msg->exec.ask.name, &ptr, &stacksize);          
    if (!ptr) {
        return 1;  /* Error, send reply */
    }
    if (!push_cpucontext(PM_CLIENT, stacksize, ptr,
            msg->exec.ask.argv)) {
        /* nothing to do, task should be removed completely */
        return 0;
    }
    q_forall(&(PM_CLIENT->chunk_q), pm_delchunks);
    starttask(PM_CLIENT->pid);
    /* no reply */
    return 0;
}

/*
*/

int
do_exit (pmmsg_t* msg, pid_t* replyto) {
    pm_task_t*  pm_task;

    q_forall(&(PM_CLIENT->chunk_q), pm_delchunks);
    stoptask(PM_PIDOF(PM_CLIENT));
    deletetask(PM_PIDOF(PM_CLIENT)); 
    vfs_deletetask(PM_CLIENT->pid);

    /* Deleting zombie children */
    q_forall(&zombie_q, pm_removechild);
    /* Making other children orphan */
    q_forall(&wait_q, pm_makechildorphan);
    q_forall(&task_q, pm_makechildorphan);

    if (!PM_CLIENT->parent) {
        /* client is orphan, remove it */
        kfree(Q_REMV(&task_q, PM_CLIENT));
        return 0; /* No reply */
    }
    /* check whether parent task is in the waiting queue */
    pm_task = (pm_task_t*) q_forall(&wait_q, pm_findmyparent);
    if (pm_task && 
            ((pm_task->waitfor == PM_PIDOF(PM_CLIENT)) || 
            (pm_task->waitfor == TASK_ANY))) {
        /* parent is waiting for client (or any) */
        Q_END(&task_q, Q_REMV(&wait_q, pm_task));        
        PM_CLIENT->exitcode = msg->exit.code; /* Save ExitCode from msg */
        msg->wait.ans.pid = PM_PIDOF(PM_CLIENT); /* PID */
        msg->wait.ans.code = PM_CLIENT->exitcode; /* ExitCode */
        *replyto = PM_PIDOF(pm_task); /* Reply to parent */
        kfree(Q_REMV(&task_q, PM_CLIENT)); /* Remove PM_CLIENT */
    } else {
        /* parent is not waiting yet */
        PM_CLIENT->exitcode = msg->exit.code;
        Q_END(&zombie_q, Q_REMV(&task_q, PM_CLIENT));
        /* Client is now zombie */
	    return 0; /* No reply */
    }
    return 1;       
}

/*
*/

int
do_wait (pmmsg_t* msg) {
    pm_task_t*  pm_task;

    PM_CLIENT->waitfor = msg->wait.ask.pid;
    pm_task = (pm_task_t*) q_forall(&zombie_q, pm_findzombie);
    if (pm_task) {	/* found a zombie child */
        msg->wait.ans.pid = PM_PIDOF(pm_task); /* PID */
        msg->wait.ans.code = pm_task->exitcode; /*Exit Code*/
        kfree(Q_REMV(&zombie_q, pm_task)); /* Remove zombie child */
        return 1; /* Reply to client */
    }
    if (!q_forall(&task_q, pm_haschild) && 
            !q_forall(&wait_q, pm_haschild)) {
        /* ERROR, no child at all */
        msg->wait.ans.pid = 0; /* PID */
        msg->wait.ans.code = 0; /* Exit Code */
        return 1; /* XXX Reply to client */
    }
    /* Client is waiting */
    Q_END(&wait_q, Q_REMV(&task_q, PM_CLIENT));
    return 0; /* No reply */
}

/*
*/

void
do_pmalloc (pmmsg_t* msg) {

    mem_chunk_t *chunk = (mem_chunk_t*) kmalloc(sizeof(mem_chunk_t));
    if (!chunk) {
        msg->malloc.ans.ptr = NULL;
        return;
    }
    chunk->ptr = kmalloc(msg->malloc.ask.size);
    if(!chunk->ptr){
        kfree(chunk);
        msg->malloc.ans.ptr = NULL;
        return;
    }
    Q_FRONT(&(PM_CLIENT->chunk_q), chunk);
    msg->malloc.ans.ptr = chunk->ptr;
    return; 
}

/*
*/

void
do_pmfree (pmmsg_t* msg) {
    mem_chunk_t* chunk = findchunk(PM_CLIENT, msg->free.ptr);
    if (chunk && chunk->ptr) {
        kfree (chunk->ptr);
    }
    kfree(Q_REMV(&(PM_CLIENT->chunk_q), chunk));
    return;
}

/*
============================================================
*/


void
pm (void* args UNUSED) {
    pid_t msg_client;
    
    pmmsg_t msg;

    q_init(&task_q);
    q_init(&wait_q);
    q_init(&zombie_q);

    while (1) {
        msg_client = receive(TASK_ANY, &msg, sizeof(msg));
        Q_FRONT(&task_q, Q_REMV(&task_q, pm_findbypid(msg_client)));

        switch(msg.cmd){

          case PM_REG:
            if (!do_reg(&msg)) {
                continue;
            }
            break;

          case PM_SPAWN:
            do_spawn(&msg);
            break;

          case PM_EXEC:
            if (!do_exec(&msg)) {
                continue;
            }
            break;

          case PM_EXIT: 
            if (!do_exit(&msg, &msg_client)) {
                continue;
            }
	        break;

          case PM_WAIT:
            if (!do_wait(&msg)) {
                continue;
            }
            break;

          case PM_MALLOC:
            do_pmalloc(&msg);
            break;

          case PM_FREE:
            do_pmfree(&msg);
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
    msg.spawn.ask.ptp = ptsk;
    msg.spawn.ask.stack = stacksize;
    msg.spawn.ask.argv = argv;
    sendrec(pmtask, &msg, sizeof(msg));
    return (msg.spawn.ans.pid);
}

/*
 * Execute
 */

int
execv (char* name, char** argv) {
    pmmsg_t msg;
    msg.cmd = PM_EXEC;
    msg.exec.ask.name = name;
    msg.exec.ask.argv = argv;
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
    msg.exit.code = code;
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
	msg.wait.ask.pid = TASK_ANY;
    sendrec(pmtask, &msg, sizeof(msg));
    if(code){
        *code = msg.wait.ans.code;
    }
    return (msg.wait.ans.pid);
}

/*
 *
 */

pid_t
waitpid (pid_t p, int* code) {
    pmmsg_t msg;
    msg.cmd = PM_WAIT;
	msg.wait.ask.pid = p;
    sendrec(pmtask, &msg, sizeof(msg));
    if (code) {
        *code = msg.wait.ans.code;
    }
    return (msg.wait.ans.pid);
}


/*
 *
 */

void*
pmmalloc (size_t size) {
    pmmsg_t msg;
    msg.cmd = PM_MALLOC;
    msg.malloc.ask.size = size;
    sendrec(pmtask, &msg, sizeof(msg));
    return (msg.malloc.ans.ptr);
}

/*
 *
 */

void
pmfree (void* ptr) {
    pmmsg_t msg;
    msg.cmd = PM_FREE;
    msg.free.ptr = ptr;
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
    msg.reg.pid = pid;
    sendrec(pmtask, &msg, sizeof(msg));
    return;
}

