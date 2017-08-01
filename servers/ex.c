#include "../kernel/kernel.h"
#include "../lib/queue.h"
#include "ex.h"


enum {
    EX_NONE,
    EX_REGPRG,
    EX_GETPRG,
};


typedef struct regprg_s {
    char* name;                     /* prg name */
    int(*ptr)(char**);              /* prg entry */
    size_t stack;                   /* stack size */
} regprg_t;


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


typedef struct ex_msg_s {
    int             cmd;
    union {
        regprg_t        regprg;
        getprg_t        getprg;
    };
} ex_msg_t;


typedef struct ex_prg_s {
    QUEUE_HEADER
    char* name;
    size_t stack;
    int(*ptr)(char**);
} ex_prg_t;


static q_head_t ex_prg_head;


static void
ex_init_prg (void) {
    q_init(&ex_prg_head);
}


static ex_prg_t*
ex_reg_prg (char* name, int(*ptr)(char**), size_t stack) {
    ex_prg_t* prg = (ex_prg_t*) kmalloc(sizeof(ex_prg_t));
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
    Q_END(&ex_prg_head, prg);
    return prg;
}


static void
ex_get_prg (char* name, int(**ptr)(char**), size_t *stack) {
    ex_prg_t* it = (ex_prg_t*)Q_FIRST(ex_prg_head);
    while (it){
        if(!strcmp(name, it->name)){
            *ptr = it->ptr;
            *stack = it->stack;
            return;
        }
        it = (ex_prg_t*)Q_NEXT(it);
    }
    *ptr = NULL;
    *stack = 0;
    return;
}


void
ex (void* args UNUSED) {
    pid_t msg_client;
    ex_msg_t        msg;
    ex_init_prg();
    while(1){
        msg_client = receive(TASK_ANY, &msg, sizeof(msg));
        switch(msg.cmd){
          case EX_REGPRG:
            if(!ex_reg_prg(msg.regprg.name,
                        msg.regprg.ptr, msg.regprg.stack)){
                continue;
            }
            break;
          case EX_GETPRG:
            ex_get_prg(msg.getprg.ask.name, &(msg.getprg.ans.ptr), &(msg.getprg.ans.stack));
            break;
        }
        send(msg_client, &msg);
    }
}


static pid_t extask;

pid_t
setexpid (pid_t pid) {
    extask = pid;
    return (pid);
}

/*
 *
 */

void
ex_regprg (char* name, int(*ptr)(char**), size_t stack) {
    ex_msg_t msg;
    msg.cmd = EX_REGPRG;
    msg.regprg.name = name;
    msg.regprg.ptr = ptr;
    msg.regprg.stack = stack;
    sendrec(extask, &msg, sizeof(msg));
    return;
}

/*
 *
 */

void
ex_getprg (char* name, int(**ptr)(char**), size_t *stack) {
    ex_msg_t msg;
    msg.cmd = EX_GETPRG;
    msg.getprg.ask.name = name;
    sendrec(extask, &msg, sizeof(msg));
    *ptr = msg.getprg.ans.ptr;
    *stack = msg.getprg.ans.stack;
}


