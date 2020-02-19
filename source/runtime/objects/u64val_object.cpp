//
//  u64val_object.cpp
//
//  Created by Edmund Kapusniak on 19/02/2020.
//  Copyright © 2020 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "u64val_object.h"

namespace kf
{

u64val_object* u64val_value_internal( vmachine* vm, uint64_t u )
{
    // Values that fit in 48 bits must be represented inline.
    assert( u > U64VAL_BOXED );

    // Check for existing object with this number.
    auto i = vm->u64vals.find( u );
    if ( i != vm->u64vals.end() )
    {
        return i->second;
    }

    // Create new object.
    u64val_object* u64val = new ( object_new( vm, U64VAL_OBJECT, sizeof( u64val_object ) ) ) u64val_object();
    u64val->u = u;

    // Insert object in intern table and return it.
    vm->u64vals.insert_or_assign( u, u64val );
    return u64val;
}

}
