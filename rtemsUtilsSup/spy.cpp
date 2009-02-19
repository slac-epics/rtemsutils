/*************************************************************************\
* Copyright (c) 2006 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 * Profile CPU use
 *
 * This code is based on RTEMS capture engine which is part of the
 * RTEMS distribution and is covered by the following copyright notice:
 **************************************************************************
 *   Copyright Objective Design Systems Pty Ltd, 2002                     *
 *   All rights reserved Objective Design Systems Pty Ltd, 2002           *
 *   Chris Johns (ccj@acm.org)                                            *
 *                                                                        *
 *   COPYRIGHT (c) 1989-1998.                                             *
 *   On-Line Applications Research Corporation (OAR).                     *
 *                                                                        *
 *   The license and distribution terms for this file may be              *
 *   found in the file LICENSE in the RTEMS  distribution.                *
 *                                                                        *
 *   This software with is provided ``as is'' and with NO WARRANTY.       *
 **************************************************************************
 */

#include <string.h>
#include <ctype.h>
#include <iocsh.h>
#include <epicsStdio.h>
#include <epicsThread.h>
#include <epicsExport.h>
#include <rtems.h>
#include <rtems/error.h>
#include <rtems/capture.h>
#include <rtems/monitor.h>

#define CAPTURE_MAX_TASKS 50
#define NAME_SIZE         24
#define WAKEUP_EVENT      RTEMS_EVENT_8

