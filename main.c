#include "kernel.h"

#include "vfs.h"
#include "drv.h"
#include "ts.h"
#include "pm.h"
#include "es.h"
#include "init.h"

#include "apps.h"
#include "sh.h"

/*
 * The main purpose of this task is to start all the servers and set
 * up the environment for the operating system. It exits at the end.
 */

void
startup (void* args UNUSED) {
    pid_t pid;
    char** initc;

    /* starting time server */
    pid = cratetask(TASK_PRIO_RT, PAGE_INVALID);
    allocatestack(pid, DEFAULT_STACK_SIZE);
    setuptask(pid, ts, NULL, NULL);
    starttask(pid);
    settspid(pid);

    /* starting virtual filesystem server */
    pid = cratetask(TASK_PRIO_HIGH, PAGE_INVALID);
    allocatestack(pid, DEFAULT_STACK_SIZE * 2);
    setuptask(pid, vfs, NULL, NULL);
    starttask(pid);
    setvfspid(pid);

    /* setting up devices and special files */
    mkdev(pipedev, NULL);
    mknod(mkdev(usart0, NULL), "ty0");
    mkdev(memfile, NULL);

    /* starting executable store server */
    pid = cratetask(TASK_PRIO_HIGH, PAGE_INVALID);
    allocatestack(pid, DEFAULT_STACK_SIZE);
    setuptask(pid, es, NULL, NULL);
    starttask(pid);
    setespid(pid);

    /* registering user programs */
    es_regprg("getty",      getty,          DEFAULT_STACK_SIZE);
    es_regprg("login",      login,          DEFAULT_STACK_SIZE);
    es_regprg("sh",         sh,             DEFAULT_STACK_SIZE+64);
    es_regprg("echo",       echo,           DEFAULT_STACK_SIZE);
    es_regprg("cat",        cat,            DEFAULT_STACK_SIZE);
    es_regprg("cap",        cap,            DEFAULT_STACK_SIZE);
    es_regprg("sleep",      sleep,          DEFAULT_STACK_SIZE);
    es_regprg("xargs",      xargs,          DEFAULT_STACK_SIZE);
    es_regprg("repeat",     repeat,         DEFAULT_STACK_SIZE);
    es_regprg("uptime",     pr_uptime,      DEFAULT_STACK_SIZE);
    es_regprg("stat",       f_stat,         DEFAULT_STACK_SIZE);
    es_regprg("mknod",      f_mknod,        DEFAULT_STACK_SIZE);
    es_regprg("grep",       grep,           DEFAULT_STACK_SIZE);
    es_regprg("init",       init,           DEFAULT_STACK_SIZE);

    /* starting process manager server */
    initc = (char**)kmalloc(sizeof(char*[2])); /* Freed in PM */
    initc[0] = "init";
    initc[1] = NULL;
    pid = cratetask(TASK_PRIO_HIGH, PAGE_INVALID);
    allocatestack(pid, DEFAULT_STACK_SIZE * 2);
    setuptask(pid, pm, initc, NULL);
    starttask(pid);
    setpmpid(pid);

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
