#include <avr/io.h>
#include <avr/interrupt.h>
#include "../../kernel/include/kernel.h"
#include "../include/hal.h"


extern int eventcode;


#if 0

/**
 * CPU context Save
 */ 
void save_context (void) {
    asm volatile (
        "push r24                       \n\t"  /* Save r24, cycle counter */
        "in  r24, __SREG__              \n\t"  /* Save SREG */
        "push r24                       \n\t"
        "push r25                       \n\t"  /* Save r25, temporary storage register */
        "push r26                       \n\t"  /* Save r26, return address */
        "push r27                       \n\t"  /* Save r27, return address */
        "push r28                       \n\t"  /* Save Y pair, Destination address */
        "push r29                       \n\t"
        "push r30                       \n\t"  /* Save Z pair, Source address */
        "push r31                       \n\t"  
        "in r28, 0x3d                   \n\t"  /* Store SP in Y */
        "in r29, 0x3e                   \n\t"  
        "in r30, 0x3d                   \n\t"  /* Store SP in Z */
        "in r31, 0x3e                   \n\t"     
        "ldd r26, Y+11                  \n\t"  /* Save return address from stack */
        "ldd r27, Y+10                  \n\t"
        "ldi r24,9                      \n\t"  /* Load 9 in cycle counter */
        "adiw r28,9                     \n\t"  /* Set source      (Y = SP + 9) */
        "adiw r30,11                      \n"  /* Set destination (Z = SP + 11) */
        "save_context_loop_head:        \n\t"  /* Cycle Head */
        "ld  r25, Y                     \n\t"  /* Move up from (Y) to (Z) */
        "st  Z, r25                     \n\t"
        "sbiw r28,1                     \n\t"  /* Decrement destination address (Y) */
        "sbiw r30,1                     \n\t"  /* Decrement source address (Z) */
        "dec r24                        \n\t"  /* Decrement cycle counter */
        "brne save_context_loop_head    \n\t"  /* Continue while cycle counter is not zero */
        "pop r25                        \n\t"  /* Increase SP by 2 */
        "pop r25                        \n\t"
        "push r0                        \n\t"  /* Push the whole context */
        "push r1                        \n\t"
        "push r2                        \n\t"
        "push r3                        \n\t"
        "push r4                        \n\t"
        "push r5                        \n\t"
        "push r6                        \n\t"
        "push r7                        \n\t"
        "push r8                        \n\t"
        "push r9                        \n\t"
        "push r10                       \n\t"
        "push r11                       \n\t"
        "push r12                       \n\t"
        "push r13                       \n\t"
        "push r14                       \n\t"
        "push r15                       \n\t"
        "push r16                       \n\t"
        "push r17                       \n\t"
        "push r18                       \n\t"
        "push r19                       \n\t"
        "push r20                       \n\t"
        "push r21                       \n\t"
        "push r22                       \n\t"
        "push r23                       \n\t"  
        "push r26                       \n\t"  /* Push return address on stack */
        "push r27                       \n\t"
        "ret                            \n\t"  /* Return */
    );
}


/**
 * CPU context Restore
 */
void restore_context (void) {
    asm volatile ( 
        "pop r27                        \n\t"  /* Save return address */
        "pop r26                        \n\t"
        "pop r23                        \n\t"  /* Pop the whole context */
        "pop r22                        \n\t"
        "pop r21                        \n\t"
        "pop r20                        \n\t"
        "pop r19                        \n\t"
        "pop r18                        \n\t"
        "pop r17                        \n\t"
        "pop r16                        \n\t"
        "pop r15                        \n\t"
        "pop r14                        \n\t"
        "pop r13                        \n\t"
        "pop r12                        \n\t"
        "pop r11                        \n\t"
        "pop r10                        \n\t"
        "pop r9                         \n\t"
        "pop r8                         \n\t"
        "pop r7                         \n\t"
        "pop r6                         \n\t"
        "pop r5                         \n\t"
        "pop r4                         \n\t"
        "pop r3                         \n\t"
        "pop r2                         \n\t"
        "pop r1                         \n\t"
        "pop r0                         \n\t"
        "push r25                       \n\t"  /* Decrease SP by 2 */
        "push r25                       \n\t"
        "in r28, 0x3d                   \n\t"  /* Save SP in Y */
        "in r29, 0x3e                   \n\t"
        "in r30, 0x3d                   \n\t"  /* Save SP in Z */
        "in r31, 0x3e                   \n\t"
        "ldi r24,9                      \n\t"  /* Set cycle counter to 9 */
        "adiw r28,3                     \n\t"  /* Set source address      (Y = SP + 3) */
        "adiw r30,1                       \n"  /* Set destination address (Z = SP + 1) */
        "restore_context_loop_head:     \n\t"  /* Cycle Head */
        "ld  r25, Y                     \n\t"  /* Move down from (Y) to (Z) */
        "st  Z, r25                     \n\t"
        "adiw r28,1                     \n\t"  /* Increment source address (Y) */
        "adiw r30,1                     \n\t"  /* Increment destination address (Z) */
        "dec r24                        \n\t"  /* Decrement cycle counter */
        "brne restore_context_loop_head \n\t"  /* Continue while cycle counter is not zero */
        "std Z+1, r26                   \n\t"  /* Store return address on stack */
        "st Z, r27                      \n\t"
        "pop r31                        \n\t"
        "pop r30                        \n\t"
        "pop r29                        \n\t"
        "pop r28                        \n\t"
        "pop r27                        \n\t"
        "pop r26                        \n\t"
        "pop r25                        \n\t"  
        "pop r24                        \n\t"
        "out __SREG__,r24               \n\t"
        "pop r24                        \n\t"
        "ret                            \n\t"
    );
}

