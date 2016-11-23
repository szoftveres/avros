#ifndef _HAL_H_
#define _HAL_H_


#define LOW(val) ((int)(val) & 0xFF)

#define HIGH(val) (((int)(val) >> 8) & 0xFF)

/*
 *
 */

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

    unsigned char   retHigh;
    unsigned char   retLow;
} cpu_context_t;

/*
 *
 */

/*
 *
 */

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

/*
 *
 */

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

void swtrap (void ) __attribute__ ((naked));


int
kcall5 (int code, int p1, int p2, int p3, int p4, int p5)
__attribute__ ((naked));

int
kcall4 (int code, int p1, int p2, int p3, int p4)
__attribute__ ((naked));

int
kcall3 (int code, int p1, int p2, int p3)
__attribute__ ((naked));

int
kcall2 (int code, int p1, int p2)
__attribute__ ((naked));

int
kcall1 (int code, int p1)
__attribute__ ((naked));

int
kcall0 (int code)
__attribute__ ((naked));




void cpu_sleep (void);

#endif
