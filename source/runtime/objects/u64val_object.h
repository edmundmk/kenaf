//
//  u64val_object.h
//
//  Created by Edmund Kapusniak on 19/02/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_U64VAL_OBJECT_H
#define KF_U64VAL_OBJECT_H

/*
    u64vals are used to store client pointers or database IDs.  Numbers less
    than or equal to 0x0000'FFFF'FFFF'FFFF (i.e. 48-bits) are stored inline as
    a kind of boxed value.  This is big enough for a user pointer on both AMD64
    and ARM64 - our NaN-boxing assumes a maximum 48-bit memory space anyway.

    Larger numbers are stored as u64val_objects on the GC heap.  There is at
    most one of each number - they are interned similarly to string keys.  This
    allows the same number to compare and hash equally without us having to add
    special cases to handle them.
*/

#include <stdint.h>
#include "../vmachine.h"

namespace kf
{

const uint64_t U64VAL_BOXED = UINT64_C( 0x0000'FFFF'FFFF'FFFF );

struct u64val_object : public object
{
    uint64_t u;
};

/*
    Inline functions.
*/

inline value u64val_value( vmachine* vm, uint64_t u )
{
    extern u64val_object* u64val_value_internal( vmachine* vm, uint64_t u );
    if ( u <= U64VAL_BOXED )
    {
        return box_u64val( u );
    }
    else
    {
        return box_object( u64val_value_internal( vm, u ) );
    }
}

inline bool u64val_check( value v, uint64_t* u )
{
    if ( box_is_u64val( v ) )
    {
        *u = unbox_u64val( v );
        return true;
    }
    else if ( box_is_object_type( v, U64VAL_OBJECT ) )
    {
        *u = ( (u64val_object*)unbox_object( v ) )->u;
        return true;
    }
    return false;
}

}

#endif

