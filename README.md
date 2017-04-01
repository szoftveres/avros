# avros

UNIX-like embedded microkernel OS for Atmel AVR CPUs

Runs on ATmega1284p with 16k RAM

Features:
- Minimal microkernel with message passing facilities, the OS entity is defined by the servers that communicate with each other via the kernel
- Multiple OS entities can coexist on the same CPU without interferring with each other
- Cooperative and/or timer interrupt based preemptive multitasking
- Virtual file system with UNIX-pipes - (filenames and folders are not implemented yet - you have to refer to each file with their respective device/inode number duets, e.g. syntax: '2/1')
- Device drivers are running as separate tasks (threads), including pipe device
- Unified device driver interface
- shell access via USARTs, multiple sessions can be spawned upon init.
- The currently implemented OS entity is UNIX-like, with familiar system calls 


Repository
==========

* main.c: first task and main function - 
    The OS executes this task first. It creates the servers in
    order (ts, vfs, es, pm, see below for meaning), sets up devices,
    registers the executables, then finally spawns 'init' which
    is the root task for every user task.

* kernel: microkernel and HAL (hardware abstraction layer) source code 
    * Basic functionalities: task creation and scheduling (priority round
      robin), message passing, interrupt handling, memory allocation
      (memory manager server is under development, see misc/)
    * idletask - runs when there's nothing else to run - halts the CPU until
      the next interrupt to save power
    

* usr
    * src/apps.c:
        * getty
        * login
        * echo
        * cat
        * cap (turns lower case letters to capitals, only for testing)
        * sleep
        * xargs
        * repeat (only for testing)
        * uptime
        * stat
        * grep (only string matching, no wildcards)
        * mknod

    * src/sh.c: shell
        * stdin / stdout redirection to/from file with '<' and '>' respectively
        * pipe with '|'
        * Background jobs with '&'
        * Multiple jobs with ';'
        * comment with '#'
        * ignore next char with '\\' prefix

    * src/init.c: init task, respawns sessions

* servers:
    * pm: process manager -  
        process hierarchy, zombie processes, exit(), wait(), exec(), spawntask()
    * vfs: virtual file server - 
        file descriptors, filp table, inodes, open(), close(), read(), write(),
        dup(), pipe(), mkdev(), mknod(), fstat(), etc...
    * sema: simple semaphore server (currently unused) - p(), v()
    * ts: time server - timer interrupt handler, 
        sleep(), uptime, real time
    * es: executables server - 
        registers runnable applications and provides them to pm when a
        task calls exec() (AVR is a Harvard architecture CPU, the OS cannot
        load the binary and burn it into the flash program memory)

* drivers:
    * tty_usart0: interrupt-driven tty driver for USART 0 device
    * memfile: memory drive device with inode management
    * pipedev: pipe device (multi-read, multi-write)

* doc: documentation (view with Dia: https://wiki.gnome.org/Apps/Dia/)

* lib:
    src/queue.c: doubly linked list


    
