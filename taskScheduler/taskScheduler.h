/**
 *	taskScheduler.h
 *
 *  Created on: 30.7. 2016.
 *      Author: Vedran Mikov
 *
 *  Task scheduler library
 *  @version 2.8.0
 *  V1.1
 *  +Implementation of queue of tasks with various parameters. Tasks identified
 *      by unique integer number (defined by higher level library)
 *  V2.1 - 22.1.2017
 *  +Added time component to task entries - each task now has a time stamp at
 *      which it needs to be executed
 *  +Implemented SysTick in interrupt mode to count time from startup providing
 *      time reference for performing task at desired point in time from startup
 *  +Added callback registration for all kernel modules to register their services
 *  +Callback functionality from now on implemented so that module first registers
 *      its service by adding entry into the callback vector. Once the task
 *      scheduler requires that service it will transfer necessary memory into
 *      kernel space and call provided callback function for particular module
 *  V2.2
 *  +Switched to linked list as internal container for tasks - allows more
 *  flexibility in adding data (and can be sorted)
 *  V2.3 - 6.2.2016
 *  +TaskEntry instance now uses dynamically allocated array for storing arguments
 *  +Implemented support for periodic tasks. Once executed task is rescheduled
 *  based on its period. For non-periodic tasks period must be set to 0.
 *  V2.4 - 20.2.2017
 *  +Implemented repeat counter. Periodic tasks can now be automatically killed
 *  after a predefined number of repeats.
 *  V2.4.1 - 25.2.2017
 *  +TaskScheduler class now offers adding single arguments of basic data types
 *  (char, float...) through common template member-function AddArg(T arg)
 *  V2.4.2 - 4.3.2017
 *  +Instead of starting SysTick in constructor, class now has InitHW() func.
 *  to start SysTick at any point.
 *  V2.4.3 - 9.3.2017
 *  +Changed TaskScheduler class into a singleton
 *  V2.5 - 25.3.2017
 *  +Implemented a member function to delete already scheduled tasks from list
 *  V2.5.1 - 2.7.2017
 *  +Change include paths for better portability, new way of printing to debug
 *  +Integration with event logger
 *  V2.6.0 - 10.7.2017
 *  +Added member functions to access internal task list and number of tasks on it
 *  +Added member function for easier scheduling of remote tasks. SyncTaskPer()
 *  should be preferred way of scheduling tasks
 *  V2.7.0 13.7.2017
 *  +Periodic tasks are rescheduled before their execution to keep time
 *  punctuality
 *  V2.8.0 - 9.9.2017
 *  +Added static member function for checking validity of kernel module UID
 *  +Periodically called functions switched to inline, declared in header
 *  +Implemented kernel callback for TS, allowing enable/disable signal for
 *  SysTick timer to be sent remotely
 *
 *  TODO:
 *  Implement UTC clock feature. If at some point program finds out what the
 *  actual time is it can save it and maintain real UTC time reference
 *  +Add PID to task so it can be killer more easily(PID of periodic task is
 *  inherited)
 */
#include "hwconfig.h"
#include "HAL/hal.h"

//  Compile following section only if hwconfig.h says to include this module
#if !defined(ROVERKERNEL_TASKSCHEDULER_TASKSCHEDULER_H_) \
    && defined(__HAL_USE_TASKSCH__)
#define ROVERKERNEL_TASKSCHEDULER_TASKSCHEDULER_H_

#include "linkedList.h"

/**
 * Callback entry into the Task scheduler from individual kernel module
 * Once initialized, each kernel module registers the services it provides into
 * a vector by inserting CallBackEntry into a global vector (handled by
 * TS_RegCallback function). CallBackEntry holds: a) Function to be called when
 * someone requests a service from kernel module; b) ServiceID of service to be
 * executed; c)Memory space used for arguments for callback function; d) Return
 * variable of the service execution
 */
struct _kernelEntry
{
    void((*callBackFunc)(void));    // Pointer to callback function
    uint8_t serviceID;              // Requested service
    uint8_t *args;                  // Arguments for service execution
    uint16_t argN;                  // Length of *args array
    int32_t  retVal;                // (Optional) Return variable of service exec
};


//  Pass to 'repeats' argument for indefinite number of repeats
#define T_PERIODIC  (-1)
//  Pass to 'time' for execution as-soon-as-possible
#define T_ASAP      (0)

//  Unique identifier of this module as registered in task scheduler
    #define TASKSCHED_UID           7
    //  Definitions of ServiceID for service offered by this module
    #define TASKSCHED_T_ENABLE      0
    #define TASKSCHED_T_KILL        1

//  Enable debug information printed on serial port
//#define __DEBUG_SESSION2__

