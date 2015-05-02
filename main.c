#include "kernel.h"

#include "dm.h"
#include "drv.h"
#include "sema.h"
#include "timer.h"
#include "pm.h"
#include "es.h"
#include "init.h"

#include "apps.h"
#include "sh.h"

/*
 * The main purpose of this task is to start and set everything in the
 * right order. When INIT is started at last, everything will be up 
 * and running.
 *
 */

void start (void) {
    pid_t pid;

    /* starting timer server */
    pid = addtask(TASK_PRIO_RT);
    launchtask(pid, timer, DEFAULT_STACK_SIZE);
    settimerpid(pid);

    /* starting semaphore server */
    //pid = addtask(TASK_PRIO_HIGH);
    //launchtask(pid, semasrv, DEFAULT_STACK_SIZE);
    //setsemapid(pid);

    /* starting device manager server */
    pid = addtask(TASK_PRIO_HIGH);
    launchtask(pid, dm, DEFAULT_STACK_SIZE+256);
    setdmpid(pid);

    /* setting up devices */
    mknod(mkdev(devnull), "null");
    mknod(mkdev(usart0), "usart0");
    mknod(mkdev(portdevA), "portA");
    mknod(DEV_FIFO, "npipe");

    /* starting executable store server */
    pid = addtask(TASK_PRIO_HIGH);
    launchtask(pid, es, DEFAULT_STACK_SIZE);
    setespid(pid);

    /* registering user programs */
    es_regprg("getty",      getty,          DEFAULT_STACK_SIZE);
    es_regprg("login",      login,          DEFAULT_STACK_SIZE);
    es_regprg("sh",         sh,             DEFAULT_STACK_SIZE);
    es_regprg("echo",       echo,           DEFAULT_STACK_SIZE);
    es_regprg("wc",         wc,             DEFAULT_STACK_SIZE);
    es_regprg("cat",        cat,            DEFAULT_STACK_SIZE);
    es_regprg("sleep",      sleep,          DEFAULT_STACK_SIZE);
    es_regprg("xargs",      xargs,          DEFAULT_STACK_SIZE);
    es_regprg("at",         at,             DEFAULT_STACK_SIZE);
    es_regprg("uptime",     pr_uptime,      DEFAULT_STACK_SIZE);
    es_regprg("stat",       f_stat,         DEFAULT_STACK_SIZE);
    es_regprg("grep",       grep,           DEFAULT_STACK_SIZE);

    /* starting process manager server */
    pid = addtask(TASK_PRIO_HIGH);
    launchtask(pid, pm, DEFAULT_STACK_SIZE+128);
    setpmpid(pid);

    /* launching init */
    pid = addtask(TASK_PRIO_DFLT);
    pmreg(pid); /* This does FS registration as well */
    launchtask(pid, init, DEFAULT_STACK_SIZE);     

    /* done, exiting */
    return;
}

/*
 *
 */

int main (void) {
    kernel(start, DEFAULT_STACK_SIZE, TASK_PRIO_DFLT);
    return 0;
}

/*
 *
 */
