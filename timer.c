#include "timer.h"
#include "kernel.h"
#include "queue.h"
#include <avr/io.h> /* timer */

/*
 *
 */

typedef struct tmrwait_s {
    QUEUE_HEADER
    pid_t           client;
    int             delay;
} tmrwait_t;

/*
 *
 */

static q_head_t     tmr_wait_q;

time_t              uptime;
time_t              globtime;

char                ticks;

/*
 * TIMER COMMANDS
 */

enum {
    TMR_NONE,
    TMR_DELAY,
    TMR_TICK,
    TMR_GET_UPTIME,
    TMR_GET_GLOBTIME,
    TMR_SET_GLOBTIME,
    TMR_EXIT,
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

typedef struct tmrmsg_s {
    int             cmd;
    union {
        delay_t         delay;          /* delay */
        time_t          uptime;         /* uptime */
        time_t          globtime;       /* global time */
    };
} tmrmsg_t;


/*
 * Returns the created timer, or NULL on error
 */

static tmrwait_t*
addtmrwait (pid_t client, int delay) {
    tmrwait_t *t;
    t = (tmrwait_t*) kmalloc(sizeof(tmrwait_t));
    if (!t) {
        return NULL;
    }
    t->client = client;
    t->delay = delay;
    Q_END(&tmr_wait_q, t);
    return (t);
}

/*
 *
 */

static q_item_t*
tmr_managedelay (q_head_t* que, q_item_t* w) {
    tmrmsg_t        msg;
    if (--(((tmrwait_t*)w)->delay)) {
        return (NULL);
    }
    send(((tmrwait_t*)w)->client, &msg); /* unlock waiting tasks */
    kfree(Q_REMV(que, w));
    return (NULL);
}

/*
 *
 */

static void
timerworker (void* args UNUSED) {
    tmrmsg_t msg;
    msg.cmd = TMR_NONE;
    pid_t tm = receive(TASK_ANY, NULL, 0);
    kirqdis();
    TCCR1B |= (1 << CS11);      /* set cca. 30 Hz */

    while (msg.cmd != TMR_EXIT) {
        TIMSK1 |= (1 << TOIE1);     /* enable TIMER1OVF interrupt */
        waitevent(EVENT_TIMER1OVF | PREEMPT_ON_EVENT);
        TIFR1 |= (1 << TOV1);     /* clear overflow bit */
        TIMSK1 &= (~(1 << TOIE1));     /* disable TIMER1OVF interrupt */
        msg.cmd = TMR_TICK;
        sendrec(tm, &msg, sizeof(tmrmsg_t));
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

static void
timer_init (void) {
    memset (&uptime, 0, sizeof(time_t));
    memset (&globtime, 0, sizeof(time_t));
    ticks = 0;
    q_init(&tmr_wait_q);
}

/*
 *
 */

void
timer (void* args UNUSED) {
    pid_t           client;
    tmrmsg_t        msg;
        
    timer_init();

    client = cratetask(TASK_PRIO_RT, PAGE_INVALID);
    allocatestack(client, DEFAULT_STACK_SIZE);
    setuptask(client, timerworker, NULL, NULL);
    starttask(client);

    send(client, NULL);

    while (1) {
        client = receive(TASK_ANY, &msg, sizeof(msg));
        switch (msg.cmd) {

          case TMR_DELAY:
            if (msg.delay.ticks) {
                addtmrwait(client, msg.delay.ticks);
                continue;
            }
            break;

          case TMR_TICK:
            /* maintain uptime counter */
            if(++ticks == 30) {
                ticks = 0;
                step_timer(&uptime);
                step_timer(&globtime);
            }
            /* maintain waiting tasks */
            q_forall(&tmr_wait_q, tmr_managedelay);
            break;

          case TMR_GET_UPTIME:
            memcpy(&(msg.uptime), &uptime, sizeof(time_t));
            break;

          case TMR_SET_GLOBTIME:
            memcpy(&globtime, &(msg.globtime), sizeof(time_t));
            break;

          case TMR_GET_GLOBTIME:
            memcpy(&(msg.globtime), &globtime, sizeof(time_t));
            break;

        }
        send(client, &msg);
    }
}

/*
 *
 */

static pid_t            timertask;

/*
 *
 */

pid_t
settimerpid (pid_t pid) {
    timertask = pid;
    return (timertask); 
}

/*
 * 
 */

void
delay (int ticks) {
    tmrmsg_t msg;
    msg.cmd = TMR_DELAY;
    msg.delay.ticks = ticks;
    sendrec(timertask, &msg, sizeof(msg));
    return;
}

/*
 * 
 */

void
getuptime (time_t* time) {
    tmrmsg_t msg;
    msg.cmd = TMR_GET_UPTIME;
    sendrec(timertask, &msg, sizeof(msg));
    memcpy(time, &(msg.uptime), sizeof(time_t));
    return;
}

/*
 * 
 */

void
gettime (time_t* time) {
    tmrmsg_t msg;
    msg.cmd = TMR_GET_GLOBTIME;
    sendrec(timertask, &msg, sizeof(msg));
    memcpy(time, &(msg.globtime), sizeof(time_t));
    return;
}

/*
 * 
 */

void
settime (time_t* time) {
    tmrmsg_t msg;
    msg.cmd = TMR_SET_GLOBTIME;
    memcpy(&(msg.uptime), time, sizeof(time_t));
    sendrec(timertask, &msg, sizeof(msg));    
    return;
}

