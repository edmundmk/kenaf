//
//  cothread_object.cpp
//
//  Created by Edmund Kapusniak on 28/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "cothread_object.h"
#include "../call_stack.h"

namespace kf
{

cothread_object::cothread_object()
{
}

cothread_object::~cothread_object()
{
}

cothread_object* cothread_new( vmachine* vm )
{
    return new ( object_new( vm, COTHREAD_OBJECT, sizeof( cothread_object ) ) ) cothread_object();
}

}

