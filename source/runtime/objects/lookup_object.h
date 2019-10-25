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
#include "object_model.h"
#include "string_object.h"

namespace kf
{

struct layout_object;
struct slots_object;
struct lookup_object;
struct selector;

/*
    Key for split map.
*/

struct layout_hashkey
{
    layout_object* layout;
    string_object* key;
};

inline bool operator == ( const layout_hashkey& a, const layout_hashkey& b )
{
    return a.layout == b.layout && a.key == b.key;
}

inline bool operator != ( const layout_hashkey& a, const layout_hashkey& b )
{
    return ! operator == ( a, b );
}

/*
    Object structures.
*/

struct layout_object
{
    ref< object > parent;
    ref< string_object > key;
    uint32_t cookie;
    uint32_t sindex;
    layout_object* next;
};

struct slots_object
{
    ref_value slots[];
};

struct lookup_object
{
    ref< slots_object > slots;
    ref< layout_object > layout;
};

/*
    Selector.
*/

struct selector
{
    uint32_t cookie;
    uint32_t sindex;
    ref_value* slot;
};

/*
    Lookup functions.
*/

lookup_object* lookup_new( vm_context* vm, lookup_object* prototype );
lookup_object* lookup_prototype( vm_context* vm, lookup_object* object );
value lookup_getkey( vm_context* vm, lookup_object* object, string_object* key, selector* sel );
void lookup_setkey( vm_context* vm, lookup_object* object, string_object* key, selector* sel, value value );

/*
    Inline functions.
*/

inline value lookup_getkey( vm_context* vm, lookup_object* object, string_object* key, selector* sel )
{
    layout_object* layout = read( object->layout );
    if ( sel->cookie != layout->cookie )
    {
        extern void lookup_selector( vm_context* vm, lookup_object* object, string_object* key, selector* sel );
        lookup_selector( vm, object, key, sel );
    }
    if ( sel->sindex != ~(uint32_t)0 )
    {
        slots_object* slots = read( object->slots );
        return read( slots->slots[ sel->sindex ] );
    }
    else
    {
        return read( *sel->slot );
    }
}

inline void lookup_setkey( vm_context* vm, lookup_object* object, string_object* key, selector* sel, value value )
{
    layout_object* layout = read( object->layout );
    if ( sel->cookie != layout->cookie )
    {
        extern void lookup_selector( vm_context* vm, lookup_object* object, string_object* key, selector* sel );
        lookup_selector( vm, object, key, sel );
    }
    if ( sel->sindex != ~(uint32_t)0 )
    {
        slots_object* slots = read( object->slots );
        write( vm, slots->slots[ sel->sindex ], value );
    }
    else
    {
        write( vm, *sel->slot, value );
    }
}

}

template <> struct std::hash< kf::layout_hashkey > : private std::hash< std::string_view >
{
    size_t operator () ( const kf::layout_hashkey& k ) { return std::hash< std::string_view >::operator () ( std::string_view( (char*)&k, sizeof( k ) ) ); }
};

#endif