extern "C" {

static rtems_id captureMutex;
static volatile int secondsDelay;
static volatile int firstTime;

static void
rtems_capture_cli_task_load_thread(rtems_task_argument arg)
{
    rtems_interval ticksPerSecond;
    rtems_event_set events;

    rtems_clock_get(RTEMS_CLOCK_GET_TICKS_PER_SECOND, &ticksPerSecond);
    for (;;) {
        rtems_capture_task_t* tasks[CAPTURE_MAX_TASKS + 1];
        unsigned long long    load[CAPTURE_MAX_TASKS + 1];
        rtems_capture_task_t* task;
        unsigned long long    total_time;
        int                   i;
        int                   j;

        /*
         * Wait for the main thread to release us
         */
        rtems_semaphore_obtain(captureMutex, RTEMS_WAIT, RTEMS_NO_TIMEOUT);

        /*
         * Iterate over the tasks and sort the highest load tasks
         * into our local arrays. We only handle a limited number of
         * tasks.
         */
        memset(tasks, 0, sizeof(tasks));
        memset(load, 0, sizeof(load));

        task = rtems_capture_get_task_list();
        while (task) {
            if (rtems_capture_task_valid(task)) {
                unsigned long long l = rtems_capture_task_delta_time(task);
                for (i = 0; i < CAPTURE_MAX_TASKS; i++) {
                    if (tasks[i]) {
                        if ((l == 0) || (l < load[i]))
                            continue;
                        for (j = (CAPTURE_MAX_TASKS - 1); j >= i; j--) {
                            tasks[j + 1] = tasks[j];
                            load[j + 1]  = load[j];
                        }
                    }
                    tasks[i] = task;
                    load[i]  = l;
                    break;
                }
            }
            task = rtems_capture_next_task(task);
        }

        total_time = 0;
        for (i = 0; i < CAPTURE_MAX_TASKS; i++)
            total_time += load[i];

        printf("\nPress <return> to terminate.\n");
        if (firstTime) {
            firstTime = 0;
        }
        else {
            printf("   PID   PRI STATE   %%CPU %%STK  NAME\n");
            for (i = 0 ; (i < CAPTURE_MAX_TASKS) && tasks[i] ; i++) {
                rtems_task_priority priority;
                int                 stack_used;
                int                 task_load;
                int                 l;
                char                name[NAME_SIZE];
    
                stack_used = rtems_capture_task_stack_usage(tasks[i]) * 100;
                stack_used /= rtems_capture_task_stack_size(tasks[i]);
                if (stack_used > 100)
                    stack_used = 100;
                task_load = ((load[i] * 1000) / total_time);
                priority = rtems_capture_task_real_priority(tasks[i]);
    
                rtems_monitor_dump_id(rtems_capture_task_id(tasks[i]));
                printf(" ");
                rtems_monitor_dump_priority(priority);
                printf(" ");
                l = rtems_monitor_dump_state(rtems_capture_task_state(tasks[i]));
                epicsThreadGetName((epicsThreadId)rtems_capture_task_id(tasks[i]),
                                    name,
                                    sizeof name);
                printf("%*c%3d.%d  %3d  %s\n", 7 - l, ' ', task_load / 10,
                                                           task_load % 10,
                                                           stack_used,
                                                           name);
            }
        }
        rtems_semaphore_release(captureMutex);
        rtems_event_receive(WAKEUP_EVENT,
                            RTEMS_EVENT_ANY |RTEMS_WAIT,
                            secondsDelay * ticksPerSecond,
                            &events);
    }
}

static void
runCaptureEngine(int seconds)
{
    rtems_status_code sc;
    static rtems_id captureTid;
    int c;

    if (!captureMutex) {
        rtems_task_priority pri;
        rtems_task_set_priority(RTEMS_SELF,RTEMS_CURRENT_PRIORITY, &pri);
        sc = rtems_semaphore_create(rtems_build_name('S', 'P', 'Y', ' '),
            0,
            RTEMS_PRIORITY|RTEMS_BINARY_SEMAPHORE|RTEMS_INHERIT_PRIORITY|RTEMS_NO_PRIORITY_CEILING|RTEMS_LOCAL,
            0,
            &captureMutex);
        if (sc != RTEMS_SUCCESSFUL) {
            printf("***** Can't create capture mutex: %s\n", rtems_status_text(sc));
            return;
        }
        sc = rtems_task_create(rtems_build_name('S', 'P', 'Y', ' '),
            pri,
            (2 * RTEMS_MINIMUM_STACK_SIZE) +
                        (CAPTURE_MAX_TASKS * (sizeof(rtems_capture_task_t*) +
                                              sizeof(unsigned long long))),
            RTEMS_NO_FLOATING_POINT|RTEMS_LOCAL,
            RTEMS_PREEMPT|RTEMS_TIMESLICE|RTEMS_NO_ASR,
            &captureTid);
        if (sc != RTEMS_SUCCESSFUL) {
            printf("***** Can't create capture task: %s\n", rtems_status_text(sc));
            rtems_semaphore_delete(captureMutex);
            captureMutex = 0;
            return;
        }
        sc = rtems_task_start(captureTid, rtems_capture_cli_task_load_thread, 0);
        if (sc != RTEMS_SUCCESSFUL) {
            printf("***** Can't start capture task: %s\n", rtems_status_text(sc));
            rtems_semaphore_delete(captureMutex);
            rtems_task_delete(captureTid);
            captureMutex = 0;
            return;
        }
    }
    if (seconds <= 0)
        seconds = 5;
    secondsDelay = seconds;
    firstTime = 1;
    sc = rtems_capture_control(1);
    if (sc != RTEMS_SUCCESSFUL) {
        printf("***** Can't enable capture engine: %s\n", rtems_status_text(sc));
        return;
    }
    rtems_semaphore_release(captureMutex);
    while ((c = getchar()) != '\n') {
        if (c == EOF)
            clearerr(stdin);
    }
    rtems_semaphore_obtain(captureMutex, RTEMS_WAIT, RTEMS_NO_TIMEOUT);
    rtems_event_send(captureTid, WAKEUP_EVENT);
    rtems_capture_control(0);
}

static void
configureCaptureEngine(void)
{
    rtems_status_code sc;

    /*
     * For now use the smallest size and the system time
     */
    sc = rtems_capture_open(100, NULL);
    if (sc != RTEMS_SUCCESSFUL) {
        printf("***** Can't initialize profiling capture engine: %s\n", rtems_status_text(sc));
        return;
    }
    printf("***** Initialized profiling capture engine.\n");
}

/*
 * IOC shell command
 */
static const iocshArg spyArg0 = {"seconds",iocshArgInt};
static const iocshArg * const spyArgs[1] = {&spyArg0};
static const iocshFuncDef spyFuncDef = {"spy", 1, spyArgs};
static void
spyCallFunc(const iocshArgBuf *args)
{
    runCaptureEngine(args[0].ival);
}

static void
spyRegisterCommands(void)
{
    static int firstTime = 1;
    if (firstTime) {
        iocshRegister(&spyFuncDef, spyCallFunc);
        firstTime = 0;
    }
}

} /* extern "C" */

/*
 * The epicsExportRegistrar creates the symbol which causes this object
 * module to be linked with the application.
 * The static constructor is necessary to ensure that the capture engine
 * is set up before any EPICS tasks are created.  The capture engine can
 * monitor stack usage only for tasks created after the engine is set up.
 */
epicsExportRegistrar(spyRegisterCommands);
class ConfigureCaptureEngine {
  public:
    ConfigureCaptureEngine() { configureCaptureEngine(); }
};
static ConfigureCaptureEngine configureCaptureEngineObj;
