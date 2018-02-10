/**
 * testModule.h
 *
 *  Created on: Feb 10, 2018
 *      Author: v125
 *
 *  This file shows a minimal example for creating a module which can be
 *  integrated with the task scheduler and event logger.
 */
#include "hwconfig.h"
#include "libs/myLib.h"

#ifndef TESTMODULE_TESTMODULE_H_
#define TESTMODULE_TESTMODULE_H_

#if defined(__HAL_USE_TASKSCH__)
    #include "taskScheduler/taskScheduler.h"
    //  Unique identifier of this module as registered in task scheduler
    #define TESTMOD_UID           3
    //  Definitions of ServiceID for service offered by this module
    #define TESTMOD_T_PRINTINT    0
    #define TESTMOD_T_PRINTSTR    1
    #define TESTMOD_T_PRINTFLOAT  2
#endif

class TestMod
{
    friend void _TESTMOD_KernelCallback(void);
    public:
        static TestMod& GetI();
        static TestMod* GetP();

        void InitHW();
        void InitSW();

        int32_t PrintInt16(int16_t intToPrint, uint8_t N);
        int32_t PrintString(char *stringToPrint);
        int32_t PrintFloat(float floatToPrint);

    protected:
        TestMod();
        ~TestMod();
        TestMod(TestMod &arg) {}              //  No definition - forbid this
        void operator=(TestMod const &arg) {} //  No definition - forbid this


        //  Interface with task scheduler - provides memory space and function
        //  to call in order for task scheduler to request service from this module
#if defined(__HAL_USE_TASKSCH__)
        _kernelEntry _kerInterface;
#endif
};

#endif /* TESTMODULE_TESTMODULE_H_ */
