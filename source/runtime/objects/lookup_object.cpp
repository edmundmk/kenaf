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

layout_object* layout_new( vm_context* vm, object* parent, string_object* key, uint32_t sindex )
{
    return nullptr;
}

slots_object* slots_new( vm_context* vm, size_t size )
{
    return nullptr;
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
        instance_layout = layout_new( vm, prototype, nullptr, -1 );
        vm->instance_layouts.insert_or_assign( prototype, instance_layout );
    }

    // Create object.
    lookup_object* object = (lookup_object*)object_new( vm, LOOKUP_OBJECT, sizeof( lookup_object ) );
    write( vm, object->slots, slots_new( vm, 4 ) );
    write( vm, object->layout, instance_layout );

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

void lookup_getsel( vm_context* vm, lookup_object* object, string_object* key, selector* sel )
{

}

void lookup_setsel( vm_context* vm, lookup_object* object, string_object* key, selector* sel )
{
}

}

