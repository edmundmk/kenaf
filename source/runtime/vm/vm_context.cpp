//
//  vm_context.cpp
//
//  Created by Edmund Kapusniak on 29/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "vm_context.h"
#include "../objects/cothread_object.h"

namespace kf
{

vm_context::vm_context()
    :   cothreads( nullptr )
    ,   global_object( nullptr )
    ,   prototypes{}
    ,   selector_self{ {}, {} }
    ,   next_cookie( 0 )
{
}

vm_context::~vm_context()
{
}

}

