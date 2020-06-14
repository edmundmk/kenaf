//
//  array_object.cpp
//
//  Created by Edmund Kapusniak on 26/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "array_object.h"

namespace kf
{

array_object* array_new( vmachine* vm, size_t capacity )
{
    array_object* array = new ( object_new( vm, ARRAY_OBJECT, sizeof( array_object ) ) ) array_object();
    if ( capacity )
    {
        winit( array->aslots, vslots_new( vm, capacity ) );
    }
    return array;
}

void array_resize( vmachine* vm, array_object* array, size_t length )
{
    vslots_object* aslots = read( array->aslots );
    size_t array_length = array->length;

    if ( length <= array_length )
    {
        // Clear slots at the end.
        for ( size_t i = length; i < array_length; ++i )
        {
            write( vm, aslots->slots[ i ], boxed_null );
        }
    }
    else
    {
        size_t aslots_count = aslots ? object_size( vm, aslots ) / sizeof( ref_value ) : 0;
        if ( length > aslots_count )
        {
            vslots_object* expand = vslots_new( vm, length );
            for ( size_t i = 0; i < array_length; ++i )
            {
                winit( expand->slots[ i ], read( aslots->slots[ i ] ) );
            }

            write( vm, array->aslots, expand );
        }
    }

    array->length = length;
}

value array_append( vmachine* vm, array_object* array, value value )
{
    array_extend( vm, array, &value, 1 );
    return value;
}

static size_t array_expand_length( size_t current, size_t minimum )
{
    size_t expanded = current * 2;
    if ( current > 512 ) expanded -= current / 2;
    expanded = std::max< size_t >( expanded, minimum );
    expanded = std::max< size_t >( expanded, 8 );
    return expanded;
}

void array_extend( vmachine* vm, array_object* array, const value* values, size_t vcount )
{
    vslots_object* aslots = read( array->aslots );
    size_t aslots_count = aslots ? object_size( vm, aslots ) / sizeof( ref_value ) : 0;
    size_t array_length = array->length;

    if ( array_length + vcount > aslots_count )
    {
        size_t expand_acount = array_expand_length( aslots_count, aslots_count + vcount );
        vslots_object* expand = vslots_new( vm, expand_acount );

        for ( size_t i = 0; i < array_length; ++i )
        {
            winit( expand->slots[ i ], read( aslots->slots[ i ] ) );
        }

        write( vm, array->aslots, expand );
        aslots = expand;
    }

    // Uninitialized slots should be null, so no need for full write barrier.
    for ( size_t i = 0; i < vcount; ++i )
    {
        winit( aslots->slots[ array_length + i ], values[ i ] );
    }

    array->length = array_length + vcount;
}

value array_insert( vmachine* vm, array_object* array, size_t index, value value )
{
    vslots_object* aslots = read( array->aslots );
    size_t aslots_count = aslots ? object_size( vm, aslots ) / sizeof( ref_value ) : 0;
    size_t array_length = array->length;

    if ( index > array_length )
    {
        raise_error( ERROR_INDEX, "array index out of range" );
    }

    if ( array_length + 1 <= aslots_count )
    {
        size_t i = array_length;
        while ( i-- > index )
        {
            write( vm, aslots->slots[ i + 1 ], read( aslots->slots[ i ] ) );
        }

        write( vm, aslots->slots[ index ], value );
    }
    else
    {
        size_t expand_acount = array_expand_length( aslots_count, aslots_count + 1 );
        vslots_object* expand = vslots_new( vm, expand_acount );

        for ( size_t i = 0; i < index; ++i )
        {
            winit( expand->slots[ i ], read( aslots->slots[ i ] ) );
        }

        winit( expand->slots[ index ], value );

        for ( size_t i = index; i < array_length; ++i )
        {
            winit( expand->slots[ i + 1 ], read( aslots->slots[ i ] ) );
        }

        write( vm, array->aslots, expand );
    }

    array->length += 1;
    return value;
}

value array_remove( vmachine* vm, array_object* array, size_t index )
{
    vslots_object* aslots = read( array->aslots );
    size_t array_length = array->length;

    if ( index >= array_length )
    {
        raise_error( ERROR_INDEX, "array index out of range" );
    }

    assert( array_length > 0 );
    value value = read( aslots->slots[ index ] );

    for ( size_t i = index; i < array_length - 1; ++i )
    {
        write( vm, aslots->slots[ i ], read( aslots->slots[ i + 1 ] ) );
    }

    // Clear final slot.
    write( vm, aslots->slots[ array_length - 1 ], boxed_null );

    return value;
}

void array_clear( vmachine* vm, array_object* array )
{
    vslots_object* aslots = read( array->aslots );
    size_t array_length = array->length;

    // Clear slots.
    for ( size_t i = 0; i < array_length; ++i )
    {
        write( vm, aslots->slots[ i ], boxed_null );
    }

    array->length = 0;
}

}

