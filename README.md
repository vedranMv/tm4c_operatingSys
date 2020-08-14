Operating system for Tiva TM4C1294
======================

This repository contains an example project for a simplistic operating system for TM4C1294 microcontroller. __Note__ that the project is for CodeComposer studio. MPU9250 is implemented as C++ singleton.

Code is taken from a [bigger project I worked on](https://github.com/vedranMv/roverRPi3)  and can be integrated with other modules already there. It is easily scalable and can host any number of modules, providing an intuitive way for implementing complex architectures that require remote command execution and changing functionality during run-time.

In the project above, I built a rover based on this scheduler and event logger. Through it, I integrated 4-5 other sensors and actuators, including the ESP8266 wifi module. In combination with GUI running on my PC, this code allowed me to execute remote commands, reconfigure software of during run-time and track health of each of the sensors connected to it. Pretty cool project if you ask me :D

You can see how remote execution and event logging is used in [this video](https://vedran.ml/public/videos/Demo.mp4)


## Task scheduler (TS)
Task scheduler uses SysTick peripheral to run internal clock. This clock counts milliseconds past since startup. Counting steps can be configured depending on the application. For systems that run fast tasks, step size will need to be small (few milliseconds), on the other hand systems executing very few, longer tasks can use longer steps (few hundred milliseconds).

Unlike conventional schedulers, this implementation focuses on start time of the task. Task is added to the schedulers' queue and it won't be executed before its starting time is equal to the internal time. Once started, task is run-to-completion without preemption.

Task scheduler support both one-off or periodic tasks. For periodic tasks it is possible to specify starting time of first time, and period between consecutive runs. Additionally, it can be specified how many times the period time is supposed to be run, finite or infinite number of times.

To fully specify a task, caller needs module (library) ID from which it tries to execute a task. Then it needs task ID which denotes the service within the library that it wishes to call. Lastly, it supplies arguments required for the service to complete its request.

Once the TS is ready to execute a task it will copy the arguments provided by the caller and call a callback function that the modules has previously registered with the scheduler. Within the callback function there's a switch statement that decides what code is being executed based on the service ID that caller has provided.

In this whole process TS is not required to know the name of the function which is executing - it essentially needs two integers and array of arguments for service execution. Because of that, new tasks can be created on run-time and list of tasks to execute can even be injected remotely through e.g. WiFi module. Simply by supplying those 3 parameters and starting time.

Task scheduler implementation is bloated with ``volatile`` keywords because in the project where it was used beforehand content of TaskScheduler singleton was often changed inside the interrupts. To prevent any compiler optimization in these areas it was required to use volatile on all critical member variables/functions.

Small part of task scheduler is also a "Task profiler". This object keeps track of execution data about the task: how many times the task has run, average run time, longest run time, how often it misses its starting time and by how much time. It has minimal impact on performance and is very useful if you're designing a real-time system. Profiling can be disabled for release code by commenting out ``_TS_PERF_ANALYSIS_`` macro from ``taskScheduler/taskScheduler.h`` file.

## Event logger (EL)
Event logger is a smaller piece of code which allows different modules to log their status during run-time. Currently, event logger supports 7 events: Uninitialized, Startup, Initialized, OK, Error, Hang and Priority inversion\*. Every module can emit any of those events during run-time and they all get picked up by the event logger and saved together with the time stamp of the event. Later on, event log can be retrieved to track error in the system as it shows when each event happened, which module emitted event and during which service execution was the event emitted.

An event log from the code in this project looks as follows:

``[0] Module 6 raised event STARTUP during task -1 ``
<br/>
``[0] Module 6 raised event INITIALIZED during task -1``
<br/>
``[0] Module 7 raised event STARTUP during task -1 ``
<br/>
``[0] Module 7 raised event INITIALIZED during task -1 ``
<br/>
``[2] Module 3 raised event STARTUP during task -1 ``
<br/>
``[2] Module 3 raised event INITIALIZED during task -1 ``
<br/>
``[1002] Module 3 raised event OK during task 2 ``
<br/>
``[9004] Module 3 raised event ERROR during task 1 ``
<br/>
``[12005] Module 3 raised event OK during task 0 ``
<br/>
``[12005] Module 3 raised event PRIOINVERSION during task 0 ``

\*Priority inversion is an event in which the module emits *OK* event after it has previously emitted an *Error* or *Hang*.

## Example code
This code shows implementation of small operating system consisting of a task scheduler and an event logger. Task scheduler is meant to execute services provided by different modules available in the operating system while event logger provides interface for modules to log their status throughout their operation.

For demonstration, two modules are created in this code. One called Test modules, represented as a singleton class and the other, called Statistics, added as a part of this file, main.cpp. Test module offers 3 services:
0. Printing in16_t number
1. Printing a string not longer than 20 char
2. Printing a float

Statistics module provides 2 services:
0. Print statistics on all currently scheduled tasks (run time, period...)
1. Print content of event logger

Compile the example, upload it to your board and open serial console to read the outcome. *Note that if you add this to your project you'll need to increase heap size in project settings to something higher than 0 (this example uses 2048)*

## Porting the code

Even though the code was developed and tested on TM4C1294, the functional code is fully decoupled from hardware through the use of Hardware Abstraction Layer (HAL). If you want to experiment with support for other boards simply create new folder in ``HAL/``, and add in the same files as in ``HAL/tm4c1294/``. Keep interface of new HAL the same as that in ``HAL/tm4c1294/``, i.e. use same function names as those in header files ``HAL/tm4c1294/*.h``, just change implementation in ``*.c`` files. Main HAL include file, ``HAL/hal.h``, then uses macros to select the right board and load appropriate board drivers.
