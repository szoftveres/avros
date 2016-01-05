#include "ts.h"
#include "kernel.h"
#include "queue.h"
#include <avr/io.h> /* timer */

/*
 *
 */

typedef struct tswait_s {
    QUEUE_HEADER
    pid_t           client;
    int             delay;
} tswait_t;

/*
 * TIMER COMMANDS
 */

enum {
    TS_NONE,
    TS_DELAY,
    TS_TICK,
    TS_GET_UPTIME,
    TS_GET_GLOBTIME,
    TS_SET_GLOBTIME,
    TS_EXIT,
};

/*
 * DELAY
 */

typedef struct delay_s {
    int             ticks;          /* ticks */
} delay_t;

/*
 *
 */

typedef struct tsmsg_s {
    int             cmd;
    union {
        delay_t         delay;          /* delay */
        time_t          uptime;         /* uptime */
        time_t          globtime;       /* global time */
    };
} tsmsg_t;


/*
 * Returns the created timer, or NULL on error
 */

static tswait_t*
addtswait (pid_t client, int delay, q_head_t* que) {
    tswait_t *t;
    t = (tswait_t*) kmalloc(sizeof(tswait_t));
    if (!t) {
        return NULL;
    }
    t->client = client;
    t->delay = delay;
    Q_END(que, t);
    return (t);
}

/*
 *
 */

static q_item_t*
ts_managedelay (q_head_t* que, q_item_t* w) {
    tsmsg_t        msg;
    if (--(((tswait_t*)w)->delay)) {
        return (NULL);
    }
    send(((tswait_t*)w)->client, &msg); /* unlock waiting tasks */
    kfree(Q_REMV(que, w));
    return (NULL);
}

/*
 *
 */

static void
tickd (void* args UNUSED) {
    tsmsg_t msg;
    msg.cmd = TS_NONE;
    pid_t tserver = receive(TASK_ANY, NULL, 0);
    kirqdis();
    TCCR1B |= (1 << CS11);      /* set cca. 30 Hz */

    while (msg.cmd != TS_EXIT) {
        TIMSK1 |= (1 << TOIE1);     /* enable TIMER1OVF interrupt */
        waitevent(EVENT_TIMER1OVF | PREEMPT_ON_EVENT);
        TIFR1 |= (1 << TOV1);     /* clear overflow bit */
        TIMSK1 &= (~(1 << TOIE1));     /* disable TIMER1OVF interrupt */
        msg.cmd = TS_TICK;
        sendrec(tserver, &msg, sizeof(tsmsg_t));
    }
}

/*
 *
 */

static void
step_timer (time_t* time) {
    if (++time->sec != 60) {return;}
    time->sec = 0;
    if (++time->min != 60) {return;}
    time->min = 0;
    if (++time->hour != 24) {return;}
    time->hour = 0;
    return;
}

/*
 *
 */

void
ts (void* args UNUSED) {
    pid_t           client;
    tsmsg_t        msg;
    q_head_t        ts_wait_q;
    time_t          uptime;
    time_t          globtime;
    char            ticks;

        
    memset (&uptime, 0, sizeof(time_t));
    memset (&globtime, 0, sizeof(time_t));
    ticks = 0;
    q_init(&ts_wait_q);


    client = cratetask(TASK_PRIO_RT, PAGE_INVALID);
    allocatestack(client, DEFAULT_STACK_SIZE);
    setuptask(client, tickd, NULL, NULL);
    starttask(client);

    send(client, NULL);

    while (1) {
        client = receive(TASK_ANY, &msg, sizeof(msg));
        switch (msg.cmd) {

          case TS_DELAY:
            if (msg.delay.ticks) {
                addtswait(client, msg.delay.ticks, &ts_wait_q);
                continue;
            }
            break;

          case TS_TICK:
            /* maintain uptime counter */
            if(++ticks == 30) {
                ticks = 0;
                step_timer(&uptime);
                step_timer(&globtime);
            }
            /* maintain waiting tasks */
            q_forall(&ts_wait_q, ts_managedelay);
            break;

          case TS_GET_UPTIME:
            memcpy(&(msg.uptime), &uptime, sizeof(time_t));
            break;

          case TS_SET_GLOBTIME:
            memcpy(&globtime, &(msg.globtime), sizeof(time_t));
            break;

          case TS_GET_GLOBTIME:
            memcpy(&(msg.globtime), &globtime, sizeof(time_t));
            break;

        }
        send(client, &msg);
    }
}

/*
 *
 */

static pid_t            tstask;

/*
 *
 */

pid_t
settspid (pid_t pid) {
    tstask = pid;
    return (tstask); 
}

/*
 * 
 */

void
delay (int ticks) {
    tsmsg_t msg;
    msg.cmd = TS_DELAY;
    msg.delay.ticks = ticks;
    sendrec(tstask, &msg, sizeof(msg));
    return;
}

/*
 * 
 */

void
getuptime (time_t* time) {
    tsmsg_t msg;
    msg.cmd = TS_GET_UPTIME;
    sendrec(tstask, &msg, sizeof(msg));
    memcpy(time, &(msg.uptime), sizeof(time_t));
    return;
}

/*
 * 
 */

void
gettime (time_t* time) {
    tsmsg_t msg;
    msg.cmd = TS_GET_GLOBTIME;
    sendrec(tstask, &msg, sizeof(msg));
    memcpy(time, &(msg.globtime), sizeof(time_t));
    return;
}

/*
 * 
 */

void
settime (time_t* time) {
    tsmsg_t msg;
    msg.cmd = TS_SET_GLOBTIME;
    memcpy(&(msg.uptime), time, sizeof(time_t));
    sendrec(tstask, &msg, sizeof(msg));    
    return;
}

