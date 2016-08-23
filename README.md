# avros

Microkernel multitasking UNIX-like OS for Atmel AVR CPU

Runs on ATmega1284p with 16k of RAM

Features:
- Minimal kernel, the OS is implemented with servers that
  communicate with each other using message passing
- Virtual file system with inodes and UNIX-pipes - 
  (filenames and folders are not implemented yet - you have to refer to each file with their device/inode duets)
- Device drivers are also running as separate tasks (threads)
- shell access via USART


Repository
==========

* kernel: microkernel source code

* usr
    * src/apps.c:
        * getty
        * login
        * echo
        * cat
        * cap (turns lower case letters to capitals, for testing)
        * sleep
        * xargs
        * repeat (for testing)
        * uptime
        * stat
        * grep (only string matching, no wildcards)
        * mknod

    * src/sh.c: shell
    * src/init.c: init task, respawns sessions

* servers:
    * pm: process manager -  
        process hierarchy, zombie processes, exit codes, wait
    * vfs: virtual file server - 
        file descriptors, filp table, inodes, open, close,
        dup, mknod, stat, etc...
    * sema: simple semaphore server (currently unused)
    * ts: time server - 
        sleep, uptime, real time
    * es: executables server - 
        registered runnable applications
        (because AVR is a Harvard architecture CPU)

* drivers:
    * tty_usart0: interrupt-driven tty driver for USART 0 device
    * memfile: memory drive device with inode management
    * pipedev: pipe device (multi-read, multi-write)

* doc: documentation (view in Dia)

* lib:
    src/queue.c: doubly linked list


    
