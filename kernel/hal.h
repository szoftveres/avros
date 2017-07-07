#ifndef _HAL_H_
#define _HAL_H_

#include "../lib/commondef.h"



void swtrap (void ) __attribute__ ((naked));

void cpu_sleep (void);


typedef struct cpu_context_s {

    unsigned char   r0;
    unsigned char   r1;
    unsigned char   r2;
    unsigned char   r3;
    unsigned char   r4;
    unsigned char   r5;
    unsigned char   r6;
    unsigned char   r7;
    unsigned char   r8;
    unsigned char   r9;
    unsigned char   r10;
    unsigned char   r11;
    unsigned char   r12;
    unsigned char   r13;
    unsigned char   r14;
    unsigned char   r15;
    unsigned char   r16;
    unsigned char   r17;
    unsigned char   r18;
    unsigned char   r19;
    unsigned char   r20;
    unsigned char   r21;
    unsigned char   r22;
    unsigned char   r23;
    unsigned char   r24;
    unsigned char   r25;
    unsigned char   r26;
    unsigned char   r27;
    unsigned char   r28;
    unsigned char   r29;
    unsigned char   r30;
    unsigned char   rSREG;
    unsigned char   r31;

//    unsigned char   call_code;
//    unsigned char   evnt_code;

    unsigned char   retHigh;
    unsigned char   retLow;
} cpu_context_t;



#define SAVE_CONTEXT()                          \
    asm volatile (  "\n\t                   "   \
                    "push r31           \n\t"   \
                    "in r31, __SREG__   \n\t"   \
                    "push r31           \n\t"   \
                    "push r30           \n\t"   \
                    "push r29           \n\t"   \
                    "push r28           \n\t"   \
                    "push r27           \n\t"   \
                    "push r26           \n\t"   \
                    "push r25           \n\t"   \
                    "push r24           \n\t"   \
                    "push r23           \n\t"   \
                    "push r22           \n\t"   \
                    "push r21           \n\t"   \
                    "push r20           \n\t"   \
                    "push r19           \n\t"   \
                    "push r18           \n\t"   \
                    "push r17           \n\t"   \
                    "push r16           \n\t"   \
                    "push r15           \n\t"   \
                    "push r14           \n\t"   \
                    "push r13           \n\t"   \
                    "push r12           \n\t"   \
                    "push r11           \n\t"   \
                    "push r10           \n\t"   \
                    "push r9            \n\t"   \
                    "push r8            \n\t"   \
                    "push r7            \n\t"   \
                    "push r6            \n\t"   \
                    "push r5            \n\t"   \
                    "push r4            \n\t"   \
                    "push r3            \n\t"   \
                    "push r2            \n\t"   \
                    "push r1            \n\t"   \
                    "push r0            \n\t"   \
    );


#define RESTORE_CONTEXT()                       \
    asm volatile (  "\n\t                   "   \
                    "pop r0             \n\t"   \
                    "pop r1             \n\t"   \
                    "pop r2             \n\t"   \
                    "pop r3             \n\t"   \
                    "pop r4             \n\t"   \
                    "pop r5             \n\t"   \
                    "pop r6             \n\t"   \
                    "pop r7             \n\t"   \
                    "pop r8             \n\t"   \
                    "pop r9             \n\t"   \
                    "pop r10            \n\t"   \
                    "pop r11            \n\t"   \
                    "pop r12            \n\t"   \
                    "pop r13            \n\t"   \
                    "pop r14            \n\t"   \
                    "pop r15            \n\t"   \
                    "pop r16            \n\t"   \
                    "pop r17            \n\t"   \
                    "pop r18            \n\t"   \
                    "pop r19            \n\t"   \
                    "pop r20            \n\t"   \
                    "pop r21            \n\t"   \
                    "pop r22            \n\t"   \
                    "pop r23            \n\t"   \
                    "pop r24            \n\t"   \
                    "pop r25            \n\t"   \
                    "pop r26            \n\t"   \
                    "pop r27            \n\t"   \
                    "pop r28            \n\t"   \
                    "pop r29            \n\t"   \
                    "pop r30            \n\t"   \
                    "pop r31            \n\t"   \
                    "out __SREG__, r31  \n\t"   \
                    "pop r31            \n\t"   \
    );

/*
 *
 */

#define LOCK()   asm("cli\n\t"::)

#define UNLOCK()  asm("sei\n\t"::)

#define RETURN() asm("ret\n\t"::)

#define RETI() asm("reti\n\t"::)

/*
 *
 */


#define GET_SP() ((void*)(((SPH<<8) & 0xFF00) | (SPL & 0xFF)))

#define SET_SP(x) do {SPL = LOW(x);SPH = HIGH(x);} while(0);


/*
 *
 */

#define GETP0(ctxt)                                                     \
    ((((unsigned int)(ctxt)->r25) << 8) | (unsigned int)(ctxt)->r24)    \

#define SETP0(ctxt, v)                                                  \
    do {                                                                \
        unsigned int v_cp = (unsigned int) (v);                         \
        (ctxt)->r25 = HIGH(v_cp);                                       \
        (ctxt)->r24 = LOW(v_cp);                                        \
    } while (0)                                                         \


#define GETP1(ctxt)                                                     \
    ((((unsigned int)(ctxt)->r23) << 8) | (unsigned int)(ctxt)->r22)    \

#define SETP1(ctxt, v)                                                  \
    do {                                                                \
        unsigned int v_cp = (unsigned int) (v);                         \
        (ctxt)->r23 = HIGH(v_cp);                                       \
        (ctxt)->r22 = LOW(v_cp);                                        \
    } while (0)                                                         \


#define GETP2(ctxt)                                                     \
    ((((unsigned int)(ctxt)->r21) << 8) | (unsigned int)(ctxt)->r20)    \

#define SETP2(ctxt, v)                                                  \
    do {                                                                \
        unsigned int v_cp = (unsigned int) (v);                         \
        (ctxt)->r21 = HIGH(v_cp);                                       \
        (ctxt)->r20 = LOW(v_cp);                                        \
    } while (0)                                                         \


#define GETP3(ctxt)                                                     \
    ((((unsigned int)(ctxt)->r19) << 8) | (unsigned int)(ctxt)->r18)    \

#define SETP3(ctxt, v)                                                  \
    do {                                                                \
        unsigned int v_cp = (unsigned int) (v);                         \
        (ctxt)->r19 = HIGH(v_cp);                                       \
        (ctxt)->r18 = LOW(v_cp);                                        \
    } while (0)                                                         \



#define GET_CTXT(task) (cpu_context_t*)(((task)->sp) + 1)

#define KERNEL_CALL(c)                                                  \
    do {                                                                \
        asm volatile("push  r16\n\t"::);                                \
        asm volatile("ldi  r16, " STRINGIFY(c) "\n\t"::);               \
        swtrap();                                                       \
        asm volatile("pop  r16\n\t"::);                                 \
    } while (0)

#define SET_KCALLCODE(ctxt, c)     (ctxt)->r16 = (c)
#define GET_KCALLCODE(ctxt)     ((ctxt)->r16)


#endif
