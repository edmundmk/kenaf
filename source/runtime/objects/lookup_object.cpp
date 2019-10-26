//
//  lookup_object.cpp
//
//  Created by Edmund Kapusniak on 25/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "lookup_object.h"
#include "../vm_context.h"

namespace kf
{

layout_object* layout_new( vm_context* vm, object* parent, string_object* key )
{
    layout_object* layout = (layout_object*)object_new( vm, LAYOUT_OBJECT, sizeof( layout_object ) );
    winit( layout->parent, parent );
    winit( layout->key, key );
    layout->cookie = ++vm->next_cookie;

    if ( layout->cookie == 0 )
    {
        throw std::out_of_range( "layout cookies exhausted" );
    }

    if ( key )
    {
        assert( header( key )->flags & FLAG_KEY );
        layout_object* parent_layout = (layout_object*)parent;
        layout->sindex = parent_layout->sindex + 1;
        if ( layout->sindex == 0 )
        {
            throw std::out_of_range( "too many object slots" );
        }

        if ( ! parent_layout->next )
        {
            parent_layout->next = layout;
        }
        else
        {
            vm->splitkey_layouts.insert_or_assign( { parent_layout, key }, layout );
        }
    }
    else
    {
        assert( header( parent )->flags & FLAG_SEALED );
        layout->sindex = (uint32_t)-1;
        vm->instance_layouts.insert_or_assign( (lookup_object*)parent, layout );
    }

    return layout;
}

oslots_object* oslots_new( vm_context* vm, size_t size )
{
    oslots_object* oslots = (oslots_object*)object_new( vm, OSLOTS_OBJECT, size * sizeof( ref_value ) );
    return oslots;
}

lookup_object* lookup_new( vm_context* vm, lookup_object* prototype )
{
    // Seal prototype.
    object_header* prototype_header = header( prototype );
    if ( ( prototype_header->flags & FLAG_SEALED ) == 0 )
    {
        prototype_header->flags |= FLAG_SEALED;
    }

    // Locate instance layout.
    layout_object* instance_layout;
    auto i = vm->instance_layouts.find( prototype );
    if ( i != vm->instance_layouts.end() )
    {
        instance_layout = i->second;
    }
    else
    {
        instance_layout = layout_new( vm, prototype, nullptr );
        vm->instance_layouts.insert_or_assign( prototype, instance_layout );
    }

    // Create object.
    lookup_object* object = (lookup_object*)object_new( vm, LOOKUP_OBJECT, sizeof( lookup_object ) );
    winit( object->oslots, oslots_new( vm, 4 ) );
    winit( object->layout, instance_layout );

    return object;
}

lookup_object* lookup_prototype( vm_context* vm, lookup_object* object )
{
    // Prototype is linked from the top of the object's layout chain.
    layout_object* layout = read( object->layout );
    while ( read( layout->key ) )
    {
        layout = (layout_object*)read( layout->parent );
    }

    return (lookup_object*)read( layout->parent );
}

bool lookup_getsel( vm_context* vm, lookup_object* object, string_object* key, selector* sel )
{
    assert( header( key )->flags & FLAG_KEY );
    layout_object* lookup_layout = read( object->layout );

    // Search layout list for key.
    layout_object* layout = lookup_layout;
    while ( string_object* layout_key = read( layout->key ) )
    {
        if ( layout_key == key )
        {
            sel->cookie = lookup_layout->cookie;
            sel->sindex = layout->sindex;
            sel->slot = nullptr;
            return true;
        }
        layout = (layout_object*)read( layout->parent );
    }

    // Search prototypes.
    while ( object )
    {
        object = (lookup_object*)read( layout->parent );
        layout = read( object->layout );
        while ( string_object* layout_key = read( layout->key ) )
        {
            if ( layout_key == key )
            {
                sel->cookie = lookup_layout->cookie;
                sel->sindex = ~(uint32_t)0;
                sel->slot = &read( object->oslots )->slots[ layout->sindex ];
                return true;
            }
            layout = (layout_object*)read( layout->parent );
        }
    }

    return false;
}

void lookup_setsel( vm_context* vm, lookup_object* object, string_object* key, selector* sel )
{
    assert( header( key )->flags & FLAG_KEY );
    layout_object* lookup_layout = read( object->layout );

    // Search layout list for key.
    layout_object* layout = lookup_layout;
    while ( string_object* layout_key = read( layout->key ) )
    {
        if ( layout_key == key )
        {
            sel->cookie = layout->cookie;
            sel->sindex = layout->sindex;
            sel->slot = nullptr;
            return;
        }
        layout = (layout_object*)read( layout->parent );
    }

    // Check if object is sealed.
    if ( header( object )->flags & FLAG_SEALED )
    {
        throw std::out_of_range( "sealed object" );
    }

    // Determine new layout.
    layout = lookup_layout->next;
    if ( read( layout->key ) != key )
    {
        auto i = vm->splitkey_layouts.find( { lookup_layout, key } );
        if ( i != vm->splitkey_layouts.end() )
        {
            layout = i->second;
        }
        else
        {
            layout = layout_new( vm, lookup_layout, key );
        }
    }
    assert( layout->sindex == lookup_layout->sindex + 1 );

    // Might need to reallocate slots.
    oslots_object* oslots = read( object->oslots );
    uint32_t oslots_size = object_size( vm, oslots ) / sizeof( ref_value );
    if ( layout->sindex >= oslots_size )
    {
        uint32_t expand_size = oslots_size * 2;
        if ( oslots_size >= 16 ) expand_size -= oslots_size / 2;
        oslots_object* expand = oslots_new( vm, expand_size );

        for ( size_t i = 0; i < oslots_size; ++i )
        {
            winit( expand->slots[ i ], read( oslots->slots[ i ] ) );
        }

        write( vm, object->oslots, expand );
    }

    // Update layout.
    write( vm, object->layout, layout );

    // Created slot.
    sel->cookie = layout->cookie;
    sel->sindex = layout->sindex;
    sel->slot = nullptr;
    return;
}

bool lookup_haskey( vm_context* vm, lookup_object* object, string_object* key )
{
    assert( header( key )->flags & FLAG_KEY );

    layout_object* layout = read( object->layout );
    while ( string_object* layout_key = read( layout->key ) )
    {
        if ( layout_key == key )
        {
            return true;
        }
        layout = (layout_object*)read( layout->parent );
    }

    return false;
}

void lookup_delkey( vm_context* vm, lookup_object* object, string_object* key )
{


}

}

