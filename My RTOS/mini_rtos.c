
#include <stdint.h>
#include "mini_rtos.h"
#include "qassert.h"

Q_DEFINE_THIS_FILE

OSThread * volatile OS_curr; /* pointer to the current thread */
OSThread * volatile OS_next; /* pointer to the next thread to run */

OSThread *OS_thread[32 + 1]; /* array of threads started so far */

// this register now contains the bit mask for threads in the order of their priority 
// higher priority with larger priority number (LSB)
uint32_t OS_readySet; /* bitmask of threads that are ready to run */

// this register now contains the bit mask for threads that are blocked (delayed) 
uint32_t OS_delayedSet; /* bitmask of threads that are delayed */

#define LOG2(x) (32U - __clz(x))

OSThread idleThread;
void main_idleThread() {
    while (1) {
        OS_onIdle();
    }
}

// we provide here the stack size and ptr for the idle thread
void OS_init(void *stkSto, uint32_t stkSize) {
    /* set the PendSV interrupt priority to the lowest level 0xFF */
		// because we want the Systick handler to schedule the next task to run
		// then after finishing, PendSV handler do the context switching  
		*(uint32_t volatile *)0xE000ED20 |= (0xFFU << 16);

    /* start idleThread thread */
    OSThread_start(&idleThread,
                   0U, /* idle thread priority */
                   &main_idleThread,
                   stkSto, stkSize);
}

void OS_sched(void) {
    /* OS_next = ... */
    if (OS_readySet == 0U) { /* idle condition? */ //all bit are zero, no thread is ready
        OS_next = OS_thread[0]; /* the idle thread */
    }
    else {
			// next thread is the thread with highest prioriy
			// LOG2 macro is an instruction (CLZ) used to get the LSB that is set to 1
        OS_next = OS_thread[LOG2(OS_readySet)];
        Q_ASSERT(OS_next != (OSThread *)0);
    }

    /* trigger PendSV, if needed */
    if (OS_next != OS_curr) {
        *(uint32_t volatile *)0xE000ED04 = (1U << 28);
    }
}

// This fn is used to run the threads for the first time
void OS_run(void) {
    /* callback to configure and start interrupts */
		// It's a fn defined by bsp to put some initilizations
    OS_onStartup();

		// Schedule the first thread to run, then it continues and never return here  
    __disable_irq();
    OS_sched();
    __enable_irq();

    /* the following code should never execute */
    Q_ERROR();
}

// This fn is used to decrement the timeout of all blocked threads,
// and if the timeout == 0, the blocked thread is unblocked and ready to run
void OS_tick(void) {
    uint32_t workingSet = OS_delayedSet;	// define a local variable to manipulate it freely
    while (workingSet != 0U) // loop until the bit mask has some bits set
		{
        OSThread *t = OS_thread[LOG2(workingSet)]; // get the first blocked (delayed) thread
        uint32_t bit;
        Q_ASSERT((t != (OSThread *)0) && (t->timeout != 0U));

        bit = (1U << (t->prio - 1U)); // a variable to be used instead of this long calculation
        --t->timeout;
        if (t->timeout == 0U)  // if the timeout is zero, then the thread is ready (set ready bit)
				{
            OS_readySet   |= bit;  /* insert to set */
            OS_delayedSet &= ~bit; /* remove from set */
        }
        workingSet &= ~bit; /* remove from working set */
    }
}

// This fn blocks the current thread until the delay period (timeout) is finished
void OS_delay(uint32_t ticks) {
    uint32_t bit;
    __disable_irq();

    /* never call OS_delay from the idleThread */
    Q_REQUIRE(OS_curr != OS_thread[0]);

		// put the current time in timeout
    OS_curr->timeout = ticks;
    bit = (1U << (OS_curr->prio - 1U));
	
		// clear the corresponding bit, the thread is now blocked (won't be scheduled)
    OS_readySet &= ~bit;
    OS_delayedSet |= bit;
	
		// shcedule away from is function and change the current thread				
		OS_sched();
    __enable_irq();
}

