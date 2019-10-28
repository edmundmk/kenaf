//
//  cothread_object.h
//
//  Created by Edmund Kapusniak on 28/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_COTHREAD_OBJECT_H
#define KF_COTHREAD_OBJECT_H

/*
    A cothread is basically a single call stack - a 'fiber' or 'green thread'.
    A cothread can be suspended in the middle of a call stack, and then resumed
    later.  Each instance of a generator is a new cothread.
*/

#include <vector>
#include "object_model.h"

namespace kf
{

/*
    Structures.
*/

struct call_frame
{
};

struct cothread_object : public object
{
    std::vector< value > stack;
    std::vector< call_frame > call_frames;
};

/*
    Functions.
*/

cothread_object* cothread_new( vm_context* vm );

}

#endif

