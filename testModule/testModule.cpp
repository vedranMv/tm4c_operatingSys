/*
 * testModule.cpp
 *
 *  Created on: Feb 10, 2018
 *      Author: v125
 */
#include "testModule/testModule.h"
#include "serialPort/uartHW.h"

//  Integration with event log, if it's present
#ifdef __HAL_USE_EVENTLOG__
    #include "init/eventLog.h"
    /**
     *  Simplify emitting events
     *  X is integer id of service that handled the task
     *  Y is the EVENTS_ enum from eventLog.h file
     */
    #define EMIT_EV(X, Y)  EventLog::EmitEvent(TESTMOD_UID, X, Y)
#endif /* __HAL_USE_EVENTLOG__ */


/**
 * Callback routine to invoke service offered by this module from task scheduler
 * @note It is assumed that once this function is called task scheduler has
 * already copied required variables into the memory space provided for it.
 */
void _TESTMOD_KernelCallback(void)
{
    TestMod& __testMod = TestMod::GetI();

    //  Check for null-pointer
    if (__testMod._kerInterface.args == 0)
        return;

    /*
     *  Data in args[] contains bytes that constitute arguments for function
     *  calls. The exact representation(i.e. whether bytes represent ints, floats)
     *  of data is known only to individual blocks of switch() function. There
     *  is no predefined data separator between arguments inside args[].
     */
    switch (__testMod._kerInterface.serviceID)
    {
    /*
     *  Print integer to serial port through TestMod::PrintInt function
     *  args[] = intToPrint(int16_t)|numOfLines(uint8_t)
     *  retVal on of myLib.h STATUS_* macros
     */
    case TESTMOD_T_PRINTINT:
        {
            int16_t intToPrint;
            uint8_t numOfLines;

            //  Extract first argument
            memcpy((void*)&intToPrint,
                    __testMod._kerInterface.args,
                    sizeof(int16_t));

            //  Extract second argument
            memcpy((void*)&numOfLines,
                   (void*)(__testMod._kerInterface.args+sizeof(int16_t)),
                    sizeof(uint8_t));

            //  Call the function & save the return value
            __testMod._kerInterface.retVal = __testMod.PrintInt16(intToPrint,
                                                                  numOfLines);
        }
        break;
    /*
     *  Print string to serial port through TestMod::PrintString function
     *  args[] = stringToPrint(max 20 bytes)
     *  retVal on of myLib.h STATUS_* macros
     */
    case TESTMOD_T_PRINTSTR:
        {
            char strToPrint[20];

            //  Extract first argument
            memcpy((void*)strToPrint,
                    __testMod._kerInterface.args,
                    20);

            //  Call the function & save the return value
            __testMod._kerInterface.retVal = __testMod.PrintString(strToPrint);
        }
        break;
    /*
     *  Print float to serial port through TestMod::PrintFloat function
     *  args[] = floatToPrint(4bytes)
     *  retVal on of myLib.h STATUS_* macros
     */
    case TESTMOD_T_PRINTFLOAT:
        {
            float floatToPrint;

            //  Extract first argument
            memcpy((void*)&floatToPrint,
                    __testMod._kerInterface.args,
                    sizeof(float));

            //  Call the function & save the return value
            __testMod._kerInterface.retVal = __testMod.PrintFloat(floatToPrint);
        }
        break;
    }

    //  Emit event based on the outcome of task
    //  Specify which service handled the task and what was the outcome
    if (__testMod._kerInterface.retVal == STATUS_OK)
        EMIT_EV(__testMod._kerInterface.serviceID, EVENT_OK);
    else
        EMIT_EV(__testMod._kerInterface.serviceID, EVENT_ERROR);
}


///-----------------------------------------------------------------------------
///         Functions for returning static instance                     [PUBLIC]
///-----------------------------------------------------------------------------

/**
 * Return reference to a singleton
 * @return reference to an internal static instance
 */
TestMod& TestMod::GetI()
{
    static TestMod singletonInstance;
    return singletonInstance;
}

/**
 * Return pointer to a singleton
 * @return pointer to a internal static instance
 */
TestMod* TestMod::GetP()
{
    return &(TestMod::GetI());
}

///-----------------------------------------------------------------------------
///         Public functions used for configuring MPU9250               [PUBLIC]
///-----------------------------------------------------------------------------

/**
 * Initialize hardware used by test module
 */
void TestMod::InitHW()
{
    //  Nothing to do here

    //  Emit startup event - notify that the module has started initialization
    //  -1 can be substituted with any number to track which function emitted event
    EMIT_EV(-1, EVENT_STARTUP);
}

/**
 * Software initialization of test module
 */
void TestMod::InitSW()
{
    //  Register module services with task scheduler
    _kerInterface.callBackFunc = _TESTMOD_KernelCallback;
    TS_RegCallback(&_kerInterface, TESTMOD_UID);

    //  Emit initialized event - notify that the module has completed
    //  initialization and is ready
    //  -1 can be substituted with any number to track which function emitted event
    EMIT_EV(-1, EVENT_INITIALIZED);
}

/**
 * Print signed 16 bit integer out through serial port
 * @param intToPrint Integer to print out
 * @param N Number of times to print the line containing the argument
 * @return One of myLib.h STATUS_* macros
 */
int32_t TestMod::PrintInt16(int16_t intToPrint, uint8_t N)
{
    //  Print the line containing the given number N times
    for (uint8_t i = 0; i < N; i++)
        DEBUG_WRITE("I'm service 0 printing int16_t: %d\n", intToPrint);

    return STATUS_OK;
}

/**
 * Print string of max size 20 to serial port. If string is longer error message
 * is printed instead.
 * @param stringToPrint
 * @return One of myLib.h STATUS_* macros
 */
int32_t TestMod::PrintString(char *stringToPrint)
{
    uint8_t i = 0;
    int32_t retVal = STATUS_OK;
    const uint8_t sizeLimit = 20;

    //  Check if string is null terminated and has the length shorter than limit
    for (i = 0; i < sizeLimit; i++)
    {
        if (stringToPrint[i] == '\0')
            break;
    }

    //  Print out message based on the length of the string
    if ((i+1) < sizeLimit)
    {
        DEBUG_WRITE("I'm service 1 printing a string: %s\n", stringToPrint);
    }
    else
    {
        DEBUG_WRITE("I'm service 1 printing a string but there was an error with you string\n");
        retVal = STATUS_ARG_ERR;
    }

    return retVal;
}

/**
 *
 * @param floatToPrint
 * @return One of myLib.h STATUS_* macros
 */
int32_t TestMod::PrintFloat(float floatToPrint)
{
    DEBUG_WRITE("I'm service 2 printing int16_t: %d.%d\n", _FTOI_(floatToPrint));

    return STATUS_OK;
}

///-----------------------------------------------------------------------------
///                      Class constructor & destructor              [PROTECTED]
///-----------------------------------------------------------------------------
TestMod::TestMod()
{
    //  Emit uninitialized event - module has been reset
    //  -1 can be substituted with any number to track which function emitted event
    EMIT_EV(-1, EVENT_UNINITIALIZED);
}
TestMod::~TestMod()
{}