#endif



#define SET_EVENTCODE_AND_JMP_TO_HANDLER(x)     \
    do {                                        \
        asm("\n\tpush   r24"                    \
            "\n\tpush   r25"::);                \
        eventcode = (x);                        \
        asm("\n\tpop    r25"                    \
            "\n\tpop    r24"                    \
            "\n\tjmp switchtokernel\n\t"::);    \
    } while(0)

#define JMP_TO_HANDLER()                        \
    do {                                        \
        asm("\n\tjmp switchtokernel\n\t"::);    \
    } while(0)

/**
 * software trap
 */
void swtrap (void) {
    LOCK();
    JMP_TO_HANDLER();
}

/**
 * timer1 interrupt handler
 */
ISR (TIMER1_OVF_vect) __attribute__ ((signal, naked));
ISR (TIMER1_OVF_vect) { /* GIE cleared automatically */
    SET_EVENTCODE_AND_JMP_TO_HANDLER(EVENT_TIMER1OVF);
}

/**
 * USART0 RX COMPLETE interrupt handler
 */
ISR (USART0_RX_vect) __attribute__ ((signal, naked));
ISR (USART0_RX_vect) { /* GIE cleared automatically */
    SET_EVENTCODE_AND_JMP_TO_HANDLER(EVENT_USART0RX);
}

/**
 * USART0 TX COMPLETE interrupt handler
 */
ISR (USART0_TX_vect) __attribute__ ((signal, naked));
ISR (USART0_TX_vect) { /* GIE cleared automatically */
    SET_EVENTCODE_AND_JMP_TO_HANDLER(EVENT_USART0TX);
}

/**
 * USART1 RX COMPLETE interrupt handler
 */
ISR (USART1_RX_vect) __attribute__ ((signal, naked));
ISR (USART1_RX_vect) { /* GIE cleared automatically */
    SET_EVENTCODE_AND_JMP_TO_HANDLER(EVENT_USART1RX);
}

/**
 * USART1 TX COMPLETE interrupt handler
 */
ISR (USART1_TX_vect) __attribute__ ((signal, naked));
ISR (USART1_TX_vect) { /* GIE cleared automatically */
    SET_EVENTCODE_AND_JMP_TO_HANDLER(EVENT_USART1TX);
}





int
kcall5 (int code UNUSED, int p1 UNUSED, int p2 UNUSED,
        int p3 UNUSED, int p4 UNUSED, int p5 UNUSED) {
    swtrap();
    RETURN();
}

int
kcall4 (int code UNUSED, int p1 UNUSED, int p2 UNUSED,
        int p3 UNUSED, int p4 UNUSED) {
    swtrap();
    RETURN();
}

int
kcall3 (int code UNUSED, int p1 UNUSED, int p2 UNUSED,
        int p3 UNUSED) {
    swtrap();
    RETURN();
}

int
kcall2 (int code UNUSED, int p1 UNUSED, int p2 UNUSED) {
    swtrap();
    RETURN();
}

int
kcall1 (int code UNUSED, int p1 UNUSED) {
    swtrap();
    RETURN();
}

int
kcall0 (int code UNUSED) {
    swtrap();
    RETURN();
}



/*
 *
 */

void
cpu_sleep (void) {
    /* IDLE mode */
    SMCR &= ~((1<<SM0) | (1<<SM1) | (1<<SM2));
    SMCR |= (1<<SE);    // Enable sleep
    asm volatile ("sleep      \n\t");
}