#ifdef __DEBUG_SESSION2__
#include "serialPort/uartHW.h"
#endif

//  Compiling with this definition will enable parts of TS code used to measure
//  performance such as missed starting time, average execution time on task...
#define _TS_PERF_ANALYSIS_

#ifdef _TS_PERF_ANALYSIS_
#include "tsProfiler.h"
#endif

//  Internal time since TaskScheduler startup (in ms); Increased by SysTick
//  interrupt. Every tick increases this variable by value passed as argument to
//  TaskScheduler::InitHW() function. Can be as little as 1ms, but can be also
//  be more, depending on system requirements
extern volatile uint64_t msSinceStartup;

/**
 * Task scheduler class implementation
 * @note Task and its arguments are added separately. First add new task and then
 * use 'AddArgs()' or AddArg<T> functions to add argument(s) for that task
 * Task scheduler allows to schedule tasks for execution at a specific point in
 * time, it's NOT a task scheduler you'd find in an operating system and it
 * doesn't perform actual context switching. Rather it runs-to-completion a
 * single task at the time. Scheduling in this case refers to ability to provide
 * a starting time/period/repeats for a task.
 ***Class implemented with volatile functions as adding tasks is permitted from
 *  within interrupts. And in future task execution might be implemented from
 *  periodic timer interrupt as well.
 */
class TaskScheduler
{
    //  Functions & classes needing direct access to all members
    friend void _TS_KernelCallback(void);
    friend void TS_GlobalCheck(void);

	public:
        volatile static TaskScheduler& GetI();
        volatile static TaskScheduler* GetP();

        static bool ValidKernModule(uint8_t libUID);

		void                InitHW(uint32_t timeStepMS = 100) volatile;
		inline void         Reset() volatile;

		uint32_t            NumOfTasks() volatile;
		const TaskEntry*    FetchNextTask(bool fromStart) volatile;

		//  Adding new tasks
		void SyncTask(uint8_t libUID, uint8_t taskID, int64_t time,
		              bool periodic = false, int32_t rep = 0) volatile;
		void SyncTaskPer(uint8_t libUID, uint8_t taskID, int64_t time,
		                 int32_t period, int32_t rep) volatile;
		void SyncTask(TaskEntry te) volatile;

		//  Add arguments for the last task added
		void AddArgs(void* arg, uint16_t argLen) volatile;

		//  Remove task for task list
		void RemoveTask(uint8_t libUID, uint8_t taskID,
		                void* arg, uint16_t argLen) volatile;
		bool RemoveTask(uint16_t PIDarg) volatile;


        TaskEntry            PopFront() volatile;
        volatile TaskEntry&  PeekFront() volatile;

		///---------------------------------------------------------------------
		///                      Inline functions                       [PUBLIC]
		///---------------------------------------------------------------------
		/**
		 * Return status of Task scheduler queue
		 * @return  true: if there's nothing in queue
		 *         false: if queue contains data
		 */
		inline bool IsEmpty() volatile
        {
            return _taskLog.IsEmpty();
        }
		/**
		 ****Template member function needs to be defined in the header file
		 * Add a single argument through the template function
		 * Allows to append argument of any type to the current task
		 * @note Once PopFront() function has been called it's not possible to append
		 * new arguments (because it's unknown if the _lastIndex node got deleted or not)
		 * @param arg data argument to append to the current task argument list
		 */
		template<typename T>
		void AddArg(T arg) volatile
		{
            //  Sensitive task, disable all interrupts
		    HAL_BOARD_InterruptEnable(false);

		    if (_lastIndex != 0)
		        _lastIndex->data.AddArg((void*)&arg, sizeof(arg));

		    //  Sensitive task done, enable interrupts again
		    HAL_BOARD_InterruptEnable(true);
		}

	private:
        TaskScheduler();
        ~TaskScheduler();
        TaskScheduler(TaskScheduler &arg) {}        //  No definition - forbid this
        void operator=(TaskScheduler const &arg) {} //  No definition - forbid this


		//  Queue of tasks to be executed, implemented as doubly linked list
		volatile LinkedList	_taskLog;
		/*
		 *  Pointer to last added item (to be able to append arguments to it)
		 *  ->Is being reset to zero after calling PopFront() function
		 *  ->volatile pointer (because it can change from within interrupt) to
		 *  a volatile object (object can be removed from within interrupt)
		 */
		volatile _llnode* volatile _lastIndex;

        //  Interface with task scheduler - provides memory space and function
        //  to call in order for task scheduler to request service from this module
        struct _kernelEntry _ker;
};

extern void TS_GlobalCheck(void);
extern void TS_RegCallback(struct _kernelEntry *arg, uint8_t uid);


#endif /* TASKSCHEDULER_H_ */
