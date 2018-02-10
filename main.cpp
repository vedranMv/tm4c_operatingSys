/**
 * Example of a simplistic operating system for TM4C1294
 *
 * This code shows implementation of small operating system consisting
 * of a task scheduler and an event logger. Task scheduler is meant to execute
 * services provided by different modules available in the operating system
 * while event logger provides interface for modules to log their status
 * throughout their operation.
 *
 * For demonstration, two modules are created in this code. One called Test
 * modules, represented as a singleton class and the other, called Statistics,
 * added as a part of this file, main.cpp. Test module offers 3 services:
 *  0) Printing in16_t number
 *  1) Printing a string not longer than 20 char
 *  2) Printing a float
 * Statistics module provides 2 services:
 *  0) Print statistics on all currently scheduled tasks (run time, period...)
 *  1) Print content of event logger
 *
 * Code in main() shows how to initialize the system and schedule 6 tasks for
 * execution. Tasks are scheduled as follows:
 *  1) Print float number 2s after startup
 *  2) Print integer every 5s starting 2s after startup. Kill the task after
 *      3 runs
 *  3) Print string of length 17 4s after startup
 *  4) Print string of length 35 9s after startup
 *  5) Print statistics for currently running tasks every 10 s starting 10s
 *      after startup. Kill the task after 2 runs
 *  6) Print content of the event log 22s after startup
 */

#include "testModule/testModule.h"
#include "HAL/hal.h"
#include "taskScheduler/taskScheduler.h"
#include "serialPort/uartHW.h"
#include "init/eventLog.h"


///-----------------------------------------------------------------------------
///     STATISTICS module used to print out statistical parameters about tasks
///     This section shows the most minimalistic way of adding new module that
///     can receive instructions from scheduler.
///     More expanded version is added as separate class in testModule.h file
///-----------------------------------------------------------------------------
//  Unique identifier of this module as registered in task scheduler
#define STATISTICS_UID      4
//  Definitions of ServiceID for service offered by this module
#define STATISTICS_T_TSCH   0  //  Print out event log data for TestModule
#define STATISTICS_T_EVLOG  1  //  Print out execution statistics for periodic
                               //  tasks in task scheduler


//  Interface with task scheduler - provides memory space and function
//  to call in order for task scheduler to request service from this module
_kernelEntry _kerInterface;

/**
 * Callback routine to invoke service offered by this module from task scheduler
 * @note It is assumed that once this function is called task scheduler has
 * already copied required variables into the memory space provided for it.
 */
void STATISTICS_KerCallback(void)
{
    static const char evName[][15] =
    {
        {"UNINITIALIZED\0"},
        {"STARTUP\0"},
        {"INITIALIZED\0"},
        {"OK\0"},
        {"HANG\0"},
        {"ERROR\0"},
        {"PRIOINVERSION\0"}
    };
    /*
     *  Data in args[] contains bytes that constitute arguments for function
     *  calls. The exact representation(i.e. whether bytes represent ints, floats)
     *  of data is known only to individual blocks of switch() function. There
     *  is no predefined data separator between arguments inside args[].
     */
    switch (_kerInterface.serviceID)
    {
    /*
     *  Print statistics for the tasks currently in the scheduler
     *  args[] = none
     *  retVal none
     */
    case STATISTICS_T_TSCH:
        {
            uint32_t Ntasks = TaskScheduler::GetI().NumOfTasks();

            //  Loop through all tasks currently in the list
            for (uint8_t i = 0; i < Ntasks; i++)
            {
                const TaskEntry *task = TaskScheduler::GetI().FetchNextTask(i==0);
                if (task == 0)
                    break;

                //  Print current time
                DEBUG_WRITE("[%d] ", msSinceStartup);

                DEBUG_WRITE("Performance for service %d from module %d:\n", \
                            task->GetTaskUID(), task->GetLibUID());

                DEBUG_WRITE("\tTask running under PID: %d, period %d ms\n", \
                            (uint16_t)task->GetPID(), task->GetPeriod());

                DEBUG_WRITE("\tNext execution of the task at: %d ms\n", \
                            (uint32_t)task->GetTimeStamp());

                DEBUG_WRITE("\tSo far task has completed %ul runs with ",
                            task->_perf.taskRuns);

                //  Calculate average runtime
                float runTim = (float)(task->_perf.accRT);
                runTim += ((float)task->_perf.msAcc)/1000.0f;

                if (task->_perf.taskRuns > 0)
                    runTim = runTim / ((float)task->_perf.taskRuns);
                else
                    runTim = 0.0;

                DEBUG_WRITE("average runtime of %d.%d ms \n", _FTOI_(runTim));

                DEBUG_WRITE("\tStart time was missed on %d runs by ",
                            (uint32_t)(task->_perf.startTimeMissCnt));

                //  Calculate average time by the which the deadline was missed
                float missTime = 0.0;
                if (task->_perf.startTimeMissCnt > 0)
                    missTime = ((float)task->_perf.startTimeMissTot) /
                               ((float)task->_perf.startTimeMissCnt);
                DEBUG_WRITE("%d.%d ms on average.\n\n", _FTOI_(missTime));
            }
        }
        break;
    /*
     *  Print out content of the event logger
     *  args[] = none
     *  retVal none
     */
    case STATISTICS_T_EVLOG:
        {
            //  Print current time
            DEBUG_WRITE("[%d] ", msSinceStartup);
            DEBUG_WRITE("Event logger data dump:\n");

            //  Loop through linked list of events and send them one by one
            volatile struct _eventEntry* node = EventLog::GetI().GetHead();
            while(node != 0)
            {

                DEBUG_WRITE("\t[%d] Module ", node->timestamp);
                DEBUG_WRITE("%d raised event ", (uint16_t)node->libUID);
                DEBUG_WRITE("%s", evName[node->event]);
                DEBUG_WRITE(" during task %d \n", node->taskID);

                node = node->next;
            }
        }
        break;
    }
}

