//
//  vm_execute.h
//
//  Created by Edmund Kapusniak on 24/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_VM_EXECUTE_H
#define KF_VM_EXECUTE_H

/*
    The actual interpreter loop.
*/

namespace kf
{

struct vmachine;
struct vm_exstate;

void vm_execute( vmachine* vm, vm_exstate state );

}

#endif

