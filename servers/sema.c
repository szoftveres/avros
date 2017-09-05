#include "kernel.h"
#include "sema.h"
#include "queue.h"
/*
 *
 */

typedef struct client_s {
    QUEUE_HEADER
    pid_t pid;
} client_t;

/*
 *
 */

typedef struct sema_s {
    QUEUE_HEADER
    unsigned int    value;
    q_head_t        client_q;
} sema_t;

/*
 *
 */

static q_head_t sema_q;

/*
 * Returns the created sema, or NULL on error
 */

static sema_t*
newsema (unsigned int val) {
    sema_t *s;
    s = (sema_t*) kmalloc(sizeof(sema_t));
    if (s) {
        q_init(&(s->client_q));
        s->value = val;
    }
    return s;
}

/*
 *
 */

static unsigned int
decreasesema (sema_t* s) {
    unsigned int val;
    if (!s) {
        return 0;
    }
    val = s->value;
    if ((s->value) != 0) {
        s->value--;
    }
    return (val);
}

/*
 *
 */

static void
increasesema (sema_t* s) {
    if(s){
        s->value++;
    }
    return;
}

/*
 * Returns the created task item, or NULL on error
 */

static client_t*
addclient (sema_t* s, pid_t pid) {
    client_t *cli;
    if (!s || !pid) {
        return NULL;
    }
    cli = (client_t*) kmalloc(sizeof(client_t));
    if (!cli) {
        return NULL;
    }
    cli->pid = pid;
    Q_END(&(s->client_q), cli);
    return (cli);
}

/*
 *
 */

#define FIRSTCLIENT(S) ((S)?((client_t*)Q_FIRST((S)->client_q)):NULL)

/*
 * Returns the removed task item, or NULL on error
 */

static client_t*
removeclient (sema_t* s, pid_t pid) {
    client_t *cli;
    if(!pid){
        return NULL;
    }
    cli = FIRSTCLIENT(s);
    if (!cli) {
        return NULL;
    }
    if (cli->pid != pid) {
        return NULL;
    }
    kfree(Q_REMV(&(s->client_q), cli));
    return (cli);
}

/*
 *
 */

static pid_t
waiter (sema_t* s) {
    client_t *cli;
    cli = FIRSTCLIENT(s);
    if (!cli) {
        return NULL;
    }
    return (cli->pid);
}

/*
 *
 */

static unsigned int
getvalue (sema_t* s) {
    if (!s) {
        return 0;
    }
    return (s->value);
}

/*
 *
 */

typedef enum {
    SEMA_ERROR    = 0,
    SEMA_CREATE,
    SEMA_DELETE,  /* No reply needed */
    SEMA_WAIT,
    SEMA_SIGNAL,  /* No reply needed */
    SEMA_GET
} sema_cmd;

/*
 *
 */

typedef struct semamsg_s {
    sema_t*        sema;
    sema_cmd       cmd;
    unsigned int   val;
} semamsg_t;

/*
 *
 */

void
semasrv (void* args UNUSED) {
    pid_t client;
    semamsg_t msg;
    /* Do some cleanup before start */
    q_init(&(sema_q));
    /* Let's go! */

    while (1) {
        client = receive(TASK_ANY, &msg, sizeof(msg));
        switch(msg.cmd){

          case SEMA_CREATE:
            msg.sema = (sema_t*) Q_FRONT(&sema_q, newsema(msg.val));
            break;

          case SEMA_DELETE:
            if(waiter(msg.sema)){
                continue; /* Sorry.. */
            }
            kfree((void*)Q_REMV(&sema_q, msg.sema));
            break;

          case SEMA_WAIT:
            if (decreasesema(msg.sema)) {
                break; /* Ok, enter */
            }
            addclient(msg.sema, client);
            continue; /* Wait */

          case SEMA_SIGNAL:
            if ((client = waiter(msg.sema)) != NULL) {
                removeclient(msg.sema, client);
                break; /* Ok, enter waiting client */
            }
            increasesema(msg.sema);
            continue; /* Nobody else waiting */

          case SEMA_GET:
            msg.val = getvalue(msg.sema);
            break;

          default:
            continue;
        }
        send(client, &msg);
    }
}

/*
 *
 */

static pid_t    sematask;

/*
 *
 */

pid_t
setsemapid (pid_t pid) {
    sematask = pid;
    return (sematask);
}

/*
 *
 */

sema
createsema (unsigned int val) {
    semamsg_t msg;
    msg.cmd = SEMA_CREATE;
    msg.val = val;
    sendrec(sematask, &msg, sizeof(msg));
    return msg.sema;
}

/*
 *
 */

void
deletesema (sema s) {
    semamsg_t msg;
    msg.cmd = SEMA_DELETE;
    msg.sema = s;
    sendrec(sematask, &msg, sizeof(msg));
    return;
}

/*
 *
 */

void
waitsema (sema s) {
    semamsg_t msg;
    msg.cmd = SEMA_WAIT;
    msg.sema = s;
    sendrec(sematask, &msg, sizeof(msg));
    return;
}

/*
 *
 */

void
signalsema (sema s) {
    semamsg_t msg;
    msg.cmd = SEMA_SIGNAL;
    msg.sema = s;
    send(sematask, &msg);
    return;
}

/*
 *
 */

unsigned int
getsema (sema s) {
    semamsg_t msg;
    msg.cmd = SEMA_GET;
    msg.sema = s;
    sendrec(sematask, &msg, sizeof(msg));
    return msg.val;
}

