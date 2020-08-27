
#ifndef MINI_RTOS_H
#define MINI_RTOS_H

#include <stdint.h>

/* Thread Control Block (TCB) */
typedef struct {
    void *sp; /* stack pointer */
   /* timeout delay down-counter */	
  	// its value is put by OS_delay() and it's decremented by OS_tick() each cycle  
		uint32_t timeout; 
    uint8_t prio; /* thread priority */
} OSThread;

typedef void (*OSThreadHandler)();

void OS_init(void *stkSto, uint32_t stkSize);

/* callback to handle the idle condition */
void OS_onIdle(void);

/* this function must be called with interrupts DISABLED */
void OS_sched(void);

/* transfer control to the RTOS to run the threads */
void OS_run(void);

/* blocking delay */
void OS_delay(uint32_t ticks);

/* process all timeouts */
void OS_tick(void);

/* callback to configure and start interrupts */
void OS_onStartup(void);

void OSThread_start(
    OSThread *me,
    uint8_t prio, /* thread priority */
    OSThreadHandler threadHandler,
    void *stkSto, uint32_t stkSize);

#endif /* MINI_RTOS_H */

