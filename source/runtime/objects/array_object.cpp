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

array_object* array_new( vm_context* vm, size_t capacity )
{
    array_object* array = (array_object*)object_new( vm, ARRAY_OBJECT, sizeof( array_object ) );
    if ( capacity )
    {
        winit( array->aslots, vslots_new( vm, capacity ) );
    }
    return array;
}

void array_resize( vm_context* vm, array_object* array, size_t length )
{
    vslots_object* aslots = read( array->aslots );
    size_t array_length = array->length;

    if ( length <= array_length )
    {
        for ( size_t i = length; i < array_length; ++i )
        {
            write( vm, aslots->slots[ i ], null_value );
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

void array_append( vm_context* vm, array_object* array, value value )
{
    array_extend( vm, array, &value, 1 );
}

void array_extend( vm_context* vm, array_object* array, value* values, size_t vcount )
{
    vslots_object* aslots = read( array->aslots );
    size_t aslots_count = aslots ? object_size( vm, aslots ) / sizeof( ref_value ) : 0;
    size_t array_length = array->length;

    if ( array_length + vcount > aslots_count )
    {
        size_t expand_acount = aslots_count * 2;
        if ( aslots_count > 512 ) expand_acount -= aslots_count / 2;
        expand_acount = std::max< size_t >( 8, std::max( vcount, expand_acount ) );

        vslots_object* expand = vslots_new( vm, expand_acount );
        for ( size_t i = 0; i < array_length; ++i )
        {
            winit( expand->slots[ i ], read( aslots->slots[ i ] ) );
        }

        write( vm, array->aslots, expand );
        aslots = expand;
    }

    for ( size_t i = 0; i < vcount; ++i )
    {
        write( vm, aslots->slots[ array_length + i ], values[ i ] );
    }

    array->length = array_length + vcount;
}

}

