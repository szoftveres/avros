#include "kernel.h"

#include "vfs.h"
#include "drv.h"
#include "sema.h"
#include "timer.h"
#include "pm.h"
#include "es.h"
#include "init.h"

#include "apps.h"
#include "sh.h"

/*
 * The main purpose of this task is to start all the servers and set
 * up the environment for the operating system.
 * It starts 'init' at the end, then exits.
 */

void
startup (void* args UNUSED) {
    pid_t pid;

    /* starting timer server */
    pid = cratetask(TASK_PRIO_RT);
    launchtask(pid, timer, NULL, NULL, DEFAULT_STACK_SIZE);
    settimerpid(pid);

    /* starting semaphore server */
    //pid = cratetask(TASK_PRIO_HIGH);
    //launchtask(pid, semasrv, DEFAULT_STACK_SIZE);
    //setsemapid(pid);

    /* starting device manager server */
    pid = cratetask(TASK_PRIO_HIGH);
    launchtask(pid, vfs, NULL, NULL, DEFAULT_STACK_SIZE * 2);
    setvfspid(pid);

    /* setting up devices/files */
    
    mkdev(pipedev, NULL);
    mknod(mkdev(usart0, NULL), "usart0", S_IFCHR);
    mkdev(memfile, NULL);

    /* starting executable store server */
    pid = cratetask(TASK_PRIO_HIGH);
    launchtask(pid, es, NULL, NULL, DEFAULT_STACK_SIZE);
    setespid(pid);

    /* registering user programs */
    es_regprg("getty",      getty,          DEFAULT_STACK_SIZE);
    es_regprg("login",      login,          DEFAULT_STACK_SIZE);
    es_regprg("sh",         sh,             DEFAULT_STACK_SIZE+64);
    es_regprg("echo",       echo,           DEFAULT_STACK_SIZE);
    es_regprg("cat",        cat,            DEFAULT_STACK_SIZE);
    es_regprg("sleep",      sleep,          DEFAULT_STACK_SIZE);
    es_regprg("xargs",      xargs,          DEFAULT_STACK_SIZE);
    es_regprg("at",         at,             DEFAULT_STACK_SIZE);
    es_regprg("repeat",     repeat,         DEFAULT_STACK_SIZE);
    es_regprg("uptime",     pr_uptime,      DEFAULT_STACK_SIZE);
    es_regprg("stat",       f_stat,         DEFAULT_STACK_SIZE);
    es_regprg("mknod",      f_mknod,        DEFAULT_STACK_SIZE);
    es_regprg("grep",       grep,           DEFAULT_STACK_SIZE);

    /* starting process manager server */
    pid = cratetask(TASK_PRIO_HIGH);
    launchtask(pid, pm, NULL, NULL, DEFAULT_STACK_SIZE * 2);
    setpmpid(pid);

    /* launching init */
    pid = cratetask(TASK_PRIO_DFLT);
    pmreg(pid); /* This does FS registration as well */
    launchtask(pid, init, NULL, NULL, DEFAULT_STACK_SIZE);     

    /* done, exiting */
    return;
}

/*
 * Main entry point of the kernel. Parameter is the first task
 */

int main (void) {
    kernel(startup, NULL, DEFAULT_STACK_SIZE, TASK_PRIO_DFLT);
    return 0;
}

/*
 *
 */
