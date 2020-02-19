//
//  lookup_object.h
//
//  Created by Edmund Kapusniak on 25/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_LOOKUP_OBJECT_H
#define KF_LOOKUP_OBJECT_H

/*
    This is the object model for the VM.  Objects support lookup using keys.
    Keys are strings which have been interned in the key map.

    Each object has a reference to a layout object (a 'hidden class').  A
    layout object associates a key with a particular slot index.  Each layout
    object is at the end of a linked list of layout objects.  Taken as a whole,
    this list associates keys with all slots in an object.

    Normally keys are added in the same order to objects with the same
    prototype.  There is a link from each layout to it successor layout.
    However, if keys are added in different orders to different objects, then
    there is a split in the layout chain.  These are stored in the vm's split
    map.

    Currently when we lookup a key on an object we follow the entire layout
    chain in order.  We rely on selectors to accelerate lookup.

    When an object is used as a prototype it is sealed.  This means that its
    layout becomes fixed, and we can accelerate lookups further by caching a
    pointer to its slots directly.  Objects that inherit from a prototype
    object begin life with an empty layout that is the root of the chain for
    this object.  The link between a prototype object and the root of its
    layout chain is stored in the vm's proto map.

    Lookups are performed using a selector.  A selector caches the result of
    lookup so that subsequent lookups with the same selector on the same
    layout can reuse the result.
*/

#include <functional>
#include <string_view>
#include "kenaf/errors.h"
#include "../vmachine.h"
#include "string_object.h"

namespace kf
{

struct layout_object;
struct vslots_object;
struct lookup_object;

/*
    Object structures.
*/

struct layout_object : public object
{
    ref< object > parent;
    ref< string_object > key;
    uint32_t cookie;
    uint32_t sindex;
    layout_object* next;
};

struct vslots_object : public object
{
    ref_value slots[ 0 ];
};

struct lookup_object : public object
{
    ref< vslots_object > oslots;
    ref< layout_object > layout;
};

/*
    Functions.
*/

layout_object* layout_new( vmachine* vm, object* parent, string_object* key );
vslots_object* vslots_new( vmachine* vm, size_t count );
lookup_object* lookup_new( vmachine* vm, lookup_object* prototype );

lookup_object* lookup_prototype( vmachine* vm, lookup_object* object );
void lookup_seal( vmachine* vm, lookup_object* object );
bool lookup_sealed( vmachine* vm, lookup_object* object );

void lookup_addkeyslot( vmachine* vm, lookup_object* object, size_t index, std::string_view keyslot );
value lookup_getkeyslot( vmachine* vm, lookup_object* object, size_t index );
void lookup_setkeyslot( vmachine* vm, lookup_object* object, size_t index, value value );

value lookup_getkey( vmachine* vm, lookup_object* object, string_object* key, selector* sel );
void lookup_setkey( vmachine* vm, lookup_object* object, string_object* key, selector* sel, value value );
bool lookup_haskey( vmachine* vm, lookup_object* object, string_object* key );
void lookup_delkey( vmachine* vm, lookup_object* object, string_object* key );

/*
    Inline functions.
*/

inline value lookup_getkeyslot( vmachine* vm, lookup_object* object, size_t index )
{
    if ( index < read( object->layout )->sindex + 1 )
    {
        return read( read( object->oslots )->slots[ index ] );
    }
    else
    {
        throw key_error( "invalid keyslot index %zu", index );
    }
}

inline void lookup_setkeyslot( vmachine* vm, lookup_object* object, size_t index, value value )
{
    if ( index < read( object->layout )->sindex + 1 )
    {
        write( vm, read( object->oslots )->slots[ index ], value );
    }
    else
    {
        throw key_error( "invalid keyslot index %zu", index );
    }
}

inline value lookup_getkey( vmachine* vm, lookup_object* object, string_object* key, selector* sel )
{
    layout_object* layout = read( object->layout );
    extern bool lookup_getsel( vmachine* vm, lookup_object* object, string_object* key, selector* sel );
    if ( sel->cookie == layout->cookie || lookup_getsel( vm, object, key, sel ) )
    {
        if ( sel->sindex != ~(uint32_t)0 )
        {
            return read( read( object->oslots )->slots[ sel->sindex ] );
        }
        else
        {
            return read( *sel->slot );
        }
    }
    else
    {
        throw key_error( "key '%s' not found", key->text );
    }
}

inline void lookup_setkey( vmachine* vm, lookup_object* object, string_object* key, selector* sel, value value )
{
    layout_object* layout = read( object->layout );
    if ( sel->cookie != layout->cookie || sel->sindex == ~(uint32_t)0 )
    {
        extern void lookup_setsel( vmachine* vm, lookup_object* object, string_object* key, selector* sel );
        lookup_setsel( vm, object, key, sel );
    }
    write( vm, read( object->oslots )->slots[ sel->sindex ], value );
}

}

#endif
