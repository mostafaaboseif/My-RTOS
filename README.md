# My-RTOS
Implemented my own RTOS from scratch on Tiva-C. It has a preemptive priority-based scheduler, so all high-priority threads meet their hard-time requirements (simulation); it supports thread blocking using Os_delay() and context switching using assembly instructions (every thread has a TCB and its own stack).

