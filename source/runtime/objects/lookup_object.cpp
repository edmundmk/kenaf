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

namespace kf
{

layout_object* layout_new( vmachine* vm, object* parent, string_object* key )
{
    layout_object* layout = new ( object_new( vm, LAYOUT_OBJECT, sizeof( layout_object ) ) ) layout_object();
    winit( layout->parent, parent );
    winit( layout->key, key );
    layout->cookie = ++vm->next_cookie;

    if ( layout->cookie == 0 )
    {
        throw std::runtime_error( "layout cookies exhausted" );
    }

    if ( key )
    {
        assert( header( key )->flags & FLAG_KEY );
        layout_object* parent_layout = (layout_object*)parent;
        layout->sindex = parent_layout->sindex + 1;
        if ( layout->sindex == (uint32_t)-1 )
        {
            throw std::runtime_error( "too many object slots" );
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
        assert( ! parent || header( parent )->type == LOOKUP_OBJECT );
        assert( ! parent || lookup_sealed( vm, (lookup_object*)parent ) );
        layout->sindex = (uint32_t)-1;
        vm->instance_layouts.insert_or_assign( (lookup_object*)parent, layout );
    }

    return layout;
}

vslots_object* vslots_new( vmachine* vm, size_t count )
{
    vslots_object* oslots = new ( object_new( vm, VSLOTS_OBJECT, count * sizeof( ref_value ) ) ) vslots_object();
    return oslots;
}

lookup_object* lookup_new( vmachine* vm, lookup_object* prototype )
{
    // Seal prototype.
    if ( prototype )
    {
        lookup_seal( vm, prototype );
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

lookup_object* lookup_prototype( vmachine* vm, lookup_object* object )
{
    // Prototype is linked from the top of the object's layout chain.
    layout_object* layout = read( object->layout );
    while ( read( layout->key ) )
    {
        layout = (layout_object*)read( layout->parent );
    }

    return (lookup_object*)read( layout->parent );
}

void lookup_seal( vmachine* vm, lookup_object* object )
{
    object_header* object_header = header( object );
    if ( ( object_header->flags & FLAG_SEALED ) == 0 )
    {
        object_header->flags |= FLAG_SEALED;
    }
}

bool lookup_sealed( vmachine* vm, lookup_object* object )
{
    return ( header( object )->flags & FLAG_SEALED ) != 0;
}

static layout_object* next_layout( vmachine* vm, layout_object* layout, string_object* key )
{
    // Check if we can follow the next layout chain.
    layout_object* next_layout = layout->next;
    if ( next_layout && read( next_layout->key ) == key )
    {
        assert( next_layout->sindex == layout->sindex + 1 );
        return next_layout;
    }

    // Otherwise, this is a split.  Might already exist.
    auto i = vm->splitkey_layouts.find( { layout, key } );
    if ( i != vm->splitkey_layouts.end() )
    {
        next_layout = i->second;
    }
    else
    {
        next_layout = layout_new( vm, layout, key );
    }

    assert( next_layout->sindex == layout->sindex + 1 );
    return next_layout;
}

static layout_object* update_layout( vmachine* vm, lookup_object* object, layout_object* layout, string_object* key )
{
    // Determine new layout.
    layout = next_layout( vm, layout, key );

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
    return layout;
}

void lookup_addkeyslot( vmachine* vm, lookup_object* object, size_t index, std::string_view keyslot )
{
    if ( lookup_sealed( vm, object ) )
    {
        raise_error( ERROR_KEY, "object is sealed" );
    }

    layout_object* layout = read( object->layout );
    if ( index != layout->sindex + 1 )
    {
        raise_error( ERROR_KEY, "keyslot added out of order" );
    }

    string_object* key = string_key( vm, keyslot.data(), keyslot.size() );
    layout = update_layout( vm, object, layout, key );
    assert( layout->sindex == index );
}

bool lookup_getsel( vmachine* vm, lookup_object* object, string_object* key, selector* sel )
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
    while ( true )
    {
        object = (lookup_object*)read( layout->parent );
        if ( ! object )
        {
            break;
        }

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

void lookup_setsel( vmachine* vm, lookup_object* object, string_object* key, selector* sel )
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
    if ( lookup_sealed( vm, object ) )
    {
        raise_error( ERROR_KEY, "object is sealed" );
    }

    // Update layout.
    layout = update_layout( vm, object, lookup_layout, key );

    // Created slot.
    sel->cookie = layout->cookie;
    sel->sindex = layout->sindex;
    sel->slot = nullptr;
    return;
}

bool lookup_haskey( vmachine* vm, lookup_object* object, string_object* key )
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

void lookup_delkey( vmachine* vm, lookup_object* object, string_object* key )
{
    assert( header( key )->flags & FLAG_KEY );

    // Check if object is sealed.
    if ( lookup_sealed( vm, object ) )
    {
        raise_error( ERROR_KEY, "object is sealed" );
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
        assert( layout->sindex == i->sindex - 1 );
        write( vm, oslots->slots[ layout->sindex ], read( oslots->slots[ i->sindex ] ) );
    }

    // Update layout.
    write( vm, object->layout, layout );
}

}

