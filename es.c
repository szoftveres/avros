#include "kernel.h"
#include "queue.h"
#include "es.h"

/*
================================================================================
    MESSAGE TYPES
*/

enum {
    ES_NONE,
    ES_REGPRG,
    ES_GETPRG,
};


/*
================================================================================
    MESSAGE PARAMETERS
*/

/*
 * REG PRG 
 */

typedef struct regprg_s {
    char* name;                     /* prg name */
    int(*ptr)(char**);              /* prg entry */
    size_t stack;                   /* stack size */
} regprg_t;

/*
 * GET PRG
 */

typedef struct getprg_ans_s {
    int(*ptr)(char**);
    size_t          stack;
} getprg_ans_t;

typedef struct getprg_ask_s {
    char            *name;
} getprg_ask_t;

typedef union getprg_u {
    getprg_ask_t ask;
    getprg_ans_t ans;
} getprg_t;

/*
 * MESSAGE
 */

typedef struct es_msg_s {
    int             cmd;
    union {
        regprg_t        regprg;
        getprg_t        getprg;
    }; 
} es_msg_t;

/*
================================================================================
    EXECUTABLE PROGRAM REGISTRATION
*/

typedef struct es_prg_s {
    QUEUE_HEADER
    char* name;
    size_t stack;
    int(*ptr)(char**);
} es_prg_t;

/*
 *
 */

static q_head_t es_prg_head;

/*
 *
 */

static void
es_init_prg (void) {
    q_init(&es_prg_head);
}

/*
 *
 */

static es_prg_t*
es_reg_prg (char* name, int(*ptr)(char**), size_t stack) {
    es_prg_t* prg = (es_prg_t*) kmalloc(sizeof(es_prg_t));
    if(!prg){
        return NULL;
    }
    prg->ptr = ptr;
    prg->stack = stack;
    prg->name = kmalloc(strlen(name)+1); // (string + 0)
    if(!prg->name){
        return NULL;
    }
    strcpy(prg->name, name);
    Q_END(&es_prg_head, prg);
    return prg;
}

/*
 *
 */

static void
es_get_prg (char* name, int(**ptr)(char**), size_t *stack) {
    es_prg_t* it = (es_prg_t*)Q_FIRST(es_prg_head);
    while (it){
         if(!strcmp(name, it->name)){
            *ptr = it->ptr;
            *stack = it->stack;
            return;
         }
        it = (es_prg_t*)Q_NEXT(it);
    }
    *ptr = NULL;
    *stack = 0;
    return;
}

/*
================================================================================
    MAIN PROGRAM
*/

void
es (void* args UNUSED) {
    pid_t msg_client;
    es_msg_t        msg;
    es_init_prg();
    while(1){
        msg_client = receive(TASK_ANY, &msg, sizeof(msg));
        switch(msg.cmd){
          case ES_REGPRG:
            if(!es_reg_prg(msg.regprg.name, 
                        msg.regprg.ptr, msg.regprg.stack)){
                continue;
            }
            break;
          case ES_GETPRG:         
            es_get_prg(msg.getprg.ask.name, &(msg.getprg.ans.ptr), &(msg.getprg.ans.stack));
            break;
        }
        send(msg_client, &msg);
    }
}

/*
================================================================================
    INTERFACE
*/

static pid_t estask;

pid_t
setespid (pid_t pid) {
    estask = pid;
    return (pid);
}

/*
 *
 */

void
es_regprg (char* name, int(*ptr)(char**), size_t stack) {
    es_msg_t msg;
    msg.cmd = ES_REGPRG;
    msg.regprg.name = name;
    msg.regprg.ptr = ptr;
    msg.regprg.stack = stack;
    sendrec(estask, &msg, sizeof(msg));
    return;
}

/*
 *
 */

void
es_getprg (char* name, int(**ptr)(char**), size_t *stack) {
    es_msg_t msg;
    msg.cmd = ES_GETPRG;
    msg.getprg.ask.name = name;
    sendrec(estask, &msg, sizeof(msg));
    *ptr = msg.getprg.ans.ptr;
    *stack = msg.getprg.ans.stack;
}