void OSThread_start(
    OSThread *me,
    uint8_t prio, /* thread priority */
    OSThreadHandler threadHandler,
    void *stkSto, uint32_t stkSize)
{
    /* round down the stack top to the 8-byte boundary
    * NOTE: ARM Cortex-M stack grows down from hi -> low memory
    */
    uint32_t *sp = (uint32_t *)((((uint32_t)stkSto + stkSize) / 8) * 8);
    uint32_t *stk_limit;

    /* priority must be in range specified
    * and the priority level must not be used previously
    */
    Q_REQUIRE((prio < Q_DIM(OS_thread))
              && (OS_thread[prio] == (OSThread *)0));

    *(--sp) = (1U << 24);  /* xPSR (thumb bit set, for historic reasons) */
    *(--sp) = (uint32_t)threadHandler; /* PC is set to the fn address */
    *(--sp) = 0x0000000EU; /* LR  */
    *(--sp) = 0x0000000CU; /* R12 */
    *(--sp) = 0x00000003U; /* R3  */
    *(--sp) = 0x00000002U; /* R2  */
    *(--sp) = 0x00000001U; /* R1  */
    *(--sp) = 0x00000000U; /* R0  */
    /* additionally, fake registers R4-R11 */
    *(--sp) = 0x0000000BU; /* R11 */
    *(--sp) = 0x0000000AU; /* R10 */
    *(--sp) = 0x00000009U; /* R9 */
    *(--sp) = 0x00000008U; /* R8 */
    *(--sp) = 0x00000007U; /* R7 */
    *(--sp) = 0x00000006U; /* R6 */
    *(--sp) = 0x00000005U; /* R5 */
    *(--sp) = 0x00000004U; /* R4 */

    /* save the top of the stack in the thread's attibute */
    me->sp = sp;

    /* round up the bottom of the stack to the 8-byte boundary */
    stk_limit = (uint32_t *)(((((uint32_t)stkSto - 1U) / 8) + 1U) * 8);

    /* pre-fill the unused part of the stack with 0xDEADBEEF */
    for (sp = sp - 1U; sp >= stk_limit; --sp) {
        *sp = 0xDEADBEEFU;
    }

    /* register the thread with the OS */
    OS_thread[prio] = me;
    me->prio = prio;
    /* make the thread ready to run */
    if (prio > 0U) {
        OS_readySet |= (1U << (prio - 1U));
    }
}


// This fn is used to perform the context switching after the task is interrupted (scheduled)
__asm
void PendSV_Handler(void) {
    IMPORT  OS_curr  /* extern variable */
    IMPORT  OS_next  /* extern variable */

    /* __disable_irq(); */
    CPSID         I

    /* if (OS_curr != (OSThread *)0) { */
		// in the first time running after reset, OS_curr is initially zero)
    LDR           r1,=OS_curr
    LDR           r1,[r1,#0x00]
    CBZ           r1,PendSV_restore

    /*     push registers r4-r11 on the stack */
    PUSH          {r4-r11}

    /*     OS_curr->sp = sp; */
		// store the actual sp register into the sp of the tcb

    LDR           r1,=OS_curr
    LDR           r1,[r1,#0x00]
    STR           sp,[r1,#0x00]
    /* } */

PendSV_restore
    /* sp = OS_next->sp; */
		// load the sp from the next thhread
    LDR           r1,=OS_next
    LDR           r1,[r1,#0x00]
    LDR           sp,[r1,#0x00]

    /* OS_curr = OS_next; */
    LDR           r1,=OS_next
    LDR           r1,[r1,#0x00]
    LDR           r2,=OS_curr
    STR           r1,[r2,#0x00]

    /* pop registers r4-r11 */
    POP           {r4-r11}

    /* __enable_irq(); */
    CPSIE         I

    /* return to the next thread */
    BX            lr
}
