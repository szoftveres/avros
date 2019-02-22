#include "kernel/kernel.h"

#include "servers/vfs.h"
#include "servers/ts.h"
#include "servers/pm.h"
#include "servers/ex.h"

#include "drivers/drv.h"

#include "usr/init.h"
#include "usr/apps.h"
#include "usr/sh.h"

/*
 * The main purpose of this task is to start all the servers and set
 * up the environment for the operating system. It exits at the end.
 */

void
startup (void* args UNUSED) {
    pid_t pid;
    char** initc;

    /* starting time server */
    pid = createtask(TASK_PRIO_RT, PAGE_INVALID);
    allocatestack(pid, DEFAULT_STACK_SIZE);
    setuptask(pid, ts, NULL, NULL);
    starttask(pid);
    settspid(pid);

    /* starting virtual filesystem server */
    pid = createtask(TASK_PRIO_HIGH, PAGE_INVALID);
    allocatestack(pid, DEFAULT_STACK_SIZE * 2);
    setuptask(pid, vfs, NULL, NULL);
    starttask(pid);
    setvfspid(pid);

    /* setting up devices and special files */
    /* pipe: 0 */
    mkdev(memfile, NULL); /* 1 */
    mkdev(tty_usart0, NULL); /* 2 */

    /* starting executable store server */
    pid = createtask(TASK_PRIO_HIGH, PAGE_INVALID);
    allocatestack(pid, DEFAULT_STACK_SIZE);
    setuptask(pid, ex, NULL, NULL);
    starttask(pid);
    setexpid(pid);

    /* registering user programs */
    ex_regprg("getty",      getty,          DEFAULT_STACK_SIZE);
    ex_regprg("login",      login,          DEFAULT_STACK_SIZE);
    ex_regprg("sh",         sh,             DEFAULT_STACK_SIZE+64);
    ex_regprg("echo",       echo,           DEFAULT_STACK_SIZE);
    ex_regprg("cat",        cat,            DEFAULT_STACK_SIZE);
    ex_regprg("cap",        cap,            DEFAULT_STACK_SIZE);
    ex_regprg("sleep",      sleep,          DEFAULT_STACK_SIZE);
    ex_regprg("xargs",      xargs,          DEFAULT_STACK_SIZE);
    ex_regprg("repeat",     repeat,         DEFAULT_STACK_SIZE);
    ex_regprg("uptime",     pr_uptime,      DEFAULT_STACK_SIZE);
    ex_regprg("stat",       f_stat,         DEFAULT_STACK_SIZE);
    ex_regprg("mknod",      f_mknod,        DEFAULT_STACK_SIZE);
    ex_regprg("grep",       grep,           DEFAULT_STACK_SIZE);
    ex_regprg("fs_debug",   fs_debug,       DEFAULT_STACK_SIZE);
    ex_regprg("init",       init,           DEFAULT_STACK_SIZE);

    /* starting process manager server */
    initc = (char**)kmalloc(sizeof(char*[2])); /* Freed in PM */
    initc[0] = "init";
    initc[1] = NULL;
    pid = createtask(TASK_PRIO_HIGH, PAGE_INVALID);
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
