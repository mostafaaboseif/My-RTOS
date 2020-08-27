# My-RTOS
Implemented my own RTOS from scratch on Tiva-C. It has a preemptive priority-based scheduler, so all high-priority threads meet their hard-time requirements (simulation); it supports thread blocking using Os_delay() and context switching using assembly instructions (every thread has a TCB and its own stack).

![Priority simulation RTOS](https://user-images.githubusercontent.com/49674839/91479051-2d8fd100-e8a1-11ea-8e83-6aefde69e698.png)
