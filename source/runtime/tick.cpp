//
//  tick.cpp
//
//  Created by Edmund Kapusniak on 21/08/2019.
//  Copyright © 2019 Edmund Kapusniak. All rights reserved.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "tick.h"

static const uint64_t NS_PER_SECOND = 1000000000;

#if __APPLE__

#include <mach/clock.h>
#include <mach/mach.h>

struct clock_service
{
    clock_service()
    {
        host_get_clock_service( mach_host_self(), SYSTEM_CLOCK, &clock );
    }

    ~clock_service()
    {
        mach_port_deallocate( mach_host_self(), clock );
    }

    clock_serv_t clock;
};

static clock_service clock_service;

uint64_t tick()
{
    mach_timespec_t timespec;
    clock_get_time( clock_service.clock, &timespec );
    return timespec.tv_sec * NS_PER_SECOND + timespec.tv_nsec;
}

double tick_seconds( uint64_t tick )
{
    return tick / NS_PER_SECOND + ( tick % NS_PER_SECOND ) * 1.0 / NS_PER_SECOND;
}

#elif __linux__

#include <time.h>

struct tick_base
{
    time_t tv_sec;
    tick_base()
    {
        timespec tm;
        clock_gettime( CLOCK_MONOTONIC, &tm );
        tv_sec = tm.tv_sec;
    }
};

static tick_base tick_base;

uint64_t tick()
{
    timespec tm;
    clock_gettime( CLOCK_MONOTONIC, &tm );
    return ( tm.tv_sec - tick_base.tv_sec ) * NS_PER_SECOND + tm.tv_nsec;
}

double tick_seconds( uint64_t tick )
{
    return tick / NS_PER_SECOND + ( tick % NS_PER_SECOND ) * 1.0 / NS_PER_SECOND;
}

#elif _WIN32

#include <stdlib.h>
#include <Windows.h>

struct qpc_frequency
{
    qpc_frequency()
    {
        QueryPerformanceFrequency( &frequency );
    }

    ~qpc_frequency()
    {
    }

    LARGE_INTEGER frequency;
};

static qpc_frequency qpc_frequency;

uint64_t tick()
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter( &counter );
    return (uint64_t)counter.QuadPart;
}


double tick_seconds( uint64_t tick )
{
    auto div = lldiv( tick, qpc_frequency.frequency.QuadPart );
    return div.quot + ( (double)div.rem / (double)qpc_frequency.frequency.QuadPart );
}

#endif

