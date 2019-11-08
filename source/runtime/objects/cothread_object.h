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
#include "../vmachine.h"

namespace kf
{

struct stack_frame;

/*
    Structures.
*/

struct cothread_object : public object
{
    cothread_object();
    ~cothread_object();

    std::vector< value > stack;
    std::vector< stack_frame > stack_frames;
    unsigned xp;
};

/*
    Functions.
*/

cothread_object* cothread_new( vmachine* vm );

}

#endif

