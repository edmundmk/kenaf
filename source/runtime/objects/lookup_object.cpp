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
#include <vector>
#include "../vm/vm_context.h"

namespace kf
{

layout_object* layout_new( vm_context* vm, object* parent, string_object* key )
{
    layout_object* layout = new ( object_new( vm, LAYOUT_OBJECT, sizeof( layout_object ) ) ) layout_object();
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

vslots_object* vslots_new( vm_context* vm, size_t count )
{
    vslots_object* oslots = new ( object_new( vm, VSLOTS_OBJECT, count * sizeof( ref_value ) ) ) vslots_object();
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
    lookup_object* object = new ( object_new( vm, LOOKUP_OBJECT, sizeof( lookup_object ) ) ) lookup_object();
    winit( object->oslots, vslots_new( vm, 4 ) );
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

static layout_object* next_layout( vm_context* vm, layout_object* layout, string_object* key )
{
    layout_object* next_layout = layout->next;

    if ( read( next_layout->key ) != key )
    {
        auto i = vm->splitkey_layouts.find( { layout, key } );
        if ( i != vm->splitkey_layouts.end() )
        {
            next_layout = i->second;
        }
        else
        {
            next_layout = layout_new( vm, layout, key );
        }
    }

    assert( next_layout->sindex == layout->sindex + 1 );
    return next_layout;
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
    layout = next_layout( vm, lookup_layout, key );

    // Might need to reallocate slots.
    vslots_object* oslots = read( object->oslots );
    size_t oslots_count = object_size( vm, oslots ) / sizeof( ref_value );
    if ( layout->sindex >= oslots_count )
    {
        size_t expand_count = oslots_count * 2;
        if ( oslots_count >= 16 ) expand_count -= oslots_count / 2;
        vslots_object* expand = vslots_new( vm, expand_count );

        for ( size_t i = 0; i < oslots_count; ++i )
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
    assert( header( key )->flags & FLAG_KEY );

    // Check if object is sealed.
    if ( header( object )->flags & FLAG_SEALED )
    {
        throw std::out_of_range( "sealed object" );
    }

    // Remember all keys that we search past.
    struct surviving_key { string_object* key; uint32_t sindex; };
    std::vector< surviving_key > surviving_keys;

    // 'Rewind' layout until we hit the key we're deleting.
    layout_object* layout = read( object->layout );
    while ( true )
    {
        string_object* layout_key = read( layout->key );
        if ( ! layout_key )
        {
            return;
        }

        uint32_t sindex = layout->sindex;
        layout = (layout_object*)read( layout->parent );
        if ( layout_key == key )
        {
            break;
        }

        surviving_keys.push_back( { layout_key, sindex } );
    }

    // Starting at layout, re-add keys.
    vslots_object* oslots = read( object->oslots );
    for ( auto i = surviving_keys.crbegin(); i != surviving_keys.crend(); ++i )
    {
        layout = next_layout( vm, layout, i->key );
        assert( layout->sindex = i->sindex - 1 );
        write( vm, oslots->slots[ layout->sindex ], read( oslots->slots[ i->sindex ] ) );
    }

    // Update layout.
    write( vm, object->layout, layout );
}

}