/**
 * Initialize routine for statistics module
 */
void STAT_InitSW()
{
    //  Register module services with task scheduler
    _kerInterface.callBackFunc = STATISTICS_KerCallback;
    TS_RegCallback(&_kerInterface, STATISTICS_UID);
}
///-----------------------------------------------------------------------------
///         End of STATISTICS module
///-----------------------------------------------------------------------------



/**
 * main.c
 */
int main(void)
{
    //  Grab reference to task scheduler object
    volatile TaskScheduler& ts = TaskScheduler::GetI();

    //  Initialize board and FPU
    HAL_BOARD_CLOCK_Init();

    //  Initialize serial port
    SerialPort::GetI().InitHW();
    DEBUG_WRITE("Initialized Uart... \n");

    //  Run initialization of event logger
    EventLog::GetI().InitSW();
    //  Start logging events
    EventLog::GetI().RecordEvents(true);
    DEBUG_WRITE("Initialized event logger... \n");

    //  Initialize hardware used by task scheduler, set time step to be 1ms.
    //  Time step gives minimum time resolution when specifying execution time.
    //  This function call also starts SysTick timer which keeps internal time!
    ts.InitHW(1);
    DEBUG_WRITE("Initialized task scheduler... \n");

    //  Initialize Test module
    TestMod::GetI().InitHW();
    TestMod::GetI().InitSW();

    //  Initialize statistics module
    STAT_InitSW();


    //  Add first task. This is non-periodic, one-off task that's executed at
    //  1000ms after startup. Task invokes PRINTFLOAT service from TESTMOD module
    ts.SyncTask(TESTMOD_UID, TESTMOD_T_PRINTFLOAT, 1000);
    //  Add argument for the task -> number we want to print
    ts.AddArg<float>(127.58);
    //  <---1st task added--->

    //  Add second task. This is periodic task, executed every 5 sec starting
    //  from 2sec after startup. This task prints out integer number 2 times
    //  Task is killed after 4 repeats
    ts.SyncTaskPer(TESTMOD_UID, TESTMOD_T_PRINTINT, 2000, 5000, 4);
    ts.AddArg<int16_t>(-8574);
    ts.AddArg<uint8_t>(2);
    //  <---2nd task added--->

    ts.SyncTask(TESTMOD_UID, TESTMOD_T_PRINTSTR, 4000);
    //  Add argument for the task -> text we want to print. Note that string is
    //  array of chars so it can be simply added using this non-template function
    ts.AddArgs((void*)"Printing at T+4s\0", 17);
    //  <---3rd task added--->

    ts.SyncTask(TESTMOD_UID, TESTMOD_T_PRINTSTR, 9000);
    //  Add argument for the task -> text we want to print. Note that string is
    //  array of chars so it can be simply added using this non-template function
    ts.AddArgs((void*)"Printing a slightly longer string", 35);
    //  <---4th task added--->

    //  Print out statistics for periodic tasks once every 10s, starting from
    //  10s after startup. Kill this task after 3 runs.
    ts.SyncTaskPer(STATISTICS_UID, STATISTICS_T_TSCH, 10000, 10000, 2);
    //  Task takes no arguments
    //  <---5th task added--->

    //  Print content of the event log 22s after **this command has been called**
    //  Note the minus sign when specifying the time. When time is given as
    //  negative it is the relative time with respect to current time. So instead
    //  of executing the task at T=22s, this task is executed at
    //  T = current_time + 22s
    ts.SyncTask(STATISTICS_UID, STATISTICS_T_EVLOG, -22000);
    //  Task takes no arguments
    //  <---6th task added--->



    DEBUG_WRITE("Added tasks in the queue... \n");
    DEBUG_WRITE("Entering task scheduler... \n");

    while(1)
        //  Run task scheduler loop
        TS_GlobalCheck();
}
