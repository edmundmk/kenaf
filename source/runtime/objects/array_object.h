//
//  array_object.h
//
//  Created by Edmund Kapusniak on 26/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_ARRAY_OBJECT_H
#define KF_ARRAY_OBJECT_H

/*
    An array object.
*/

#include "../vmachine.h"
#include "lookup_object.h"

namespace kf
{

/*
    Array structure.
*/

struct array_object : public object
{
    ref< vslots_object > aslots;
    size_t length;
};

/*
    Functions.
*/

array_object* array_new( vmachine* vm, size_t capacity );
value array_getindex( vmachine* vm, array_object* array, size_t index );
value array_setindex( vmachine* vm, array_object* array, size_t index, value value );
void array_resize( vmachine* vm, array_object* array, size_t length );
value array_append( vmachine* vm, array_object* array, value value );
void array_extend( vmachine* vm, array_object* array, const value* values, size_t vcount );
value array_insert( vmachine* vm, array_object* array, size_t index, value value );
value array_remove( vmachine* vm, array_object* array, size_t index );
void array_clear( vmachine* vm, array_object* array );

/*
    Inline functions.
*/

inline value array_getindex( vmachine* vm, array_object* array, size_t index )
{
    if ( index < array->length )
    {
        return read( read( array->aslots )->slots[ index ] );
    }
    else
    {
        raise_error( ERROR_INDEX, "array index out of range" );
    }
}

inline value array_setindex( vmachine* vm, array_object* array, size_t index, value value )
{
    if ( index < array->length )
    {
        write( vm, read( array->aslots )->slots[ index ], value );
    }
    else
    {
        raise_error( ERROR_INDEX, "array index out of range" );
    }
    return value;
}

};

#endif

